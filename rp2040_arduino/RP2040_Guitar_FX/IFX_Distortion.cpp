#include "IFX_Distortion.h"

// =============================================================================
// 69-tap equiripple FIR low-pass filter (fc = fs/4)
// 0.25 dB pass-band ripple, 60 dB stop-band attenuation
// Coefficients identical to original IFX_Overdrive (Philip Salmony / InfiniFX)
// =============================================================================
const float IFX_DIST_LPF_INP_COEF[IFX_DIST_LPF_INP_LENGTH] = {
    -0.00020692388031130378f,
    -0.00054497771639121860f,
    -0.00106486378554213470f,
    -0.00163642520777623650f,
    -0.00201239902475433900f,
    -0.00186373289091749300f,
    -0.00089377211852528700f,
     0.00100392507910410010f,
     0.00360340201563720100f,
     0.00629550382477281600f,
     0.00818906490649225900f,
     0.00837592349490335000f,
     0.00629851367006586300f,
     0.00209212864464293000f,
    -0.00325549465831147200f,
    -0.00805109270691112400f,
    -0.01037620040368802800f,
    -0.00879211810732450800f,
    -0.00303755475142310600f,
     0.00556805410937860300f,
     0.01425105314579677900f,
     0.01951596056273417400f,
     0.01833642159152087000f,
     0.00944762455140935400f,
    -0.00572570377143160600f,
    -0.02291994165474042800f,
    -0.03585932375996474000f,
    -0.03793377289247158000f,
    -0.02428016049227825500f,
     0.00638028656087557200f,
     0.05078858080975439500f,
     0.10148403327365461000f,
     0.14844395877866350000f,
     0.18160934822751035000f,
     0.19357173063571662000f,   // centre tap (n=34)
     0.18160934822751035000f,
     0.14844395877866350000f,
     0.10148403327365461000f,
     0.05078858080975439500f,
     0.00638028656087557200f,
    -0.02428016049227825500f,
    -0.03793377289247158000f,
    -0.03585932375996474000f,
    -0.02291994165474042800f,
    -0.00572570377143160600f,
     0.00944762455140935400f,
     0.01833642159152087000f,
     0.01951596056273417400f,
     0.01425105314579677900f,
     0.00556805410937860300f,
    -0.00303755475142310600f,
    -0.00879211810732450800f,
    -0.01037620040368802800f,
    -0.00805109270691112400f,
    -0.00325549465831147200f,
     0.00209212864464293000f,
     0.00629851367006586300f,
     0.00837592349490335000f,
     0.00818906490649225900f,
     0.00629550382477281600f,
     0.00360340201563720100f,
     0.00100392507910410010f,
    -0.00089377211852528700f,
    -0.00186373289091749300f,
    -0.00201239902475433900f,
    -0.00163642520777623650f,
    -0.00106486378554213470f,
    -0.00054497771639121860f,
    -0.00020692388031130378f
};

// -----------------------------------------------------------------------------
void IFX_Distortion_Init(IFX_Distortion *d, float fs,
                         float hpfHz, float gain,
                         float lpfHz, float damp)
{
    d->T = 1.0f / fs;

    // FIR buffer
    for (uint8_t n = 0; n < IFX_DIST_LPF_INP_LENGTH; n++) d->lpfInpBuf[n] = 0.0f;
    d->lpfInpBufIndex = 0;
    d->lpfInpOut      = 0.0f;

    // HPF state
    d->hpfInpBufIn[0]  = 0.0f; d->hpfInpBufIn[1]  = 0.0f;
    d->hpfInpBufOut[0] = 0.0f; d->hpfInpBufOut[1] = 0.0f;
    d->hpfInpWcT       = 2.0f * M_PI * hpfHz * d->T;
    d->hpfInpOut       = 0.0f;

    // Clipper
    d->preGain = gain;
    d->Q       = -0.2f;

    // Output LPF state
    d->lpfOutBufIn[0]  = 0.0f; d->lpfOutBufIn[1]  = 0.0f; d->lpfOutBufIn[2]  = 0.0f;
    d->lpfOutBufOut[0] = 0.0f; d->lpfOutBufOut[1] = 0.0f; d->lpfOutBufOut[2] = 0.0f;
    d->lpfOutWcT       = 2.0f * M_PI * lpfHz * d->T;
    d->lpfOutDamp      = damp;
    d->lpfOutOut       = 0.0f;
    d->out             = 0.0f;
}

