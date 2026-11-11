#pragma GCC optimize ("O2")
#include "effect_chain.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "ifx_mix.h"
/* __dmb() = Data Memory Barrier: ensures all writes before it are globally visible
 * before any read/write after it. Required for correct cross-core visibility
 * (Core 1 writes params; Core 0 audio IRQ reads them). */
#if defined(__arm__)
#define PARAM_WRITE_BARRIER() __asm volatile ("dmb sy" ::: "memory")
#else   /* host builds (tools/ test kits) */
#define PARAM_WRITE_BARRIER() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#endif

/* Compile-time guard: EQ param count must match the actual number of EQ bands */
_Static_assert(EQ_PARAM_COUNT == EQ10_BANDS,
    "EQ_PARAM_COUNT in effect_chain.h must equal EQ10_BANDS in ifx_10band_eq.h");

/* Model-dependent labels and voice range for the Boost Factory params. */
static void boost_model_changed(EffectChain *ec) {
    ParamDesc *p = ec->params[FX_BOOST];
    uint8_t m = (uint8_t)p[BF_MODEL].value;
    const bf_model_desc *md = &BF_MODELS[m < BF_MODEL_COUNT ? m : 0];
    p[BF_DRIVE].name = md->driveLabel;
    p[BF_TONE].name  = md->toneLabel;
    p[BF_VOICE].max  = (float)(md->nVoices - 1);
    p[BF_DRIVE].value = md->defDrive * 10.0f;
    p[BF_TONE].value  = md->defTone * 10.0f;
    IFX_BF_SetDrive(&ec->boost, md->defDrive);
    IFX_BF_SetTone(&ec->boost, md->defTone);
    if (p[BF_VOICE].value > p[BF_VOICE].max) p[BF_VOICE].value = 0.0f;
    IFX_BF_SetModel(&ec->boost, m);
    IFX_BF_SetVoice(&ec->boost, (uint8_t)p[BF_VOICE].value);
}

