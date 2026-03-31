/*
 * ZOMBI SS Multi-Effect DSP Unit
 * ================================
 * Target: RP2350 (Raspberry Pi Pico 2) via Arduino-Pico core
 *
 * Architecture:
 *   Core 0: Real-time audio DSP (PIO I2S + DMA interrupt)
 *   Core 1: UI (SH1106 OLED + EC11 rotary encoder + buttons)
 *
 * Audio I/O:
 *   Input:  PCM1808 ADC via PIO1 I2S
 *   Output: PCM5102 DAC via PIO0 I2S
 *
 * Effects Chain:
 *   1. Noise Gate   2. Overdrive   3. Peaking EQ
 *   4. Chorus       5. Delay
 *
 * Original DSP algorithms by Philip Salmony @ phils-lab.net
 * Ported to RP2350 Arduino for Zombi SS
 */

#include <Arduino.h>
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include <math.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "effect_chain.h"
#include "i2s_audio.h"
#ifdef __cplusplus
}
#endif

#include "sh1106_oled.h"
#include "rotary_encoder.h"
#include "ui_system.h"
#include "splash_screen.h"

/* ===== Global State ===== */

/* Effect chain - shared between cores */
static EffectChain g_fx_chain;

/* UI components (Core 1 only) */
static SH1106 g_oled;
static RotaryEncoder g_encoder;
static UISystem g_ui;

/* ===== Diagnostic test tone state ===== */
/* Plays a 1kHz sine wave for 2 seconds on boot to verify DAC hardware
 * independently of the ADC. If you hear this tone, the DAC pipeline works.
 * If silent, the problem is hardware: check XSMT wiring, I2S connections. */
#define TONE_DURATION_FRAMES  (AUDIO_SAMPLE_RATE * 2)   /* 2 seconds */
static volatile uint32_t tone_phase_acc = 0;
/* Phase increment for 1kHz at 48kHz: 1000/48000 * 2^32 ≈ 89478.5 → 89479 */
static const uint32_t TONE_PHASE_INC = 89479U;

/* ===== Core 0: Audio DSP Callback ===== */

/*
 * Called from DMA IRQ on Core 0.
 * Latency: 64 frames / 48kHz = ~1.33ms per buffer = ~2.67ms round-trip.
 */
static void audio_process_callback(const int32_t *input, int32_t *output, uint32_t frame_count_arg) {
    static uint32_t total_frames = 0;

    for (uint32_t i = 0; i < frame_count_arg; i++) {
        int32_t out_i32;

        if (total_frames < TONE_DURATION_FRAMES) {
            /* --- Diagnostic 1kHz sine tone (first 2 seconds) --- */
            /* Fast sine approximation using phase accumulator.
             * Maps 0–2^32 → 0–2π via Bhaskara I approximation. */
            uint32_t phase = tone_phase_acc;
            tone_phase_acc += TONE_PHASE_INC;

            /* Normalise phase to [0, 1) then use sinf for correctness */
            float t   = (float)phase * (1.0f / 4294967296.0f); /* 0..1 */
            float s   = sinf(t * 6.2831853f) * 0.5f;           /* ±0.5 amplitude */

            out_i32 = (int32_t)(s * 8388607.0f);
            if (out_i32 >  8388607) out_i32 =  8388607;
            if (out_i32 < -8388608) out_i32 = -8388608;
            out_i32 <<= 8;

            total_frames++;
        } else {
            /* --- Normal guitar processing --- */
            int32_t raw_left = input[i * 2];

            /* 24-bit I2S → float [-1.0, +1.0]
             * I2S standard has a 1-bit delay: first BCK rising edge after LRCK
             * holds old data; actual MSB starts on the second rising edge.
             * The raw 32-bit word is: [delay_bit | audio_23..0 | padding_6bits]
             * Shift out the delay bit before extracting the 24-bit value. */
            float inp = (float)((raw_left << 1) >> 8) / 8388608.0f;

            /* Process effect chain */
            float out = EffectChain_Process(&g_fx_chain, inp);

            /* Float → 24-bit I2S (left-justified in 32-bit word) */
            out_i32 = (int32_t)(out * 8388607.0f);
            if (out_i32 >  8388607) out_i32 =  8388607;
            if (out_i32 < -8388608) out_i32 = -8388608;
            out_i32 <<= 8;
        }

        /* Stereo output (same signal both channels) */
        output[i * 2]     = out_i32;
        output[i * 2 + 1] = out_i32;
    }
}

/* ===== Core 0: setup() and loop() ===== */

void setup() {
    /* Overclock to 150 MHz for DSP headroom */
    set_sys_clock_khz(SYS_CLOCK_KHZ, true);

    /* Generate SCKI for PCM1808 ADC on GP22.
     * PCM1808 slave mode still requires SCKI for its internal sigma-delta
     * converter — without it the ADC produces no output and there is silence.
     * Target: 256 × 48000 = 12.288 MHz.
     * PWM: 150 MHz / 12 = 12.5 MHz (1.7% off, within PCM1808 tolerance). */
    {
        uint scki_slice = pwm_gpio_to_slice_num(22);
        uint scki_chan  = pwm_gpio_to_channel(22);
        gpio_set_function(22, GPIO_FUNC_PWM);
        pwm_set_clkdiv(scki_slice, 1.0f);
        pwm_set_wrap(scki_slice, 11);               /* 12 counts → 12.5 MHz  */
        pwm_set_chan_level(scki_slice, scki_chan, 6); /* 50% duty cycle       */
        pwm_set_enabled(scki_slice, true);
    }

    /* PCM5102: drive XSMT HIGH to unmute DAC analog output.
     * Without this the PCM5102 output stage is in soft-mute regardless of
     * valid I2S data — the most common cause of complete DAC silence.
     * XSMT must be driven; leaving it floating or low keeps the mute active. */
    gpio_init(PCM5102_XSMT_PIN);
    gpio_set_dir(PCM5102_XSMT_PIN, GPIO_OUT);
    gpio_put(PCM5102_XSMT_PIN, 1);

    Serial.begin(115200);

    /* Init effect chain */
    EffectChain_Init(&g_fx_chain);

    /* Init PIO I2S audio (DMA + IRQ driven on Core 0) */
    I2SAudio_Init(audio_process_callback);

    /* Small delay for Core 1 to show splash */
    delay(100);

    /* Start audio */
    I2SAudio_Start();
}

void loop() {
    /* Audio runs entirely in DMA IRQ.
     * Core 0 main loop is free for future use:
     * - Preset save/load via flash
     * - USB MIDI parameter control
     * - Serial debug output
     */
    tight_loop_contents();
}

/* ===== Core 1: setup1() and loop1() ===== */
/* arduino-pico runs these on the second core automatically */

void setup1() {
    /* Wait for Core 0 to finish set_sys_clock_khz() before
       initializing Wire. I2C clock divider depends on system clock. */
    delay(300);

    /* Init OLED display */
    g_oled.init(OLED_SDA_PIN, OLED_SCL_PIN, OLED_I2C_ADDR);

    /* Init rotary encoder and buttons */
    g_encoder.init();

    /* Init UI system (shows splash screen on first update) */
    g_ui.init(&g_oled, &g_encoder, &g_fx_chain);
}

void loop1() {
    g_ui.update();

    /* ~60 Hz UI refresh */
    delay(16);
}
