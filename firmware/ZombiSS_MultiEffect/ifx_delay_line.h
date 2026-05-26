#ifndef IFX_DELAYLINE_H
#define IFX_DELAYLINE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Max ~0.67s at 48kHz - fits in RP2350 SRAM */
#define IFX_DELAYLINE_MAXLENGTH 32000

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
