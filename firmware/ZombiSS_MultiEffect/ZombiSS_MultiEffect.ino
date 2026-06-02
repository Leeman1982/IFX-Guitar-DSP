/*
 * ZOMBI SS Multi-Effect DSP Unit
 * ================================
 * Target: RP2350 (Raspberry Pi Pico 2) via Arduino-Pico core
 *
 * Architecture:
 *   Core 0: Real-time audio DSP (PIO I2S + DMA interrupt)
 *   Core 1: UI (SH1106 OLED + EC11 rotary encoder + buttons)
 *
 * Audio I/O (both chips in I2S Philips format, FMT=GND):
 *   Input:  PCM1808 ADC via PIO1 I2S (slave; SCKI on GP22, BCK/LRCK bridged)
 *   Output: PCM5102 DAC via PIO0 I2S (master; XSMT on GP0 held HIGH)
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

/* ===== Core 0: Audio DSP Callback ===== */

/*
 * Called from DMA IRQ on Core 0.
 * Latency: 64 frames / 48kHz = ~1.33ms per buffer = ~2.67ms round-trip.
 */
static void audio_process_callback(const int32_t *input, int32_t *output, uint32_t frame_count) {
    for (uint32_t i = 0; i < frame_count; i++) {
        /* Read left channel (mono guitar input).
         *
         * I2S Philips word captured by the PIO (PCM1808, FMT=GND):
         *   bit 31   = delay bit (1-clock I2S delay; junk, ignore)
         *   bits 30-7= B23..B0  (24-bit signed audio, MSB first)
         *   bits 6-0 = trailing zeros
         * Extract: ((uint32_t)raw << 1) drops the delay bit into bit31, then
         * an arithmetic >> 8 sign-extends the 24-bit value. The uint32_t cast
         * keeps the left shift well-defined (no signed-overflow UB). */
        int32_t raw_left = input[i * 2];
        float inp = (float)((int32_t)((uint32_t)raw_left << 1) >> 8) / 8388608.0f;

        /* Process effect chain */
        float out = EffectChain_Process(&g_fx_chain, inp);

        /* Float → 24-bit I2S Philips word (PCM5102, FMT=GND):
         *   bit 31   = 0        (delay slot; DAC ignores it)
         *   bits 30-7= B23..B0  (24-bit signed audio)
         *   bits 6-0 = 0
         * Mask to 24 bits then shift left 7 to land audio in bits 30..7. */
        int32_t audio24 = (int32_t)(out * 8388607.0f);
        if (audio24 >  8388607) audio24 =  8388607;
        if (audio24 < -8388608) audio24 = -8388608;
        int32_t out_i32 = (int32_t)(((uint32_t)audio24 & 0x00FFFFFFu) << 7);

        /* Stereo output (same signal both channels) */
        output[i * 2]     = out_i32;
        output[i * 2 + 1] = out_i32;
    }
}

/* ===== Core 0: setup() and loop() ===== */

void setup() {
    /* Overclock to 150 MHz for DSP headroom */
    set_sys_clock_khz(SYS_CLOCK_KHZ, true);

    /*
     * PCM1808 SCKI (system master clock) on GP22 via PWM.
     * The ADC's sigma-delta modulator REQUIRES SCKI even in slave mode;
     * without it the ADC produces nothing. Target 256*Fs = 12.288 MHz.
     * PWM: 150 MHz / 12 = 12.5 MHz (+1.7 %, well within PCM1808 tolerance).
     */
    gpio_set_function(PCM1808_SCKI_PIN, GPIO_FUNC_PWM);
    {
        uint slice = pwm_gpio_to_slice_num(PCM1808_SCKI_PIN);
        uint chan  = pwm_gpio_to_channel(PCM1808_SCKI_PIN);
        pwm_set_clkdiv(slice, 1.0f);
        pwm_set_wrap(slice, 11);             /* 0..11 = 12 counts -> /12 */
        pwm_set_chan_level(slice, chan, 6);  /* 50 % duty */
        pwm_set_enabled(slice, true);
    }

    /*
     * PCM5102 XSMT (soft-mute) on GP0 — must be HIGH before/while audio runs.
     * Floating or LOW keeps the DAC muted (silence).
     */
    pinMode(PCM5102_XSMT_PIN, OUTPUT);
    digitalWrite(PCM5102_XSMT_PIN, HIGH);

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
