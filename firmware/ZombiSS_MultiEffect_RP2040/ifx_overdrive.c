#include "ifx_overdrive.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

float IFX_OD_LPF_INP_COEF[IFX_OVERDRIVE_LPF_INP_LENGTH] = {
    -0.00020692388031130378f,
    -0.0005449777163912186f,
    -0.0010648637855421347f,
    -0.0016364252077762365f,
    -0.002012399024754339f,
    -0.001863732890917493f,
    -0.000893772118525287f,
     0.0010039250791041001f,
     0.003603402015637201f,
     0.006295503824772816f,
     0.008189064906492259f,
     0.00837592349490335f,
     0.006298513670065863f,
     0.00209212864464293f,
    -0.003255494658311472f,
    -0.008051092706911124f,
    -0.010376200403688028f,
    -0.008792118107324508f,
    -0.003037554751423106f,
     0.005568054109378603f,
     0.014251053145796779f,
     0.019515960562734174f,
     0.01833642159152087f,
     0.009447624551409354f,
    -0.005725703771431606f,
    -0.022919941654740428f,
    -0.03585932375996474f,
    -0.03793377289247158f,
    -0.024280160492278255f,
     0.006380286560875572f,
     0.050788580809754395f,
     0.10148403327365461f,
     0.1484439587786635f,
     0.18160934822751035f,
     0.19357173063571662f,
     0.18160934822751035f,
     0.1484439587786635f,
     0.10148403327365461f,
     0.050788580809754395f,
     0.006380286560875572f,
    -0.024280160492278255f,
    -0.03793377289247158f,
    -0.03585932375996474f,
    -0.022919941654740428f,
    -0.005725703771431606f,
     0.009447624551409354f,
     0.01833642159152087f,
     0.019515960562734174f,
     0.014251053145796779f,
     0.005568054109378603f,
    -0.003037554751423106f,
    -0.008792118107324508f,
    -0.010376200403688028f,
    -0.008051092706911124f,
    -0.003255494658311472f,
     0.00209212864464293f,
     0.006298513670065863f,
     0.00837592349490335f,
     0.008189064906492259f,
     0.006295503824772816f,
     0.003603402015637201f,
     0.0010039250791041001f,
    -0.000893772118525287f,
    -0.001863732890917493f,
    -0.002012399024754339f,
    -0.0016364252077762365f,
    -0.0010648637855421347f,
    -0.0005449777163912186f,
    -0.00020692388031130378f
};

void IFX_Overdrive_Init(IFX_Overdrive *od, float samplingFrequencyHz,
                         float hpfCutoffFrequencyHz, float odPreGain,
                         float lpfCutoffFrequencyHz, float lpfDamping) {
    od->T = 1.0f / samplingFrequencyHz;

    od->hpfInpBufIn[0]  = 0.0f; od->hpfInpBufIn[1]  = 0.0f;
    od->hpfInpBufOut[0] = 0.0f; od->hpfInpBufOut[1] = 0.0f;
    od->hpfInpWcT = 2.0f * M_PI * hpfCutoffFrequencyHz * od->T;
    od->hpfInpOut = 0.0f;

    for (uint8_t n = 0; n < IFX_OVERDRIVE_LPF_INP_LENGTH; n++) {
        od->lpfInpBuf[n] = 0.0f;
    }
    od->lpfInpBufIndex = 0;
    od->lpfInpOut = 0.0f;

    od->preGain   = odPreGain;
    od->boostGain = 0.0f;
    od->threshold = 1.0f / 3.0f;

    od->lpfOutWcT  = 2.0f * M_PI * lpfCutoffFrequencyHz * od->T;
    od->lpfOutDamp = lpfDamping;

    for (uint8_t n = 0; n < 3; n++) {
        od->lpfOutBufIn[n]  = 0.0f;
        od->lpfOutBufOut[n] = 0.0f;
    }

    od->Q = -0.2f;
    od->out = 0.0f;
}

void IFX_Overdrive_SetGain(IFX_Overdrive *od, float gain) {
    od->preGain = gain;
}

