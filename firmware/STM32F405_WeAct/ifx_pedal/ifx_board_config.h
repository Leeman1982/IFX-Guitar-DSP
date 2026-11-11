#ifndef IFX_BOARD_CONFIG_H_FILE
#define IFX_BOARD_CONFIG_H_FILE
/*
 * ZombiSS effects on the WeAct STM32F405RGT6 core board — pin map.
 * Pulled in by ../../ZombiSS_MultiEffect/config.h when IFX_BOARD_CONFIG_H is
 * defined (see pedalflash.json).  Pin codes: board.h PB(n)/PC(n).
 *
 *  Audio (I2S2 full duplex, MCU is master — fixed pins):
 *    PB12 WS/LRCK ──► PCM1808 LRC  + PCM5102 LCK
 *    PB13 CK/BCK  ──► PCM1808 BCK  + PCM5102 BCK
 *    PB15 SD out  ──► PCM5102 DIN
 *    PB14 SD in   ◄── PCM1808 OUT
 *    PC6  MCK     ──► PCM1808 SCK (256·fs = 12.288 MHz)   PCM5102 SCK → GND
 *    PC7          ──► PCM5102 XSMT (high = unmuted)
 *  UI (all optional — without them the board boots into the default chain):
 *    PB6 SCL, PB7 SDA      SH1106/SSD1306 OLED (bit-banged I2C)
 *    PC0 A, PC1 B          EC11 encoder (encoder push = MUX CH7)
 *    PC8..PC11 S0..S3, PC12 SIG   CD74HC4067 button MUX (same channels as ZombiSS)
 *  On board:  PB2 LED (heartbeat; fast = DSP overload), PC13 KEY (preset cycle)
 */
#include "board.h"

#define AUDIO_SAMPLE_RATE     48000      /* actual: 47 991 Hz (PLLI2S) */
#define AUDIO_BIT_DEPTH       24
#define AUDIO_BUFFER_FRAMES   64
#define AUDIO_BUFFER_TOTAL    (AUDIO_BUFFER_FRAMES * 2)

#define PCM5102_XSMT_PIN      PC(7)

#define OLED_SDA_PIN          PB(7)
#define OLED_SCL_PIN          PB(6)
#define OLED_I2C_ADDR         0x3C
#define OLED_I2C_FREQ         400000

#define ENCODER_PIN_A         PC(0)
#define ENCODER_PIN_B         PC(1)

#define MUX_S0_PIN            PC(8)
#define MUX_S1_PIN            PC(9)
#define MUX_S2_PIN            PC(10)
#define MUX_S3_PIN            PC(11)
#define MUX_SIG_PIN           PC(12)

#define MUX_CH_FX1            0
#define MUX_CH_FX2            1
#define MUX_CH_FX3            2
#define MUX_CH_FX4            3
#define MUX_CH_FX5            4
#define MUX_CH_BACK           5
#define MUX_CH_CONFIRM        6
#define MUX_CH_ENC_SW         7
#define MUX_CH_HOLD           8
#define MUX_CH_FX6            9
#define MUX_CH_FX7            10
#define MUX_SCAN_CHANNELS     11

#define DEBOUNCE_MS           15
#define LONG_PRESS_MS         500

#define BF_FUZZ_OSFACTOR      2   /* the CPU fallback drops to 1× if needed */

#endif
