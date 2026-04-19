#pragma once
#include <math.h>

// =============================================================================
// Shared effect parameters – written by Core 0 (UI), read by Core 1 (DSP)
//
// Each member is `volatile` so the compiler never caches the value in a
// register across cores.  On Cortex-M0+, a naturally-aligned 32-bit store /
// load is a single instruction and therefore atomic, making this safe for
// single-writer / single-reader float parameters.
// =============================================================================

struct DistortionParams {
    volatile bool   enabled;
    volatile float  gain;           // pre-gain     1.0 – 500.0
    volatile float  hpf_freq;       // input HPF    80  – 500  Hz
    volatile float  tone_freq;      // output LPF   500 – 10000 Hz
    volatile float  tone_damp;      // LPF damping  0.5 – 2.0
    volatile float  level;          // output scale 0.0 – 1.0
    volatile bool   needs_update;   // Core 1 re-runs SetLPF / SetHPF when true
};

struct ChorusParams {
    volatile bool   enabled;
    volatile float  rate;           // LFO rate  0.1 – 8.0 Hz  (both voices track)
    volatile float  depth;          // LFO depth 5   – 80  samples
    volatile float  mix;            // wet/dry   0.0 – 1.0
    volatile float  delay_ms;       // base delay 5  – 30  ms
    volatile bool   needs_update;
};

struct EQParams {
    volatile bool   enabled;
    volatile float  bass_db;        // -12 – +12 dB
    volatile float  mid_db;         // -12 – +12 dB
    volatile float  treb_db;        // -12 – +12 dB
    volatile float  mid_freq;       // 200 – 5000 Hz
    volatile float  mid_bw;         // 100 – 2000 Hz bandwidth
    volatile bool   needs_update;
};

// Globals – defined in the .ino, declared here for other TUs
extern DistortionParams g_dist;
extern ChorusParams     g_chorus;
extern EQParams         g_eq;

// dB ↔ linear helpers (header-only, used across UI and DSP)
inline float db_to_linear(float db)      { return powf(10.0f, db / 20.0f); }
inline float linear_to_db(float linear)  { return 20.0f * log10f(linear);  }

// Clamp helper
inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
