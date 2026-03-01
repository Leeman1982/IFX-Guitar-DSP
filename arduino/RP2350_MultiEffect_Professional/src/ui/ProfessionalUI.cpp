#include "ProfessionalUI.h"

ProfessionalUI::ProfessionalUI(U8G2* disp, RotaryEncoderUI* enc)
    : display(disp), encoder(enc), effectCount(0), parameterCount(0),
      currentPage(PAGE_HOME), selectedEffect(0), selectedParameter(0),
      cpuLoad(0.0f), vuPeakL(0.0f), vuPeakR(0.0f), vuRmsL(0.0f), vuRmsR(0.0f),
      vuPeakLSmooth(0.0f), vuPeakRSmooth(0.0f), vuRmsLSmooth(0.0f), vuRmsRSmooth(0.0f) {
  // Initialize parameter cache
  for (uint8_t i = 0; i < MAX_EFFECTS; i++) {
    for (uint8_t j = 0; j < MAX_PARAMS_PER_EFFECT; j++) {
      effectParamCache[i][j] = 0.0f;
    }
  }
}

void ProfessionalUI::begin() {
  currentPage = PAGE_HOME;
  selectedEffect = 0;
  selectedParameter = 0;
}

void ProfessionalUI::update() {
  encoder->update();
  handleInput();
  updateParameterCache();

  // Smooth VU meter values
  const float alpha = 0.2f;
  vuPeakLSmooth = vuPeakLSmooth * (1.0f - alpha) + vuPeakL * alpha;
  vuPeakRSmooth = vuPeakRSmooth * (1.0f - alpha) + vuPeakR * alpha;
  vuRmsLSmooth = vuRmsLSmooth * (1.0f - alpha) + vuRmsL * alpha;
  vuRmsRSmooth = vuRmsRSmooth * (1.0f - alpha) + vuRmsR * alpha;
}

void ProfessionalUI::draw() {
  switch (currentPage) {
    case PAGE_HOME:
      drawHomePage();
      break;
    case PAGE_EFFECT_CHAIN:
      drawEffectChainPage();
      break;
    case PAGE_EFFECT_PARAM:
      drawParameterPage();
      break;
    case PAGE_INFO:
      drawInfoPage();
      break;
  }
}

void ProfessionalUI::addEffect(const char* name, const char* shortName, bool* enabledPtr) {
  if (effectCount < MAX_EFFECTS) {
    effects[effectCount].name = name;
    effects[effectCount].shortName = shortName;
    effects[effectCount].enabled = enabledPtr;
    effects[effectCount].paramCount = 0;
    effects[effectCount].firstParamIndex = parameterCount;
    effectCount++;
  }
}

void ProfessionalUI::addParameter(uint8_t effectIndex, const char* name, float minVal, float maxVal, float defaultVal, const char* unit) {
  if (parameterCount < MAX_PARAMETERS && effectIndex < effectCount) {
    parameters[parameterCount].name = name;
    parameters[parameterCount].minValue = minVal;
    parameters[parameterCount].maxValue = maxVal;
    parameters[parameterCount].currentValue = defaultVal;
    parameters[parameterCount].unit = unit;
    parameters[parameterCount].effectIndex = effectIndex;

    effects[effectIndex].paramCount++;
    parameterCount++;
  }
}

float* ProfessionalUI::getEffectParameters(uint8_t effectIndex) {
  if (effectIndex < effectCount) {
    return effectParamCache[effectIndex];
  }
  return nullptr;
}

void ProfessionalUI::setCPULoad(float load) {
  cpuLoad = load;
}

void ProfessionalUI::setVULevels(float peakL, float peakR, float rmsL, float rmsR) {
  vuPeakL = peakL;
  vuPeakR = peakR;
  vuRmsL = rmsL;
  vuRmsR = rmsR;
}

