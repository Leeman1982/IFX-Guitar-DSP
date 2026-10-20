#ifndef IFX_IIR_PEAKING_FILTER_H
#define IFX_IIR_PEAKING_FILTER_H

#include <math.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float sampleTime_s;
    /* normalised biquad (a0 = 1), transposed direct form II */
    float b0, b1, b2, a1, a2;
    float s1, s2;
    uint8_t unity;          /* 0 dB band: exact pass-through, skipped */
} IFX_PeakingFilter;

void  IFX_PeakingFilter_Init(IFX_PeakingFilter *filt, float sampleRate_Hz);
void  IFX_PeakingFilter_SetParameters(IFX_PeakingFilter *filt, float centerFrequency_Hz,
                                       float bandwidth_Hz, float boostCut_linear);
static inline float IFX_PeakingFilter_Update(IFX_PeakingFilter *filt, float in) {
    if (filt->unity) return in;
    float y = filt->b0 * in + filt->s1;
    filt->s1 = filt->b1 * in - filt->a1 * y + filt->s2;
    filt->s2 = filt->b2 * in - filt->a2 * y;
    return y;
}

#ifdef __cplusplus
}
#endif

#endif
