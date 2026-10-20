/*
 * ZOMBI SS Multi-Effect DSP Unit — MUX Edition
 * =============================================
 * Target:   RP2350 (Raspberry Pi Pico 2) via Arduino-Pico core
 *
 * Architecture:
 *   Core 0: PIO I2S + DMA interrupt; noise gate → overdrive → EQ → chorus →
 *           delay → master volume
 *   Core 1: Boost Factory circuit models, run from a SIO doorbell interrupt
 *           one block ahead of core 0 (pipelined: +1 block = 1.33 ms latency),
 *           plus the UI (SSD1306 OLED, EC11 encoder, CD74HC4067 MUX buttons)
 *           and Boost Factory matrix rebuilds in loop1()
 *
 * Audio I/O:
 *   Input:  PCM1808 ADC via PIO1 I2S (SCKI on GP22 via PWM)
 *   Output: PCM5102A DAC via PIO0 I2S (SD/XSMT on GP0; FMT=GND internal = I2S Philips)
 *
 * Effects Chain:
 *   1. Boost Factory (9 circuit-modelled pedals)   2. Noise Gate
 *   3. Overdrive   4. 10-Band EQ   5. Chorus   6. Delay
 *
 * Clock: set "CPU Speed" in the Arduino Tools menu (240 MHz recommended —
 * the transistor fuzz models need it).  The sketch no longer overrides it.
 *
 * Original DSP algorithms by Philip Salmony @ phils-lab.net
 * Ported to RP2350 Arduino for Zombi SS
 */

#include <Arduino.h>
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "pico/multicore.h"
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
 * I2S output word layout (PCM5102A module, I2S Philips, FMT=GND):
 *   Bit 31:    delay bit (0 — I2S Philips 1-clock delay; PCM5102A ignores it)
 *   Bits 30–7: B23..B0 (24-bit signed audio, MSB first)
 *   Bits 6–0:  zero padding
 * Conversion: float * 2^23 → 24-bit int, mask 24 bits, shift left 7.
 */
/* ---- DSP load meters (DWT cycle counter — one per Cortex-M33 core) ------- */
#define DWT_CTRL   (*(volatile uint32_t *)0xE0001000u)
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004u)
#define DEMCR      (*(volatile uint32_t *)0xE000EDFCu)
static uint32_t g_block_cycles = 1;
static volatile uint8_t g_load0 = 0, g_load1 = 0;   /* smoothed % of block period */
static volatile uint8_t g_peak0 = 0, g_peak1 = 0;   /* peak since last read       */
static volatile uint32_t g_late_blocks = 0;         /* core 1 missed its deadline */

static void dwt_enable(void) {
    DEMCR |= (1u << 24);          /* TRCENA */
    DWT_CYCCNT = 0;
    DWT_CTRL |= 1u;               /* CYCCNTENA */
}

static inline void load_update(volatile uint8_t *avg, volatile uint8_t *peak, uint32_t cycles) {
    uint32_t pct = (uint32_t)(((uint64_t)cycles * 100u) / g_block_cycles);
    if (pct > 255u) pct = 255u;
    if (pct > *peak) *peak = (uint8_t)pct;
    *avg = (uint8_t)((*avg * 15u + pct) / 16u);
}

extern "C" uint8_t Audio_GetLoadPct(void)  { return g_load0 > g_load1 ? g_load0 : g_load1; }
extern "C" uint8_t Audio_GetLoad0(void)    { return g_load0; }
extern "C" uint8_t Audio_GetLoad1(void)    { return g_load1; }

/* ---- Core 1: Boost Factory block pipeline --------------------------------
 * Core 0's DMA IRQ hands block k to core 1 (doorbell) and, in the same IRQ,
 * finishes block k−1 that core 1 rendered during the previous period. */
static float g_bfBuf[2][AUDIO_BUFFER_FRAMES];
static volatile uint32_t g_bfReq  = 0;   /* last block id handed to core 1 */
static volatile uint32_t g_bfDone = 0;   /* last block id core 1 finished  */
static volatile uint32_t g_bfLen  = AUDIO_BUFFER_FRAMES;
static int g_bell = -1;

static void __not_in_flash_func(core1_bell_irq)(void) {
    /* All doorbells share one IRQ line, and the arduino-pico core uses one
     * of them itself (flash-safe idling) — only act on ours. */
    if (g_bell < 0 || !multicore_doorbell_is_set_current_core((uint)g_bell)) return;
    multicore_doorbell_clear_current_core((uint)g_bell);
    uint32_t id = g_bfReq;
    if (id == g_bfDone) return;
    uint32_t t0 = DWT_CYCCNT;
    EffectChain_ProcessBoost(&g_fx_chain, g_bfBuf[id & 1u], (int)g_bfLen);
    __asm volatile ("dmb sy" ::: "memory");
    g_bfDone = id;
    load_update(&g_load1, &g_peak1, DWT_CYCCNT - t0);
}

