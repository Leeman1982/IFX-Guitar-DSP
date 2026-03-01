#include "MenuSystem.h"

MenuSystem::MenuSystem(Adafruit_SSD1306* disp, RotaryEncoderUI* enc)
    : display(disp), encoder(enc), effectCount(0), parameterCount(0),
      currentMode(MENU_MAIN), selectedEffect(0), selectedParameter(0),
      currentEffectIndex(-1) {
}

void MenuSystem::begin() {
    currentMode = MENU_MAIN;
    selectedEffect = 0;
    selectedParameter = 0;
}

void MenuSystem::update() {
    encoder->update();

    if (currentMode == MENU_MAIN) {
        handleMainMenuInput();
    } else {
        handleParameterMenuInput();
    }
}

void MenuSystem::draw() {
    if (currentMode == MENU_MAIN) {
        drawMainMenu();
    } else {
        drawParameterMenu();
    }
}

void MenuSystem::addEffect(const char* name, bool* enabledPtr) {
    if (effectCount < MAX_EFFECTS) {
        effects[effectCount].name = name;
        effects[effectCount].enabled = enabledPtr;
        effectCount++;
    }
}

void MenuSystem::addParameter(const char* name, float minVal, float maxVal, float defaultVal) {
    if (parameterCount < MAX_PARAMETERS) {
        parameters[parameterCount].name = name;
        parameters[parameterCount].minValue = minVal;
        parameters[parameterCount].maxValue = maxVal;
        parameters[parameterCount].currentValue = defaultVal;

        // Calculate reasonable step size
        float range = maxVal - minVal;
        parameters[parameterCount].step = range / 100.0f;

        // Extract effect index from parameter name (format: "EffectName:ParamName")
        // For simplicity, we'll determine effect by order of parameter addition
        String paramName = String(name);
        int colonPos = paramName.indexOf(':');
        if (colonPos > 0) {
            String effectPrefix = paramName.substring(0, colonPos);
            parameters[parameterCount].effectIndex = getEffectIndexFromName(effectPrefix.c_str());
        } else {
            parameters[parameterCount].effectIndex = 0;
        }

        parameterCount++;
    }
}

uint8_t MenuSystem::getEffectIndexFromName(const char* name) {
    // Map short names to effect indices
    if (strncmp(name, "NG", 2) == 0) return 0;  // NoiseGate
    if (strncmp(name, "OD", 2) == 0) return 1;  // Overdrive
    if (strncmp(name, "CH", 2) == 0) return 2;  // Chorus
    if (strncmp(name, "DL", 2) == 0) return 3;  // Delay
    return 0;
}

float MenuSystem::getParameterValue(const char* name) {
    for (uint8_t i = 0; i < parameterCount; i++) {
        if (strcmp(parameters[i].name, name) == 0) {
            return parameters[i].currentValue;
        }
    }
    return 0.0f;
}

void MenuSystem::setParameterValue(const char* name, float value) {
    for (uint8_t i = 0; i < parameterCount; i++) {
        if (strcmp(parameters[i].name, name) == 0) {
            parameters[i].currentValue = constrain(value, parameters[i].minValue, parameters[i].maxValue);
            return;
        }
    }
}

void MenuSystem::handleMainMenuInput() {
    int8_t delta = encoder->getDelta();

    if (delta != 0) {
        selectedEffect += delta;
        if (selectedEffect >= effectCount) {
            selectedEffect = 0;
        } else if (selectedEffect < 0) {
            selectedEffect = effectCount - 1;
        }
    }

    if (encoder->wasClicked()) {
        // Toggle effect on/off OR enter parameter menu
        if (encoder->isPressed()) {
            // Long press - enter parameter menu
            currentEffectIndex = selectedEffect;
            currentMode = MENU_PARAMETERS;
            selectedParameter = 0;
        } else {
            // Short press - toggle effect
            *effects[selectedEffect].enabled = !(*effects[selectedEffect].enabled);
        }
    }
}

