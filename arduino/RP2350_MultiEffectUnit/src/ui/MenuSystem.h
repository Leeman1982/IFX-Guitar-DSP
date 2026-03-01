#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include "RotaryEncoder.h"

#define MAX_EFFECTS 8
#define MAX_PARAMETERS 32

enum MenuMode {
    MENU_MAIN,          // Show all effects and their enable/disable state
    MENU_PARAMETERS     // Show parameters for selected effect
};

struct Effect {
    const char* name;
    bool* enabled;
};

struct Parameter {
    const char* name;
    float minValue;
    float maxValue;
    float currentValue;
    float step;
    uint8_t effectIndex;  // Which effect this parameter belongs to
};

class MenuSystem {
public:
    MenuSystem(Adafruit_SSD1306* disp, RotaryEncoderUI* enc);

    void begin();
    void update();
    void draw();

    void addEffect(const char* name, bool* enabledPtr);
    void addParameter(const char* name, float minVal, float maxVal, float defaultVal);

    float getParameterValue(const char* name);
    void setParameterValue(const char* name, float value);

private:
    Adafruit_SSD1306* display;
    RotaryEncoderUI* encoder;

    Effect effects[MAX_EFFECTS];
    uint8_t effectCount;

    Parameter parameters[MAX_PARAMETERS];
    uint8_t parameterCount;

    MenuMode currentMode;
    uint8_t selectedEffect;
    uint8_t selectedParameter;
    int8_t currentEffectIndex;  // For parameter view

    void drawMainMenu();
    void drawParameterMenu();
    void handleMainMenuInput();
    void handleParameterMenuInput();

    uint8_t getEffectIndexFromName(const char* name);
};

#endif
