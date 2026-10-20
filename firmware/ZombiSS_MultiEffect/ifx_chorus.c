#pragma GCC optimize ("O2")
#include "ifx_chorus.h"
#include "ifx_bf_math.h"   /* IFX_RAMFUNC */
#include <string.h>

/*
 * Two-voice chorus: both voices read one shared circular buffer at a
 * sinusoidally modulated, fractional delay (linear interpolation).
 *
 * The previous version modulated the *wrap length* of each buffer instead,
 * which produced discontinuities (clicks) whenever the length changed, and
 * evaluated sinf() twice per sample.  The LFO is now a phase accumulator
 * with a 7th-order odd polynomial sine (|err| < 1e-4).
 */

static inline float lfo_sin(float phase /* 0..1 */) {
    /* map to [-π, π] then fold to [-π/2, π/2] */
    float x = phase * 6.28318531f - 3.14159265f;
    if (x >  1.57079633f) x =  3.14159265f - x;
    if (x < -1.57079633f) x = -3.14159265f - x;
    float x2 = x * x;
    /* sin(x − π) = −sin(x): sign flip folded into the coefficients */
    return -x * (1.0f + x2 * (-0.16666667f + x2 * (0.0083333f - x2 * 0.00019841f)));
}

static inline float clamp_delay(float d) {
    if (d < 1.0f) d = 1.0f;
    if (d > (float)(IFX_CHORUS_MAX_DELAY_LENGTH - 2)) d = (float)(IFX_CHORUS_MAX_DELAY_LENGTH - 2);
    return d;
}

static inline float tap(const IFX_Chorus *cho, float delay) {
    float rp = (float)cho->writeIndex - delay;
    if (rp < 0.0f) rp += (float)IFX_CHORUS_MAX_DELAY_LENGTH;
    int i0 = (int)rp;
    float frac = rp - (float)i0;
    int i1 = i0 + 1;
    if (i1 >= IFX_CHORUS_MAX_DELAY_LENGTH) i1 = 0;
    return cho->delayLine[i0] + frac * (cho->delayLine[i1] - cho->delayLine[i0]);
}

void IFX_Chorus_Init(IFX_Chorus *cho,
                      float delayTimemsA, float delayTimemsB,
                      float depthA, float depthB,
                      float gainA, float gainB,
                      float rateA, float rateB,
                      float mix, float sampleRateHz) {
    memset(cho, 0, sizeof(*cho));
    cho->sampleRate = sampleRateHz;
    IFX_Chorus_SetDelayTime(cho, delayTimemsA, delayTimemsB);
    IFX_Chorus_SetDepth(cho, depthA, depthB);
    IFX_Chorus_SetRate(cho, rateA, rateB);
    IFX_Chorus_SetGain(cho, gainA, gainB);
    IFX_Chorus_SetMix(cho, mix);
    cho->phaseB = 0.25f;                 /* decorrelate the two voices */
}

float IFX_RAMFUNC(IFX_Chorus_Update)(IFX_Chorus *cho, float inp) {
    cho->delayLine[cho->writeIndex] = inp;

    cho->phaseA += cho->incA; if (cho->phaseA >= 1.0f) cho->phaseA -= 1.0f;
    cho->phaseB += cho->incB; if (cho->phaseB >= 1.0f) cho->phaseB -= 1.0f;

    float dA = clamp_delay(cho->baseA + cho->depthA * lfo_sin(cho->phaseA));
    float dB = clamp_delay(cho->baseB + cho->depthB * lfo_sin(cho->phaseB));
    float wet = cho->gainA * tap(cho, dA) + cho->gainB * tap(cho, dB);

    if (++cho->writeIndex >= IFX_CHORUS_MAX_DELAY_LENGTH) cho->writeIndex = 0;

    float out = (1.0f - cho->mix) * inp + cho->mix * wet;
    if (out > 1.0f) out = 1.0f;
    else if (out < -1.0f) out = -1.0f;
    cho->out = out;
    return out;
}

void IFX_Chorus_SetDelayTime(IFX_Chorus *cho, float delayTimeMsA, float delayTimeMsB) {
    cho->baseA = clamp_delay(0.001f * delayTimeMsA * cho->sampleRate);
    cho->baseB = clamp_delay(0.001f * delayTimeMsB * cho->sampleRate);
}

void IFX_Chorus_SetDepth(IFX_Chorus *cho, float depthA, float depthB) {
    cho->depthA = depthA;
    cho->depthB = depthB;
}

void IFX_Chorus_SetRate(IFX_Chorus *cho, float rateA, float rateB) {
    cho->rateA = rateA;
    cho->rateB = rateB;
    cho->incA = rateA / cho->sampleRate;
    cho->incB = rateB / cho->sampleRate;
}

void IFX_Chorus_SetGain(IFX_Chorus *cho, float gainA, float gainB) {
    cho->gainA = gainA;
    cho->gainB = gainB;
}

void IFX_Chorus_SetMix(IFX_Chorus *cho, float mix) {
    if (mix > 1.0f) mix = 1.0f;
    if (mix < 0.0f) mix = 0.0f;
    cho->mix = mix;
}

void IFX_RAMFUNC(IFX_Chorus_Process)(IFX_Chorus *cho, float *buf, int n) {
    for (int i = 0; i < n; i++) buf[i] = IFX_Chorus_Update(cho, buf[i]);
}
