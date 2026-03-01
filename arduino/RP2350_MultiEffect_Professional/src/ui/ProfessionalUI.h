#ifndef PROFESSIONAL_UI_H
#define PROFESSIONAL_UI_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "RotaryEncoder.h"

#define MAX_EFFECTS 8
#define MAX_PARAMETERS 32
#define MAX_PARAMS_PER_EFFECT 8

enum UIPage {
  PAGE_HOME,          // VU meters + active effects
  PAGE_EFFECT_CHAIN,  // Visual effect chain
  PAGE_EFFECT_PARAM,  // Effect parameter editing
  PAGE_INFO           // System info
};

struct Effect {
  const char* name;
  const char* shortName;
  bool* enabled;
  uint8_t paramCount;
  uint8_t firstParamIndex;
};

struct Parameter {
  const char* name;
  float minValue;
  float maxValue;
  float currentValue;
  const char* unit;
  uint8_t effectIndex;
};

class ProfessionalUI {
public:
  ProfessionalUI(U8G2* disp, RotaryEncoderUI* enc);

  void begin();
  void update();
  void draw();

  void addEffect(const char* name, const char* shortName, bool* enabledPtr);
  void addParameter(uint8_t effectIndex, const char* name, float minVal, float maxVal, float defaultVal, const char* unit);

  float* getEffectParameters(uint8_t effectIndex);

  void setCPULoad(float load);
  void setVULevels(float peakL, float peakR, float rmsL, float rmsR);

private:
  U8G2* display;
  RotaryEncoderUI* encoder;

  Effect effects[MAX_EFFECTS];
  Parameter parameters[MAX_PARAMETERS];
  float effectParamCache[MAX_EFFECTS][MAX_PARAMS_PER_EFFECT];

  uint8_t effectCount;
  uint8_t parameterCount;

  UIPage currentPage;
  int8_t selectedEffect;
  int8_t selectedParameter;

  float cpuLoad;
  float vuPeakL, vuPeakR, vuRmsL, vuRmsR;

  // VU meter smoothing
  float vuPeakLSmooth, vuPeakRSmooth;
  float vuRmsLSmooth, vuRmsRSmooth;

  // Input handling
  void handleInput();
  void handleHomePage();
  void handleEffectChainPage();
  void handleParameterPage();
  void handleInfoPage();

  // Drawing functions
  void drawHomePage();
  void drawEffectChainPage();
  void drawParameterPage();
  void drawInfoPage();

  // UI Components
  void drawHeader(const char* title);
  void drawFooter(const char* left, const char* right);
  void drawVUMeter(uint8_t x, uint8_t y, uint8_t width, uint8_t height, float level, bool isPeak);
  void drawProgressBar(uint8_t x, uint8_t y, uint8_t width, uint8_t height, float value, float min, float max);
  void drawEffectIcon(uint8_t x, uint8_t y, uint8_t effectIndex, bool active);
  void drawBattery(uint8_t x, uint8_t y, float level);

  // Helper functions
  float mapFloat(float x, float in_min, float in_max, float out_min, float out_max);
  void updateParameterCache();
};

#endif
