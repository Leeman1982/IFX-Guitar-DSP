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
    /* Input low-pass filter: doubled circular buffer so the 69-tap window is
     * always contiguous (no modulo in the hot loop) */
    float   lpfInpBuf[2 * IFX_OVERDRIVE_LPF_INP_LENGTH];
    uint8_t lpfInpBufIndex;
    /* Input high-pass filter (1st-order bilinear): y = g·(x − x1) + a·y1 */
    float hpfInpWcT, hpfG, hpfA, hpfX1, hpfY1;
    /* Overdrive settings */
    float preGain;
    float boostGain;
    float Q;
    float clipOffset;          /* Q / (1 − e^{dQ}), recomputed in SetQ */
    /* Output low-pass filter (2nd-order bilinear), coefficients precomputed */
    float lpfOutWcT, lpfOutDamp;
    float lpfB, lpfA1, lpfA2;
    float lpfX1, lpfX2, lpfY1, lpfY2;
    float out;
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
/* Block form (audio core): in-place over n samples. */
void  IFX_Overdrive_Process(IFX_Overdrive *od, float *buf, int n);

#ifdef __cplusplus
}
#endif

#endif
