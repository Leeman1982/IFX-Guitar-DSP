#include "ifx_peaking_filter.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

void IFX_PeakingFilter_Init(IFX_PeakingFilter *filt, float sampleRate_Hz) {
    filt->sampleTime_s = 1.0f / sampleRate_Hz;
    filt->s1 = filt->s2 = 0.0f;
    filt->unity = 1;
    IFX_PeakingFilter_SetParameters(filt, 1.0f, 0.0f, 1.0f);
}

/* Bilinear transform (pre-warped) of the analog peaking section
 *   H(s) = (s² + G·(ω/Q)·s + ω²) / (s² + (ω/Q)·s + ω²)
 * — same response as the original Phil's Lab implementation, just
 * normalised once here instead of dividing on every sample. */
void IFX_PeakingFilter_SetParameters(IFX_PeakingFilter *filt, float centerFrequency_Hz,
                                      float bandwidth_Hz, float boostCut_linear) {
    float wcT = 2.0f * tanf(M_PI * centerFrequency_Hz * filt->sampleTime_s);
    float Q = (centerFrequency_Hz > 0.001f) ? bandwidth_Hz / centerFrequency_Hz : 0.707f;
    float w2 = wcT * wcT;
    float inv = 1.0f / (4.0f + 2.0f * Q * wcT + w2);
    filt->b0 = (4.0f + 2.0f * boostCut_linear * Q * wcT + w2) * inv;
    filt->b1 = (2.0f * w2 - 8.0f) * inv;
    filt->b2 = (4.0f - 2.0f * boostCut_linear * Q * wcT + w2) * inv;
    filt->a1 = filt->b1;
    filt->a2 = (4.0f - 2.0f * Q * wcT + w2) * inv;
    uint8_t unity = (boostCut_linear == 1.0f);
    if (unity != filt->unity) filt->s1 = filt->s2 = 0.0f;   /* states were frozen */
    filt->unity = unity;
}
