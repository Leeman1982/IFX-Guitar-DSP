#include "IFX_Chorus.h"

// Exact port of IFX_Chorus.c from InfiniFX-Reloaded-TikiDrive (Philip Salmony)

void IFX_Chorus_Init(IFX_Chorus *c,
                     float delayMsA,  float delayMsB,
                     float depthA,    float depthB,
                     float gainA,     float gainB,
                     float rateA,     float rateB,
                     float mix,
                     float sampleRateHz)
{
    c->depthA = depthA;
    c->depthB = depthB;
    c->rateA  = rateA;
    c->rateB  = rateB;
    c->gainA  = gainA;
    c->gainB  = gainB;
    c->mix    = mix;

    c->sampleTime = 1.0f / sampleRateHz;
    c->periodA    = 1.0f / rateA;
    c->periodB    = 1.0f / rateB;

    for (uint16_t n = 0; n < IFX_CHORUS_MAX_DELAY_LENGTH; n++) {
        c->delayLineA[n] = 0.0f;
        c->delayLineB[n] = 0.0f;
    }

    c->delayLineBaseLengthA = (uint16_t)(0.001f * delayMsA * sampleRateHz);
    c->delayLineBaseLengthB = (uint16_t)(0.001f * delayMsB * sampleRateHz);

    // Clamp LFO depth so modulated length never underflows (< 1) or overflows buffer.
    // This prevents the abrupt phase-jump that would occur if the fallback path fires.
    uint16_t maxDepthA = (c->delayLineBaseLengthA > 1) ? (c->delayLineBaseLengthA - 1) : 0;
    if (maxDepthA > IFX_CHORUS_MAX_DELAY_LENGTH - c->delayLineBaseLengthA - 1)
        maxDepthA = IFX_CHORUS_MAX_DELAY_LENGTH - c->delayLineBaseLengthA - 1;
    if (c->depthA > (float)maxDepthA) c->depthA = (float)maxDepthA;

    uint16_t maxDepthB = (c->delayLineBaseLengthB > 1) ? (c->delayLineBaseLengthB - 1) : 0;
    if (maxDepthB > IFX_CHORUS_MAX_DELAY_LENGTH - c->delayLineBaseLengthB - 1)
        maxDepthB = IFX_CHORUS_MAX_DELAY_LENGTH - c->delayLineBaseLengthB - 1;
    if (c->depthB > (float)maxDepthB) c->depthB = (float)maxDepthB;

    c->delayLineIndexA = 0;
    c->delayLineIndexB = 0;
    c->timeA = 0.0f;
    c->timeB = 0.0f;
    c->out   = 0.0f;
}

float IFX_Chorus_Update(IFX_Chorus *c, float in)
{
    // Write input into both delay lines
    c->delayLineA[c->delayLineIndexA] = in;
    c->delayLineB[c->delayLineIndexB] = in;

    c->delayLineIndexA++;
    c->delayLineIndexB++;

    // Sine LFOs modulate the read-back length
    int16_t lfoA = (int16_t)(c->depthA * sinf(6.28318530718f * c->rateA * c->timeA));
    int16_t lfoB = (int16_t)(c->depthB * sinf(6.28318530718f * c->rateB * c->timeB));

    // Clamp LFO output so (base + lfo) stays in [1, MAX-1] – no phase jump
    int16_t maxModA = (int16_t)(c->delayLineBaseLengthA - 1);
    int16_t minModA = -(int16_t)(c->delayLineBaseLengthA - 1);
    if (lfoA > maxModA) lfoA = maxModA;
    if (lfoA < minModA) lfoA = minModA;

    int16_t maxModB = (int16_t)(c->delayLineBaseLengthB - 1);
    int16_t minModB = -(int16_t)(c->delayLineBaseLengthB - 1);
    if (lfoB > maxModB) lfoB = maxModB;
    if (lfoB < minModB) lfoB = minModB;

    c->delayLineLengthA = (uint16_t)(c->delayLineBaseLengthA + lfoA);
    c->delayLineLengthB = (uint16_t)(c->delayLineBaseLengthB + lfoB);

    if (c->delayLineIndexA >= c->delayLineLengthA) c->delayLineIndexA = 0;
    if (c->delayLineIndexB >= c->delayLineLengthB) c->delayLineIndexB = 0;

    // Advance LFO timers
    c->timeA += c->sampleTime;
    c->timeB += c->sampleTime;
    if (c->timeA >= c->periodA) c->timeA -= c->periodA;
    if (c->timeB >= c->periodB) c->timeB -= c->periodB;

    // Mix: (1-mix)*dry + mix*(gainA*voiceA + gainB*voiceB)
    c->out = (1.0f - c->mix) * in
           + c->mix * (c->gainA * c->delayLineA[c->delayLineIndexA]
                     + c->gainB * c->delayLineB[c->delayLineIndexB]);

    if      (c->out >  1.0f) c->out =  1.0f;
    else if (c->out < -1.0f) c->out = -1.0f;

    return c->out;
}
