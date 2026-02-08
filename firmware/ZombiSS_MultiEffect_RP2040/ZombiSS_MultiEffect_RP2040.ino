/*
 * ZOMBI SS Multi-Effect DSP Unit - RP2040 Version
 * =================================================
 * Target: RP2040 (Raspberry Pi Pico) via Arduino-Pico core
 *
 * REQUIRED LIBRARY: Install "U8g2" via Arduino Library Manager
 *   Sketch > Include Library > Manage Libraries > search "U8g2" > Install
 *
 * Architecture:
 *   Core 0: Real-time audio DSP (PIO I2S + DMA interrupt)
 *   Core 1: UI (SH1106 OLED via U8g2 + EC11 rotary encoder + buttons)
 *
 * RP2040 vs RP2350 differences:
 *   - 264KB SRAM → reduced delay buffer (375ms max vs 660ms)
 *   - Cortex-M0+ (no FPU) → 32kHz sample rate (vs 48kHz)
 *   - 133MHz clock (vs 150MHz)
 *   - Larger audio buffer (128 frames vs 64) for M0+ headroom
 *
 * Audio I/O:
 *   Input:  PCM1808 ADC via PIO1 I2S
 *   Output: PCM5102 DAC via PIO0 I2S
 *
 * Original DSP algorithms by Philip Salmony @ phils-lab.net
 */

#include <Arduino.h>
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
static EffectChain g_fx_chain;
static SH1106 g_oled;
static RotaryEncoder g_encoder;
static UISystem g_ui;

/* ===== Core 0: Audio DSP Callback ===== */

static void audio_process_callback(const int32_t *input, int32_t *output, uint32_t frame_count) {
    for (uint32_t i = 0; i < frame_count; i++) {
        int32_t raw_left = input[i * 2];

        /* 24-bit I2S → float */
        float inp = (float)(raw_left >> 8) / 8388608.0f;

        /* Process */
        float out = EffectChain_Process(&g_fx_chain, inp);

        /* Float → 24-bit I2S */
        int32_t out_i32 = (int32_t)(out * 8388607.0f);
        if (out_i32 >  8388607) out_i32 =  8388607;
        if (out_i32 < -8388608) out_i32 = -8388608;
        out_i32 <<= 8;

        output[i * 2]     = out_i32;
        output[i * 2 + 1] = out_i32;
    }
}

/* ===== Core 0 ===== */

void setup() {
    /* RP2040: 133 MHz */
    set_sys_clock_khz(SYS_CLOCK_KHZ, true);

    Serial.begin(115200);

    EffectChain_Init(&g_fx_chain);
    I2SAudio_Init(audio_process_callback);

    delay(100);
    I2SAudio_Start();
}

void loop() {
    tight_loop_contents();
}

/* ===== Core 1: UI ===== */

void setup1() {
    /* Short delay for Core 0 to finish clock setup */
    delay(200);

    /* OLED uses U8g2 software I2C (bit-bang) - no Wire dependency */
    g_oled.init(OLED_SDA_PIN, OLED_SCL_PIN, OLED_I2C_ADDR);
    g_encoder.init();
    g_ui.init(&g_oled, &g_encoder, &g_fx_chain);
}

void loop1() {
    g_ui.update();
    delay(16);  /* ~60 Hz UI */
}
