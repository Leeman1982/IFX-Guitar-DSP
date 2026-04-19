// =============================================================================
// RP2040 Zero – Guitar Multi-FX Pedal
// =============================================================================
//
// Hardware
// --------
//   PCM1808  – 24-bit stereo ADC   (I2S slave, guitar input via TL071 buffer)
//   PCM5102A – 32-bit stereo DAC   (I2S slave, audio output)
//   CD74HC4067 – 16-ch MUX         (footswitches + rotary encoder)
//   HD44780 16×2 LCD (PCF8574 I2C) – menu UI
//
// Core allocation
// ---------------
//   Core 0  →  setup() / loop()   : UI, LCD, MUX scanning, parameter updates
//   Core 1  →  setup1() / loop1() : I2S audio DMA, DSP processing
//
// Signal chain (Core 1, per sample)
//   Guitar → PCM1808 → [EQ] → [Distortion] → [Chorus] → PCM5102A
//
// Arduino libraries required (install via Library Manager)
// ---------------------------------------------------------
//   • arduino-audio-tools  by Phil Schatzmann
//   • LiquidCrystal_I2C    by Frank de Brabander
//
// Board package
//   • Raspberry Pi Pico / RP2040  (Earle Philhower port)
//   Board: "Raspberry Pi Pico Zero"  or  "Raspberry Pi Pico"
//   CPU speed: 250 MHz  (overclocked – set_sys_clock_khz, see config.h)
//
// Flashing
//   Hold BOOTSEL, plug USB, drag-and-drop UF2  OR  use Arduino IDE → Upload.
// =============================================================================

#include <Wire.h>
#include <hardware/pwm.h>
#include <hardware/clocks.h>

// Audio tools (I2S)
#include "AudioTools.h"

// Project headers
#include "config.h"
#include "pins.h"
#include "effect_params.h"
#include "IFX_Distortion.h"
#include "IFX_Chorus.h"
#include "IFX_EQ.h"
#include "MuxScanner.h"
#include "LCDMenu.h"

// =============================================================================
// Global parameter instances  (defined here, declared extern in effect_params.h)
// =============================================================================
DistortionParams g_dist;
ChorusParams     g_chorus;
EQParams         g_eq;

// =============================================================================
// I2S streams
// =============================================================================
I2SStream i2s_rx;   // PCM1808 → RP2040  (ADC input)
I2SStream i2s_tx;   // RP2040 → PCM5102A (DAC output)

// =============================================================================
// DSP effect instances  (Core 1 only – no sharing needed)
// =============================================================================
static IFX_EQ          dsp_eq;
static IFX_Distortion  dsp_dist;
static IFX_Chorus      dsp_chorus;

// Audio sample buffer (interleaved stereo int32)
static int32_t rx_buf[AUDIO_BUFFER_FRAMES * AUDIO_CHANNELS];
static int32_t tx_buf[AUDIO_BUFFER_FRAMES * AUDIO_CHANNELS];

// =============================================================================
// UI instances  (Core 0 only)
// =============================================================================
static MuxScanner mux;
static LCDMenu    menu(LCD_I2C_ADDR, 16, 2);

