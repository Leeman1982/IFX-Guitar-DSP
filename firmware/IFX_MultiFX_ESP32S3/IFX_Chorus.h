/*
*
*   InfiniFX - Chorus (ESP32 port)
*
*   Two LFO-modulated, linearly interpolated taps on a single delay
*   line, mixed with the dry signal.
*
*/

#ifndef IFX_CHORUS_H
#define IFX_CHORUS_H

#include <stdint.h>
#include <math.h>

/* 4096 samples = ~93 ms at 44.1 kHz */
#define IFX_CHORUS_BUF_LENGTH 4096

typedef struct {
    /* Delay line */
    float    line[IFX_CHORUS_BUF_LENGTH];
    uint32_t writeIndex;

    /* Base delays and modulation depths (samples) */
    float baseDelayA;
    float baseDelayB;
    float depthA;
    float depthB;

    /* LFOs */
    float rateA;
    float rateB;
    float phaseA;
    float phaseB;
    float sampleTime;

    /* Mix */
    float gainA;
    float gainB;
    float mix;

    float out;

} IFX_Chorus;

void  IFX_Chorus_Init(IFX_Chorus *cho,
                      float delayTimeMsA, float delayTimeMsB,
                      float depthMsA, float depthMsB,
                      float gainA, float gainB,
                      float rateA, float rateB,
                      float mix,
                      float sampleRateHz);

void  IFX_Chorus_SetRate(IFX_Chorus *cho, float rateA, float rateB);
void  IFX_Chorus_SetDepth(IFX_Chorus *cho, float depthMsA, float depthMsB);
void  IFX_Chorus_SetMix(IFX_Chorus *cho, float mix);
float IFX_Chorus_Update(IFX_Chorus *cho, float inp);

#endif