static void init_param_descs(EffectChain *ec) {

    /* ===== Boost Factory ===== */
    ec->fxNames[FX_BOOST] = "BOOST";
    ec->paramCount[FX_BOOST] = BF_PARAM_COUNT;
    ec->params[FX_BOOST][BF_MODEL] = (ParamDesc){"Model", 0.0f, (float)(BF_MODEL_COUNT - 1), 1.0f, 0.0f, "", 1};
    ec->params[FX_BOOST][BF_VOICE] = (ParamDesc){"Voice", 0.0f, 3.0f,   1.0f,  0.0f, "", 1};
    ec->params[FX_BOOST][BF_DRIVE] = (ParamDesc){"Drive", 0.0f, 10.0f,  0.2f,  6.0f, "", 0};
    ec->params[FX_BOOST][BF_TONE]  = (ParamDesc){"Tone",  0.0f, 10.0f,  0.2f,  5.0f, "", 0};
    ec->params[FX_BOOST][BF_LEVEL] = (ParamDesc){"Level", -30.0f, 12.0f, 0.5f, 0.0f, "dB", 0};

    /* ===== Noise Gate ===== */
    ec->fxNames[FX_NOISEGATE] = "GATE";
    ec->paramCount[FX_NOISEGATE] = NG_PARAM_COUNT;
    ec->params[FX_NOISEGATE][NG_THRESHOLD] = (ParamDesc){"Threshold", 0.01f, 0.5f,   0.01f, 0.1f,  ""};
    ec->params[FX_NOISEGATE][NG_ATTACK]    = (ParamDesc){"Attack",    0.5f,  50.0f,  0.5f,  2.0f,  "ms"};
    ec->params[FX_NOISEGATE][NG_RELEASE]   = (ParamDesc){"Release",   0.5f,  50.0f,  0.5f,  2.0f,  "ms"};
    ec->params[FX_NOISEGATE][NG_HOLD]      = (ParamDesc){"Hold",      1.0f,  200.0f, 1.0f,  20.0f, "ms"};

    /* ===== Overdrive ===== */
    ec->fxNames[FX_OVERDRIVE] = "DRIVE";
    ec->paramCount[FX_OVERDRIVE] = OD_PARAM_COUNT;
    ec->params[FX_OVERDRIVE][OD_GAIN]       = (ParamDesc){"Gain",    1.0f,   200.0f,  1.0f,  40.0f,  ""};
    ec->params[FX_OVERDRIVE][OD_BOOST]      = (ParamDesc){"Boost",   0.0f,   100.0f,  1.0f,  0.0f,   ""};
    ec->params[FX_OVERDRIVE][OD_HPF_CUTOFF] = (ParamDesc){"HPF",     50.0f,  2000.0f, 10.0f, 150.0f, "Hz"};
    ec->params[FX_OVERDRIVE][OD_LPF_CUTOFF] = (ParamDesc){"LPF",     500.0f, 20000.0f,100.0f,5000.0f,"Hz"};
    ec->params[FX_OVERDRIVE][OD_LPF_DAMP]   = (ParamDesc){"Damping", 0.1f,   2.0f,    0.05f, 1.0f,   ""};
    ec->params[FX_OVERDRIVE][OD_Q_CLIP]     = (ParamDesc){"Clip Q",  -0.5f, -0.01f,   0.01f,-0.2f,   ""};

    /* ===== 10-Band EQ (Metallica preset defaults) ===== */
    ec->fxNames[FX_EQ] = "10B EQ";
    ec->paramCount[FX_EQ] = EQ_PARAM_COUNT;
    ec->params[FX_EQ][EQ_31HZ]  = (ParamDesc){"31 Hz",  -15.0f, 15.0f, 0.5f,  6.0f, "dB"};
    ec->params[FX_EQ][EQ_63HZ]  = (ParamDesc){"63 Hz",  -15.0f, 15.0f, 0.5f,  5.0f, "dB"};
    ec->params[FX_EQ][EQ_125HZ] = (ParamDesc){"125 Hz", -15.0f, 15.0f, 0.5f,  3.0f, "dB"};
    ec->params[FX_EQ][EQ_250HZ] = (ParamDesc){"250 Hz", -15.0f, 15.0f, 0.5f, -2.0f, "dB"};
    ec->params[FX_EQ][EQ_500HZ] = (ParamDesc){"500 Hz", -15.0f, 15.0f, 0.5f, -4.0f, "dB"};
    ec->params[FX_EQ][EQ_1KHZ]  = (ParamDesc){"1k Hz",  -15.0f, 15.0f, 0.5f, -6.0f, "dB"};
    ec->params[FX_EQ][EQ_2KHZ]  = (ParamDesc){"2k Hz",  -15.0f, 15.0f, 0.5f, -5.0f, "dB"};
    ec->params[FX_EQ][EQ_4KHZ]  = (ParamDesc){"4k Hz",  -15.0f, 15.0f, 0.5f, -3.0f, "dB"};
    ec->params[FX_EQ][EQ_8KHZ]  = (ParamDesc){"8k Hz",  -15.0f, 15.0f, 0.5f,  4.0f, "dB"};
    ec->params[FX_EQ][EQ_16KHZ] = (ParamDesc){"16k Hz", -15.0f, 15.0f, 0.5f,  3.0f, "dB"};

    /* ===== Chorus ===== */
    ec->fxNames[FX_CHORUS] = "CHORUS";
    ec->paramCount[FX_CHORUS] = CH_PARAM_COUNT;
    ec->params[FX_CHORUS][CH_DELAY_A] = (ParamDesc){"Delay A",  1.0f,  40.0f,  0.5f,  10.0f, "ms"};
    ec->params[FX_CHORUS][CH_DELAY_B] = (ParamDesc){"Delay B",  1.0f,  40.0f,  0.5f,  15.0f, "ms"};
    ec->params[FX_CHORUS][CH_DEPTH_A] = (ParamDesc){"Depth A",  0.0f,  20.0f,  0.5f,  10.0f, ""};
    ec->params[FX_CHORUS][CH_DEPTH_B] = (ParamDesc){"Depth B",  0.0f,  20.0f,  0.5f,  10.0f, ""};
    ec->params[FX_CHORUS][CH_RATE_A]  = (ParamDesc){"Rate A",   0.1f,  10.0f,  0.1f,  1.0f,  "Hz"};
    ec->params[FX_CHORUS][CH_RATE_B]  = (ParamDesc){"Rate B",   0.1f,  10.0f,  0.1f,  1.3f,  "Hz"};
    ec->params[FX_CHORUS][CH_GAIN_A]  = (ParamDesc){"Gain A",   0.0f,  1.0f,   0.05f, 0.25f, ""};
    ec->params[FX_CHORUS][CH_GAIN_B]  = (ParamDesc){"Gain B",   0.0f,  1.0f,   0.05f, 0.25f, ""};
    ec->params[FX_CHORUS][CH_MIX]     = (ParamDesc){"Mix",      0.0f,  1.0f,   0.05f, 0.5f,  ""};

    /* ===== Delay ===== */
    ec->fxNames[FX_DELAY] = "DELAY";
    ec->paramCount[FX_DELAY] = DL_PARAM_COUNT;
    ec->params[FX_DELAY][DL_TIME]     = (ParamDesc){"Time",     10.0f, IFX_DELAY_MAX_MS, 10.0f,
                                         IFX_DELAY_MAX_MS < 300.0f ? IFX_DELAY_MAX_MS : 300.0f, "ms"};
    ec->params[FX_DELAY][DL_MIX]      = (ParamDesc){"Mix",      0.0f,  1.0f,   0.05f, 0.25f,  ""};
    ec->params[FX_DELAY][DL_FEEDBACK] = (ParamDesc){"Feedback", 0.0f,  0.95f,  0.05f, 0.4f,   ""};
    ec->params[FX_DELAY][DL_MODE]     = (ParamDesc){"Mode",     0.0f,  (float)(IFX_DLY_MODE_COUNT - 1), 1.0f, 0.0f, "", 1};
    ec->params[FX_DELAY][DL_TONE]     = (ParamDesc){"Tone",     500.0f, 12000.0f, 250.0f, 6000.0f, "Hz"};
    ec->params[FX_DELAY][DL_MOD]      = (ParamDesc){"Wow/Flut", 0.0f,  1.0f,   0.05f, 0.15f,  ""};
    ec->params[FX_DELAY][DL_TRAILS]   = (ParamDesc){"Trails",   0.0f,  1.0f,   1.0f,  1.0f,   "", 1};

    /* ===== Plate reverb ===== */
    ec->fxNames[FX_REVERB] = "REVERB";
    ec->paramCount[FX_REVERB] = RV_PARAM_COUNT;
    ec->params[FX_REVERB][RV_MIX]      = (ParamDesc){"Mix",      0.0f,  1.0f,    0.05f, 0.3f,    ""};
    ec->params[FX_REVERB][RV_DECAY]    = (ParamDesc){"Decay",    0.0f,  0.98f,   0.02f, 0.6f,    ""};
    ec->params[FX_REVERB][RV_TONE]     = (ParamDesc){"Tone",     1000.0f, 16000.0f, 250.0f, 6000.0f, "Hz"};
    ec->params[FX_REVERB][RV_PREDELAY] = (ParamDesc){"PreDelay", 0.0f,  100.0f,  2.0f,  10.0f,   "ms"};
    ec->params[FX_REVERB][RV_ROUTING]  = (ParamDesc){"Routing",  0.0f,  2.0f,    1.0f,  0.0f,    "", 1};
    ec->params[FX_REVERB][RV_TRAILS]   = (ParamDesc){"Trails",   0.0f,  1.0f,    1.0f,  1.0f,    "", 1};
}

