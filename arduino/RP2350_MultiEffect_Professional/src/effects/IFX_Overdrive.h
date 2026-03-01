/*
 * InfiniFX - Overdrive
 *
 * Author: Philip Salmony @ phils-lab.net
 * Ported to Arduino for RP2350
 */

#ifndef IFX_OVERDRIVE_H
#define IFX_OVERDRIVE_H

#include <Arduino.h>
#include <math.h>

/* Input low-pass filter (fc = fs / 4), 0.25dB ripple in pass-band, 60dB attenuation at stop-band, 69 taps */
#define IFX_OVERDRIVE_LPF_INP_LENGTH 69

extern const float IFX_OD_LPF_INP_COEF[IFX_OVERDRIVE_LPF_INP_LENGTH];

typedef struct {
    /* Sampling time */
    float T;

    /* Input low-pass filter */
    float lpfInpBuf[IFX_OVERDRIVE_LPF_INP_LENGTH];
    uint8_t lpfInpBufIndex;
    float lpfInpOut;

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
} IFX_Overdrive;

void IFX_Overdrive_Init(IFX_Overdrive *od, float samplingFrequencyHz);
void IFX_Overdrive_SetPreGain(IFX_Overdrive *od, float gain);
void IFX_Overdrive_SetInputHPF(IFX_Overdrive *od, float hpfCutoffFrequencyHz);
void IFX_Overdrive_SetInputLPF(IFX_Overdrive *od, float lpfCutoffFrequencyHz);
float IFX_Overdrive_Process(IFX_Overdrive *od, float inp);

#endif
