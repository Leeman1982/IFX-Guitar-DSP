#ifndef EFFECT_CHAIN_H
#define EFFECT_CHAIN_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "ifx_noisegate.h"
#include "ifx_overdrive.h"
#include "ifx_chorus.h"
#include "ifx_delay.h"
#include "ifx_peaking_filter.h"

/* Effect indices */
#define FX_NOISEGATE   0
#define FX_OVERDRIVE   1
#define FX_EQ          2
#define FX_CHORUS      3
#define FX_DELAY       4
#define FX_COUNT       5

#define SAMPLE_RATE_HZ 48000.0f

/* Noise Gate params */
#define NG_THRESHOLD    0
#define NG_ATTACK       1
#define NG_RELEASE      2
#define NG_HOLD         3
#define NG_PARAM_COUNT  4

/* Overdrive params */
#define OD_GAIN         0
#define OD_BOOST        1
#define OD_HPF_CUTOFF   2
#define OD_LPF_CUTOFF   3
#define OD_LPF_DAMP     4
#define OD_Q_CLIP       5
#define OD_PARAM_COUNT  6

/* Peaking EQ params */
#define EQ_CENTER_FREQ  0
#define EQ_BANDWIDTH    1
#define EQ_BOOST_CUT    2
#define EQ_PARAM_COUNT  3

/* Chorus params */
#define CH_DELAY_A      0
#define CH_DELAY_B      1
#define CH_DEPTH_A      2
#define CH_DEPTH_B      3
#define CH_RATE_A       4
#define CH_RATE_B       5
#define CH_GAIN_A       6
#define CH_GAIN_B       7
#define CH_MIX          8
#define CH_PARAM_COUNT  9

/* Delay params */
#define DL_TIME         0
#define DL_MIX          1
#define DL_FEEDBACK     2
#define DL_PARAM_COUNT  3

#define MAX_PARAMS_PER_FX 9

typedef struct {
    const char *name;
    float min;
    float max;
    float step;
    float value;
    const char *unit;
} ParamDesc;

typedef struct {
    IFX_NoiseGate    noiseGate;
    IFX_Overdrive    overdrive;
    IFX_PeakingFilter eq;
    IFX_Chorus       chorus;
    IFX_Delay        delay;

    bool active[FX_COUNT];
    float masterVolume;

    ParamDesc params[FX_COUNT][MAX_PARAMS_PER_FX];
    uint8_t paramCount[FX_COUNT];
    const char *fxNames[FX_COUNT];
} EffectChain;

void  EffectChain_Init(EffectChain *ec);
float EffectChain_Process(EffectChain *ec, float inp);
void  EffectChain_ToggleEffect(EffectChain *ec, uint8_t fxIndex);
void  EffectChain_SetParam(EffectChain *ec, uint8_t fxIndex, uint8_t paramIndex, float value);
void  EffectChain_SetMasterVolume(EffectChain *ec, float vol);

#ifdef __cplusplus
}
#endif

#endif