void ProfessionalUI::handleInput() {
  int8_t delta = encoder->getDelta();
  bool clicked = encoder->wasClicked();
  bool longPress = encoder->wasLongPress();

  switch (currentPage) {
    case PAGE_HOME:
      handleHomePage();
      break;
    case PAGE_EFFECT_CHAIN:
      handleEffectChainPage();
      break;
    case PAGE_EFFECT_PARAM:
      handleParameterPage();
      break;
    case PAGE_INFO:
      handleInfoPage();
      break;
  }

  // Long press always goes to home
  if (longPress) {
    currentPage = PAGE_HOME;
    selectedEffect = 0;
  }
}

void ProfessionalUI::handleHomePage() {
  int8_t delta = encoder->getDelta();

  if (delta != 0) {
    // Cycle through pages
    int8_t page = (int8_t)currentPage + delta;
    if (page < 0) page = PAGE_INFO;
    if (page > PAGE_INFO) page = PAGE_HOME;
    currentPage = (UIPage)page;
  }

  if (encoder->wasClicked()) {
    currentPage = PAGE_EFFECT_CHAIN;
    selectedEffect = 0;
  }
}

void ProfessionalUI::handleEffectChainPage() {
  int8_t delta = encoder->getDelta();

  if (delta != 0) {
    selectedEffect += delta;
    if (selectedEffect < 0) selectedEffect = effectCount - 1;
    if (selectedEffect >= effectCount) selectedEffect = 0;
  }

  if (encoder->wasClicked()) {
    // Toggle effect or enter parameter page
    if (effects[selectedEffect].paramCount > 0) {
      currentPage = PAGE_EFFECT_PARAM;
      selectedParameter = 0;
    } else {
      *effects[selectedEffect].enabled = !(*effects[selectedEffect].enabled);
    }
  }
}

void ProfessionalUI::handleParameterPage() {
  int8_t delta = encoder->getDelta();

  if (delta != 0) {
    // Adjust parameter value
    uint8_t paramIdx = effects[selectedEffect].firstParamIndex + selectedParameter;
    if (paramIdx < parameterCount) {
      float step = (parameters[paramIdx].maxValue - parameters[paramIdx].minValue) / 100.0f;
      parameters[paramIdx].currentValue += delta * step;
      parameters[paramIdx].currentValue = constrain(
        parameters[paramIdx].currentValue,
        parameters[paramIdx].minValue,
        parameters[paramIdx].maxValue
      );
    }
  }

  if (encoder->wasClicked()) {
    // Next parameter or back to chain
    selectedParameter++;
    if (selectedParameter >= effects[selectedEffect].paramCount) {
      selectedParameter = 0;
      currentPage = PAGE_EFFECT_CHAIN;
    }
  }
}

void ProfessionalUI::handleInfoPage() {
  if (encoder->wasClicked()) {
    currentPage = PAGE_HOME;
  }
}

void ProfessionalUI::drawHomePage() {
  display->clearBuffer();

  // Header
  drawHeader("GUITAR DSP");

  // VU Meters - Left and Right
  uint8_t meterHeight = 8;
  uint8_t meterY = 16;

  // Left channel label
  display->setFont(u8g2_font_5x7_tr);
  display->drawStr(2, meterY + 6, "L");

  // Left VU meter
  drawVUMeter(12, meterY, 110, meterHeight, vuRmsLSmooth, false);

  // Right channel label
  display->drawStr(2, meterY + meterHeight + 9, "R");

  // Right VU meter
  drawVUMeter(12, meterY + meterHeight + 3, 110, meterHeight, vuRmsRSmooth, false);

  // Active effects display
  uint8_t effectsY = 36;
  display->setFont(u8g2_font_5x7_tr);
  display->drawStr(2, effectsY, "Active:");

  uint8_t iconX = 2;
  uint8_t iconY = effectsY + 4;
  for (uint8_t i = 0; i < effectCount; i++) {
    if (*effects[i].enabled) {
      drawEffectIcon(iconX, iconY, i, true);
      iconX += 32;
      if (iconX > 100) break; // Don't overflow
    }
  }

  // Footer with CPU load
  char footer[32];
  sprintf(footer, "CPU:%d%%", (int)cpuLoad);
  drawFooter(footer, "Press");

  display->sendBuffer();
}

