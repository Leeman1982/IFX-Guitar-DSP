#ifndef IFX_DELAY_H
#define IFX_DELAY_H

#include <Arduino.h>

// Pre-defined maximum delay line length
#define IFX_DELAY_MAX_LINE_LENGTH 32500

typedef struct {
    // Settings
    float mix;      // Mix setting: 1 = Wet, 0 = Dry
    float feedback;

    // Delay line buffer and index
    float line[IFX_DELAY_MAX_LINE_LENGTH];
    uint32_t lineIndex;

    // Delay line length (delay time = delay line length / sample rate)
    uint32_t lineLength;

    // Output
    float out;
} IFX_Delay;

void IFX_Delay_Init(IFX_Delay *dly, float sampleRateHz);
float IFX_Delay_Process(IFX_Delay *dly, float inp);
void IFX_Delay_SetDelayTime(IFX_Delay *dly, float delayTimeMs);
void IFX_Delay_SetFeedback(IFX_Delay *dly, float feedback);
void IFX_Delay_SetMix(IFX_Delay *dly, float mix);

#endif
