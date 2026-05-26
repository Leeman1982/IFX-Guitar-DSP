#ifndef CONFIG_H
#define CONFIG_H

/*
 * ZOMBI SS Multi-Effect DSP Unit
 * Hardware Configuration for RP2040 (Arduino-Pico)
 *
 * RP2040 constraints vs RP2350:
 *   - 264KB SRAM (vs 520KB) → reduced delay/chorus buffers
 *   - Cortex-M0+ no FPU (vs M33 FPU) → 32kHz sample rate
 *   - 133MHz max (vs 150MHz)
 */

/* ===== Audio Config ===== */
#define AUDIO_SAMPLE_RATE     32000   /* 32kHz (matches original STM32 version) */
#define AUDIO_BIT_DEPTH       24
#define AUDIO_BUFFER_FRAMES   128     /* Larger buffer for M0+ headroom (~4ms) */
#define AUDIO_BUFFER_TOTAL    (AUDIO_BUFFER_FRAMES * 2) /* Stereo L+R */

/* ===== I2S Output - PCM5102 DAC ===== */
#define I2S_OUT_DIN_PIN       16
#define I2S_OUT_BCK_PIN       17      /* BCK=17, LRCK=18 (consecutive) */
#define I2S_OUT_LRCK_PIN      18

/* ===== I2S Input - PCM1808 ADC ===== */
#define I2S_IN_DOUT_PIN       19
#define I2S_IN_LRCK_PIN       20
#define I2S_IN_BCK_PIN        21

/* ===== I2C OLED Display (SH1106 128x64) ===== */
#define OLED_SDA_PIN          4
#define OLED_SCL_PIN          5
#define OLED_I2C_ADDR         0x3C
#define OLED_I2C_FREQ         400000

/* ===== Rotary Encoder (EC11 on Estardyn module) ===== */
#define ENCODER_PIN_A         10
#define ENCODER_PIN_B         11
#define ENCODER_PIN_SW        12

/* ===== Module Buttons ===== */
#define SWITCH_BACK_PIN       14
#define SWITCH_CONFIRM_PIN    15

/* ===== Effect Bypass Momentary Switches ===== */
#define SWITCH_FX1_PIN        6     /* Noise Gate */
#define SWITCH_FX2_PIN        7     /* Overdrive */
#define SWITCH_FX3_PIN        8     /* EQ */
#define SWITCH_FX4_PIN        9     /* Chorus */
#define SWITCH_FX5_PIN        13    /* Delay */

/* ===== Debounce ===== */
#define DEBOUNCE_MS           5
#define LONG_PRESS_MS         500

/* ===== System Clock ===== */
#define SYS_CLOCK_KHZ         133000  /* RP2040 max stable */

#endif
