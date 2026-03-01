#ifndef VUMETER_H
#define VUMETER_H

#include <Arduino.h>

class VUMeter {
public:
  VUMeter();

  void begin(float sampleRate);
  void update(float sample);

  float getPeakLevel();
  float getRMSLevel();
  float getDecibels();

  void reset();

private:
  float peakLevel;
  float rmsLevel;
  float sampleRate;

  // Peak detection
  float peakDecay;
  uint32_t lastPeakTime;

  // RMS calculation
  float sumSquares;
  uint16_t sampleCount;
  uint16_t rmsWindowSize;
};

#endif
