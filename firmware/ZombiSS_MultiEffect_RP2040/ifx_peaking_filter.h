#ifndef IFX_IIR_PEAKING_FILTER_H
#define IFX_IIR_PEAKING_FILTER_H

#include <math.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float sampleTime_s;
    float x[3];
    float y[3];
    float a[3];
    float b[3];
} IFX_PeakingFilter;

void  IFX_PeakingFilter_Init(IFX_PeakingFilter *filt, float sampleRate_Hz);
void  IFX_PeakingFilter_SetParameters(IFX_PeakingFilter *filt, float centerFrequency_Hz,
                                       float bandwidth_Hz, float boostCut_linear);
float IFX_PeakingFilter_Update(IFX_PeakingFilter *filt, float in);

#ifdef __cplusplus
}
#endif

#endif
