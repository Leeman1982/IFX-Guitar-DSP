#include "ifx_peaking_filter.h"
#include "pico/platform.h"

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

float __not_in_flash_func(IFX_PeakingFilter_Update)(IFX_PeakingFilter *filt, float in) {
    /* Direct Form I with states cached in locals: identical arithmetic to
     * the shift-array version, but the compiler keeps everything in FPU
     * registers instead of re-loading struct members (pointer aliasing
     * otherwise forces a load/store per term). x[2]/y[2] slots are unused. */
    float x1 = filt->x[0];   /* in[n-1] */
    float x2 = filt->x[1];   /* in[n-2] */
    float y1 = filt->y[0];   /* out[n-1] */
    float y2 = filt->y[1];   /* out[n-2] */

    float y0 = (filt->a[0] * in + filt->a[1] * x1 + filt->a[2] * x2
              + filt->b[1] * y1 + filt->b[2] * y2) * filt->b[0];

    filt->x[1] = x1;  filt->x[0] = in;
    filt->y[1] = y1;  filt->y[0] = y0;

    return y0;
}
