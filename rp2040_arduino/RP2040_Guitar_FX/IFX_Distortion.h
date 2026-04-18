#pragma once
// =============================================================================
// IFX Distortion  –  exact port of InfiniFX IFX_Overdrive (Philip Salmony)
//
// Signal path per sample:
//   1. 69-tap FIR anti-alias LPF  (fc = fs/4)
//   2. 1st-order IIR HPF          (removes low-end mud before clipping)
//   3. Asymmetric soft-clipper     (exponential waveshaper, Q = -0.2)
//   4. 2nd-order IIR resonant LPF (tone / "cabinet" filter)
//   5. Hard limiter  [-1, 1]
// =============================================================================
#include <stdint.h>
#include <math.h>

#define IFX_DIST_LPF_INP_LENGTH  69    // anti-alias FIR tap count

extern const float IFX_DIST_LPF_INP_COEF[IFX_DIST_LPF_INP_LENGTH];

typedef struct {
    float T;                                        // sample period (1/fs)

    // ── Stage 1 : anti-alias FIR LPF ─────────────────────────────────────
    float   lpfInpBuf[IFX_DIST_LPF_INP_LENGTH];
    uint8_t lpfInpBufIndex;
    float   lpfInpOut;

    // ── Stage 2 : 1st-order IIR HPF ─────────────────────────────────────
    float hpfInpBufIn[2];
    float hpfInpBufOut[2];
    float hpfInpWcT;
    float hpfInpOut;

    // ── Stage 3 : asymmetric clipper ─────────────────────────────────────
    float preGain;
    float Q;                                        // fixed = -0.2f

    // ── Stage 4 : 2nd-order resonant LPF ────────────────────────────────
    float lpfOutBufIn[3];
    float lpfOutBufOut[3];
    float lpfOutWcT;
    float lpfOutDamp;
    float lpfOutOut;

    float out;
} IFX_Distortion;

void  IFX_Distortion_Init   (IFX_Distortion *d, float fs,
                              float hpfHz, float gain,
                              float lpfHz, float damp);
void  IFX_Distortion_SetGain(IFX_Distortion *d, float gain);
void  IFX_Distortion_SetHPF (IFX_Distortion *d, float hpfHz);
void  IFX_Distortion_SetLPF (IFX_Distortion *d, float lpfHz, float damp);
float IFX_Distortion_Update (IFX_Distortion *d, float in);
