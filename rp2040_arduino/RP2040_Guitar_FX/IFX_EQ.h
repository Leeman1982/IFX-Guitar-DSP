#pragma once
// =============================================================================
// IFX 3-Band Parametric EQ
// Built from three IFX_PeakingFilter biquads (Philip Salmony / InfiniFX)
//
// Band layout:
//   BASS    – peaking filter centred at ~100 Hz   (wide shelf-like bandwidth)
//   MID     – peaking filter centred at user freq  (adjustable freq & BW)
//   TREBLE  – peaking filter centred at ~6 kHz    (wide shelf-like bandwidth)
//
// Each band uses the exact bilinear-transform peaking filter from
// IFX_PeakingFilter.c/.h, ported verbatim.
// =============================================================================
#include <stdint.h>
#include <math.h>

// ── IFX PeakingFilter (verbatim struct + prototypes) ─────────────────────────
typedef struct {
    float sampleTime_s;
    float x[3];
    float y[3];
    float a[3];   // numerator  (x) coefficients
    float b[3];   // denominator (y) coefficients  (stored inverted – see .cpp)
} IFX_PeakingFilter;

void  IFX_PeakingFilter_Init         (IFX_PeakingFilter *f, float sampleRate_Hz);
void  IFX_PeakingFilter_SetParameters(IFX_PeakingFilter *f,
                                       float centerFreq_Hz,
                                       float bandwidth_Hz,
                                       float boostCut_linear);
float IFX_PeakingFilter_Update       (IFX_PeakingFilter *f, float in);

// ── 3-Band EQ wrapper ────────────────────────────────────────────────────────
typedef struct {
    IFX_PeakingFilter bass;
    IFX_PeakingFilter mid;
    IFX_PeakingFilter treb;

    float sampleRate;

    // Current band frequencies & bandwidths (stored for re-init)
    float bass_freq_hz,  bass_bw_hz;
    float mid_freq_hz,   mid_bw_hz;
    float treb_freq_hz,  treb_bw_hz;
} IFX_EQ;

void  IFX_EQ_Init(IFX_EQ *eq, float sampleRate_Hz,
                  float bass_freq,  float bass_bw,
                  float mid_freq,   float mid_bw,
                  float treb_freq,  float treb_bw);

// Set individual band gains (linear: >1 = boost, <1 = cut)
void  IFX_EQ_SetBass  (IFX_EQ *eq, float gain_linear);
void  IFX_EQ_SetMid   (IFX_EQ *eq, float gain_linear, float freq_hz, float bw_hz);
void  IFX_EQ_SetTreble(IFX_EQ *eq, float gain_linear);

float IFX_EQ_Update   (IFX_EQ *eq, float in);