void MenuSystem::handleParameterMenuInput() {
    int8_t delta = encoder->getDelta();

    // Find parameters for current effect
    uint8_t firstParam = 255;
    uint8_t paramCountForEffect = 0;

    for (uint8_t i = 0; i < parameterCount; i++) {
        if (parameters[i].effectIndex == currentEffectIndex) {
            if (firstParam == 255) {
                firstParam = i;
            }
            paramCountForEffect++;
        }
    }

    if (paramCountForEffect == 0) {
        // No parameters, go back to main menu
        currentMode = MENU_MAIN;
        return;
    }

    if (encoder->wasClicked()) {
        // Exit to main menu
        currentMode = MENU_MAIN;
        return;
    }

    if (delta != 0) {
        // Adjust selected parameter's value
        uint8_t paramIndex = firstParam + selectedParameter;
        parameters[paramIndex].currentValue += delta * parameters[paramIndex].step;
        parameters[paramIndex].currentValue = constrain(
            parameters[paramIndex].currentValue,
            parameters[paramIndex].minValue,
            parameters[paramIndex].maxValue
        );
    }
}

void MenuSystem::drawMainMenu() {
    display->clearDisplay();
    display->setTextSize(1);
    display->setTextColor(SSD1306_WHITE);

    // Title
    display->setCursor(0, 0);
    display->println("Effects Chain:");
    display->drawLine(0, 10, 128, 10, SSD1306_WHITE);

    // Display effects (show 4 at a time)
    uint8_t startIndex = 0;
    if (selectedEffect > 3) {
        startIndex = selectedEffect - 3;
    }

    for (uint8_t i = 0; i < 4 && (startIndex + i) < effectCount; i++) {
        uint8_t effectIdx = startIndex + i;
        uint8_t y = 14 + (i * 12);

        // Highlight selected effect
        if (effectIdx == selectedEffect) {
            display->fillRect(0, y - 1, 128, 10, SSD1306_WHITE);
            display->setTextColor(SSD1306_BLACK);
        } else {
            display->setTextColor(SSD1306_WHITE);
        }

        display->setCursor(2, y);

        // Show enabled/disabled state
        if (*effects[effectIdx].enabled) {
            display->print("[X] ");
        } else {
            display->print("[ ] ");
        }

        display->print(effects[effectIdx].name);
    }

    display->display();
}

void MenuSystem::drawParameterMenu() {
    display->clearDisplay();
    display->setTextSize(1);
    display->setTextColor(SSD1306_WHITE);

    // Title - show effect name
    display->setCursor(0, 0);
    display->print(effects[currentEffectIndex].name);
    display->println(" Params:");
    display->drawLine(0, 10, 128, 10, SSD1306_WHITE);

    // Find and display parameters for this effect
    uint8_t displayLine = 0;
    for (uint8_t i = 0; i < parameterCount; i++) {
        if (parameters[i].effectIndex == currentEffectIndex) {
            uint8_t y = 14 + (displayLine * 12);

            if (displayLine == selectedParameter) {
                display->fillRect(0, y - 1, 128, 10, SSD1306_WHITE);
                display->setTextColor(SSD1306_BLACK);
            } else {
                display->setTextColor(SSD1306_WHITE);
            }

            display->setCursor(2, y);

            // Extract parameter name (after colon)
            String fullName = String(parameters[i].name);
            int colonPos = fullName.indexOf(':');
            String paramName = (colonPos > 0) ? fullName.substring(colonPos + 1) : fullName;

            display->print(paramName);
            display->print(": ");
            display->print(parameters[i].currentValue, 1);

            displayLine++;

            if (displayLine >= 4) break;  // Max 4 parameters visible
        }
    }

    // Show hint
    display->setTextColor(SSD1306_WHITE);
    display->setCursor(0, 56);
    display->print("Click to exit");

    display->display();
}
