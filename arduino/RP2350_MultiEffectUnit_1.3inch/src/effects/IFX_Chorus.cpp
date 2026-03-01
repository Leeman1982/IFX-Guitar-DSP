#include "IFX_Chorus.h"

void IFX_Chorus_Init(IFX_Chorus *cho, float sampleRateHz) {
    /* Store chorus parameters with defaults */
    cho->depthA = 50.0f;
    cho->depthB = 50.0f;

    cho->rateA = 1.5f;
    cho->rateB = 1.8f;

    cho->periodA = 1.0f / cho->rateA;
    cho->periodB = 1.0f / cho->rateB;

    cho->gainA = 0.5f;
    cho->gainB = 0.5f;
    cho->mix = 0.5f;

    cho->sampleTime = 1.0f / sampleRateHz;

    /* Reset delay lines */
    for (uint16_t n = 0; n < IFX_CHORUS_MAX_DELAY_LENGTH; n++) {
        cho->delayLineA[n] = 0.0f;
        cho->delayLineB[n] = 0.0f;
    }

    // Base delay of ~10ms for A and ~15ms for B
    cho->delayLineBaseLengthA = (uint16_t)(0.010f * sampleRateHz);
    cho->delayLineBaseLengthB = (uint16_t)(0.015f * sampleRateHz);

    cho->delayLineIndexA = 0;
    cho->delayLineIndexB = 0;

    /* Reset LFO timers */
    cho->timeA = 0.0f;
    cho->timeB = 0.0f;

    /* Clear output */
    cho->out = 0.0f;
}

float IFX_Chorus_ProcessA(IFX_Chorus *cho, float inp) {
    /* Update delay line A */
    cho->delayLineA[cho->delayLineIndexA] = inp;
    cho->delayLineIndexA++;

    /* Get LFO output and modulate delay line length */
    int16_t lfoA = (int16_t)(cho->depthA * sinf(6.28318530718f * cho->rateA * cho->timeA));
    cho->delayLineLengthA = (uint16_t)(cho->delayLineBaseLengthA + lfoA);

    if (cho->delayLineIndexA >= cho->delayLineLengthA) {
        cho->delayLineIndexA = 0;
    }

    /* Update LFO timer */
    cho->timeA += cho->sampleTime;
    if (cho->timeA >= cho->periodA) {
        cho->timeA -= cho->periodA;
    }

    /* Mix with dry signal */
    cho->out = (1.0f - cho->mix) * inp + cho->mix * cho->gainA * cho->delayLineA[cho->delayLineIndexA];

    /* Clamp output */
    if (cho->out > 1.0f) {
        cho->out = 1.0f;
    } else if (cho->out < -1.0f) {
        cho->out = -1.0f;
    }

    return cho->out;
}

float IFX_Chorus_ProcessB(IFX_Chorus *cho, float inp) {
    /* Update delay line B */
    cho->delayLineB[cho->delayLineIndexB] = inp;
    cho->delayLineIndexB++;

    /* Get LFO output and modulate delay line length */
    int16_t lfoB = (int16_t)(cho->depthB * sinf(6.28318530718f * cho->rateB * cho->timeB));
    cho->delayLineLengthB = (uint16_t)(cho->delayLineBaseLengthB + lfoB);

    if (cho->delayLineIndexB >= cho->delayLineLengthB) {
        cho->delayLineIndexB = 0;
    }

    /* Update LFO timer */
    cho->timeB += cho->sampleTime;
    if (cho->timeB >= cho->periodB) {
        cho->timeB -= cho->periodB;
    }

    /* Mix with dry signal */
    cho->out = (1.0f - cho->mix) * inp + cho->mix * cho->gainB * cho->delayLineB[cho->delayLineIndexB];

    /* Clamp output */
    if (cho->out > 1.0f) {
        cho->out = 1.0f;
    } else if (cho->out < -1.0f) {
        cho->out = -1.0f;
    }

    return cho->out;
}

void IFX_Chorus_SetRateA(IFX_Chorus *cho, float rate) {
    cho->rateA = rate;
    cho->periodA = 1.0f / rate;
}

void IFX_Chorus_SetRateB(IFX_Chorus *cho, float rate) {
    cho->rateB = rate;
    cho->periodB = 1.0f / rate;
}

void IFX_Chorus_SetDepthA(IFX_Chorus *cho, float depth) {
    cho->depthA = depth * 100.0f; // Scale 0-1 to 0-100 samples
}

void IFX_Chorus_SetDepthB(IFX_Chorus *cho, float depth) {
    cho->depthB = depth * 100.0f;
}

void IFX_Chorus_SetMix(IFX_Chorus *cho, float mix) {
    cho->mix = mix;
}
