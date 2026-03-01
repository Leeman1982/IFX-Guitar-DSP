#include "VUMeter.h"
#include <math.h>

VUMeter::VUMeter()
    : peakLevel(0.0f), rmsLevel(0.0f), sampleRate(48000.0f),
      peakDecay(0.95f), lastPeakTime(0), sumSquares(0.0f),
      sampleCount(0), rmsWindowSize(1024) {
}

void VUMeter::begin(float sr) {
  sampleRate = sr;
  rmsWindowSize = (uint16_t)(sampleRate * 0.05f); // 50ms window
  reset();
}

void VUMeter::update(float sample) {
  // Update peak level
  float absSample = abs(sample);
  if (absSample > peakLevel) {
    peakLevel = absSample;
    lastPeakTime = millis();
  } else {
    // Decay peak over time
    uint32_t now = millis();
    if (now - lastPeakTime > 10) { // Decay after 10ms
      peakLevel *= peakDecay;
      lastPeakTime = now;
    }
  }

  // Update RMS level
  sumSquares += sample * sample;
  sampleCount++;

  if (sampleCount >= rmsWindowSize) {
    rmsLevel = sqrt(sumSquares / sampleCount);
    sumSquares = 0.0f;
    sampleCount = 0;
  }
}

float VUMeter::getPeakLevel() {
  return peakLevel;
}

float VUMeter::getRMSLevel() {
  return rmsLevel;
}

float VUMeter::getDecibels() {
  float level = (rmsLevel > peakLevel) ? rmsLevel : peakLevel;
  if (level < 0.00001f) return -60.0f;
  return 20.0f * log10(level);
}

void VUMeter::reset() {
  peakLevel = 0.0f;
  rmsLevel = 0.0f;
  sumSquares = 0.0f;
  sampleCount = 0;
  lastPeakTime = millis();
}
