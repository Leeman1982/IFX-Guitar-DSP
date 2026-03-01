#ifndef IFX_CHORUS_H
#define IFX_CHORUS_H

#include <Arduino.h>
#include <math.h>

#define IFX_CHORUS_MAX_DELAY_LENGTH 2048

typedef struct {
    /* Delay lines */
    float delayLineA[IFX_CHORUS_MAX_DELAY_LENGTH];
    float delayLineB[IFX_CHORUS_MAX_DELAY_LENGTH];
    uint16_t delayLineBaseLengthA;
    uint16_t delayLineBaseLengthB;
    uint16_t delayLineLengthA;
    uint16_t delayLineLengthB;
    uint16_t delayLineIndexA;
    uint16_t delayLineIndexB;

    /* Chorus parameters */
    float depthA;
    float depthB;

    float rateA;
    float rateB;

    float gainA;
    float gainB;
    float mix;

    /* Timers for LFOs */
    float sampleTime;
    float timeA;
    float timeB;
    float periodA;
    float periodB;

    /* Chorus output */
    float out;
} IFX_Chorus;

void IFX_Chorus_Init(IFX_Chorus *cho, float sampleRateHz);
float IFX_Chorus_ProcessA(IFX_Chorus *cho, float inp);
float IFX_Chorus_ProcessB(IFX_Chorus *cho, float inp);
void IFX_Chorus_SetRateA(IFX_Chorus *cho, float rate);
void IFX_Chorus_SetRateB(IFX_Chorus *cho, float rate);
void IFX_Chorus_SetDepthA(IFX_Chorus *cho, float depth);
void IFX_Chorus_SetDepthB(IFX_Chorus *cho, float depth);
void IFX_Chorus_SetMix(IFX_Chorus *cho, float mix);

#endif
