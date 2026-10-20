#ifndef EFFECT_CHAIN_H
#define EFFECT_CHAIN_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "ifx_boost_factory.h"
#include "ifx_noisegate.h"
#include "ifx_overdrive.h"
#include "ifx_10band_eq.h"
#include "ifx_chorus.h"
#include "ifx_delay.h"

/* ===== Effect indices (chain order) ===== */
#define FX_BOOST       0   /* Boost Factory (9 circuit models) — footswitch FX1 */
#define FX_NOISEGATE   1   /* Noise gate           — footswitch FX2 */
#define FX_OVERDRIVE   2   /* Overdrive/distortion — footswitch FX3 */
#define FX_EQ          3   /* 10-band graphic EQ   — footswitch FX4 */
#define FX_CHORUS      4   /* Chorus               — footswitch FX5 */
#define FX_DELAY       5   /* Delay                — menu toggle only */
#define FX_COUNT       6

#define SAMPLE_RATE_HZ 48000.0f

/* Boost Factory params */
#define BF_MODEL        0   /* enum: pedal model                          */
#define BF_VOICE        1   /* enum: per-model variant (diodes, pickup …) */
#define BF_DRIVE        2   /* 0–10: drive/gain/fuzz/sustain pot          */
#define BF_TONE         3   /* 0–10: tone/filter/treble/guitar volume     */
#define BF_LEVEL        4   /* dB, output level                           */
#define BF_PARAM_COUNT  5

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

/* 10-Band EQ params — one dB gain per octave band */
#define EQ_31HZ         0
#define EQ_63HZ         1
#define EQ_125HZ        2
#define EQ_250HZ        3
#define EQ_500HZ        4
#define EQ_1KHZ         5
#define EQ_2KHZ         6
#define EQ_4KHZ         7
#define EQ_8KHZ         8
#define EQ_16KHZ        9
#define EQ_PARAM_COUNT  10  /* == EQ10_BANDS */

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

#define MAX_PARAMS_PER_FX 10  /* EQ_PARAM_COUNT is the maximum */

typedef struct {
    const char *name;
    float min;
    float max;
    float step;
    float value;
    const char *unit;
    uint8_t isEnum;   /* value is an index: display via EffectChain_FormatValue */
} ParamDesc;

typedef struct {
    IFX_BoostFactory boost;
    IFX_NoiseGate    noiseGate;
    IFX_Overdrive    overdrive;
    IFX_10BandEQ     eq;
    IFX_Chorus       chorus;
    IFX_Delay        delay;

    bool active[FX_COUNT];
    float masterVolume;

    ParamDesc params[FX_COUNT][MAX_PARAMS_PER_FX];
    uint8_t paramCount[FX_COUNT];
    const char *fxNames[FX_COUNT];
} EffectChain;

void  EffectChain_Init(EffectChain *ec);
/* Process n mono samples in place (n ≤ BF_BLOCK_MAX): Boost + Rest. */
void  EffectChain_ProcessBlock(EffectChain *ec, float *buf, int n);
/* The two halves separately, so they can run on different cores:
 * Boost Factory (core 1, doorbell IRQ) and everything after it (core 0). */
void  EffectChain_ProcessBoost(EffectChain *ec, float *buf, int n);
void  EffectChain_ProcessRest(EffectChain *ec, float *buf, int n);
/* CPU fallback for the Boost Factory (render at 1× instead of 2×/4×). */
void  EffectChain_SetBoostLowCpu(EffectChain *ec, bool low);
/* UI core: rebuild circuit models after parameter changes (call often). */
void  EffectChain_Service(EffectChain *ec);
/* Text for enum-valued params; returns false for plain numeric params. */
bool  EffectChain_FormatValue(const EffectChain *ec, uint8_t fxIndex, uint8_t paramIndex,
                              char *buf, unsigned size);
void  EffectChain_ToggleEffect(EffectChain *ec, uint8_t fxIndex);
void  EffectChain_SetParam(EffectChain *ec, uint8_t fxIndex, uint8_t paramIndex, float value);
void  EffectChain_SetMasterVolume(EffectChain *ec, float vol);

#ifdef __cplusplus
}
#endif

#endif /* EFFECT_CHAIN_H */