void EffectChain_Init(EffectChain *ec) {
    init_param_descs(ec);

    ec->active[FX_BOOST]     = false;
    ec->active[FX_NOISEGATE] = false;
    ec->active[FX_OVERDRIVE] = true;
    ec->active[FX_EQ]        = true;   /* EQ on by default — Metallica preset */
    ec->active[FX_CHORUS]    = false;
    ec->active[FX_DELAY]     = false;
    ec->active[FX_REVERB]    = false;
    ec->masterVolume = 0.8f;

    IFX_BF_Init(&ec->boost, SAMPLE_RATE_HZ);
    boost_model_changed(ec);
    IFX_BF_SetDrive(&ec->boost, ec->params[FX_BOOST][BF_DRIVE].value * 0.1f);
    IFX_BF_SetTone(&ec->boost,  ec->params[FX_BOOST][BF_TONE].value * 0.1f);
    IFX_BF_SetLevelDb(&ec->boost, ec->params[FX_BOOST][BF_LEVEL].value);
    IFX_BF_Service(&ec->boost);

    IFX_NoiseGate_Init(&ec->noiseGate,
        ec->params[FX_NOISEGATE][NG_THRESHOLD].value,
        ec->params[FX_NOISEGATE][NG_ATTACK].value,
        ec->params[FX_NOISEGATE][NG_RELEASE].value,
        ec->params[FX_NOISEGATE][NG_HOLD].value,
        SAMPLE_RATE_HZ);

    IFX_Overdrive_Init(&ec->overdrive, SAMPLE_RATE_HZ,
        ec->params[FX_OVERDRIVE][OD_HPF_CUTOFF].value,
        ec->params[FX_OVERDRIVE][OD_GAIN].value,
        ec->params[FX_OVERDRIVE][OD_LPF_CUTOFF].value,
        ec->params[FX_OVERDRIVE][OD_LPF_DAMP].value);
    IFX_Overdrive_SetQ(&ec->overdrive, ec->params[FX_OVERDRIVE][OD_Q_CLIP].value);

    IFX_10BandEQ_Init(&ec->eq, SAMPLE_RATE_HZ);
    /* Init already loads IFX_EQ10_METALLICA_PRESET; param defaults above
     * match that preset exactly, so no further SetBand calls are needed. */

    IFX_Chorus_Init(&ec->chorus,
        ec->params[FX_CHORUS][CH_DELAY_A].value,
        ec->params[FX_CHORUS][CH_DELAY_B].value,
        ec->params[FX_CHORUS][CH_DEPTH_A].value,
        ec->params[FX_CHORUS][CH_DEPTH_B].value,
        ec->params[FX_CHORUS][CH_GAIN_A].value,
        ec->params[FX_CHORUS][CH_GAIN_B].value,
        ec->params[FX_CHORUS][CH_RATE_A].value,
        ec->params[FX_CHORUS][CH_RATE_B].value,
        ec->params[FX_CHORUS][CH_MIX].value,
        SAMPLE_RATE_HZ);

    IFX_Delay_Init(&ec->delay,
        ec->params[FX_DELAY][DL_TIME].value,
        ec->params[FX_DELAY][DL_MIX].value,
        ec->params[FX_DELAY][DL_FEEDBACK].value,
        SAMPLE_RATE_HZ);
    IFX_Delay_SetMode(&ec->delay, (uint8_t)ec->params[FX_DELAY][DL_MODE].value);
    IFX_Delay_SetTone(&ec->delay, ec->params[FX_DELAY][DL_TONE].value);
    IFX_Delay_SetMod(&ec->delay,  ec->params[FX_DELAY][DL_MOD].value);

    IFX_Reverb_Init(&ec->reverb, SAMPLE_RATE_HZ);
    IFX_Reverb_SetMix(&ec->reverb,      ec->params[FX_REVERB][RV_MIX].value);
    IFX_Reverb_SetDecay(&ec->reverb,    ec->params[FX_REVERB][RV_DECAY].value);
    IFX_Reverb_SetDamping(&ec->reverb,  ec->params[FX_REVERB][RV_TONE].value);
    IFX_Reverb_SetPreDelay(&ec->reverb, ec->params[FX_REVERB][RV_PREDELAY].value);

    for (int i = 0; i < FX_COUNT; i++) ec->fade[i] = ec->active[i] ? 1.0f : 0.0f;
    ec->hold = false;
    ec->dDry = ec->rDry = 1.0f;
    ec->dWet = ec->rWet = 0.0f;
    ec->dRun = ec->rRun = false;
    ec->dQuiet = ec->rQuiet = 0;
}


