/*
 * ZOMBI SS Multi-Effect DSP Unit
 * Target: RP2350 (Raspberry Pi Pico 2)
 *
 * Architecture:
 *   Core 0: Real-time audio DSP processing (I2S DMA callback)
 *   Core 1: UI rendering (SH1106 OLED + EC11 rotary encoder + buttons)
 *
 * Audio I/O:
 *   Input:  PCM1808 ADC via I2S (PIO1)
 *   Output: PCM5102 DAC via I2S (PIO0)
 *
 * Effects chain:
 *   1. Noise Gate
 *   2. Overdrive (asymmetric soft clipping)
 *   3. Peaking EQ
 *   4. Chorus (dual LFO)
 *   5. Delay (with feedback)
 *
 * Each effect toggled on/off via dedicated momentary switch.
 * All parameters adjustable via rotary encoder + OLED UI.
 *
 * Original DSP algorithms by Philip Salmony @ phils-lab.net
 * Ported to RP2350 with PIO I2S for Zombi SS
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"

#include "effect_chain.h"
#include "i2s_audio.h"
#include "sh1106_oled.h"
#include "rotary_encoder.h"
#include "ui_system.h"

/* ===== Global state ===== */

/* Effect chain - shared between cores (Core 0 reads params, Core 1 writes params) */
static EffectChain g_fx_chain;

/* UI components (Core 1 only) */
static SH1106 g_oled;
static RotaryEncoder g_encoder;
static UISystem g_ui;

/* ===== Core 0: Audio DSP Processing ===== */

/*
 * Audio callback - called from DMA IRQ on Core 0.
 * Processes AUDIO_BUFFER_FRAMES stereo frames.
 * Input: 32-bit I2S samples (24-bit left-justified)
 * Output: 32-bit I2S samples (24-bit left-justified)
 *
 * Latency: AUDIO_BUFFER_FRAMES / AUDIO_SAMPLE_RATE
 *        = 64 / 48000 = ~1.33ms per buffer
 *        = ~2.67ms total (double-buffered)
 */
static void audio_process_callback(const int32_t *input, int32_t *output, uint32_t frame_count) {
    for (uint32_t i = 0; i < frame_count; i++) {
        /* Read stereo input (left channel only for mono guitar processing) */
        int32_t raw_left = input[i * 2];

        /* Convert 24-bit I2S to float [-1.0, +1.0] */
        /* PCM1808 outputs 24-bit left-justified in 32-bit word */
        float inp = (float)(raw_left >> 8) / 8388608.0f;  /* 2^23 */

        /* Process through effect chain */
        float out = EffectChain_Process(&g_fx_chain, inp);

        /* Convert float back to 24-bit I2S */
        int32_t out_i32 = (int32_t)(out * 8388607.0f);
        if (out_i32 > 8388607) out_i32 = 8388607;
        if (out_i32 < -8388608) out_i32 = -8388608;
        out_i32 <<= 8;  /* Left-justify for I2S */

        /* Output same signal to both L and R */
        output[i * 2]     = out_i32;
        output[i * 2 + 1] = out_i32;
    }
}

/* ===== Core 1: UI Task ===== */

static void core1_ui_task(void) {
    /* Init OLED display on I2C0 */
    /* SDA=4, SCL=5 (standard Pico I2C0 pins) */
    SH1106_Init(&g_oled, i2c0, 4, 5);

    /* Init rotary encoder and buttons */
    RotaryEncoder_Init(&g_encoder);

    /* Init UI system */
    UISystem_Init(&g_ui, &g_oled, &g_encoder, &g_fx_chain);

    /* UI main loop - runs continuously on Core 1 */
    while (true) {
        UISystem_Update(&g_ui);

        /* ~60 Hz UI refresh rate - don't hog the bus */
        sleep_ms(16);
    }
}

/* ===== Main (Core 0) ===== */

int main(void) {
    /* Overclock to 150MHz for maximum DSP headroom */
    set_sys_clock_khz(150000, true);

    stdio_init_all();

    /* Initialize effect chain with default parameters */
    EffectChain_Init(&g_fx_chain);

    /* Initialize I2S audio with PIO (runs on Core 0 via DMA IRQ) */
    I2SAudio_Init(audio_process_callback);

    /* Launch Core 1 for UI */
    multicore_launch_core1(core1_ui_task);

    /* Small delay to let Core 1 show splash screen */
    sleep_ms(100);

    /* Start audio processing */
    I2SAudio_Start();

    /* Core 0 main loop - audio runs via DMA interrupts,
       so main loop can handle housekeeping */
    while (true) {
        /* Core 0 is mostly idle here - audio runs in DMA IRQ.
           Could add additional processing like:
           - Parameter smoothing
           - Preset save/load
           - USB MIDI input
           For now, just yield to save power. */
        tight_loop_contents();
    }

    return 0;
}
