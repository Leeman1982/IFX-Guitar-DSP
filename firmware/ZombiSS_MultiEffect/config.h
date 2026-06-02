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
#define PCM5102_XSMT_PIN      0     /* GP0 -> XSMT/SD: drive HIGH to UNMUTE */
/*
 * PCM5102 DAC wiring (the RP2350 is I2S master for output):
 *   VIN  -> 3V3            (board has its own regulator; 3.3 V logic OK)
 *   GND  -> GND
 *   DIN  -> GP16
 *   BCK  -> GP17
 *   LCK  -> GP18           (a.k.a. WS / LRCK)
 *   SCK  -> GND            (no system clock: enables internal PLL)
 *   XSMT -> GP0            (HIGH = unmuted; LOW/floating = MUTED -> silence)
 *   FMT  -> GND            (I2S Philips format; matches firmware word layout)
 * If you prefer, XSMT may instead be hard-wired to 3V3; the firmware still
 * drives GP0 HIGH so either is safe.
 */

/* ===== I2S Input - PCM1808 ADC ===== */
/* DOUT, LRCK, BCK must be consecutive for PIO in-base mapping */
#define I2S_IN_DOUT_PIN       19
#define I2S_IN_LRCK_PIN       20
#define I2S_IN_BCK_PIN        21
#define PCM1808_SCKI_PIN      22    /* GP22 -> SCKI: 12.5 MHz master clock (PWM) */
/*
 * PCM1808 ADC wiring (SLAVE mode; the RP2350 supplies ALL its clocks):
 *
 *   ###########################################################
 *   #  POWER: the PCM1808 module needs its analog supply too!  #
 *   #  Connect the board's 5V (VCC) pin AND GND. With only the #
 *   #  digital/3V3 rail powered the modulator does NOT run and #
 *   #  you get total silence even though the I2S clocks look   #
 *   #  fine. (This was the real "no signal" cause.)            #
 *   ###########################################################
 *
 *   5V/VCC -> 5V            (board regulates to 3.3 V analog/digital)
 *   GND    -> GND
 *   OUT    -> GP19          (DOUT, serial data to RP2350)
 *   SCKI   -> GP22          (12.5 MHz system clock from PWM; REQUIRED)
 *   BCK    -> GP17          (shared with DAC BCK; also bridge GP17->GP21)
 *   LRC    -> GP18          (shared with DAC LRCK; also bridge GP18->GP20)
 *   FMT    -> GND           (I2S Philips, 24-bit)
 *   MD0    -> GND           } both LOW = SLAVE mode (RP2350 drives BCK/LRC)
 *   MD1    -> GND           }
 *
 * BRIDGE WIRES (required): the input PIO reads BCK/LRCK on GP21/GP20, so
 *   GP17 -> GP21  (BCK)
 *   GP18 -> GP20  (LRCK)
 * Without these the ADC capture has no clock reference -> no input.
 */

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
#define SYS_CLOCK_KHZ         150000

#endif /* CONFIG_H */