/* ---- click-free switching ------------------------------------------------
 * Toggling an effect used to swap dry ↔ wet at a sample boundary, which
 * clicks (loudly, on a 40× overdrive).  Every effect now crossfades over
 * FADE_MS; while fully off it is skipped as before (no CPU cost). */
#define FADE_MS      6.0f
#define FADE_STEP    (1000.0f / (FADE_MS * SAMPLE_RATE_HZ))
#define GAIN_STEP    (1.0f / 480.0f)            /* 10 ms dry/wet gain ramps */
#define TAIL_SILENT  5.0e-4f                    /* ≈ −66 dBFS               */

static inline void xfade(float *buf, const float *dry, int n, float *fade, bool on) {
    float f = *fade;
    for (int i = 0; i < n; i++) {
        if (on) { f += FADE_STEP; if (f > 1.0f) f = 1.0f; }
        else    { f -= FADE_STEP; if (f < 0.0f) f = 0.0f; }
        buf[i] = dry[i] + f * (buf[i] - dry[i]);
    }
    *fade = f;
}

#define RUN_FX(ec, FX, CALL) do {                                          \
        bool on_ = (ec)->active[FX];                                       \
        float *fd_ = &(ec)->fade[FX];                                      \
        if (on_ && *fd_ >= 1.0f) { CALL; }                                 \
        else if (on_ || *fd_ > 0.0f) {                                     \
            float dry_[BF_BLOCK_MAX];                                      \
            memcpy(dry_, buf, (size_t)n * sizeof(float));                  \
            CALL;                                                          \
            xfade(buf, dry_, n, fd_, on_);                                 \
        }                                                                  \
    } while (0)

