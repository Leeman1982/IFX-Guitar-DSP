/*
*
*   InfiniFX - Delay (ESP32 port)
*
*   The delay line is heap-allocated at init: PSRAM when available
*   (long delay times), internal RAM otherwise.
*
*/

#ifndef IFX_DELAY_H
#define IFX_DELAY_H

#include <stdint.h>

typedef struct {

    // Settings
    float mix; // Mix setting: 1 = Wet, 0 = Dry
    float feedback;

    // Delay line buffer and index
    float    *line;
    uint32_t  maxLineLength;
    uint32_t  lineIndex;

    // Delay line length (delay time = delay line length / sample rate)
    uint32_t lineLength;

    // Output
    float out;

} IFX_Delay;

/* Returns 0 on allocation failure */
uint8_t IFX_Delay_Init(IFX_Delay *dly, float maxDelayTime_ms, float delayTime_ms, float mix, float feedback, float sampleRate_Hz);
float   IFX_Delay_Update(IFX_Delay *dly, float inp);
void    IFX_Delay_SetLength(IFX_Delay *dly, float delayTime_ms, float sampleRate_Hz);
void    IFX_Delay_SetMix(IFX_Delay *dly, float mix);
void    IFX_Delay_SetFeedback(IFX_Delay *dly, float feedback);

#endif
