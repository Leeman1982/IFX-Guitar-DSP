#include "ifx_chorus.h"
#include "pico/platform.h"

/* Fast sin(2*pi*p) for p in [0,1): parabolic approximation with one
 * refinement step (max error ~0.1% — far below audibility for an LFO,
 * and ~10x cheaper than libm sinf in the 48 kHz hot path). */
static inline float lfo_sin01(float p) {
    float v = 2.0f * p - 1.0f;               /* [-1, 1) */
    float s = -4.0f * v * (1.0f - fabsf(v)); /* parabola: sin(2*pi*p) approx */
    return 0.225f * (s * fabsf(s) - s) + s;  /* precision refinement */
}

void IFX_Chorus_Init(IFX_Chorus *cho,
                      float delayTimemsA, float delayTimemsB,
                      float depthA, float depthB,
                      float gainA, float gainB,
                      float rateA, float rateB,
                      float mix, float sampleRateHz) {
    cho->depthA = depthA;
    cho->depthB = depthB;
    cho->rateA  = rateA;
    cho->rateB  = rateB;
    cho->gainA  = gainA;
    cho->gainB  = gainB;
    cho->periodA = 1.0f / rateA;
    cho->periodB = 1.0f / rateB;
    cho->mix = mix;
    cho->sampleTime = 1.0f / sampleRateHz;

    for (uint16_t n = 0; n < IFX_CHORUS_MAX_DELAY_LENGTH; n++) {
        cho->delayLineA[n] = 0.0f;
        cho->delayLineB[n] = 0.0f;
    }

    cho->delayLineBaseLengthA = (uint16_t)(0.001f * delayTimemsA * sampleRateHz);
    cho->delayLineBaseLengthB = (uint16_t)(0.001f * delayTimemsB * sampleRateHz);
    cho->delayLineIndexA = 0;
    cho->delayLineIndexB = 0;
    cho->delayLineLengthA = cho->delayLineBaseLengthA;
    cho->delayLineLengthB = cho->delayLineBaseLengthB;
    cho->timeA = 0.0f;
    cho->timeB = 0.0f;
    cho->out = 0.0f;
}

float __not_in_flash_func(IFX_Chorus_Update)(IFX_Chorus *cho, float inp) {
    cho->delayLineA[cho->delayLineIndexA] = inp;
    cho->delayLineB[cho->delayLineIndexB] = inp;
    cho->delayLineIndexA++;
    cho->delayLineIndexB++;

    /* timeA/timeB stay in [0, period), so time*rate is the LFO phase in [0,1) */
    int16_t lfoA = (int16_t)(cho->depthA * lfo_sin01(cho->rateA * cho->timeA));
    int16_t lfoB = (int16_t)(cho->depthB * lfo_sin01(cho->rateB * cho->timeB));

    cho->delayLineLengthA = (uint16_t)(cho->delayLineBaseLengthA + lfoA);
    cho->delayLineLengthB = (uint16_t)(cho->delayLineBaseLengthB + lfoB);

    if (cho->delayLineLengthA < 1) cho->delayLineLengthA = 1;
    if (cho->delayLineLengthB < 1) cho->delayLineLengthB = 1;
    if (cho->delayLineLengthA >= IFX_CHORUS_MAX_DELAY_LENGTH)
        cho->delayLineLengthA = IFX_CHORUS_MAX_DELAY_LENGTH - 1;
    if (cho->delayLineLengthB >= IFX_CHORUS_MAX_DELAY_LENGTH)
        cho->delayLineLengthB = IFX_CHORUS_MAX_DELAY_LENGTH - 1;

    if (cho->delayLineIndexA >= cho->delayLineLengthA) cho->delayLineIndexA = 0;
    if (cho->delayLineIndexB >= cho->delayLineLengthB) cho->delayLineIndexB = 0;

    cho->timeA += cho->sampleTime;
    cho->timeB += cho->sampleTime;
    if (cho->timeA >= cho->periodA) cho->timeA -= cho->periodA;
    if (cho->timeB >= cho->periodB) cho->timeB -= cho->periodB;

    cho->out = (1.0f - cho->mix) * inp
             + cho->mix * (cho->gainA * cho->delayLineA[cho->delayLineIndexA]
                         + cho->gainB * cho->delayLineB[cho->delayLineIndexB]);

    if (cho->out > 1.0f) cho->out = 1.0f;
    else if (cho->out < -1.0f) cho->out = -1.0f;

    return cho->out;
}

void IFX_Chorus_SetDelayTime(IFX_Chorus *cho, float delayTimeMsA, float delayTimeMsB) {
    float sampleRateHz = 1.0f / cho->sampleTime;
    cho->delayLineBaseLengthA = (uint16_t)(0.001f * delayTimeMsA * sampleRateHz);
    cho->delayLineBaseLengthB = (uint16_t)(0.001f * delayTimeMsB * sampleRateHz);
    if (cho->delayLineBaseLengthA < 1) cho->delayLineBaseLengthA = 1;
    if (cho->delayLineBaseLengthB < 1) cho->delayLineBaseLengthB = 1;
    if (cho->delayLineBaseLengthA >= IFX_CHORUS_MAX_DELAY_LENGTH)
        cho->delayLineBaseLengthA = IFX_CHORUS_MAX_DELAY_LENGTH - 1;
    if (cho->delayLineBaseLengthB >= IFX_CHORUS_MAX_DELAY_LENGTH)
        cho->delayLineBaseLengthB = IFX_CHORUS_MAX_DELAY_LENGTH - 1;
}

void IFX_Chorus_SetDepth(IFX_Chorus *cho, float depthA, float depthB) {
    cho->depthA = depthA;
    cho->depthB = depthB;
}

void IFX_Chorus_SetRate(IFX_Chorus *cho, float rateA, float rateB) {
    cho->rateA = rateA;
    cho->rateB = rateB;
    cho->periodA = 1.0f / rateA;
    cho->periodB = 1.0f / rateB;
    /* Restart the LFO phase: if the rate jumps up while time is near the
     * end of the old (longer) period, time*rate would exceed 1 for many
     * samples until the single-subtraction wrap catches up. */
    cho->timeA = 0.0f;
    cho->timeB = 0.0f;
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
