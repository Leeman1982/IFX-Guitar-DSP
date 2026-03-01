#ifndef MENUSYSTEM_U8G2_H
#define MENUSYSTEM_U8G2_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "RotaryEncoder.h"

#define MAX_EFFECTS 8
#define MAX_PARAMETERS 32

enum MenuMode {
  MENU_MAIN,
  MENU_PARAMETERS
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
  uint8_t effectIndex;
};

class MenuSystem_U8g2 {
public:
  MenuSystem_U8g2(U8G2* disp, RotaryEncoderUI* enc);

  void begin();
  void update();
  void draw();

  void addEffect(const char* name, bool* enabledPtr);
  void addParameter(const char* name, float minVal, float maxVal, float defaultVal);

  float getParameterValue(const char* name);
  void setParameterValue(const char* name, float value);

  void handleBackButton();
  void handleConfirmButton();

private:
  U8G2* display;
  RotaryEncoderUI* encoder;

  Effect effects[MAX_EFFECTS];
  Parameter parameters[MAX_PARAMETERS];
  uint8_t effectCount;
  uint8_t parameterCount;

  MenuMode currentMode;
  int8_t selectedEffect;
  int8_t selectedParameter;
  int8_t currentEffectIndex;

  void handleMainMenuInput();
  void handleParameterMenuInput();

  void drawMainMenu();
  void drawParameterMenu();

  uint8_t getEffectIndexFromName(const char* name);
};

#endif
