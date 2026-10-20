#pragma GCC optimize ("O2")
#include "ifx_overdrive.h"
#include "ifx_bf_math.h"
#include <string.h>

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

#define OD_D 8.0f   /* clipper knee sharpness */

static void od_set_hpf(IFX_Overdrive *od) {
    float w = od->hpfInpWcT;
    od->hpfG = 2.0f / (2.0f + w);
    od->hpfA = (2.0f - w) / (2.0f + w);
}

static void od_set_lpf(IFX_Overdrive *od) {
    float w = od->lpfOutWcT, w2 = w * w, d = od->lpfOutDamp;
    float inv = 1.0f / (4.0f + 4.0f * d * w + w2);
    od->lpfB  = w2 * inv;
    od->lpfA1 = 2.0f * (w2 - 4.0f) * inv;
    od->lpfA2 = (4.0f - 4.0f * d * w + w2) * inv;
}

void IFX_Overdrive_Init(IFX_Overdrive *od, float samplingFrequencyHz,
                         float hpfCutoffFrequencyHz, float odPreGain,
                         float lpfCutoffFrequencyHz, float lpfDamping) {
    memset(od, 0, sizeof(*od));
    od->T = 1.0f / samplingFrequencyHz;
    od->hpfInpWcT = 2.0f * M_PI * hpfCutoffFrequencyHz * od->T;
    od_set_hpf(od);
    od->preGain   = odPreGain;
    od->boostGain = 0.0f;
    od->lpfOutWcT  = 2.0f * M_PI * lpfCutoffFrequencyHz * od->T;
    od->lpfOutDamp = lpfDamping;
    od_set_lpf(od);
    IFX_Overdrive_SetQ(od, -0.2f);
}

void IFX_Overdrive_SetGain(IFX_Overdrive *od, float gain) {
    od->preGain = gain;
}

void IFX_Overdrive_SetBoost(IFX_Overdrive *od, float boost) {
    od->boostGain = boost;
}

void IFX_Overdrive_SetHPF(IFX_Overdrive *od, float hpfCutoffFrequencyHz) {
    od->hpfInpWcT = 2.0f * M_PI * hpfCutoffFrequencyHz * od->T;
    od_set_hpf(od);
}

void IFX_Overdrive_SetLPF(IFX_Overdrive *od, float lpfCutoffFrequencyHz, float lpfDamping) {
    od->lpfOutWcT  = 2.0f * M_PI * lpfCutoffFrequencyHz * od->T;
    od->lpfOutDamp = lpfDamping;
    od_set_lpf(od);
}

void IFX_Overdrive_SetQ(IFX_Overdrive *od, float Q) {
    od->Q = Q;
    od->clipOffset = Q / (1.0f - expf(OD_D * Q));
}

float IFX_RAMFUNC(IFX_Overdrive_Update)(IFX_Overdrive *od, float inp) {
    /* FIR LPF at fs/4 (69 taps, linear phase).  Symmetric coefficients:
     * fold the window so only 35 multiplies are needed. */
    const int L = IFX_OVERDRIVE_LPF_INP_LENGTH;
    uint8_t idx = od->lpfInpBufIndex;
    od->lpfInpBuf[idx] = inp;
    od->lpfInpBuf[idx + L] = inp;
    idx = (uint8_t)(idx + 1 == L ? 0 : idx + 1);
    od->lpfInpBufIndex = idx;
    const float *w = &od->lpfInpBuf[idx];          /* oldest … newest */
    float acc0 = IFX_OD_LPF_INP_COEF[L / 2] * w[L / 2], acc1 = 0.0f;
    for (int n = 0; n < L / 2; n += 2) {
        acc0 += IFX_OD_LPF_INP_COEF[n]     * (w[n]     + w[L - 1 - n]);
        acc1 += IFX_OD_LPF_INP_COEF[n + 1] * (w[n + 1] + w[L - 2 - n]);
    }
    float lpf = acc0 + acc1;

    /* 1st-order IIR HPF to remove low frequency mud */
    float hpf = od->hpfG * (lpf - od->hpfX1) + od->hpfA * od->hpfY1;
    od->hpfX1 = lpf;
    od->hpfY1 = hpf;

    /* Asymmetrical soft clipping */
    float xGain = (od->preGain + od->boostGain) * hpf;
    float clipOut = od->clipOffset;
    float diff = xGain - od->Q;
    if (diff >= 0.00001f) clipOut += diff / (1.0f - bf_exp(-OD_D * diff));

    /* 2nd-order IIR LPF to remove HF artifacts */
    float y = od->lpfB * (clipOut + 2.0f * od->lpfX1 + od->lpfX2)
            - od->lpfA1 * od->lpfY1 - od->lpfA2 * od->lpfY2;
    od->lpfX2 = od->lpfX1; od->lpfX1 = clipOut;
    od->lpfY2 = od->lpfY1; od->lpfY1 = y;

    /* Hard limiter */
    if (y > 1.0f) y = 1.0f;
    else if (y < -1.0f) y = -1.0f;
    od->out = y;
    return y;
}

void IFX_RAMFUNC(IFX_Overdrive_Process)(IFX_Overdrive *od, float *buf, int n) {
    for (int i = 0; i < n; i++) buf[i] = IFX_Overdrive_Update(od, buf[i]);
}
