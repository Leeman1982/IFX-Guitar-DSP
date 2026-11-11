#ifndef IFX_REVERB_H
#define IFX_REVERB_H

#include <stdint.h>
#include "ifx_bf_math.h"   /* bf_halfband (half-rate option) */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Plate reverb — J. Dattorro, "Effect Design Part 1: Reverberator and Other
 * Filters", JAES vol. 45 no. 9, 1997 (figure-of-eight tank).  Written from
 * the paper for this project (MIT); NOT the DaisySP/Csound ReverbSc used in
 * the GuitarML pedal, which is LGPL-2.1.
 *
 * From the GuitarML pedal we keep the idea of int16 delay storage: all lines
 * live in one int16 pool (~82 KB at 48 kHz instead of ~164 KB as float).
 *
 *   in ─ feed ─ predelay ─ bandwidth LPF ─ 4 input allpasses ─┬─ left tank ─┐
 *                                                            └─ right tank ┘ (cross-coupled)
 *   wet = 14 output taps (L+R summed — the ZombiSS output is mono)
 *
 * Decay → 1 with Freeze: input muted, damping off, lossless tank.
 */

/* Build options (small-RAM / slow targets override them):
 *   IFX_REVERB_HALF_RATE 1 → the tank runs at fs/2 behind IIR half-band
 *                            decimation/interpolation: half the RAM and about
 *                            half the CPU; wet bandwidth ≈ 10 kHz (plates are
 *                            damped well below that anyway).
 *   IFX_REVERB_POOL        → int16 samples for all lines; if the lines do not
 *                            fit, Init shrinks the tank until they do. */
#ifndef IFX_REVERB_HALF_RATE
#define IFX_REVERB_HALF_RATE 0
#endif
#ifndef IFX_REVERB_POOL
#if IFX_REVERB_HALF_RATE
#define IFX_REVERB_POOL      20800u   /* tank at 24 kHz + 100 ms predelay */
#else
#define IFX_REVERB_POOL      42000u   /* tank at 48 kHz + 100 ms predelay */
#endif
#endif
#define IFX_REVERB_MAX_PRE_MS 100.0f

typedef struct {
    uint32_t off, len, idx;
} IFX_RvLine;

typedef struct {
    int16_t  pool[IFX_REVERB_POOL];
    IFX_RvLine pre, in1, in2, in3, in4;
    IFX_RvLine apL1, dlL1, apL2, dlL2;     /* left tank  */
    IFX_RvLine apR1, dlR1, apR2, dlR2;     /* right tank */
    uint32_t tapL[7], tapR[7];             /* output tap offsets (samples) */
    float    excursion, apL1Nom, apR1Nom;  /* modulated allpass delays     */
    float    lfoPh, lfoInc;

    float    fs;
    float    preDelay;          /* samples                       */
    float    decay;             /* 0 … 0.98 (user)               */
    float    damping;           /* one-pole coefficient (0 = none) */
    float    dampHz;
    float    bandwidth;         /* input LPF coefficient          */
    float    mix;
    float    feed, feedTarget;
    uint8_t  freeze;

    float    bwZ, dampL, dampR;
    float    out;
    float    tankScale;          /* lengths relative to Dattorro's 29761 Hz */
#if IFX_REVERB_HALF_RATE
    bf_halfband dn, up;         /* fs ↔ fs/2                                */
    float    pairIn[2], pairOut[2];
    uint8_t  phase;
#endif
} IFX_Reverb;

void  IFX_Reverb_Init(IFX_Reverb *rv, float sampleRate_Hz);
/* Wet only (in and wet may alias). */
void  IFX_Reverb_ProcessWet(IFX_Reverb *rv, const float *in, float *wet, int n);
/* In place, mixed with the pedal mix law (ifx_mix.h). */
void  IFX_Reverb_Process(IFX_Reverb *rv, float *buf, int n);

void  IFX_Reverb_SetDecay(IFX_Reverb *rv, float decay01);      /* 0 … 0.98 */
void  IFX_Reverb_SetDamping(IFX_Reverb *rv, float cutoff_Hz);  /* tank LPF */
void  IFX_Reverb_SetPreDelay(IFX_Reverb *rv, float ms);
void  IFX_Reverb_SetMix(IFX_Reverb *rv, float mix);
void  IFX_Reverb_SetFeed(IFX_Reverb *rv, float target01);
void  IFX_Reverb_SetFreeze(IFX_Reverb *rv, uint8_t on);
void  IFX_Reverb_Clear(IFX_Reverb *rv);

#ifdef __cplusplus
}
#endif

#endif
