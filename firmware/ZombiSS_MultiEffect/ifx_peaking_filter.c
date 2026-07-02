#include "ifx_peaking_filter.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

void IFX_PeakingFilter_Init(IFX_PeakingFilter *filt, float sampleRate_Hz) {
    filt->sampleTime_s = 1.0f / sampleRate_Hz;
    for (uint8_t n = 0; n < 3; n++) {
        filt->x[n] = 0.0f;
        filt->y[n] = 0.0f;
    }
    IFX_PeakingFilter_SetParameters(filt, 1.0f, 0.0f, 1.0f);
}

void IFX_PeakingFilter_SetParameters(IFX_PeakingFilter *filt, float centerFrequency_Hz,
                                      float bandwidth_Hz, float boostCut_linear) {
    float wcT = 2.0f * tanf(M_PI * centerFrequency_Hz * filt->sampleTime_s);
    float Q = (centerFrequency_Hz > 0.001f) ? bandwidth_Hz / centerFrequency_Hz : 0.707f;

    filt->a[0] = 4.0f + 2.0f * boostCut_linear * Q * wcT + wcT * wcT;
    filt->a[1] = 2.0f * wcT * wcT - 8.0f;
    filt->a[2] = 4.0f - 2.0f * boostCut_linear * Q * wcT + wcT * wcT;

    float denom = 4.0f + 2.0f * Q * wcT + wcT * wcT;
    filt->b[0] = 1.0f / denom;
    filt->b[1] = -(2.0f * wcT * wcT - 8.0f);
    filt->b[2] = -(4.0f - 2.0f * Q * wcT + wcT * wcT);
}

float IFX_PeakingFilter_Update(IFX_PeakingFilter *filt, float in) {
    filt->x[2] = filt->x[1];
    filt->x[1] = filt->x[0];
    filt->x[0] = in;

    filt->y[2] = filt->y[1];
    filt->y[1] = filt->y[0];

    filt->y[0] = (filt->a[0] * filt->x[0] + filt->a[1] * filt->x[1] + filt->a[2] * filt->x[2]
               + filt->b[1] * filt->y[1] + filt->b[2] * filt->y[2]) * filt->b[0];

    return filt->y[0];
}
