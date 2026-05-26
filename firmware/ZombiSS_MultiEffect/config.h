#ifndef CONFIG_H
#define CONFIG_H

/*
 * ZOMBI SS Multi-Effect DSP Unit
 * Hardware Configuration for RP2350 (Arduino-Pico)
 * MUX Edition — buttons routed via CD74HC4067 16-ch multiplexer
 */

/* ===== Audio Config ===== */
#define AUDIO_SAMPLE_RATE     48000
#define AUDIO_BIT_DEPTH       24
#define AUDIO_BUFFER_FRAMES   64    /* Per half-buffer (~1.33ms) */
#define AUDIO_BUFFER_TOTAL    (AUDIO_BUFFER_FRAMES * 2) /* Stereo L+R */

/* ===== I2S Output — PCM5102 DAC ===== */
#define I2S_OUT_DIN_PIN       16
#define I2S_OUT_BCK_PIN       17    /* BCK=17, LRCK=18 (must be consecutive) */
#define I2S_OUT_LRCK_PIN      18

/* ===== I2S Input — PCM1808 ADC ===== */
/* DOUT, LRCK, BCK must be consecutive for PIO in-base mapping */
#define I2S_IN_DOUT_PIN       19
#define I2S_IN_LRCK_PIN       20
#define I2S_IN_BCK_PIN        21

/* ===== PCM1808 ADC Control ===== */
#define PCM1808_SCKI_PIN      22    /* PWM output: 12.5 MHz master clock */

/* ===== PCM5102A DAC Control ===== */
#define PCM5102_XSMT_PIN      0     /* GP0 → SD/XSMT: HIGH = unmuted */
/*
 * PCM5102A module pinout (VCC/GND/SD/MC/BCK/DIN/WS):
 *   SD  → GP0   (soft-mute: driven HIGH by firmware to unmute)
 *   MC  → leave NC  (module ties SCK to GND internally — no-SCK auto mode)
 *   BCK → GP17, DIN → GP16, WS → GP18
 *   FMT is tied GND inside the module → I2S Philips format
 *     (bit31=delay, bits30-7=B23..B0, bits6-0=padding)
 *
 * Bridge wires required — PCM1808 is I2S slave and needs clock from output PIO:
 *   GP17 → GP21  (BCK  to ADC BCK)
 *   GP18 → GP20  (LRCK to ADC LRCK)
 */

/* ===== I2C OLED Display (SSD1306 128×64) ===== */
#define OLED_SDA_PIN          4
#define OLED_SCL_PIN          5
#define OLED_I2C_ADDR         0x3C
#define OLED_I2C_FREQ         400000

/* ===== Rotary Encoder — EC11 (direct GPIO, needs fast quadrature read) ===== */
#define ENCODER_PIN_A         10
#define ENCODER_PIN_B         11
/* Encoder push-button is on MUX CH7 — see below */

/* ===== Multiplexer — CD74HC4067 (16-channel) ===== */
/*
 * Wiring:
 *   EN  → GND    (always enabled)
 *   S0  → GP2
 *   S1  → GP3
 *   S2  → GP6
 *   S3  → GP7
 *   SIG → GP8    (RP2350 internal pull-up enabled; button pressed = SIG pulled LOW)
 *
 * Each button: one leg to the MUX channel pin, other leg to GND.
 * Channel assignments (0–7 used; 8–15 spare):
 */
#define MUX_S0_PIN            2
#define MUX_S1_PIN            3
#define MUX_S2_PIN            6
#define MUX_S3_PIN            7
#define MUX_SIG_PIN           8

#define MUX_CH_FX1            0    /* TS Boost toggle   */
#define MUX_CH_FX2            1    /* Noise Gate toggle */
#define MUX_CH_FX3            2    /* Overdrive toggle  */
#define MUX_CH_FX4            3    /* EQ toggle         */
#define MUX_CH_FX5            4    /* Chorus toggle     */
#define MUX_CH_BACK           5    /* Nav: back         */
#define MUX_CH_CONFIRM        6    /* Nav: confirm      */
#define MUX_CH_ENC_SW         7    /* Encoder push      */

/* ===== Timing ===== */
#define DEBOUNCE_MS           15   /* Button debounce window (ms) */
#define LONG_PRESS_MS         500  /* Encoder long-press threshold (ms) */

/* ===== System Clock ===== */
#define SYS_CLOCK_KHZ         150000

#endif /* CONFIG_H */
