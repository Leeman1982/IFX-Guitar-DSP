#ifndef IFX_DELAYLINE_H
#define IFX_DELAYLINE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Reduced from 32000 to 12000 for RP2040 264KB SRAM (~375ms at 32kHz) */
#define IFX_DELAYLINE_MAXLENGTH 12000

typedef struct {
    uint32_t length;
    uint32_t index;
    float memory[IFX_DELAYLINE_MAXLENGTH];
} IFX_DelayLine;

void  IFX_DelayLine_Init(IFX_DelayLine *dlyLn, float delayTime_ms, float sampleRate_Hz);
float IFX_DelayLine_Update(IFX_DelayLine *dlyLn, float inp);
void  IFX_DelayLine_SetLength(IFX_DelayLine *dlyLn, float delayTime_ms, float sampleRate_Hz);

#ifdef __cplusplus
}
#endif

#endif
