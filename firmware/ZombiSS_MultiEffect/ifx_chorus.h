#ifndef IFX_CHORUS_H
#define IFX_CHORUS_H

#include <stdint.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IFX_CHORUS_MAX_DELAY_LENGTH 2048

typedef struct {
    float delayLineA[IFX_CHORUS_MAX_DELAY_LENGTH];
    float delayLineB[IFX_CHORUS_MAX_DELAY_LENGTH];
    uint16_t delayLineBaseLengthA;
    uint16_t delayLineBaseLengthB;
    uint16_t delayLineLengthA;
    uint16_t delayLineLengthB;
    uint16_t delayLineIndexA;
    uint16_t delayLineIndexB;

    float depthA;
    float depthB;
    float rateA;
    float rateB;
    float gainA;
    float gainB;
    float mix;

    float sampleTime;
    float timeA;
    float timeB;
    float periodA;
    float periodB;

    float out;
} IFX_Chorus;

void  IFX_Chorus_Init(IFX_Chorus *cho,
                       float delayTimemsA, float delayTimemsB,
                       float depthA, float depthB,
                       float gainA, float gainB,
                       float rateA, float rateB,
                       float mix, float sampleRateHz);
float IFX_Chorus_Update(IFX_Chorus *cho, float inp);
void  IFX_Chorus_SetDelayTime(IFX_Chorus *cho, float delayTimeMsA, float delayTimeMsB);
void  IFX_Chorus_SetDepth(IFX_Chorus *cho, float depthA, float depthB);
void  IFX_Chorus_SetRate(IFX_Chorus *cho, float rateA, float rateB);
void  IFX_Chorus_SetGain(IFX_Chorus *cho, float gainA, float gainB);
void  IFX_Chorus_SetMix(IFX_Chorus *cho, float mix);

#ifdef __cplusplus
}
#endif

#endif
