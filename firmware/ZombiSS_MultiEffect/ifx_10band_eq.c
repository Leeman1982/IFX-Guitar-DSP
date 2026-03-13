#include "ifx_10band_eq.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

const float IFX_EQ10_FREQS[EQ10_BANDS] = {
    31.25f, 62.5f, 125.0f, 250.0f, 500.0f,
    1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
};

/* Metallica scooped-mid rhythm tone */
const float IFX_EQ10_METALLICA_PRESET[EQ10_BANDS] = {
     6.0f,  /* 31 Hz  - sub bass depth  */
     5.0f,  /* 63 Hz  - low-end body    */
     3.0f,  /* 125 Hz - bass warmth     */
    -2.0f,  /* 250 Hz - cut mud         */
    -4.0f,  /* 500 Hz - scoop low-mid   */
    -6.0f,  /* 1k Hz  - classic scoop   */
    -5.0f,  /* 2k Hz  - upper-mid scoop */
    -3.0f,  /* 4k Hz  - cut harshness   */
     4.0f,  /* 8k Hz  - presence/attack */
     3.0f,  /* 16k Hz - air/definition  */
};

void IFX_10BandEQ_Init(IFX_10BandEQ *eq, float sampleRate_Hz) {
    for (uint8_t i = 0; i < EQ10_BANDS; i++) {
        IFX_PeakingFilter_Init(&eq->bands[i], sampleRate_Hz);
        eq->gainDb[i] = IFX_EQ10_METALLICA_PRESET[i];
        float fc = IFX_EQ10_FREQS[i];
        /* Q = 1.41 for octave bandwidth → bw = fc / Q */
        float bw = fc / 1.41f;
        float linear = powf(10.0f, eq->gainDb[i] / 20.0f);
        IFX_PeakingFilter_SetParameters(&eq->bands[i], fc, bw, linear);
    }
}

void IFX_10BandEQ_SetBand(IFX_10BandEQ *eq, uint8_t band, float gainDb) {
    if (band >= EQ10_BANDS) return;
    eq->gainDb[band] = gainDb;
    float fc  = IFX_EQ10_FREQS[band];
    float bw  = fc / 1.41f;
    float lin = powf(10.0f, gainDb / 20.0f);
    IFX_PeakingFilter_SetParameters(&eq->bands[band], fc, bw, lin);
}

float IFX_10BandEQ_Update(IFX_10BandEQ *eq, float in) {
    float sig = in;
    for (uint8_t i = 0; i < EQ10_BANDS; i++) {
        sig = IFX_PeakingFilter_Update(&eq->bands[i], sig);
    }
    return sig;
}
