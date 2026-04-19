#pragma once

// =============================================================================
// RP2040 Zero – Guitar Multi-FX  |  Global Configuration
// =============================================================================

// ── Audio ────────────────────────────────────────────────────────────────────
// Overclocked to 250 MHz – RP2040 is well-characterised at this frequency.
// SCKI PWM:  250 000 000 / 20 = 12 500 000 Hz  (ideal 12 288 000, Δ = +1.7 %)
//            Inaudible offset; PCM1808 slave mode tolerates ±5 % on SCKI.
// DSP headroom at 250 MHz vs 98 MHz: ~2.5× more cycles per audio buffer,
// enabling the 69-tap FIR + biquad cascade with plenty of margin.
// Buffer shrunk to 32 frames → ~0.67 ms round-trip latency (pro grade).
#define SYS_CLK_KHZ         250000

#define SAMPLE_RATE         48000
#define BITS_PER_SAMPLE     32          // 32-bit I2S frames (24-bit audio data)
#define AUDIO_CHANNELS      2
#define AUDIO_BUFFER_FRAMES 32          // ≈ 0.67 ms round-trip latency

#define LCD_I2C_ADDR        0x27        // PCF8574 backpack – change to 0x3F if needed

// ── Distortion defaults ───────────────────────────────────────────────────────
// Recommended by original InfiniFX author (Philip Salmony)
#define DIST_GAIN_DEFAULT       50.0f   // pre-gain  (range 1 – 500)
#define DIST_HPF_DEFAULT        150.0f  // Hz        (range 80 – 500)
#define DIST_TONE_DEFAULT       4000.0f // Hz LPF    (range 500 – 10 000)
#define DIST_TONE_DAMP_DEFAULT  1.0f    // damping   (range 0.5 – 2.0)
#define DIST_LEVEL_DEFAULT      0.7f    // 0 – 1.0

// ── Chorus defaults ──────────────────────────────────────────────────────────
// Dual-voice chorus, voices slightly detuned for width
#define CHO_DELAY_A_MS_DEFAULT  15.0f   // ms  (range 5 – 30)
#define CHO_DELAY_B_MS_DEFAULT  17.0f   // ms
#define CHO_DEPTH_A_DEFAULT     30.0f   // samples  (range 5 – 80)
#define CHO_DEPTH_B_DEFAULT     25.0f
#define CHO_GAIN_A_DEFAULT      0.7f
#define CHO_GAIN_B_DEFAULT      0.7f
#define CHO_RATE_A_DEFAULT      0.9f    // Hz  (range 0.1 – 8.0)
#define CHO_RATE_B_DEFAULT      1.1f    // Hz  (slightly detuned)
#define CHO_MIX_DEFAULT         0.45f   // 0 – 1.0

// ── EQ defaults ───────────────────────────────────────────────────────────────
// 3-band peaking EQ using IFX_PeakingFilter biquads
#define EQ_BASS_FREQ_HZ     100.0f
#define EQ_BASS_BW_HZ       200.0f
#define EQ_MID_FREQ_HZ      800.0f
#define EQ_MID_BW_HZ        400.0f
#define EQ_TREB_FREQ_HZ     6000.0f
#define EQ_TREB_BW_HZ       4000.0f

#define EQ_BASS_DB_DEFAULT   0.0f      // range -12 to +12 dB
#define EQ_MID_DB_DEFAULT    0.0f
#define EQ_TREB_DB_DEFAULT   0.0f
#define EQ_MID_FREQ_DEFAULT  800.0f    // adjustable 200 – 5000 Hz
#define EQ_MID_BW_DEFAULT    400.0f    // adjustable 100 – 2000 Hz

// ── UI timing ────────────────────────────────────────────────────────────────
#define SWITCH_DEBOUNCE_MS      30
#define ENCODER_DEBOUNCE_MS     4
#define LCD_UPDATE_MS           80
#define MUX_FULL_SCAN_US        800     // time for one full 6-channel MUX sweep