// =============================================================================
// SCKI generation – ~12.5 MHz via RP2040 PWM  (PCM1808 system clock)
// At 250 MHz sys clock:  wrap = 19  →  period = 20 clocks
// Frequency = 250 000 000 / 20 = 12 500 000 Hz  (ideal 12 288 000, Δ = +1.7 %)
// PCM1808 slave mode tolerates ±5 % on SCKI – well within spec.
// 50 % duty: level = (wrap + 1) / 2 = 10
// =============================================================================
static void scki_pwm_init()
{
    gpio_set_function(PIN_SCKI, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(PIN_SCKI);
    uint chan  = pwm_gpio_to_channel(PIN_SCKI);
    pwm_set_clkdiv_int_frac(slice, 1, 0);
    pwm_set_wrap(slice, 19);               // period = 20 clocks
    pwm_set_chan_level(slice, chan, (19 + 1) / 2);  // 50 % duty
    pwm_set_enabled(slice, true);
}

// =============================================================================
// Parameter initialisation (called before Core 1 starts)
// =============================================================================
static void params_init()
{
    g_dist.enabled      = true;
    g_dist.gain         = DIST_GAIN_DEFAULT;
    g_dist.hpf_freq     = DIST_HPF_DEFAULT;
    g_dist.tone_freq    = DIST_TONE_DEFAULT;
    g_dist.tone_damp    = DIST_TONE_DAMP_DEFAULT;
    g_dist.level        = DIST_LEVEL_DEFAULT;
    g_dist.needs_update = false;

    g_chorus.enabled      = true;
    g_chorus.rate         = CHO_RATE_A_DEFAULT;
    g_chorus.depth        = CHO_DEPTH_A_DEFAULT;
    g_chorus.mix          = CHO_MIX_DEFAULT;
    g_chorus.delay_ms     = CHO_DELAY_A_MS_DEFAULT;
    g_chorus.needs_update = false;

    g_eq.enabled      = true;
    g_eq.bass_db      = EQ_BASS_DB_DEFAULT;
    g_eq.mid_db       = EQ_MID_DB_DEFAULT;
    g_eq.treb_db      = EQ_TREB_DB_DEFAULT;
    g_eq.mid_freq     = EQ_MID_FREQ_DEFAULT;
    g_eq.mid_bw       = EQ_MID_BW_DEFAULT;
    g_eq.needs_update = false;
}

// =============================================================================
// CORE 0 – setup & loop  (UI / control)
// =============================================================================
void setup()
{
    // Overclock to 250 MHz – ~2.5× DSP headroom; SCKI PWM and I2S configured accordingly
    set_sys_clock_khz(SYS_CLK_KHZ, true);

    // SCKI for PCM1808 (must start before I2S)
    scki_pwm_init();

    // Initialise default parameters
    params_init();

    // Status LEDs
    pinMode(PIN_LED_DIST,   OUTPUT);
    pinMode(PIN_LED_CHORUS, OUTPUT);
    pinMode(PIN_LED_EQ,     OUTPUT);
    digitalWrite(PIN_LED_DIST,   g_dist.enabled   ? HIGH : LOW);
    digitalWrite(PIN_LED_CHORUS, g_chorus.enabled ? HIGH : LOW);
    digitalWrite(PIN_LED_EQ,     g_eq.enabled     ? HIGH : LOW);

    // I2C for LCD  (Wire uses GP6/GP7 per pins.h)
    Wire.setSDA(PIN_I2C_SDA);
    Wire.setSCL(PIN_I2C_SCL);
    Wire.begin();

    // Multiplexer
    mux.begin();

    // LCD menu
    menu.begin();
    menu.update(0, false, true);  // draw initial screen

    // Core 1 starts automatically after setup() returns
}

void loop()
{
    static uint32_t last_lcd = 0;

    // ── Scan MUX (footswitches + encoder) ───────────────────────────────
    mux.scan();

    // ── Footswitches: toggle bypass ──────────────────────────────────────
    if (mux.distSwitchPressed()) {
        g_dist.enabled = !g_dist.enabled;
        digitalWrite(PIN_LED_DIST, g_dist.enabled ? HIGH : LOW);
        menu.notifyBypassChange();
    }
    if (mux.chorusSwitchPressed()) {
        g_chorus.enabled = !g_chorus.enabled;
        digitalWrite(PIN_LED_CHORUS, g_chorus.enabled ? HIGH : LOW);
        menu.notifyBypassChange();
    }
    if (mux.eqSwitchPressed()) {
        g_eq.enabled = !g_eq.enabled;
        digitalWrite(PIN_LED_EQ, g_eq.enabled ? HIGH : LOW);
        menu.notifyBypassChange();
    }

    // ── LCD menu update at ~LCD_UPDATE_MS rate ────────────────────────────
    uint32_t now = millis();
    if ((now - last_lcd) >= LCD_UPDATE_MS) {
        last_lcd = now;
        int8_t delta    = mux.encoderDelta();
        bool   pressed  = mux.encoderPressed();
        menu.update(delta, pressed);
    }
}

// =============================================================================
// CORE 1 – setup1 & loop1  (Audio DSP)
// =============================================================================
void setup1()
{
    // Small delay to let Core 0 finish sys_clock change and SCKI init
    delay(50);

    // ── Initialise DSP effect instances ──────────────────────────────────
    IFX_EQ_Init(&dsp_eq, SAMPLE_RATE,
                 EQ_BASS_FREQ_HZ, EQ_BASS_BW_HZ,
                 EQ_MID_FREQ_HZ,  EQ_MID_BW_HZ,
                 EQ_TREB_FREQ_HZ, EQ_TREB_BW_HZ);

    IFX_Distortion_Init(&dsp_dist, SAMPLE_RATE,
                         DIST_HPF_DEFAULT, DIST_GAIN_DEFAULT,
                         DIST_TONE_DEFAULT, DIST_TONE_DAMP_DEFAULT);

    IFX_Chorus_Init(&dsp_chorus,
                    CHO_DELAY_A_MS_DEFAULT, CHO_DELAY_B_MS_DEFAULT,
                    CHO_DEPTH_A_DEFAULT,    CHO_DEPTH_B_DEFAULT,
                    CHO_GAIN_A_DEFAULT,     CHO_GAIN_B_DEFAULT,
                    CHO_RATE_A_DEFAULT,     CHO_RATE_B_DEFAULT,
                    CHO_MIX_DEFAULT,        SAMPLE_RATE);

    // ── I2S TX (PCM5102A) – master: generates BCK and WS ─────────────────
    auto tx_cfg                = i2s_tx.defaultConfig(TX_MODE);
    tx_cfg.sample_rate         = SAMPLE_RATE;
    tx_cfg.bits_per_sample     = BITS_PER_SAMPLE;
    tx_cfg.channels            = AUDIO_CHANNELS;
    tx_cfg.pin_bck             = PIN_I2S_BCK;
    tx_cfg.pin_ws              = PIN_I2S_WS;
    tx_cfg.pin_data            = PIN_DAC_DATA;
    tx_cfg.is_master           = true;
    i2s_tx.begin(tx_cfg);

    // ── I2S RX (PCM1808) – slave: listens to same BCK/WS ─────────────────
    auto rx_cfg                = i2s_rx.defaultConfig(RX_MODE);
    rx_cfg.sample_rate         = SAMPLE_RATE;
    rx_cfg.bits_per_sample     = BITS_PER_SAMPLE;
    rx_cfg.channels            = AUDIO_CHANNELS;
    rx_cfg.pin_bck             = PIN_I2S_BCK;
    rx_cfg.pin_ws              = PIN_I2S_WS;
    rx_cfg.pin_data            = PIN_ADC_DATA;
    rx_cfg.is_master           = false;
    i2s_rx.begin(rx_cfg);
}

// ── Process one float sample through the full effects chain ─────────────────
static inline float process_sample(float in)
{
    float y = in;

    // 1. EQ  (shapes tone before distortion – removes bass mud)
    if (g_eq.enabled)
        y = IFX_EQ_Update(&dsp_eq, y);

    // 2. Distortion
    if (g_dist.enabled) {
        float out = IFX_Distortion_Update(&dsp_dist, y);
        y = out * g_dist.level;
    }

    // 3. Chorus (modulation last – classic studio order)
    if (g_chorus.enabled)
        y = IFX_Chorus_Update(&dsp_chorus, y);

    return y;
}

// ── Re-apply parameters when Core 0 signals a change ─────────────────────────
static void apply_param_updates()
{
    // Clear flag BEFORE applying – if Core 0 sets it again mid-apply we catch
    // it on the very next call rather than silently discarding the update.
    if (g_dist.needs_update) {
        g_dist.needs_update = false;
        __sync_synchronize();
        IFX_Distortion_SetGain(&dsp_dist, g_dist.gain);
        IFX_Distortion_SetHPF (&dsp_dist, g_dist.hpf_freq);
        IFX_Distortion_SetLPF (&dsp_dist, g_dist.tone_freq, g_dist.tone_damp);
    }

    if (g_chorus.needs_update) {
        g_chorus.needs_update = false;
        __sync_synchronize();
        float rateB  = g_chorus.rate  * (CHO_RATE_B_DEFAULT / CHO_RATE_A_DEFAULT);
        float delayB = g_chorus.delay_ms + 2.0f;
        IFX_Chorus_Init(&dsp_chorus,
                        g_chorus.delay_ms, delayB,
                        g_chorus.depth,    g_chorus.depth * 0.85f,
                        CHO_GAIN_A_DEFAULT, CHO_GAIN_B_DEFAULT,
                        g_chorus.rate,  rateB,
                        g_chorus.mix,
                        SAMPLE_RATE);
    }

    if (g_eq.needs_update) {
        g_eq.needs_update = false;
        __sync_synchronize();
        IFX_EQ_SetBass  (&dsp_eq, db_to_linear(g_eq.bass_db));
        IFX_EQ_SetMid   (&dsp_eq, db_to_linear(g_eq.mid_db),
                          g_eq.mid_freq, g_eq.mid_bw);
        IFX_EQ_SetTreble(&dsp_eq, db_to_linear(g_eq.treb_db));
    }
}

void loop1()
{
    // Check for parameter updates from Core 0 (infrequent – encoder events)
    apply_param_updates();

    // ── Read audio buffer from PCM1808 ─────────────────────────────────
    size_t bytes_to_read = sizeof(rx_buf);
    size_t bytes_read    = i2s_rx.readBytes((uint8_t *)rx_buf, bytes_to_read);

    if (bytes_read != bytes_to_read) return;   // partial read – retry next call

    // ── Per-sample processing ─────────────────────────────────────────
    for (int i = 0; i < AUDIO_BUFFER_FRAMES; i++) {
        // PCM1808 outputs 24-bit in MSB-aligned 32-bit frame.
        // Divide by 2^31 gives normalised float in [-1, 1].
        float left  = (float)rx_buf[i * 2    ] * (1.0f / 2147483648.0f);
        float right = (float)rx_buf[i * 2 + 1] * (1.0f / 2147483648.0f);

        // Guitar is mono – sum and halve to prevent clipping
        float mono = (left + right) * 0.5f;

        float out = process_sample(mono);

        // Clamp before conversion: float rounding at ±1.0f can overflow INT32
        out = clampf(out, -1.0f, 1.0f);
        int32_t pcm = (int32_t)(out * 2147483647.0f);
        tx_buf[i * 2    ] = pcm;
        tx_buf[i * 2 + 1] = pcm;
    }

    // ── Write processed buffer to PCM5102A ────────────────────────────
    i2s_tx.write((uint8_t *)tx_buf, sizeof(tx_buf));
}