void IFX_Distortion_SetGain(IFX_Distortion *d, float gain)
{
    d->preGain = gain;
}

void IFX_Distortion_SetHPF(IFX_Distortion *d, float hpfHz)
{
    d->hpfInpWcT = 2.0f * M_PI * hpfHz * d->T;
}

void IFX_Distortion_SetLPF(IFX_Distortion *d, float lpfHz, float damp)
{
    d->lpfOutWcT  = 2.0f * M_PI * lpfHz * d->T;
    d->lpfOutDamp = damp;
}

float IFX_Distortion_Update(IFX_Distortion *d, float in)
{
    // ── Stage 1: anti-alias FIR LPF (fc = fs/4) ──────────────────────────
    d->lpfInpBuf[d->lpfInpBufIndex++] = in;
    if (d->lpfInpBufIndex == IFX_DIST_LPF_INP_LENGTH) d->lpfInpBufIndex = 0;

    d->lpfInpOut = 0.0f;
    uint8_t idx  = d->lpfInpBufIndex;
    for (uint8_t n = 0; n < IFX_DIST_LPF_INP_LENGTH; n++) {
        if (idx == 0) idx = IFX_DIST_LPF_INP_LENGTH - 1; else idx--;
        d->lpfInpOut += IFX_DIST_LPF_INP_COEF[n] * d->lpfInpBuf[idx];
    }

    // ── Stage 2: 1st-order bilinear IIR HPF ──────────────────────────────
    d->hpfInpBufIn[1]  = d->hpfInpBufIn[0];
    d->hpfInpBufIn[0]  = d->lpfInpOut;
    d->hpfInpBufOut[1] = d->hpfInpBufOut[0];
    d->hpfInpBufOut[0] =
        (2.0f * (d->hpfInpBufIn[0] - d->hpfInpBufIn[1])
         + (2.0f - d->hpfInpWcT) * d->hpfInpBufOut[1])
        / (2.0f + d->hpfInpWcT);
    d->hpfInpOut = d->hpfInpBufOut[0];

    // ── Stage 3: asymmetric exponential soft-clipper ──────────────────────
    // Implements the Doidic waveshaper from the InfiniFX Overdrive paper.
    // Q = -0.2 introduces slight asymmetry → even harmonics → warmth.
    float xGain   = d->preGain * d->hpfInpOut;
    const float dv = 8.0f;
    float clipOut  = d->Q / (1.0f - expf(dv * d->Q));
    if ((xGain - d->Q) >= 0.00001f) {
        clipOut += (xGain - d->Q) / (1.0f - expf(-dv * (xGain - d->Q)));
    }

    // ── Stage 4: 2nd-order resonant IIR LPF (tone / cabinet) ─────────────
    d->lpfOutBufIn[2]  = d->lpfOutBufIn[1];
    d->lpfOutBufIn[1]  = d->lpfOutBufIn[0];
    d->lpfOutBufIn[0]  = clipOut;

    d->lpfOutBufOut[2] = d->lpfOutBufOut[1];
    d->lpfOutBufOut[1] = d->lpfOutBufOut[0];

    float wc2 = d->lpfOutWcT * d->lpfOutWcT;
    d->lpfOutBufOut[0] =
        (wc2 * (d->lpfOutBufIn[0] + 2.0f * d->lpfOutBufIn[1] + d->lpfOutBufIn[2])
         - 2.0f * (wc2 - 4.0f)            * d->lpfOutBufOut[1]
         - (4.0f - 4.0f * d->lpfOutDamp * d->lpfOutWcT + wc2) * d->lpfOutBufOut[2])
        / (4.0f + 4.0f * d->lpfOutDamp * d->lpfOutWcT + wc2);

    d->lpfOutOut = d->lpfOutBufOut[0];

    // ── Stage 5: hard clip + output level ────────────────────────────────
    d->out = d->lpfOutOut;
    if      (d->out >  1.0f) d->out =  1.0f;
    else if (d->out < -1.0f) d->out = -1.0f;

    return d->out;
}