void IFX_Overdrive_SetBoost(IFX_Overdrive *od, float boost) {
    od->boostGain = boost;
}

void IFX_Overdrive_SetHPF(IFX_Overdrive *od, float hpfCutoffFrequencyHz) {
    od->hpfInpWcT = 2.0f * M_PI * hpfCutoffFrequencyHz * od->T;
}

void IFX_Overdrive_SetLPF(IFX_Overdrive *od, float lpfCutoffFrequencyHz, float lpfDamping) {
    od->lpfOutWcT  = 2.0f * M_PI * lpfCutoffFrequencyHz * od->T;
    od->lpfOutDamp = lpfDamping;
}

void IFX_Overdrive_SetQ(IFX_Overdrive *od, float Q) {
    od->Q = Q;
}

float IFX_Overdrive_Update(IFX_Overdrive *od, float inp) {
    /* FIR LPF at fs/4 to prevent aliasing from signal squaring */
    od->lpfInpBuf[od->lpfInpBufIndex] = inp;
    od->lpfInpBufIndex++;
    if (od->lpfInpBufIndex == IFX_OVERDRIVE_LPF_INP_LENGTH) {
        od->lpfInpBufIndex = 0;
    }

    od->lpfInpOut = 0.0f;
    uint8_t index = od->lpfInpBufIndex;
    for (uint8_t n = 0; n < IFX_OVERDRIVE_LPF_INP_LENGTH; n++) {
        if (index == 0) {
            index = IFX_OVERDRIVE_LPF_INP_LENGTH - 1;
        } else {
            index--;
        }
        od->lpfInpOut += IFX_OD_LPF_INP_COEF[n] * od->lpfInpBuf[index];
    }

    /* 1st-order IIR HPF to remove low frequency mud */
    od->hpfInpBufIn[1] = od->hpfInpBufIn[0];
    od->hpfInpBufIn[0] = od->lpfInpOut;
    od->hpfInpBufOut[1] = od->hpfInpBufOut[0];
    od->hpfInpBufOut[0] = (2.0f * (od->hpfInpBufIn[0] - od->hpfInpBufIn[1])
                          + (2.0f - od->hpfInpWcT) * od->hpfInpBufOut[1])
                          / (2.0f + od->hpfInpWcT);
    od->hpfInpOut = od->hpfInpBufOut[0];

    /* Asymmetrical soft clipping */
    float xGain = (od->preGain + od->boostGain) * od->hpfInpOut;
    const float d = 8.0f;

    float clipOut = od->Q / (1.0f - expf(d * od->Q));
    if ((xGain - od->Q) >= 0.00001f) {
        clipOut += (xGain - od->Q) / (1.0f - expf(-d * (xGain - od->Q)));
    }

    /* 2nd-order IIR LPF to remove HF artifacts */
    od->lpfOutBufIn[2] = od->lpfOutBufIn[1];
    od->lpfOutBufIn[1] = od->lpfOutBufIn[0];
    od->lpfOutBufIn[0] = clipOut;

    od->lpfOutBufOut[2] = od->lpfOutBufOut[1];
    od->lpfOutBufOut[1] = od->lpfOutBufOut[0];

    float wcT2 = od->lpfOutWcT * od->lpfOutWcT;
    od->lpfOutBufOut[0] = wcT2 * (od->lpfOutBufIn[0] + 2.0f * od->lpfOutBufIn[1] + od->lpfOutBufIn[2])
                        - 2.0f * (wcT2 - 4.0f) * od->lpfOutBufOut[1]
                        - (4.0f - 4.0f * od->lpfOutDamp * od->lpfOutWcT + wcT2) * od->lpfOutBufOut[2];
    od->lpfOutBufOut[0] /= (4.0f + 4.0f * od->lpfOutDamp * od->lpfOutWcT + wcT2);
    od->lpfOutOut = od->lpfOutBufOut[0];

    /* Hard limiter */
    od->out = od->lpfOutOut;
    if (od->out > 1.0f) od->out = 1.0f;
    else if (od->out < -1.0f) od->out = -1.0f;

    return od->out;
}
