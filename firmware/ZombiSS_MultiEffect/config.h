#ifndef CONFIG_H
#define CONFIG_H

/*
 * ZOMBI SS Multi-Effect DSP Unit
 * Hardware Configuration for RP2350 (Arduino-Pico)
 */

/* ===== Audio Config ===== */
#define AUDIO_SAMPLE_RATE     48000
#define AUDIO_BIT_DEPTH       24
#define AUDIO_BUFFER_FRAMES   64    /* Per half-buffer (~1.33ms) */
#define AUDIO_BUFFER_TOTAL    (AUDIO_BUFFER_FRAMES * 2) /* Stereo L+R */

/* ===== I2S Output - PCM5102 DAC ===== */
#define I2S_OUT_DIN_PIN       16
#define I2S_OUT_BCK_PIN       17    /* BCK=17, LRCK=18 (consecutive) */
#define I2S_OUT_LRCK_PIN      18

/* ===== I2S Input - PCM1808 ADC ===== */
/* DOUT, LRCK, BCK must be consecutive for PIO in-base mapping */
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
#define SWITCH_FX1_PIN        6     /* TS Boost   (FX_TSBOOST)   */
#define SWITCH_FX2_PIN        7     /* Noise Gate (FX_NOISEGATE) */
#define SWITCH_FX3_PIN        8     /* Overdrive  (FX_OVERDRIVE) */
#define SWITCH_FX4_PIN        9     /* 10-Band EQ (FX_EQ)        */
#define SWITCH_FX5_PIN        13    /* Chorus     (FX_CHORUS)    */
/* FX_DELAY has no dedicated footswitch — toggle via OLED menu   */

/* ===== PCM5102 DAC Control ===== */
#define PCM5102_XSMT_PIN      0     /* GP0 → XSMT: drive HIGH to unmute analog output */
/* Note: FMT pin tie to GND (I2S standard), SCK tie to GND (no SCK mode) on your module */

/* ===== Debounce ===== */
#define DEBOUNCE_MS           5
#define LONG_PRESS_MS         500

/* ===== System Clock ===== */
#define SYS_CLOCK_KHZ         150000

#endif /* CONFIG_H */
