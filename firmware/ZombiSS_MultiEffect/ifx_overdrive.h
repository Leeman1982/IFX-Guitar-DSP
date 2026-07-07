#ifndef IFX_OVERDRIVE_H
#define IFX_OVERDRIVE_H

#define _USE_MATH_DEFINES

#include <stdint.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Input low-pass filter (fc = fs / 4), 69 taps */
#define IFX_OVERDRIVE_LPF_INP_LENGTH 69

extern float IFX_OD_LPF_INP_COEF[IFX_OVERDRIVE_LPF_INP_LENGTH];

typedef struct {
    float T;

    /* Input low-pass filter */
    float   lpfInpBuf[IFX_OVERDRIVE_LPF_INP_LENGTH];
    uint8_t lpfInpBufIndex;
    float   lpfInpOut;

    /* Input high-pass filter */
    float hpfInpBufIn[2];
    float hpfInpBufOut[2];
    float hpfInpWcT;
    float hpfInpOut;

    /* Overdrive settings */
    float preGain;
    float boostGain;
    float threshold;

    /* Output low-pass filter */
    float lpfOutBufIn[3];
    float lpfOutBufOut[3];
    float lpfOutWcT;
    float lpfOutDamp;
    float lpfOutOut;

    float out;
    float Q;
    /* Precomputed Q / (1 - e^(d*Q)) — depends only on Q, so it is evaluated
     * in Init/SetQ instead of paying one expf() per sample in the hot path. */
    float clipConst;
} IFX_Overdrive;

void  IFX_Overdrive_Init(IFX_Overdrive *od, float samplingFrequencyHz,
                          float hpfCutoffFrequencyHz, float odPreGain,
                          float lpfCutoffFrequencyHz, float lpfDamping);
void  IFX_Overdrive_SetGain(IFX_Overdrive *od, float gain);
void  IFX_Overdrive_SetBoost(IFX_Overdrive *od, float boost);
void  IFX_Overdrive_SetHPF(IFX_Overdrive *od, float hpfCutoffFrequencyHz);
void  IFX_Overdrive_SetLPF(IFX_Overdrive *od, float lpfCutoffFrequencyHz, float lpfDamping);
void  IFX_Overdrive_SetQ(IFX_Overdrive *od, float Q);
float IFX_Overdrive_Update(IFX_Overdrive *od, float inp);

#ifdef __cplusplus
}
#endif

#endif