void ProfessionalUI::drawEffectChainPage() {
  display->clearBuffer();

  // Header
  drawHeader("EFFECT CHAIN");

  // Draw effect chain
  uint8_t y = 16;
  for (uint8_t i = 0; i < effectCount && i < 4; i++) {
    uint8_t lineY = y + (i * 12);

    // Highlight selected
    if (i == selectedEffect) {
      display->drawBox(0, lineY - 9, 128, 11);
      display->setDrawColor(0); // Inverted text
    } else {
      display->setDrawColor(1);
    }

    // Effect icon
    drawEffectIcon(2, lineY - 7, i, *effects[i].enabled);

    // Effect name
    display->setFont(u8g2_font_6x10_tr);
    display->drawStr(26, lineY, effects[i].name);

    // Status indicator
    if (*effects[i].enabled) {
      display->drawStr(110, lineY, "ON");
    } else {
      display->drawStr(108, lineY, "OFF");
    }

    display->setDrawColor(1); // Reset
  }

  drawFooter("Rotate", "Select");
  display->sendBuffer();
}

void ProfessionalUI::drawParameterPage() {
  display->clearBuffer();

  // Header with effect name
  drawHeader(effects[selectedEffect].name);

  // Draw current parameter
  uint8_t paramIdx = effects[selectedEffect].firstParamIndex + selectedParameter;
  if (paramIdx < parameterCount) {
    Parameter* param = &parameters[paramIdx];

    // Parameter name
    display->setFont(u8g2_font_7x14_tr);
    display->drawStr(4, 24, param->name);

    // Parameter value
    char valueStr[32];
    if (strcmp(param->unit, "%") == 0) {
      sprintf(valueStr, "%.0f%%", param->currentValue * 100.0f);
    } else if (strcmp(param->unit, "") == 0) {
      sprintf(valueStr, "%.0f", param->currentValue);
    } else {
      sprintf(valueStr, "%.1f %s", param->currentValue, param->unit);
    }

    display->setFont(u8g2_font_helvB10_tr);
    uint8_t strWidth = display->getStrWidth(valueStr);
    display->drawStr(64 - strWidth/2, 42, valueStr);

    // Progress bar
    drawProgressBar(4, 48, 120, 8, param->currentValue, param->minValue, param->maxValue);
  }

  // Footer
  char footer[32];
  sprintf(footer, "%d/%d", selectedParameter + 1, effects[selectedEffect].paramCount);
  drawFooter(footer, "Next");

  display->sendBuffer();
}

void ProfessionalUI::drawInfoPage() {
  display->clearBuffer();

  // Header
  drawHeader("SYSTEM INFO");

  // Display system information
  display->setFont(u8g2_font_6x10_tr);

  uint8_t y = 18;
  display->drawStr(4, y, "Sample Rate: 48kHz");
  y += 11;
  display->drawStr(4, y, "Bit Depth: 16-bit");
  y += 11;

  char cpuStr[32];
  sprintf(cpuStr, "CPU Load: %.1f%%", cpuLoad);
  display->drawStr(4, y, cpuStr);
  y += 11;

  char bufferStr[32];
  sprintf(bufferStr, "Buffer: 256 samples");
  display->drawStr(4, y, bufferStr);

  drawFooter("RP2350", "Back");
  display->sendBuffer();
}

void ProfessionalUI::drawHeader(const char* title) {
  display->setFont(u8g2_font_6x10_tr);
  display->drawStr(2, 9, title);
  display->drawLine(0, 11, 128, 11);
}

