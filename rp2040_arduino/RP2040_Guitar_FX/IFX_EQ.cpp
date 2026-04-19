#include "IFX_EQ.h"

// =============================================================================
// IFX_PeakingFilter  –  verbatim port of IFX_PeakingFilter.c (Philip Salmony)
// Bilinear-transform 2nd-order peaking EQ biquad.
// boostCut_linear > 1 → boost, < 1 → cut, = 1 → all-pass.
// =============================================================================

void IFX_PeakingFilter_Init(IFX_PeakingFilter *f, float sampleRate_Hz)
{
    f->sampleTime_s = 1.0f / sampleRate_Hz;
    for (uint8_t n = 0; n < 3; n++) { f->x[n] = 0.0f; f->y[n] = 0.0f; }
    IFX_PeakingFilter_SetParameters(f, 1.0f, 0.0f, 1.0f);  // all-pass default
}

void IFX_PeakingFilter_SetParameters(IFX_PeakingFilter *f,
                                      float centerFreq_Hz,
                                      float bandwidth_Hz,
                                      float boostCut_linear)
{
    // Pre-warp centre frequency: wcT = 2 * tan(pi * fc * T)
    float wcT = 2.0f * tanf(M_PI * centerFreq_Hz * f->sampleTime_s);
    float Q   = bandwidth_Hz / centerFreq_Hz;   // quality factor from bandwidth

    // Numerator (x) coefficients
    f->a[0] = 4.0f + 2.0f * boostCut_linear * Q * wcT + wcT * wcT;
    f->a[1] = 2.0f * wcT * wcT - 8.0f;
    f->a[2] = 4.0f - 2.0f * boostCut_linear * Q * wcT + wcT * wcT;

    // Denominator (y) – stored as pre-divided / sign-inverted for the Update loop
    f->b[0] =  1.0f / (4.0f + 2.0f * Q * wcT + wcT * wcT);  // 1 / denom
    f->b[1] = -(2.0f * wcT * wcT - 8.0f);                    // -a1
    f->b[2] = -(4.0f - 2.0f * Q * wcT + wcT * wcT);          // -a2
}

float IFX_PeakingFilter_Update(IFX_PeakingFilter *f, float in)
{
    f->x[2] = f->x[1]; f->x[1] = f->x[0]; f->x[0] = in;
    f->y[2] = f->y[1]; f->y[1] = f->y[0];

    f->y[0] = (f->a[0] * f->x[0] + f->a[1] * f->x[1] + f->a[2] * f->x[2]
             +                      f->b[1] * f->y[1] + f->b[2] * f->y[2])
             * f->b[0];

    return f->y[0];
}

// =============================================================================
// 3-Band EQ wrapper
// =============================================================================

void IFX_EQ_Init(IFX_EQ *eq, float sampleRate_Hz,
                 float bass_freq,  float bass_bw,
                 float mid_freq,   float mid_bw,
                 float treb_freq,  float treb_bw)
{
    eq->sampleRate   = sampleRate_Hz;
    eq->bass_freq_hz = bass_freq;  eq->bass_bw_hz = bass_bw;
    eq->mid_freq_hz  = mid_freq;   eq->mid_bw_hz  = mid_bw;
    eq->treb_freq_hz = treb_freq;  eq->treb_bw_hz = treb_bw;

    IFX_PeakingFilter_Init(&eq->bass, sampleRate_Hz);
    IFX_PeakingFilter_Init(&eq->mid,  sampleRate_Hz);
    IFX_PeakingFilter_Init(&eq->treb, sampleRate_Hz);

    // Unity gain initialisation (all-pass)
    IFX_PeakingFilter_SetParameters(&eq->bass, bass_freq, bass_bw, 1.0f);
    IFX_PeakingFilter_SetParameters(&eq->mid,  mid_freq,  mid_bw,  1.0f);
    IFX_PeakingFilter_SetParameters(&eq->treb, treb_freq, treb_bw, 1.0f);
}

void IFX_EQ_SetBass(IFX_EQ *eq, float gain_linear)
{
    IFX_PeakingFilter_SetParameters(&eq->bass,
                                     eq->bass_freq_hz,
                                     eq->bass_bw_hz,
                                     gain_linear);
}

void IFX_EQ_SetMid(IFX_EQ *eq, float gain_linear, float freq_hz, float bw_hz)
{
    eq->mid_freq_hz = freq_hz;
    eq->mid_bw_hz   = bw_hz;
    IFX_PeakingFilter_SetParameters(&eq->mid, freq_hz, bw_hz, gain_linear);
}

void IFX_EQ_SetTreble(IFX_EQ *eq, float gain_linear)
{
    IFX_PeakingFilter_SetParameters(&eq->treb,
                                     eq->treb_freq_hz,
                                     eq->treb_bw_hz,
                                     gain_linear);
}

float IFX_EQ_Update(IFX_EQ *eq, float in)
{
    float y = IFX_PeakingFilter_Update(&eq->bass, in);
    y       = IFX_PeakingFilter_Update(&eq->mid,  y);
    y       = IFX_PeakingFilter_Update(&eq->treb, y);
    return y;
}
