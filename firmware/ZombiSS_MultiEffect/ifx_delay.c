#pragma GCC optimize ("O2")
#include "ifx_delay.h"
#include "ifx_mix.h"
#include "ifx_bf_math.h"   /* IFX_RAMFUNC */
#include <math.h>
#include <string.h>

#define N_LINE        IFX_DELAY_MAX_LINE_LENGTH
#define MOD_HOP       32          /* modulator update interval, samples */
#define WOW_HZ        0.9f
#define FLUTTER_HZ    7.0f
#define WOW_SAMPLES   40.0f       /* peak wow excursion at depth 1 (~±0.8 ms)   */
#define FLUT_SAMPLES  5.0f        /* peak flutter excursion at depth 1          */
#define FEED_RAMP     (1.0f / 480.0f)   /* 10 ms @ 48 kHz */
#define GLIDE_MAX     0.5f

/* ---- smooth gradient noise (1-D Perlin-style, hashed lattice) ----------- */
static inline float lattice_grad(int32_t i) {
    uint32_t h = (uint32_t)i & 4095u;          /* period 4096 → wrap is seamless */
    h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16;
    return (float)(h & 0xFFFFu) * (2.0f / 65535.0f) - 1.0f;
}

static inline float gnoise(float x) {
    float fl = floorf(x);
    int32_t i = (int32_t)fl;
    float f = x - fl;
    float u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);
    float a = lattice_grad(i) * f;
    float b = lattice_grad(i + 1) * (f - 1.0f);
    return 2.0f * (a + u * (b - a));            /* ≈ [-1, 1] */
}

static void mod_next(IFX_Delay *d) {
    d->wowX  += WOW_HZ     * (float)MOD_HOP / d->fs;
    d->flutX += FLUTTER_HZ * (float)MOD_HOP / d->fs;
    if (d->wowX  >= 4096.0f) d->wowX  -= 4096.0f;
    if (d->flutX >= 4096.0f) d->flutX -= 4096.0f;
    float wow  = (gnoise(d->wowX) + 0.5f * gnoise(2.0f * d->wowX + 17.3f)) * (1.0f / 1.5f);
    float flut = gnoise(d->flutX);
    float target = d->modDepth * (WOW_SAMPLES * wow + FLUT_SAMPLES * flut);
    d->modStep = (target - d->modCur) * (1.0f / (float)MOD_HOP);
}

/* ---- fractional read, `del` samples behind the write head (1 … N−2) ----- */
static inline float tap(const IFX_Delay *d, float del) {
    float rp = (float)d->w - del;
    if (rp < 0.0f) rp += (float)N_LINE;
    uint32_t i0 = (uint32_t)rp;
    float fr = rp - (float)i0;                 /* before the wrap guard! */
    if (i0 >= N_LINE) i0 -= N_LINE;            /* rp rounded up to exactly N */
    uint32_t i1 = i0 + 1u; if (i1 >= N_LINE) i1 = 0;
    float a = IFX_S16ToF(d->line[i0]);
    float b = IFX_S16ToF(d->line[i1]);
    return a + fr * (b - a);
}

static inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

/* One sample: returns the wet signal. */
static inline float dly_tick(IFX_Delay *d, float in) {
    if (d->modCount == 0) { mod_next(d); d->modCount = MOD_HOP; }
    d->modCount--;
    d->modCur += d->modStep;

    /* glide toward the new time; slew-limited so the head never runs faster
     * than ±GLIDE_MAX samples/sample (pitch bend ≤ ±50 %, never backwards) */
    float dt = d->timeCoef * (d->timeTarget - d->timeCur);
    if (dt >  GLIDE_MAX) dt =  GLIDE_MAX;
    if (dt < -GLIDE_MAX) dt = -GLIDE_MAX;
    d->timeCur += dt;

    float ft = d->freeze ? 0.0f : d->feedTarget;
    if      (d->feed < ft - FEED_RAMP) d->feed += FEED_RAMP;
    else if (d->feed > ft + FEED_RAMP) d->feed -= FEED_RAMP;
    else                               d->feed  = ft;

    float T = d->timeCur;
    float rd;
    if (d->mode == IFX_DLY_NORMAL) {
        rd = tap(d, clampf(T + d->modCur, 1.0f, (float)N_LINE - 2.0f));
    } else {
        /* Two grains, half a period apart, sin² windows (they sum to 1). */
        float ph = d->headPh, phB = ph + 0.5f;
        if (phB >= 1.0f) phB -= 1.0f;
        float s = IFX_SinPi01(ph);
        float wA = s * s;
        float W, dA, dB;
        if (d->mode == IFX_DLY_OCTAVE) {
            /* head moves at 2× speed: delay shrinks 1 sample per sample */
            W = IFX_DELAY_OCT_WINDOW;
            float base = clampf(T + d->modCur, 1.0f, (float)N_LINE - W - 4.0f);
            dA = base + W * (1.0f - ph);
            dB = base + W * (1.0f - phB);
        } else {
            /* reverse: head runs backwards, delay grows 2 samples per sample;
             * each grain plays the last W samples backwards (W = Time) */
            W = clampf(T, 64.0f, (float)(N_LINE - 4u) * 0.5f);
            dA = 1.0f + 2.0f * W * ph;
            dB = 1.0f + 2.0f * W * phB;
        }
        rd = wA * tap(d, dA) + (1.0f - wA) * tap(d, dB);
        ph += 1.0f / W;
        if (ph >= 1.0f) ph -= 1.0f;
        d->headPh = ph;
    }

    float wet;
    float fb;
    if (d->freeze) { wet = rd; fb = 1.0f; d->toneZ = rd; }   /* lossless loop */
    else {
        d->toneZ += d->toneCoef * (rd - d->toneZ);
        wet = d->toneZ;
        fb = d->feedback;
    }

    d->line[d->w] = IFX_FToS16(d->feed * in + fb * wet);
    if (++d->w >= N_LINE) d->w = 0;
    return wet;
}

