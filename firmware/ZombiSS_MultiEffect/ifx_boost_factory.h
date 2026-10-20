#ifndef IFX_BOOST_FACTORY_H
#define IFX_BOOST_FACTORY_H

/*
 * Boost Factory — circuit-modelled boost / overdrive / distortion / fuzz.
 *
 * Replaces the old single-curve TS Boost.  Nine pedals are simulated at
 * component level (see ifx_bf_models.c and BOOST_FACTORY.md):
 *   TS808, Klon, Distortion+ / DOD 250, RAT / Turbo RAT, DS-1, Big Muff Pi,
 *   Fuzz Face (Ge), Fuzz Face (Si), Rangemaster.
 *
 * Threading model (RP2350 dual core):
 *   - Setters + IFX_BF_Service() run on the UI core.  Service() rebuilds the
 *     circuit matrices (double precision) into the coefficient set the audio
 *     core is NOT using, then publishes it.
 *   - IFX_BF_Process() runs in the audio IRQ and only swaps sets at a block
 *     boundary, so the audio core never sees a half-written matrix.
 */

#include <stdint.h>
#include <stdbool.h>
#include "ifx_bf_models.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Analog scaling: PCM1808 full scale ≈ 3 Vpp at VCC = 5 V, so ±1.0 digital
 * maps to ±1.5 V at the "guitar" input of the modelled pedal. */
#ifndef BF_IN_FS_VOLTS
#define BF_IN_FS_VOLTS  1.5f
#endif
#ifndef BF_OUT_FS_VOLTS
#define BF_OUT_FS_VOLTS 1.5f
#endif

#define BF_BLOCK_MAX 64     /* largest block handed to IFX_BF_Process()   */
#define BF_OS_MAX    4

typedef struct {
    bf_dk       dk[BF_MAX_STAGES];
    bf_dk_state init[BF_MAX_STAGES];
    bf_halfband up0, up1, dn0, dn1;     /* pre-rolled to the DC operating point */
    float       slew0[BF_MAX_STAGES];
    float       inGain;
    uint8_t     model;
    uint8_t     reset;
    /* stage layout, resolved at build time */
    uint8_t     nPre, s0, s1, preRail, osf, auxOs, auxBase;
} bf_coefset;

typedef struct {
    bf_coefset set[2];
    volatile uint8_t  pubIdx;
    volatile uint32_t pubSeq;
    volatile uint32_t ackSeq;

    /* UI-core side */
    uint8_t model, voice;
    float drive, tone;
    volatile bool dirty;
    bool modelDirty;
    uint8_t osCap;        /* 0 = model default; 1 = force 1× (CPU fallback) */

    /* audio-core side */
    uint8_t  cur;
    uint32_t seenSeq;
    bf_dk_state st[BF_MAX_STAGES];
    bf_halfband up0, up1, dn0, dn1, ua0, ua1;
    float slew[BF_MAX_STAGES];
    float in1[BF_BLOCK_MAX];
    float os[BF_BLOCK_MAX * BF_OS_MAX], osAux[BF_BLOCK_MAX * BF_OS_MAX];
    volatile float levelTarget;
    float level;
    float fade;
    float fs;
} IFX_BoostFactory;

void  IFX_BF_Init(IFX_BoostFactory *bf, float sampleRate_Hz);
void  IFX_BF_SetModel(IFX_BoostFactory *bf, uint8_t model);
void  IFX_BF_SetVoice(IFX_BoostFactory *bf, uint8_t voice);
void  IFX_BF_SetDrive(IFX_BoostFactory *bf, float drive_0to1);
void  IFX_BF_SetTone(IFX_BoostFactory *bf, float tone_0to1);
void  IFX_BF_SetLevelDb(IFX_BoostFactory *bf, float dB);
bool  IFX_BF_Service(IFX_BoostFactory *bf);          /* UI core            */
/* CPU fallback (UI core): render the oversampled stages at 1× (true) or
 * at the model's normal rate (false).  Takes effect via Service(). */
void  IFX_BF_SetLowCpu(IFX_BoostFactory *bf, bool low);
bool  IFX_BF_IsLowCpu(const IFX_BoostFactory *bf);
/* Process n ≤ BF_BLOCK_MAX samples in place (audio core). */
void  IFX_BF_Process(IFX_BoostFactory *bf, float *buf, int n);
void  IFX_BF_Reset(IFX_BoostFactory *bf);            /* audio core: re-arm */

uint8_t     IFX_BF_ModelCount(void);
const char *IFX_BF_ModelName(uint8_t m);
uint8_t     IFX_BF_VoiceCount(uint8_t m);
const char *IFX_BF_VoiceName(uint8_t m, uint8_t v);

#ifdef __cplusplus
}
#endif

#endif /* IFX_BOOST_FACTORY_H */
