#ifndef IFX_CHORUS_H
#define IFX_CHORUS_H

#include <stdint.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IFX_CHORUS_MAX_DELAY_LENGTH 2048

typedef struct {
    float delayLine[IFX_CHORUS_MAX_DELAY_LENGTH];   /* shared write buffer   */
    uint16_t writeIndex;

    float baseA, baseB;     /* base delay (samples)                        */
    float depthA, depthB;   /* modulation depth (samples, peak)            */
    float rateA, rateB;     /* LFO Hz                                      */
    float incA, incB;       /* LFO phase increment (cycles / sample)       */
    float phaseA, phaseB;   /* LFO phase 0..1                              */
    float gainA, gainB;
    float mix;

    float sampleRate;
    float out;
} IFX_Chorus;

void  IFX_Chorus_Init(IFX_Chorus *cho,
                       float delayTimemsA, float delayTimemsB,
                       float depthA, float depthB,
                       float gainA, float gainB,
                       float rateA, float rateB,
                       float mix, float sampleRateHz);
float IFX_Chorus_Update(IFX_Chorus *cho, float inp);
/* Block form (audio core): in-place over n samples. */
void  IFX_Chorus_Process(IFX_Chorus *cho, float *buf, int n);
void  IFX_Chorus_SetDelayTime(IFX_Chorus *cho, float delayTimeMsA, float delayTimeMsB);
void  IFX_Chorus_SetDepth(IFX_Chorus *cho, float depthA, float depthB);
void  IFX_Chorus_SetRate(IFX_Chorus *cho, float rateA, float rateB);
void  IFX_Chorus_SetGain(IFX_Chorus *cho, float gainA, float gainB);
void  IFX_Chorus_SetMix(IFX_Chorus *cho, float mix);

#ifdef __cplusplus
}
#endif

#endif
