#ifndef IFX_DELAY_H
#define IFX_DELAY_H

#include <stdint.h>

/* Max ~660ms at 48kHz */
#define IFX_DELAY_MAX_LINE_LENGTH 32000

typedef struct {
    float mix;
    float feedback;
    float line[IFX_DELAY_MAX_LINE_LENGTH];
    uint32_t lineIndex;
    uint32_t lineLength;
    float out;
} IFX_Delay;

void  IFX_Delay_Init(IFX_Delay *dly, float delayTime_ms, float mix, float feedback, float sampleRate_Hz);
float IFX_Delay_Update(IFX_Delay *dly, float inp);
void  IFX_Delay_SetLength(IFX_Delay *dly, float delayTime_ms, float sampleRate_Hz);
void  IFX_Delay_SetMix(IFX_Delay *dly, float mix);
void  IFX_Delay_SetFeedback(IFX_Delay *dly, float feedback);

#endif
