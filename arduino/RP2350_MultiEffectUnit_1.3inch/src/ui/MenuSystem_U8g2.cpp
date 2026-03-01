#include "MenuSystem_U8g2.h"

MenuSystem_U8g2::MenuSystem_U8g2(U8G2* disp, RotaryEncoderUI* enc)
    : display(disp), encoder(enc), effectCount(0), parameterCount(0),
      currentMode(MENU_MAIN), selectedEffect(0), selectedParameter(0),
      currentEffectIndex(-1) {
}

void MenuSystem_U8g2::begin() {
    currentMode = MENU_MAIN;
    selectedEffect = 0;
    selectedParameter = 0;
}

void MenuSystem_U8g2::update() {
    encoder->update();

    if (currentMode == MENU_MAIN) {
        handleMainMenuInput();
    } else {
        handleParameterMenuInput();
    }
}

void MenuSystem_U8g2::draw() {
    if (currentMode == MENU_MAIN) {
        drawMainMenu();
    } else {
        drawParameterMenu();
    }
}

void MenuSystem_U8g2::addEffect(const char* name, bool* enabledPtr) {
    if (effectCount < MAX_EFFECTS) {
        effects[effectCount].name = name;
        effects[effectCount].enabled = enabledPtr;
        effectCount++;
    }
}

void MenuSystem_U8g2::addParameter(const char* name, float minVal, float maxVal, float defaultVal) {
    if (parameterCount < MAX_PARAMETERS) {
        parameters[parameterCount].name = name;
        parameters[parameterCount].minValue = minVal;
        parameters[parameterCount].maxValue = maxVal;
        parameters[parameterCount].currentValue = defaultVal;

        // Calculate reasonable step size
        float range = maxVal - minVal;
        parameters[parameterCount].step = range / 100.0f;

        // Extract effect index from parameter name (format: "EffectName:ParamName")
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

uint8_t MenuSystem_U8g2::getEffectIndexFromName(const char* name) {
    // Map short names to effect indices
    if (strncmp(name, "NG", 2) == 0) return 0;  // NoiseGate
    if (strncmp(name, "OD", 2) == 0) return 1;  // Overdrive
    if (strncmp(name, "CH", 2) == 0) return 2;  // Chorus
    if (strncmp(name, "DL", 2) == 0) return 3;  // Delay
    return 0;
}

float MenuSystem_U8g2::getParameterValue(const char* name) {
    for (uint8_t i = 0; i < parameterCount; i++) {
        if (strcmp(parameters[i].name, name) == 0) {
            return parameters[i].currentValue;
        }
    }
    return 0.0f;
}

void MenuSystem_U8g2::setParameterValue(const char* name, float value) {
    for (uint8_t i = 0; i < parameterCount; i++) {
        if (strcmp(parameters[i].name, name) == 0) {
            parameters[i].currentValue = constrain(value, parameters[i].minValue, parameters[i].maxValue);
            return;
        }
    }
}

void MenuSystem_U8g2::handleBackButton() {
    if (currentMode == MENU_PARAMETERS) {
        // Go back to main menu
        currentMode = MENU_MAIN;
    }
}

void MenuSystem_U8g2::handleConfirmButton() {
    if (currentMode == MENU_MAIN) {
        // Enter parameter menu for selected effect
        currentEffectIndex = selectedEffect;
        currentMode = MENU_PARAMETERS;
        selectedParameter = 0;
    } else {
        // In parameter menu, confirm does nothing (use back to exit)
    }
}

void MenuSystem_U8g2::handleMainMenuInput() {
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
        // Toggle effect on/off
        *effects[selectedEffect].enabled = !(*effects[selectedEffect].enabled);
    }
}

void MenuSystem_U8g2::handleParameterMenuInput() {
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

    if (delta != 0) {
        // Adjust selected parameter's value
        uint8_t paramIndex = firstParam + selectedParameter;
        if (paramIndex < parameterCount) {
            parameters[paramIndex].currentValue += delta * parameters[paramIndex].step;
            parameters[paramIndex].currentValue = constrain(
                parameters[paramIndex].currentValue,
                parameters[paramIndex].minValue,
                parameters[paramIndex].maxValue
            );
        }
    }
}

void MenuSystem_U8g2::drawMainMenu() {
    display->clearBuffer();
    display->setFont(u8g2_font_6x10_tf);

    // Title
    display->drawStr(0, 10, "Effects Chain:");
    display->drawLine(0, 12, 128, 12);

    // Display effects (show 4 at a time)
    uint8_t startIndex = 0;
    if (selectedEffect > 3) {
        startIndex = selectedEffect - 3;
    }

    for (uint8_t i = 0; i < 4 && (startIndex + i) < effectCount; i++) {
        uint8_t effectIdx = startIndex + i;
        uint8_t y = 24 + (i * 12);

        // Highlight selected effect
        if (effectIdx == selectedEffect) {
            display->setDrawColor(1);
            display->drawBox(0, y - 9, 128, 11);
            display->setDrawColor(0);  // Inverted text
        } else {
            display->setDrawColor(1);  // Normal text
        }

        // Show enabled/disabled state
        char buffer[32];
        if (*effects[effectIdx].enabled) {
            sprintf(buffer, "[X] %s", effects[effectIdx].name);
        } else {
            sprintf(buffer, "[ ] %s", effects[effectIdx].name);
        }

        display->drawStr(2, y, buffer);
    }

    display->setDrawColor(1);  // Reset to normal
    display->sendBuffer();
}

void MenuSystem_U8g2::drawParameterMenu() {
    display->clearBuffer();
    display->setFont(u8g2_font_6x10_tf);

    // Title - show effect name
    char title[32];
    sprintf(title, "%s Params:", effects[currentEffectIndex].name);
    display->drawStr(0, 10, title);
    display->drawLine(0, 12, 128, 12);

    // Find and display parameters for this effect
    uint8_t displayLine = 0;
    for (uint8_t i = 0; i < parameterCount; i++) {
        if (parameters[i].effectIndex == currentEffectIndex) {
            uint8_t y = 24 + (displayLine * 12);

            if (displayLine == selectedParameter) {
                display->setDrawColor(1);
                display->drawBox(0, y - 9, 128, 11);
                display->setDrawColor(0);  // Inverted text
            } else {
                display->setDrawColor(1);  // Normal text
            }

            // Extract parameter name (after colon)
            String fullName = String(parameters[i].name);
            int colonPos = fullName.indexOf(':');
            String paramName = (colonPos > 0) ? fullName.substring(colonPos + 1) : fullName;

            // Format value display
            char buffer[32];
            sprintf(buffer, "%s: %.1f", paramName.c_str(), parameters[i].currentValue);
            display->drawStr(2, y, buffer);

            displayLine++;

            if (displayLine >= 4) break;  // Max 4 parameters visible
        }
    }

    // Show hint at bottom
    display->setDrawColor(1);  // Reset to normal
    display->setFont(u8g2_font_5x7_tf);
    display->drawStr(0, 62, "BACK to exit");

    display->setDrawColor(1);  // Reset to normal
    display->sendBuffer();
}
