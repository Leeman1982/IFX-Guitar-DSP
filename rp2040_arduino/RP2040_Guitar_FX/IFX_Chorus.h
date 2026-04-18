#pragma once
// =============================================================================
// IFX Chorus  –  exact port of InfiniFX IFX_Chorus (Philip Salmony)
//
// Dual-voice BBD-style chorus.  Two independent LFO-modulated delay lines
// (A and B) are mixed with the dry signal.  Slightly detuning rateB from
// rateA produces a wide, lush stereo image even on a mono output.
// =============================================================================
#include <stdint.h>
#include <math.h>

#define IFX_CHORUS_MAX_DELAY_LENGTH  2048   // samples  (≈ 42 ms @ 48 kHz)

typedef struct {
    // ── Delay lines ──────────────────────────────────────────────────────
    float    delayLineA[IFX_CHORUS_MAX_DELAY_LENGTH];
    float    delayLineB[IFX_CHORUS_MAX_DELAY_LENGTH];
    uint16_t delayLineBaseLengthA;
    uint16_t delayLineBaseLengthB;
    uint16_t delayLineLengthA;
    uint16_t delayLineLengthB;
    uint16_t delayLineIndexA;
    uint16_t delayLineIndexB;

    // ── LFO parameters ──────────────────────────────────────────────────
    float depthA;
    float depthB;
    float rateA;
    float rateB;
    float gainA;
    float gainB;
    float mix;

    // ── LFO state ────────────────────────────────────────────────────────
    float sampleTime;
    float timeA;
    float timeB;
    float periodA;
    float periodB;

    float out;
} IFX_Chorus;

void  IFX_Chorus_Init  (IFX_Chorus *c,
                         float delayMsA,  float delayMsB,
                         float depthA,    float depthB,
                         float gainA,     float gainB,
                         float rateA,     float rateB,
                         float mix,
                         float sampleRateHz);

float IFX_Chorus_Update(IFX_Chorus *c, float in);
