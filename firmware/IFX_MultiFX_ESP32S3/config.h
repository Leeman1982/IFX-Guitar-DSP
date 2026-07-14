/*
*
*   InfiniFX Multi-FX - ESP32-S3 port
*   Hardware configuration / pin map
*
*   Target: ESP32-S3-WROOM-1 (Arduino IDE, "ESP32S3 Dev Module")
*   Audio:  PCM1808 (ADC, I2S slave, 256fs MCLK) + PCM5102 (DAC)
*   UI:     1.3" SH1106 OLED (I2C), 5 momentary footswitches, 4 pots
*
*/

#ifndef CONFIG_H
#define CONFIG_H

/* ------------------------------------------------------------------ */
/* Audio engine                                                       */
/* ------------------------------------------------------------------ */
#define SAMPLE_RATE_HZ      44100
#define BLOCK_FRAMES        64      /* frames per processing block    */

/* ------------------------------------------------------------------ */
/* I2S (one full-duplex port, PCM1808 and PCM5102 share BCK/LRCK)     */
/*                                                                    */
/*   MCLK  -> PCM1808 SCKI          (PCM5102 SCK pin tied to GND,     */
/*   BCK   -> PCM1808 BCK  + PCM5102 BCK       it uses its internal   */
/*   WS    -> PCM1808 LRC  + PCM5102 LCK       PLL)                   */
/*   DOUT  -> PCM5102 DIN                                             */
/*   DIN   <- PCM1808 DOUT                                            */
/* ------------------------------------------------------------------ */
#define PIN_I2S_MCLK        14
#define PIN_I2S_BCLK        15
#define PIN_I2S_WS          16
#define PIN_I2S_DOUT        17
#define PIN_I2S_DIN         18

/* ------------------------------------------------------------------ */
/* OLED SH1106 128x64, I2C                                            */
/* ------------------------------------------------------------------ */
#define PIN_OLED_SDA        8
#define PIN_OLED_SCL        9

/* ------------------------------------------------------------------ */
/* Potentiometers (ADC1 channels). Wire: 3V3 - wiper to pin - GND     */
/* ------------------------------------------------------------------ */
#define PIN_POT_1           4
#define PIN_POT_2           5
#define PIN_POT_3           6
#define PIN_POT_4           7

/* ------------------------------------------------------------------ */
/* Momentary footswitches, one per effect, switch to GND (pull-ups    */
/* enabled internally).                                               */
/*   Short press : toggle effect on/off                               */
/*   Long press  : select effect for editing (pots -> its params)     */
/* ------------------------------------------------------------------ */
#define PIN_FS_GATE         10
#define PIN_FS_BOOST        11
#define PIN_FS_OVERDRIVE    12
#define PIN_FS_CHORUS       13
#define PIN_FS_DELAY        21

/* ------------------------------------------------------------------ */
/* UI behaviour                                                       */
/* ------------------------------------------------------------------ */
#define BUTTON_DEBOUNCE_MS      30
#define BUTTON_LONGPRESS_MS     500

/* Pot must move by this much (0..1) after a page change before it
 * takes over the parameter (prevents value jumps).                   */
#define POT_PICKUP_THRESHOLD    0.03f

/* 1st order low-pass filter constant for pot readings.
 * 0 = no filtering, closer to 1 = heavier filtering.                 */
#define POT_FILTER_ALPHA        0.85f

#endif
