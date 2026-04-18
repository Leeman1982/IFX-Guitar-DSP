#pragma once

// =============================================================================
// RP2040 Zero – Guitar Multi-FX  |  Pin Definitions
// =============================================================================
//
// Hardware wiring summary
// -----------------------
//  PCM1808 ADC board  (ShengYang purple module)
//    BCK  → GP0          Bit clock  (RP2040 I2S master out)
//    LRC  → GP1          Word select / LRCK
//    OUT  → GP2          Serial data  (ADC → RP2040)
//    SCK  → GP4          System clock ~12.288 MHz  (PWM generated)
//    FMY  → GND          I2S format
//    MDI  → GND          Slave mode
//    MDO  → GND          Slave mode
//    3.3  → 3V3
//    GND  → GND
//
//  PCM5102A DAC board  (GY-PCM5102 / Tenstar module)
//    BCK  → GP0          Shared bit clock
//    LCK  → GP1          Shared word select
//    DIN  → GP3          Serial data  (RP2040 → DAC)
//    GND  → GND
//    VCC  → 3V3 or 5V (per board)
//    FLT  → GND          Normal latency filter
//    DEMP → GND          De-emphasis off
//    XSMT → 3V3          Unmuted
//
//  TL071 input buffer (guitar → PCM1808)
//    +9V rail, R1/R2 = 1MΩ bias divider, C1 = 100nF coupling
//    Output → LIN / RIN on PCM1808 board
//
//  CD74HC4067 16-ch multiplexer
//    S0 → GP8    S1 → GP9    S2 → GP10    S3 → GP11
//    SIG → GP12  (digital GPIO – pulled-up internally)
//    EN  → GP13  (active LOW – drive LOW to enable)
//
//  MUX channel map:
//    CH0 = Distortion footswitch (momentary, N/O, to GND)
//    CH1 = Chorus     footswitch
//    CH2 = EQ         footswitch
//    CH3 = Encoder CLK  (A)
//    CH4 = Encoder DT   (B)
//    CH5 = Encoder SW   (push, to GND)
//
//  16×2 LCD  (HD44780 + PCF8574 I2C backpack, addr 0x27)
//    SDA → GP6
//    SCL → GP7
//
//  Status LEDs  (series 220Ω to GND)
//    Distortion → GP14
//    Chorus     → GP15
//    EQ         → GP16

// ── I2S ─────────────────────────────────────────────────────────────────────
#define PIN_I2S_BCK         0   // shared BCK → PCM1808 BCK + PCM5102A BCK
#define PIN_I2S_WS          1   // shared WS  → PCM1808 LRC + PCM5102A LCK
#define PIN_ADC_DATA        2   // PCM1808 DATA out  (I2S RX)
#define PIN_DAC_DATA        3   // PCM5102A DIN      (I2S TX)
#define PIN_SCKI            4   // ~12.288 MHz PWM → PCM1808 SCK

// ── I2C ──────────────────────────────────────────────────────────────────────
#define PIN_I2C_SDA         6
#define PIN_I2C_SCL         7

// ── Multiplexer ──────────────────────────────────────────────────────────────
#define PIN_MUX_S0          8
#define PIN_MUX_S1          9
#define PIN_MUX_S2          10
#define PIN_MUX_S3          11
#define PIN_MUX_SIG         12
#define PIN_MUX_EN          13

// ── Status LEDs ──────────────────────────────────────────────────────────────
#define PIN_LED_DIST        14
#define PIN_LED_CHORUS      15
#define PIN_LED_EQ          16

// ── MUX channel assignments ───────────────────────────────────────────────────
#define MUX_CH_SW_DIST      0
#define MUX_CH_SW_CHORUS    1
#define MUX_CH_SW_EQ        2
#define MUX_CH_ENC_A        3
#define MUX_CH_ENC_B        4
#define MUX_CH_ENC_SW       5