static inline float ramp_to(float g, float t) {
    if (g < t - GAIN_STEP) return g + GAIN_STEP;
    if (g > t + GAIN_STEP) return g - GAIN_STEP;
    return t;
}

static float block_peak(const float *x, int n) {
    float p = 0.0f;
    for (int i = 0; i < n; i++) { float a = fabsf(x[i]); if (a > p) p = a; }
    return p;
}

/* Delay + reverb.  Ideas from the GuitarML pedal: selectable routing
 * (delay→reverb, parallel, reverb→delay), trails, and a hold/freeze switch.
 * A switched-off time effect keeps running with its input muted until its
 * tail has died away, so re-enabling it never replays stale audio; with
 * Trails = On that tail is also heard. */
static void time_stage(EffectChain *ec, float *buf, int n) {
    const bool dOn = ec->active[FX_DELAY], rOn = ec->active[FX_REVERB];
    const bool dTrails = ec->params[FX_DELAY][DL_TRAILS].value > 0.5f;
    const bool rTrails = ec->params[FX_REVERB][RV_TRAILS].value > 0.5f;
    const float dMix = ec->params[FX_DELAY][DL_MIX].value;
    const float rMix = ec->params[FX_REVERB][RV_MIX].value;
    const uint8_t route = (uint8_t)ec->params[FX_REVERB][RV_ROUTING].value;

    if (dOn) { ec->dRun = true; ec->dQuiet = 0; }
    if (rOn) { ec->rRun = true; ec->rQuiet = 0; }
    if (!ec->dRun && !ec->rRun) return;

    IFX_Delay_SetFeed(&ec->delay, dOn ? 1.0f : 0.0f);
    IFX_Reverb_SetFeed(&ec->reverb, rOn ? 1.0f : 0.0f);
    IFX_Delay_SetFreeze(&ec->delay, ec->hold && dOn);
    IFX_Reverb_SetFreeze(&ec->reverb, ec->hold && rOn);

    const float dDryT = dOn ? IFX_MixDry(dMix) : 1.0f;
    const float dWetT = (dOn || dTrails) ? IFX_MixWet(dMix) : 0.0f;
    const float rDryT = rOn ? IFX_MixDry(rMix) : 1.0f;
    const float rWetT = (rOn || rTrails) ? IFX_MixWet(rMix) : 0.0f;

    float wd[BF_BLOCK_MAX], wr[BF_BLOCK_MAX];
    float dDry = ec->dDry, dWet = ec->dWet, rDry = ec->rDry, rWet = ec->rWet;

    if (route == ROUTE_PARALLEL) {
        if (ec->dRun) IFX_Delay_ProcessWet(&ec->delay, buf, wd, n);   else memset(wd, 0, sizeof(float) * (size_t)n);
        if (ec->rRun) IFX_Reverb_ProcessWet(&ec->reverb, buf, wr, n); else memset(wr, 0, sizeof(float) * (size_t)n);
        for (int i = 0; i < n; i++) {
            dDry = ramp_to(dDry, dDryT); dWet = ramp_to(dWet, dWetT);
            rDry = ramp_to(rDry, rDryT); rWet = ramp_to(rWet, rWetT);
            buf[i] = buf[i] * dDry * rDry + wd[i] * dWet + wr[i] * rWet;
        }
    } else {
        const bool delayFirst = (route != ROUTE_VERB_DLY);
        for (int stage = 0; stage < 2; stage++) {
            bool isDelay = (stage == 0) == delayFirst;
            if (isDelay) {
                if (!ec->dRun) { memset(wd, 0, sizeof(float) * (size_t)n); continue; }
                IFX_Delay_ProcessWet(&ec->delay, buf, wd, n);
                for (int i = 0; i < n; i++) {
                    dDry = ramp_to(dDry, dDryT); dWet = ramp_to(dWet, dWetT);
                    buf[i] = buf[i] * dDry + wd[i] * dWet;
                }
            } else {
                if (!ec->rRun) { memset(wr, 0, sizeof(float) * (size_t)n); continue; }
                IFX_Reverb_ProcessWet(&ec->reverb, buf, wr, n);
                for (int i = 0; i < n; i++) {
                    rDry = ramp_to(rDry, rDryT); rWet = ramp_to(rWet, rWetT);
                    buf[i] = buf[i] * rDry + wr[i] * rWet;
                }
            }
        }
    }
    ec->dDry = dDry; ec->dWet = dWet; ec->rDry = rDry; ec->rWet = rWet;

    /* stop a switched-off effect once its tail is silent for longer than
     * its longest internal delay */
    if (!dOn && ec->dRun) {
        if (block_peak(wd, n) < TAIL_SILENT) ec->dQuiet += (uint32_t)n; else ec->dQuiet = 0;
        if (ec->dQuiet > (uint32_t)ec->delay.timeCur * 2u + 4800u) {
            ec->dRun = false; ec->dDry = 1.0f; ec->dWet = 0.0f;
        }
    }
    if (!rOn && ec->rRun) {
        if (block_peak(wr, n) < TAIL_SILENT) ec->rQuiet += (uint32_t)n; else ec->rQuiet = 0;
        if (ec->rQuiet > (uint32_t)ec->reverb.preDelay + 9600u) {
            ec->rRun = false; ec->rDry = 1.0f; ec->rWet = 0.0f;
        }
    }
}

