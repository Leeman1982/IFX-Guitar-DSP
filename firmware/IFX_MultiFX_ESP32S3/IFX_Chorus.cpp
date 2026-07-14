#include "IFX_Chorus.h"

void IFX_Chorus_Init(IFX_Chorus *cho,
                     float delayTimeMsA, float delayTimeMsB,
                     float depthMsA, float depthMsB,
                     float gainA, float gainB,
                     float rateA, float rateB,
                     float mix,
                     float sampleRateHz) {

    cho->sampleTime = 1.0f / sampleRateHz;

    /* Convert times to samples */
    cho->baseDelayA = 0.001f * delayTimeMsA * sampleRateHz;
    cho->baseDelayB = 0.001f * delayTimeMsB * sampleRateHz;

    IFX_Chorus_SetDepth(cho, depthMsA, depthMsB);

    cho->gainA = gainA;
    cho->gainB = gainB;
    cho->mix   = mix;

    cho->rateA = rateA;
    cho->rateB = rateB;

    /* Reset LFO phases, offset B for a wider image */
    cho->phaseA = 0.0f;
    cho->phaseB = 0.25f;

    /* Clear delay line */
    for (uint32_t n = 0; n < IFX_CHORUS_BUF_LENGTH; n++) {
        cho->line[n] = 0.0f;
    }
    cho->writeIndex = 0;

    cho->out = 0.0f;
}

void IFX_Chorus_SetRate(IFX_Chorus *cho, float rateA, float rateB) {

    cho->rateA = rateA;
    cho->rateB = rateB;

}

void IFX_Chorus_SetDepth(IFX_Chorus *cho, float depthMsA, float depthMsB) {

    float sampleRateHz = 1.0f / cho->sampleTime;

    cho->depthA = 0.001f * depthMsA * sampleRateHz;
    cho->depthB = 0.001f * depthMsB * sampleRateHz;

    /* Keep base delay + depth within the delay line (leave 4 samples margin) */
    float maxDepthA = (float) IFX_CHORUS_BUF_LENGTH - cho->baseDelayA - 4.0f;
    float maxDepthB = (float) IFX_CHORUS_BUF_LENGTH - cho->baseDelayB - 4.0f;

    if (cho->depthA > maxDepthA) { cho->depthA = maxDepthA; }
    if (cho->depthB > maxDepthB) { cho->depthB = maxDepthB; }

    /* Depth may also not exceed the base delay (tap can't read the future) */
    if (cho->depthA > cho->baseDelayA - 4.0f) { cho->depthA = cho->baseDelayA - 4.0f; }
    if (cho->depthB > cho->baseDelayB - 4.0f) { cho->depthB = cho->baseDelayB - 4.0f; }

}

void IFX_Chorus_SetMix(IFX_Chorus *cho, float mix) {

    cho->mix = mix;

}

/* Linearly interpolated read, delaySamples behind the write index */
static inline float IFX_Chorus_Tap(IFX_Chorus *cho, float delaySamples) {

    float readPos = (float) cho->writeIndex - delaySamples;

    if (readPos < 0.0f) {
        readPos += (float) IFX_CHORUS_BUF_LENGTH;
    }

    uint32_t i0   = (uint32_t) readPos;
    float    frac = readPos - (float) i0;
    uint32_t i1   = i0 + 1;

    if (i1 >= IFX_CHORUS_BUF_LENGTH) {
        i1 = 0;
    }

    return (1.0f - frac) * cho->line[i0] + frac * cho->line[i1];

}

float IFX_Chorus_Update(IFX_Chorus *cho, float inp) {

    /* Write input into delay line */
    cho->line[cho->writeIndex] = inp;

    cho->writeIndex++;
    if (cho->writeIndex >= IFX_CHORUS_BUF_LENGTH) {
        cho->writeIndex = 0;
    }

    /* LFO-modulated tap delays */
    float delayA = cho->baseDelayA + cho->depthA * sinf(6.28318530718f * cho->phaseA);
    float delayB = cho->baseDelayB + cho->depthB * sinf(6.28318530718f * cho->phaseB);

    /* Advance LFO phases */
    cho->phaseA += cho->rateA * cho->sampleTime;
    cho->phaseB += cho->rateB * cho->sampleTime;

    if (cho->phaseA >= 1.0f) { cho->phaseA -= 1.0f; }
    if (cho->phaseB >= 1.0f) { cho->phaseB -= 1.0f; }

    /* Sum tap outputs and mix with dry signal */
    float wet = cho->gainA * IFX_Chorus_Tap(cho, delayA) + cho->gainB * IFX_Chorus_Tap(cho, delayB);

    cho->out = (1.0f - cho->mix) * inp + cho->mix * wet;

    /* Clamp output */
    if (cho->out > 1.0f) {
        cho->out = 1.0f;
    } else if (cho->out < -1.0f) {
        cho->out = -1.0f;
    }

    return cho->out;

}
