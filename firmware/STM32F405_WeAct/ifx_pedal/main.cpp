/*
 * ZombiSS multi-effect on the WeAct STM32F405RGT6 core board.
 *
 * Same DSP and UI sources as the RP2350 build (../../ZombiSS_MultiEffect),
 * compiled for one Cortex-M4F core at 168 MHz:
 *   DMA IRQ (priority 1):  Boost Factory → gate → overdrive → EQ → chorus →
 *                          delay ⇄ reverb → master volume
 *   main loop:             OLED / encoder / MUX UI, Boost Factory rebuilds,
 *                          CPU-overload fallback, LED heartbeat
 * Differences from the RP2350 build (RAM: 128 KB DMA-capable SRAM + 64 KB CCM):
 *   delay max 450 ms (was 1.5 s), reverb tank at 24 kHz (IFX_REVERB_HALF_RATE),
 *   gate RMS window 25 ms — all set in pedalflash.json.
 *
 * Without any UI hardware it still works: KEY (PC13) cycles three presets.
 */
#include "board.h"
#include "Arduino.h"

extern "C" {
#include "effect_chain.h"
#include "audio_i2s.h"
}
#include "sh1106_oled.h"
#include "rotary_encoder.h"
#include "ui_system.h"

/* ~126 KB: must sit in SRAM, the only region big enough (CCM is 64 KB) */
__attribute__((section(".sram"))) static EffectChain g_chain;
static SH1106        g_oled;
static RotaryEncoder g_encoder;
static UISystem      g_ui;

/* ---- DSP load (DWT cycles, % of one block period) ----------------------- */
static uint32_t g_blockCycles;
static volatile uint8_t g_loadRest, g_loadBoost, g_peak;

extern "C" uint8_t Audio_GetLoad0(void)   { return g_loadRest; }
extern "C" uint8_t Audio_GetLoad1(void)   { return g_loadBoost; }
extern "C" uint8_t Audio_GetLoadPct(void) { return (uint8_t)(g_loadRest + g_loadBoost); }

static inline void load_avg(volatile uint8_t *avg, uint32_t cycles) {
    uint32_t pct = (uint32_t)(((uint64_t)cycles * 100u) / g_blockCycles);
    if (pct > 255u) pct = 255u;
    *avg = (uint8_t)((*avg * 15u + pct) / 16u);
}

static void audio_cb(const float *in, float *out, int n) {
    static float smoothVol = 0.8f;
    float buf[AUDIO_FRAMES];
    for (int i = 0; i < n; i++) buf[i] = in[i];

    uint32_t t0 = DWT->CYCCNT;
    EffectChain_ProcessBoost(&g_chain, buf, n);
    uint32_t t1 = DWT->CYCCNT;
    EffectChain_ProcessRest(&g_chain, buf, n);
    uint32_t t2 = DWT->CYCCNT;

    const float target = g_chain.masterVolume;
    for (int i = 0; i < n; i++) {              /* same click-free ramp as the RP2350 build */
        if      (smoothVol < target - 0.002f) smoothVol += 0.002f;
        else if (smoothVol > target + 0.002f) smoothVol -= 0.002f;
        else                                   smoothVol  = target;
        out[i] = buf[i] * smoothVol;
    }
    load_avg(&g_loadBoost, t1 - t0);
    load_avg(&g_loadRest, t2 - t1);
    uint32_t tot = (uint32_t)(((uint64_t)(t2 - t0) * 100u) / g_blockCycles);
    if (tot > g_peak) g_peak = (uint8_t)(tot > 255u ? 255u : tot);
}

/* ---- presets on the on-board KEY (no UI hardware needed) ---------------- */
static void set_active(uint8_t fx, bool on) {
    if (g_chain.active[fx] != on) EffectChain_ToggleEffect(&g_chain, fx);
}
static void apply_preset(int p) {
    static const bool presets[3][FX_COUNT] = {
        /* BOOST  GATE   DRIVE  EQ     CHORUS DELAY  REVERB */
        { false, false, true,  true,  false, false, false },   /* 0: boot default (drive + EQ) */
        { false, false, false, false, true,  true,  true  },   /* 1: clean, chorus + delay + verb */
        { false, false, false, false, false, false, false },   /* 2: everything off (dry)        */
    };
    for (uint8_t fx = 0; fx < FX_COUNT; fx++) set_active(fx, presets[p][fx]);
    g_ui.forceRedraw();
}

int main(void) {
    board_init();                                  /* 168 MHz, SysTick, DWT, LED, KEY */
    g_blockCycles = (uint32_t)(((uint64_t)SystemCoreClock * AUDIO_FRAMES) / AUDIO_SAMPLE_RATE);

    gpio_config(PCM5102_XSMT_PIN, GPIO_OUT, GPIO_NOPULL, 0, false, 0);
    gpio_write(PCM5102_XSMT_PIN, 1);               /* unmute the PCM5102 */

    EffectChain_Init(&g_chain);                    /* builds the boost model (soft double: slow, once) */
    audio_start(audio_cb);

    g_oled.init(OLED_SDA_PIN, OLED_SCL_PIN, OLED_I2C_ADDR);   /* harmless with no OLED */
    g_encoder.init();
    g_ui.init(&g_oled, &g_encoder, &g_chain);

    int preset = 0;
    bool keyWas = false;
    uint32_t keyT = 0, hotSince = 0, ledT = 0, lateSeen = 0;
    for (;;) {
        g_ui.update();
        EffectChain_Service(&g_chain);
        uint32_t now = millis();

        /* KEY: debounced press → next preset */
        bool key = !gpio_read(BOARD_KEY_PIN);
        if (key != keyWas && now - keyT > 30) {
            keyT = now; keyWas = key;
            if (key) { preset = (preset + 1) % 3; apply_preset(preset); }
        }

        /* Overload fallback, single core: first render the boost at 1×,
         * then switch the boost off.  Late blocks = audible dropouts. */
        uint32_t late = audio_late_blocks();
        bool hot = (g_loadRest + g_loadBoost > 92) || late != lateSeen;
        lateSeen = late;
        if (!hot) hotSince = now;
        else if (now - hotSince > 500) {
            if (g_chain.active[FX_BOOST] && !IFX_BF_IsLowCpu(&g_chain.boost))
                EffectChain_SetBoostLowCpu(&g_chain, true);
            else if (g_chain.active[FX_BOOST])
                EffectChain_ToggleEffect(&g_chain, FX_BOOST);
            hotSince = now;
            g_ui.forceRedraw();
        }

        /* LED: 1 Hz heartbeat, 8 Hz while overloaded */
        if (now - ledT > (hot ? 60u : 500u)) { ledT = now; gpio_toggle(BOARD_LED_PIN); }

        board_delay_ms(16);                        /* ≈ 60 Hz UI, like loop1() */
    }
}