void EffectChain_ProcessBoost(EffectChain *ec, float *buf, int n) {
    RUN_FX(ec, FX_BOOST, IFX_BF_Process(&ec->boost, buf, n));
}

void EffectChain_ProcessRest(EffectChain *ec, float *buf, int n) {
    /* each effect runs a tight loop over the block (better register reuse
     * and far fewer cross-file calls than per-sample chaining) */
    RUN_FX(ec, FX_NOISEGATE, IFX_NoiseGate_Process(&ec->noiseGate, buf, n));
    RUN_FX(ec, FX_OVERDRIVE, IFX_Overdrive_Process(&ec->overdrive, buf, n));
    RUN_FX(ec, FX_EQ,        IFX_10BandEQ_Process(&ec->eq, buf, n));
    RUN_FX(ec, FX_CHORUS,    IFX_Chorus_Process(&ec->chorus, buf, n));
    time_stage(ec, buf, n);
    /* Master volume is NOT applied here — the audio callback applies a
     * smoothed version to prevent clicks on large rapid changes. */
    for (int i = 0; i < n; i++) {
        float sig = buf[i];
        if (sig >  1.0f) sig =  1.0f;
        if (sig < -1.0f) sig = -1.0f;
        buf[i] = sig;
    }
}

void EffectChain_ProcessBlock(EffectChain *ec, float *buf, int n) {
    EffectChain_ProcessBoost(ec, buf, n);
    EffectChain_ProcessRest(ec, buf, n);
}