static void __not_in_flash_func(audio_process_callback)(const int32_t *input, int32_t *output,
                                    uint32_t frame_count) {
    uint32_t t0 = DWT_CYCCNT;

    /* Master-volume smoother: ramps at 0.002/sample (~500 samples = ~10 ms full-scale).
     * Prevents audible clicks when the user adjusts master volume mid-signal.
     * Lives here (Core 0 only) so it needs no cross-core synchronisation. */
    static float smooth_vol = 0.8f;
    float buf[AUDIO_BUFFER_FRAMES];
    if (frame_count > AUDIO_BUFFER_FRAMES) frame_count = AUDIO_BUFFER_FRAMES;

    /* 1. Collect block k−1 from core 1.  It had a whole block period; if it
     *    is still busy (overload), wait at most ~25 % of a period, then fall
     *    back to the dry signal for this block rather than miss the DMA. */
    const uint32_t prev = g_bfReq;
    float *done = g_bfBuf[prev & 1u];
    if (g_bell >= 0) {
        uint32_t w0 = DWT_CYCCNT;
        while (g_bfDone != prev && (DWT_CYCCNT - w0) < g_block_cycles / 4u) { }
    }
    bool ok = (g_bell < 0) || (g_bfDone == prev);
    if (!ok) g_late_blocks++;
    for (uint32_t i = 0; i < frame_count; i++) buf[i] = done[i];   /* wet (or stale) */

    /* 2. Hand block k to core 1: PCM1808 word = [delay bit | B23..B0 | 7 zeros].
     *    (uint32_t) cast makes the shift defined; >>8 on int32_t is arithmetic
     *    on every ARM/GCC target. */
    float *next = g_bfBuf[(prev + 1u) & 1u];
    if (ok) {
        for (uint32_t i = 0; i < frame_count; i++) {
            int32_t raw = input[i * 2];
            next[i] = (float)((int32_t)((uint32_t)raw << 1) >> 8) * (1.0f / 8388608.0f);
        }
        g_bfLen = frame_count;
        __asm volatile ("dmb sy" ::: "memory");
        g_bfReq = prev + 1u;
        if (g_bell >= 0) multicore_doorbell_set_other_core((uint)g_bell);
        else { EffectChain_ProcessBoost(&g_fx_chain, next, (int)frame_count); g_bfDone = g_bfReq; }
    } else {
        /* core 1 overrun: play this block dry, keep core 1's pipeline intact */
        for (uint32_t i = 0; i < frame_count; i++) {
            int32_t raw = input[i * 2];
            buf[i] = (float)((int32_t)((uint32_t)raw << 1) >> 8) * (1.0f / 8388608.0f);
        }
    }

    /* 3. Rest of the chain on core 0 (in parallel with core 1's block k).
     * Individual aligned float reads from g_fx_chain are atomic on Cortex-M33;
     * the DMB barrier in SetParam ensures writes from Core 1 are visible here. */
    EffectChain_ProcessRest(&g_fx_chain, buf, (int)frame_count);

    /* masterVolume is a 4-byte aligned float; single-word reads are atomic. */
    const float target = g_fx_chain.masterVolume;
    for (uint32_t i = 0; i < frame_count; i++) {
        if      (smooth_vol < target - 0.002f) smooth_vol += 0.002f;
        else if (smooth_vol > target + 0.002f) smooth_vol -= 0.002f;
        else                                    smooth_vol  = target;

        float out = buf[i] * smooth_vol;
        /* Last-resort safety net: NaN compares false against every < / > test,
         * and casting a NaN float to int32_t is undefined behaviour. */
        if (out != out) out = 0.0f;
        if (out >  1.0f) out =  1.0f;
        if (out < -1.0f) out = -1.0f;

        /* I2S Philips word: bit31=0 (delay), bits30-7=B23..B0, bits6-0=0. */
        int32_t audio24 = (int32_t)(out * 8388607.0f);
        int32_t out_i32 = (int32_t)(((uint32_t)audio24 & 0x00FFFFFFu) << 7);
        output[i * 2]     = out_i32;
        output[i * 2 + 1] = out_i32;
    }

    load_update(&g_load0, &g_peak0, DWT_CYCCNT - t0);
}