void ProfessionalUI::drawFooter(const char* left, const char* right) {
  display->drawLine(0, 56, 128, 56);
  display->setFont(u8g2_font_5x7_tr);
  if (left) {
    display->drawStr(2, 63, left);
  }
  if (right) {
    uint8_t strWidth = display->getStrWidth(right);
    display->drawStr(126 - strWidth, 63, right);
  }
}

void ProfessionalUI::drawVUMeter(uint8_t x, uint8_t y, uint8_t width, uint8_t height, float level, bool isPeak) {
  // Draw outline
  display->drawFrame(x, y, width, height);

  // Calculate fill width (logarithmic scale for better visual representation)
  float db = 20.0f * log10(level + 0.001f); // Add small value to avoid log(0)
  db = constrain(db, -60.0f, 0.0f);
  float normalized = (db + 60.0f) / 60.0f; // Convert -60dB to 0dB range to 0-1

  uint8_t fillWidth = (uint8_t)(normalized * (width - 2));

  // Draw segments with different shading
  for (uint8_t i = 0; i < fillWidth; i++) {
    uint8_t segmentX = x + 1 + i;
    float segmentLevel = (float)i / (width - 2);

    // Color zones: Green (0-70%), Yellow (70-90%), Red (90-100%)
    if (segmentLevel < 0.7f) {
      // Green zone - solid
      display->drawVLine(segmentX, y + 1, height - 2);
    } else if (segmentLevel < 0.9f) {
      // Yellow zone - pattern
      if (i % 2 == 0) {
        display->drawVLine(segmentX, y + 1, height - 2);
      }
    } else {
      // Red zone - sparse pattern
      if (i % 3 == 0) {
        display->drawVLine(segmentX, y + 1, height - 2);
      }
    }
  }
}

void ProfessionalUI::drawProgressBar(uint8_t x, uint8_t y, uint8_t width, uint8_t height, float value, float min, float max) {
  // Draw outline
  display->drawFrame(x, y, width, height);

  // Calculate fill
  float normalized = (value - min) / (max - min);
  normalized = constrain(normalized, 0.0f, 1.0f);
  uint8_t fillWidth = (uint8_t)(normalized * (width - 4));

  // Draw fill
  if (fillWidth > 0) {
    display->drawBox(x + 2, y + 2, fillWidth, height - 4);
  }
}

void ProfessionalUI::drawEffectIcon(uint8_t x, uint8_t y, uint8_t effectIndex, bool active) {
  if (effectIndex >= effectCount) return;

  // Draw box for icon
  if (active) {
    display->drawFrame(x, y, 20, 16);
  } else {
    display->drawFrame(x, y, 20, 16);
    // Draw X through inactive effects
    display->drawLine(x + 2, y + 2, x + 18, y + 14);
    display->drawLine(x + 18, y + 2, x + 2, y + 14);
  }

  // Draw short name
  display->setFont(u8g2_font_4x6_tr);
  uint8_t strWidth = display->getStrWidth(effects[effectIndex].shortName);
  display->drawStr(x + (20 - strWidth) / 2, y + 11, effects[effectIndex].shortName);
}

void ProfessionalUI::drawBattery(uint8_t x, uint8_t y, float level) {
  // Simple battery icon
  display->drawFrame(x, y, 14, 6);
  display->drawBox(x + 14, y + 2, 2, 2); // Terminal

  uint8_t fill = (uint8_t)(level * 12);
  if (fill > 0) {
    display->drawBox(x + 1, y + 1, fill, 4);
  }
}

float ProfessionalUI::mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void ProfessionalUI::updateParameterCache() {
  for (uint8_t e = 0; e < effectCount; e++) {
    for (uint8_t p = 0; p < effects[e].paramCount; p++) {
      uint8_t paramIdx = effects[e].firstParamIndex + p;
      if (paramIdx < parameterCount) {
        effectParamCache[e][p] = parameters[paramIdx].currentValue;
      }
    }
  }
}