void EffectChain_SetBoostLowCpu(EffectChain *ec, bool low) {
    IFX_BF_SetLowCpu(&ec->boost, low);
}

void EffectChain_Service(EffectChain *ec) {
    IFX_BF_Service(&ec->boost);
}

bool EffectChain_FormatValue(const EffectChain *ec, uint8_t fx, uint8_t pi,
                             char *buf, unsigned size) {
    if (fx == FX_DELAY && pi == DL_MODE) {
        snprintf(buf, size, "%s", IFX_Delay_ModeName((uint8_t)ec->params[FX_DELAY][DL_MODE].value));
        return true;
    }
    if ((fx == FX_DELAY && pi == DL_TRAILS) || (fx == FX_REVERB && pi == RV_TRAILS)) {
        snprintf(buf, size, "%s", ec->params[fx][pi].value > 0.5f ? "ON" : "OFF");
        return true;
    }
    if (fx == FX_REVERB && pi == RV_ROUTING) {
        static const char *const r[3] = { "DLY>VERB", "PARALLEL", "VERB>DLY" };
        uint8_t v = (uint8_t)ec->params[FX_REVERB][RV_ROUTING].value;
        snprintf(buf, size, "%s", v < 3 ? r[v] : "?");
        return true;
    }
    if (fx != FX_BOOST) return false;
    uint8_t model = (uint8_t)ec->params[FX_BOOST][BF_MODEL].value;
    if (pi == BF_MODEL) {
        /* trailing '~' = running at 1× because the CPU fallback kicked in */
        snprintf(buf, size, "%s%s", IFX_BF_ModelName(model), IFX_BF_IsLowCpu(&ec->boost) ? "~" : "");
        return true;
    }
    if (pi == BF_VOICE) {
        snprintf(buf, size, "%s", IFX_BF_VoiceName(model, (uint8_t)ec->params[FX_BOOST][BF_VOICE].value));
        return true;
    }
    return false;
}