void IFX_Delay_Init(IFX_Delay *dly, float delayTime_ms, float mix, float feedback, float sampleRate_Hz) {
    memset(dly, 0, sizeof(*dly));
    dly->fs = sampleRate_Hz;
    dly->timeCoef = 1.0f - expf(-1.0f / (0.08f * sampleRate_Hz));   /* 80 ms glide */
    IFX_Delay_SetLength(dly, delayTime_ms, sampleRate_Hz);
    dly->timeCur = dly->timeTarget;
    IFX_Delay_SetMix(dly, mix);
    IFX_Delay_SetFeedback(dly, feedback);
    IFX_Delay_SetTone(dly, 8000.0f);
    dly->feed = dly->feedTarget = 1.0f;
    dly->wowX = 3.7f; dly->flutX = 11.1f;
}

void IFX_Delay_Clear(IFX_Delay *dly) {
    memset(dly->line, 0, sizeof(dly->line));
    dly->toneZ = 0.0f;
}

void IFX_RAMFUNC(IFX_Delay_ProcessWet)(IFX_Delay *dly, const float *in, float *wet, int n) {
    for (int i = 0; i < n; i++) wet[i] = dly_tick(dly, in[i]);
}

float IFX_RAMFUNC(IFX_Delay_Update)(IFX_Delay *dly, float inp) {
    float w = dly_tick(dly, inp);
    float out = IFX_MixDry(dly->mix) * inp + IFX_MixWet(dly->mix) * w;
    if (out > 1.0f) out = 1.0f;
    else if (out < -1.0f) out = -1.0f;
    dly->out = out;
    return out;
}

void IFX_RAMFUNC(IFX_Delay_Process)(IFX_Delay *dly, float *buf, int n) {
    for (int i = 0; i < n; i++) buf[i] = IFX_Delay_Update(dly, buf[i]);
}

void IFX_Delay_SetLength(IFX_Delay *dly, float delayTime_ms, float sampleRate_Hz) {
    float t = 0.001f * delayTime_ms * sampleRate_Hz;
    dly->timeTarget = clampf(t, 1.0f, (float)N_LINE - 2.0f);
}

void IFX_Delay_SetMix(IFX_Delay *dly, float mix)          { dly->mix = clampf(mix, 0.0f, 1.0f); }
void IFX_Delay_SetFeedback(IFX_Delay *dly, float fb)      { dly->feedback = clampf(fb, 0.0f, 0.98f); }
void IFX_Delay_SetMod(IFX_Delay *dly, float depth01)      { dly->modDepth = clampf(depth01, 0.0f, 1.0f); }
void IFX_Delay_SetFeed(IFX_Delay *dly, float target01)    { dly->feedTarget = clampf(target01, 0.0f, 1.0f); }
void IFX_Delay_SetFreeze(IFX_Delay *dly, uint8_t on)      { dly->freeze = on ? 1 : 0; }

void IFX_Delay_SetMode(IFX_Delay *dly, uint8_t mode) {
    if (mode >= IFX_DLY_MODE_COUNT) mode = IFX_DLY_NORMAL;
    dly->mode = mode;
    dly->headPh = 0.0f;
}

void IFX_Delay_SetTone(IFX_Delay *dly, float cutoff_Hz) {
    float fc = clampf(cutoff_Hz, 200.0f, 0.45f * dly->fs);
    dly->toneCoef = 1.0f - expf(-6.28318531f * fc / dly->fs);
}

const char *IFX_Delay_ModeName(uint8_t mode) {
    static const char *const names[IFX_DLY_MODE_COUNT] = { "NORMAL", "OCTAVE", "REVERSE" };
    return mode < IFX_DLY_MODE_COUNT ? names[mode] : "?";
}
