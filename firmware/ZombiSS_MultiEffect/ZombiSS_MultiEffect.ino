/*
 * ZOMBI SS Multi-Effect DSP Unit — MUX Edition
 * =============================================
 * Target:   RP2350 (Raspberry Pi Pico 2) via Arduino-Pico core
 *
 * Architecture:
 *   Core 0: Real-time audio DSP (PIO I2S + DMA interrupt)
 *   Core 1: UI (SSD1306 OLED + EC11 rotary encoder + CD74HC4067 MUX buttons)
 *
 * Audio I/O:
 *   Input:  PCM1808 ADC via PIO1 I2S (SCKI on GP22 via PWM)
 *   Output: PCM5102 DAC via PIO0 I2S (XSMT on GP0, FMT tied HIGH = LJ mode)
 *
 * Effects Chain:
 *   1. TS Boost   2. Noise Gate   3. Overdrive
 *   4. 10-Band EQ 5. Chorus       6. Delay
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

static EffectChain   g_fx_chain;
static SH1106        g_oled;
static RotaryEncoder g_encoder;
static UISystem      g_ui;

/* ===== Core 0: Audio DSP Callback ===== */

/*
 * Called from DMA IRQ, Core 0.  Latency: 64 frames / 48 kHz ≈ 1.33 ms.
 *
 * I2S input word layout (PCM1808, I2S Philips, 24-bit):
 *   Bit 31:    delay bit (0 — PCM1808 I2S 1-clock delay)
 *   Bits 30–7: B23..B0 (24-bit signed audio, MSB first)
 *   Bits 6–0:  zero padding
 * Conversion: (raw << 1) >> 8  →  sign-extended 24-bit int / 2^23  →  float.
 *
 * I2S output word layout (PCM5102, Left-Justified, FMT=HIGH):
 *   Bits 31–8: B23..B0 (left-justified 24-bit signed audio)
 *   Bits 7–0:  zero padding
 * Conversion: float * 2^23, clamp, << 8  →  left-justified 32-bit word.
 */
static void audio_process_callback(const int32_t *input, int32_t *output,
                                    uint32_t frame_count) {
    /* Master-volume smoother: ramps at 0.002/sample (~500 samples = ~10 ms full-scale).
     * Prevents audible clicks when the user adjusts master volume mid-signal.
     * Lives here (Core 0 only) so it needs no cross-core synchronisation. */
    static float smooth_vol = 0.8f;

    for (uint32_t i = 0; i < frame_count; i++) {
        /* Snap smooth_vol toward the current target.
         * masterVolume is a 4-byte aligned float; single-word reads are atomic
         * on ARM Cortex-M33, so no lock is needed for this scalar read. */
        float target = g_fx_chain.masterVolume;
        if      (smooth_vol < target - 0.002f) smooth_vol += 0.002f;
        else if (smooth_vol > target + 0.002f) smooth_vol -= 0.002f;
        else                                    smooth_vol  = target;

        /* PCM1808 I2S word: [delay_bit | B23..B0 | 7 zeros]
         *   bit31 = 0  (PCM1808 Philips 1-clock delay)
         *   bits30..7  = audio bits B23..B0
         *   bits6..0   = zero padding
         *
         * (uint32_t) cast makes the left-shift defined; the subsequent cast
         * to int32_t and arithmetic >>8 are implementation-defined under C99
         * but are always arithmetic-right-shift on every ARM/GCC/Clang target.
         * This is the standard idiom in embedded audio DSP. */
        int32_t raw  = input[i * 2];
        float   inp  = (float)((int32_t)((uint32_t)raw << 1) >> 8) / 8388608.0f;

        /* Run effect chain — DSP only, master volume NOT applied inside.
         * Individual aligned float reads from g_fx_chain are atomic on Cortex-M33;
         * the DMB barrier in SetParam ensures writes from Core 1 are visible here. */
        float out = EffectChain_Process(&g_fx_chain, inp) * smooth_vol;
        if (out >  1.0f) out =  1.0f;
        if (out < -1.0f) out = -1.0f;

        int32_t out_i32 = (int32_t)(out * 8388607.0f);
        out_i32 <<= 8;

        output[i * 2]     = out_i32;
        output[i * 2 + 1] = out_i32;
    }
}

/* ===== Core 0: setup() / loop() ===== */

void setup() {
    set_sys_clock_khz(SYS_CLOCK_KHZ, true);

    /*
     * PCM1808 SCKI on GP22 via PWM.
     * ADC requires SCKI even in slave mode (internal sigma-delta clock).
     * Target: 256 × Fs = 12.288 MHz.
     * PWM:  150 MHz / 12 = 12.5 MHz (1.7 % off; PCM1808 tolerance ±6 %).
     */
    gpio_set_function(PCM1808_SCKI_PIN, GPIO_FUNC_PWM);
    {
        uint slice = pwm_gpio_to_slice_num(PCM1808_SCKI_PIN);
        uint chan  = pwm_gpio_to_channel(PCM1808_SCKI_PIN);
        pwm_set_clkdiv(slice, 1.0f);
        pwm_set_wrap(slice, 11);
        pwm_set_chan_level(slice, chan, 6);
        pwm_set_enabled(slice, true);
    }

    /*
     * PCM5102 XSMT (soft-mute) on GP0.
     * Must be HIGH before audio starts; floating or LOW = muted.
     */
    pinMode(PCM5102_XSMT_PIN, OUTPUT);
    digitalWrite(PCM5102_XSMT_PIN, HIGH);

    Serial.begin(115200);

    EffectChain_Init(&g_fx_chain);
    I2SAudio_Init(audio_process_callback);

    delay(100);
    I2SAudio_Start();
}

void loop() {
    tight_loop_contents();
}

/* ===== Core 1: setup1() / loop1() ===== */

void setup1() {
    delay(300);   /* wait for Core 0 sys-clock change before I2C init */

    g_oled.init(OLED_SDA_PIN, OLED_SCL_PIN, OLED_I2C_ADDR);
    g_encoder.init();
    g_ui.init(&g_oled, &g_encoder, &g_fx_chain);
}

void loop1() {
    g_ui.update();
    delay(16);   /* ≈ 60 Hz UI refresh */
}