/* PCM1808 needs SCKI = 256, 384 or 512 × fs.  Derive it from whatever system
 * clock the Arduino "CPU Speed" menu selected with an integer PWM divider,
 * picking the ratio with the smallest error (240 MHz → ÷13 = 18.46 MHz =
 * 384.6·fs, +0.16 %; 150 MHz → ÷12 = 12.5 MHz = 260·fs, +1.7 %). */
static void start_pcm1808_scki(void) {
    const uint32_t sys = clock_get_hz(clk_sys);
    const uint32_t ratios[3] = { 256u, 384u, 512u };
    uint32_t bestDiv = 12; float bestErr = 1e9f;
    for (int r = 0; r < 3; r++) {
        float target = (float)AUDIO_SAMPLE_RATE * (float)ratios[r];
        uint32_t div = (uint32_t)((float)sys / target + 0.5f);
        if (div < 2) continue;
        float f = (float)sys / (float)div;
        if (f > 25.0e6f) continue;                       /* PCM1808 SCKI max */
        float err = fabsf(f / target - 1.0f);
        if (err < bestErr) { bestErr = err; bestDiv = div; }
    }
    gpio_set_function(PCM1808_SCKI_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(PCM1808_SCKI_PIN);
    uint chan  = pwm_gpio_to_channel(PCM1808_SCKI_PIN);
    pwm_set_clkdiv(slice, 1.0f);
    pwm_set_wrap(slice, bestDiv - 1);
    pwm_set_chan_level(slice, chan, bestDiv / 2);
    pwm_set_enabled(slice, true);
}

/* ===== Core 0: setup() / loop() ===== */

void setup() {
    /* System clock comes from the Arduino Tools → CPU Speed menu (F_CPU).
     * The I2S PIO dividers and SCKI are derived from clock_get_hz(), so any
     * menu setting keeps LRCK at exactly 48 kHz. */
    start_pcm1808_scki();

    /* DWT cycle counter for the DSP load meter */
    dwt_enable();
    g_block_cycles = (uint32_t)(((uint64_t)clock_get_hz(clk_sys) * AUDIO_BUFFER_FRAMES) / AUDIO_SAMPLE_RATE);

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
    /* Boost Factory renderer: doorbell IRQ on this core (core 0 rings it
     * once per audio block).  Claimed before audio starts producing blocks;
     * until then core 0 renders the boost itself (g_bell < 0). */
    dwt_enable();
    int bell = multicore_doorbell_claim_unused((1u << 0) | (1u << 1), true);
    uint bellIrq = multicore_doorbell_irq_num((uint)bell);
    irq_add_shared_handler(bellIrq, core1_bell_irq, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_enabled(bellIrq, true);
    g_bfDone = g_bfReq;           /* empty pipeline */
    __asm volatile ("dmb sy" ::: "memory");
    g_bell = bell;

    delay(300);   /* let Core 0 bring up audio before I2C init */

    g_oled.init(OLED_SDA_PIN, OLED_SCL_PIN, OLED_I2C_ADDR);
    g_encoder.init();
    g_ui.init(&g_oled, &g_encoder, &g_fx_chain);
}

void loop1() {
    g_ui.update();
    /* Rebuild Boost Factory circuit matrices after knob turns (double
     * precision, a few ms) — on this core, preempted by the audio doorbell. */
    EffectChain_Service(&g_fx_chain);

    /* CPU fallback: if the Boost Factory keeps core 1 above ~90 % of the
     * block period (or misses deadlines) for ~0.5 s, render the current
     * model at 1× instead of 2×/4×.  Re-armed on every model change. */
    static uint32_t hotSince = 0;
    static uint32_t lateSeen = 0;
    uint32_t now = millis();
    bool hot = (g_load1 > 90) || (g_late_blocks != lateSeen);
    lateSeen = g_late_blocks;
    if (!hot) hotSince = now;
    else if (now - hotSince > 500 && !IFX_BF_IsLowCpu(&g_fx_chain.boost)) {
        EffectChain_SetBoostLowCpu(&g_fx_chain, true);
        g_ui.forceRedraw();
        Serial.println("Boost Factory: CPU fallback -> 1x oversampling for this model");
    }

    static uint32_t lastReport = 0;
    if (now - lastReport > 2000) {
        lastReport = now;
        uint8_t p0 = g_peak0, p1 = g_peak1; g_peak0 = 0; g_peak1 = 0;
        Serial.printf("DSP load core0 %u%% (peak %u%%)  core1 %u%% (peak %u%%)  late %lu  @ %lu MHz\n",
                      g_load0, p0, g_load1, p1, (unsigned long)g_late_blocks,
                      (unsigned long)(clock_get_hz(clk_sys) / 1000000));
    }
    delay(16);   /* ≈ 60 Hz UI refresh */
}