void EffectChain_SetParam(EffectChain *ec, uint8_t fxIndex, uint8_t paramIndex, float value) {
    if (fxIndex >= FX_COUNT) return;
    if (paramIndex >= ec->paramCount[fxIndex]) return;

    ParamDesc *p = &ec->params[fxIndex][paramIndex];
    if (value < p->min) value = p->min;
    if (value > p->max) value = p->max;
    p->value = value;

    switch (fxIndex) {
    case FX_BOOST:
        switch (paramIndex) {
        case BF_MODEL: IFX_BF_SetModel(&ec->boost, (uint8_t)value); boost_model_changed(ec); break;
        case BF_VOICE: IFX_BF_SetVoice(&ec->boost, (uint8_t)value); break;
        case BF_DRIVE: IFX_BF_SetDrive(&ec->boost, value * 0.1f); break;
        case BF_TONE:  IFX_BF_SetTone(&ec->boost,  value * 0.1f); break;
        case BF_LEVEL: IFX_BF_SetLevelDb(&ec->boost, value); break;
        } break;

    case FX_NOISEGATE:
        switch (paramIndex) {
        case NG_THRESHOLD: IFX_NoiseGate_SetThreshold(&ec->noiseGate, value); break;
        case NG_ATTACK: case NG_RELEASE:
            IFX_NoiseGate_SetAttackRelease(&ec->noiseGate,
                ec->params[FX_NOISEGATE][NG_ATTACK].value,
                ec->params[FX_NOISEGATE][NG_RELEASE].value,
                SAMPLE_RATE_HZ); break;
        case NG_HOLD: IFX_NoiseGate_SetHoldTime(&ec->noiseGate, value); break;
        } break;

    case FX_OVERDRIVE:
        switch (paramIndex) {
        case OD_GAIN:      IFX_Overdrive_SetGain(&ec->overdrive, value); break;
        case OD_BOOST:     IFX_Overdrive_SetBoost(&ec->overdrive, value); break;
        case OD_HPF_CUTOFF:IFX_Overdrive_SetHPF(&ec->overdrive, value); break;
        case OD_LPF_CUTOFF: case OD_LPF_DAMP:
            IFX_Overdrive_SetLPF(&ec->overdrive,
                ec->params[FX_OVERDRIVE][OD_LPF_CUTOFF].value,
                ec->params[FX_OVERDRIVE][OD_LPF_DAMP].value); break;
        case OD_Q_CLIP:    IFX_Overdrive_SetQ(&ec->overdrive, value); break;
        } break;

    case FX_EQ:
        /* paramIndex == band index (0..9) */
        IFX_10BandEQ_SetBand(&ec->eq, paramIndex, value);
        break;

    case FX_CHORUS:
        switch (paramIndex) {
        case CH_DELAY_A: case CH_DELAY_B:
            IFX_Chorus_SetDelayTime(&ec->chorus,
                ec->params[FX_CHORUS][CH_DELAY_A].value,
                ec->params[FX_CHORUS][CH_DELAY_B].value); break;
        case CH_DEPTH_A: case CH_DEPTH_B:
            IFX_Chorus_SetDepth(&ec->chorus,
                ec->params[FX_CHORUS][CH_DEPTH_A].value,
                ec->params[FX_CHORUS][CH_DEPTH_B].value); break;
        case CH_RATE_A: case CH_RATE_B:
            IFX_Chorus_SetRate(&ec->chorus,
                ec->params[FX_CHORUS][CH_RATE_A].value,
                ec->params[FX_CHORUS][CH_RATE_B].value); break;
        case CH_GAIN_A: case CH_GAIN_B:
            IFX_Chorus_SetGain(&ec->chorus,
                ec->params[FX_CHORUS][CH_GAIN_A].value,
                ec->params[FX_CHORUS][CH_GAIN_B].value); break;
        case CH_MIX: IFX_Chorus_SetMix(&ec->chorus, value); break;
        } break;

    case FX_DELAY:
        switch (paramIndex) {
        case DL_TIME:     IFX_Delay_SetLength(&ec->delay, value, SAMPLE_RATE_HZ); break;
        case DL_MIX:      IFX_Delay_SetMix(&ec->delay, value); break;
        case DL_FEEDBACK: IFX_Delay_SetFeedback(&ec->delay, value); break;
        case DL_MODE:     IFX_Delay_SetMode(&ec->delay, (uint8_t)value); break;
        case DL_TONE:     IFX_Delay_SetTone(&ec->delay, value); break;
        case DL_MOD:      IFX_Delay_SetMod(&ec->delay, value); break;
        case DL_TRAILS:   break;   /* read by the audio core */
        } break;

    case FX_REVERB:
        switch (paramIndex) {
        case RV_MIX:      IFX_Reverb_SetMix(&ec->reverb, value); break;
        case RV_DECAY:    IFX_Reverb_SetDecay(&ec->reverb, value); break;
        case RV_TONE:     IFX_Reverb_SetDamping(&ec->reverb, value); break;
        case RV_PREDELAY: IFX_Reverb_SetPreDelay(&ec->reverb, value); break;
        default: break;   /* routing / trails: read by the audio core */
        } break;
    }
    PARAM_WRITE_BARRIER();
}

void EffectChain_SetMasterVolume(EffectChain *ec, float vol) {
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    ec->masterVolume = vol;
    PARAM_WRITE_BARRIER();
}

void EffectChain_ToggleEffect(EffectChain *ec, uint8_t fxIndex) {
    if (fxIndex < FX_COUNT) {
        ec->active[fxIndex] = !ec->active[fxIndex];
        PARAM_WRITE_BARRIER();
    }
}

void EffectChain_SetHold(EffectChain *ec, bool on) {
    ec->hold = on;
    PARAM_WRITE_BARRIER();
}
