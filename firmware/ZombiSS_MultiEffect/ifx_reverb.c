#pragma GCC optimize ("O2")
#include "ifx_reverb.h"
#include "ifx_mix.h"
#include "ifx_bf_math.h"   /* IFX_RAMFUNC */
#include <math.h>
#include <string.h>

/* Dattorro's lengths are given at 29761 Hz. */
#define DAT_FS        29761.0f
#define FEED_RAMP     (1.0f / 480.0f)
#define IN_SCALE      0.5f      /* tank headroom: int16 lines saturate at ±1 */
#define OUT_SCALE     (0.3f / IN_SCALE)

static uint32_t sc(float k, float n) { return (uint32_t)(n * k + 0.5f); }

/* ---- int16 line helpers -------------------------------------------------- */
static inline float rv_oldest(const IFX_Reverb *rv, const IFX_RvLine *l) {
    return IFX_S16ToF(rv->pool[l->off + l->idx]);
}
/* sample written d samples ago (1 ≤ d ≤ len) */
static inline float rv_rd(const IFX_Reverb *rv, const IFX_RvLine *l, uint32_t d) {
    int32_t i = (int32_t)l->idx - (int32_t)d;
    if (i < 0) i += (int32_t)l->len;
    return IFX_S16ToF(rv->pool[l->off + (uint32_t)i]);
}
static inline float rv_rd_frac(const IFX_Reverb *rv, const IFX_RvLine *l, float d) {
    float rp = (float)l->idx - d;
    if (rp < 0.0f) rp += (float)l->len;
    uint32_t i0 = (uint32_t)rp;
    float fr = rp - (float)i0;                 /* before the wrap guard! */
    if (i0 >= l->len) i0 -= l->len;
    uint32_t i1 = i0 + 1u; if (i1 >= l->len) i1 = 0;
    float a = IFX_S16ToF(rv->pool[l->off + i0]);
    float b = IFX_S16ToF(rv->pool[l->off + i1]);
    return a + fr * (b - a);
}
static inline void rv_wr(IFX_Reverb *rv, IFX_RvLine *l, float x) {
    rv->pool[l->off + l->idx] = IFX_FToS16(x);
    if (++l->idx >= l->len) l->idx = 0;
}
/* Schroeder allpass, delay = len */
static inline float rv_ap(IFX_Reverb *rv, IFX_RvLine *l, float x, float g) {
    float d = rv_oldest(rv, l);
    float v = x - g * d;
    rv_wr(rv, l, v);
    return d + g * v;
}
/* allpass with a modulated, fractional delay */
static inline float rv_ap_mod(IFX_Reverb *rv, IFX_RvLine *l, float x, float g, float delay) {
    float d = rv_rd_frac(rv, l, delay);
    float v = x - g * d;
    rv_wr(rv, l, v);
    return d + g * v;
}

static void assign(IFX_RvLine *l, uint32_t *cursor, uint32_t len) {
    l->off = *cursor; l->len = len; l->idx = 0;
    *cursor += len;
}

static uint32_t pool_needed(float k, float fsTank) {
    static const float n[12] = { 142, 107, 379, 277, 672, 4453, 1800, 3720, 908, 4217, 2656, 3163 };
    uint32_t c = (uint32_t)(IFX_REVERB_MAX_PRE_MS * 0.001f * fsTank) + 2u;
    for (int i = 0; i < 12; i++) c += sc(k, n[i]);
    return c + 2u * ((uint32_t)(16.0f * k) + 3u);
}

void IFX_Reverb_Init(IFX_Reverb *rv, float sampleRate_Hz) {
    memset(rv, 0, sizeof(*rv));
#if IFX_REVERB_HALF_RATE
    rv->fs = 0.5f * sampleRate_Hz;              /* the tank's own rate */
    bf_halfband_init(&rv->dn, 0);
    bf_halfband_init(&rv->up, 0);
#else
    rv->fs = sampleRate_Hz;
#endif
    float fsTank = rv->fs > 48000.0f ? 48000.0f : rv->fs;
    float k = fsTank / DAT_FS;
    /* if the configured pool is too small, shrink the tank until it fits */
    while (pool_needed(k, fsTank) > IFX_REVERB_POOL && k > 0.1f) k *= 0.97f;
    rv->tankScale = k;

    rv->excursion = 16.0f * k;
    uint32_t ex = (uint32_t)rv->excursion + 3u;
    uint32_t c = 0;
    assign(&rv->pre, &c, (uint32_t)(IFX_REVERB_MAX_PRE_MS * 0.001f * fsTank) + 2u);
    assign(&rv->in1, &c, sc(k, 142));
    assign(&rv->in2, &c, sc(k, 107));
    assign(&rv->in3, &c, sc(k, 379));
    assign(&rv->in4, &c, sc(k, 277));
    rv->apL1Nom = 672.0f * k;  assign(&rv->apL1, &c, sc(k, 672) + ex);
    assign(&rv->dlL1, &c, sc(k, 4453));
    assign(&rv->apL2, &c, sc(k, 1800));
    assign(&rv->dlL2, &c, sc(k, 3720));
    rv->apR1Nom = 908.0f * k;  assign(&rv->apR1, &c, sc(k, 908) + ex);
    assign(&rv->dlR1, &c, sc(k, 4217));
    assign(&rv->apR2, &c, sc(k, 2656));
    assign(&rv->dlR2, &c, sc(k, 3163));
    /* c ≤ IFX_REVERB_POOL is guaranteed by the pool_needed() loop above */

    static const float tl[7] = { 266, 2974, 1913, 1996, 1990, 187, 1066 };
    static const float tr[7] = { 353, 3627, 1228, 2673, 2111, 335, 121 };
    for (int i = 0; i < 7; i++) { rv->tapL[i] = sc(k, tl[i]); rv->tapR[i] = sc(k, tr[i]); }

    rv->lfoInc = 1.0f / rv->fs;                 /* 1 Hz tank modulation */
    rv->bandwidth = 0.9995f;
    rv->feed = rv->feedTarget = 1.0f;
    IFX_Reverb_SetDecay(rv, 0.5f);
    IFX_Reverb_SetDamping(rv, 6000.0f);
    IFX_Reverb_SetPreDelay(rv, 10.0f);
    IFX_Reverb_SetMix(rv, 0.3f);
}

void IFX_Reverb_Clear(IFX_Reverb *rv) {
    memset(rv->pool, 0, sizeof(rv->pool));
    rv->bwZ = rv->dampL = rv->dampR = 0.0f;
}

static inline float rv_tick(IFX_Reverb *rv, float in) {
    float ft = rv->freeze ? 0.0f : rv->feedTarget;
    if      (rv->feed < ft - FEED_RAMP) rv->feed += FEED_RAMP;
    else if (rv->feed > ft + FEED_RAMP) rv->feed -= FEED_RAMP;
    else                                rv->feed  = ft;

    const float decay = rv->freeze ? 1.0f : rv->decay;
    const float damp  = rv->freeze ? 0.0f : rv->damping;
    float dd2 = decay + 0.15f;
    if (dd2 < 0.25f) dd2 = 0.25f;
    if (dd2 > 0.50f) dd2 = 0.50f;

    /* predelay + input bandwidth filter + input diffusion */
    float x = rv->feed * in * IN_SCALE;
    uint32_t P = (uint32_t)rv->preDelay;
    float y;
    if (P == 0) { y = x; rv_wr(rv, &rv->pre, x); }
    else        { y = rv_rd(rv, &rv->pre, P); rv_wr(rv, &rv->pre, x); }
    rv->bwZ += rv->bandwidth * (y - rv->bwZ);
    float d = rv_ap(rv, &rv->in1, rv->bwZ, 0.75f);
    d = rv_ap(rv, &rv->in2, d, 0.75f);
    d = rv_ap(rv, &rv->in3, d, 0.625f);
    d = rv_ap(rv, &rv->in4, d, 0.625f);

    /* tank LFOs in quadrature */
    float ph = rv->lfoPh + rv->lfoInc; if (ph >= 1.0f) ph -= 1.0f; rv->lfoPh = ph;
    float phQ = ph + 0.25f; if (phQ >= 1.0f) phQ -= 1.0f;
    float sA = IFX_Sin2Pi01(ph), sB = IFX_Sin2Pi01(phQ);

    float fromR = rv_oldest(rv, &rv->dlR2);   /* tank outputs of the previous sample */
    float fromL = rv_oldest(rv, &rv->dlL2);

    /* left half */
    float a = d + decay * fromR;
    a = rv_ap_mod(rv, &rv->apL1, a, -0.70f, rv->apL1Nom + rv->excursion * sA);
    float t = rv_oldest(rv, &rv->dlL1); rv_wr(rv, &rv->dlL1, a);
    rv->dampL = t + damp * (rv->dampL - t);
    t = rv_ap(rv, &rv->apL2, rv->dampL * decay, dd2);
    rv_wr(rv, &rv->dlL2, t);

    /* right half */
    float b = d + decay * fromL;
    b = rv_ap_mod(rv, &rv->apR1, b, -0.70f, rv->apR1Nom + rv->excursion * sB);
    t = rv_oldest(rv, &rv->dlR1); rv_wr(rv, &rv->dlR1, b);
    rv->dampR = t + damp * (rv->dampR - t);
    t = rv_ap(rv, &rv->apR2, rv->dampR * decay, dd2);
    rv_wr(rv, &rv->dlR2, t);

    /* output taps (Dattorro table 2), L and R summed to mono */
    const uint32_t *L = rv->tapL, *R = rv->tapR;
    float yl =  rv_rd(rv, &rv->dlR1, L[0]) + rv_rd(rv, &rv->dlR1, L[1])
              - rv_rd(rv, &rv->apR2, L[2]) + rv_rd(rv, &rv->dlR2, L[3])
              - rv_rd(rv, &rv->dlL1, L[4]) - rv_rd(rv, &rv->apL2, L[5])
              - rv_rd(rv, &rv->dlL2, L[6]);
    float yr =  rv_rd(rv, &rv->dlL1, R[0]) + rv_rd(rv, &rv->dlL1, R[1])
              - rv_rd(rv, &rv->apL2, R[2]) + rv_rd(rv, &rv->dlL2, R[3])
              - rv_rd(rv, &rv->dlR1, R[4]) - rv_rd(rv, &rv->apR2, R[5])
              - rv_rd(rv, &rv->dlR2, R[6]);
    return OUT_SCALE * (yl + yr);
}

#if IFX_REVERB_HALF_RATE
/* One fs-rate sample in, one out; the tank ticks on every second sample.
 * Latency: one output pair (2 samples) plus the half-band group delay. */
static inline float rv_io(IFX_Reverb *rv, float x) {
    float y;
    if (rv->phase == 0) {
        rv->pairIn[0] = x;
        y = rv->pairOut[0];
        rv->phase = 1;
    } else {
        rv->pairIn[1] = x;
        y = rv->pairOut[1];
        float t = rv_tick(rv, bf_hb_down(&rv->dn, rv->pairIn));
        bf_hb_up(&rv->up, t, rv->pairOut);
        rv->phase = 0;
    }
    return y;
}
#else
#define rv_io rv_tick
#endif

void IFX_RAMFUNC(IFX_Reverb_ProcessWet)(IFX_Reverb *rv, const float *in, float *wet, int n) {
    for (int i = 0; i < n; i++) wet[i] = rv_io(rv, in[i]);
}

void IFX_RAMFUNC(IFX_Reverb_Process)(IFX_Reverb *rv, float *buf, int n) {
    const float dg = IFX_MixDry(rv->mix), wg = IFX_MixWet(rv->mix);
    for (int i = 0; i < n; i++) {
        float x = buf[i];
        float o = dg * x + wg * rv_io(rv, x);
        if (o > 1.0f) o = 1.0f; else if (o < -1.0f) o = -1.0f;
        buf[i] = o;
    }
    rv->out = buf[n - 1];
}

static inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

void IFX_Reverb_SetDecay(IFX_Reverb *rv, float decay01) { rv->decay = clampf(decay01, 0.0f, 0.98f); }
void IFX_Reverb_SetMix(IFX_Reverb *rv, float mix)       { rv->mix = clampf(mix, 0.0f, 1.0f); }
void IFX_Reverb_SetFeed(IFX_Reverb *rv, float t)        { rv->feedTarget = clampf(t, 0.0f, 1.0f); }
void IFX_Reverb_SetFreeze(IFX_Reverb *rv, uint8_t on)   { rv->freeze = on ? 1 : 0; }

void IFX_Reverb_SetDamping(IFX_Reverb *rv, float cutoff_Hz) {
    float fc = clampf(cutoff_Hz, 500.0f, 0.45f * rv->fs);
    rv->dampHz = fc;
    rv->damping = expf(-6.28318531f * fc / rv->fs);
}

void IFX_Reverb_SetPreDelay(IFX_Reverb *rv, float ms) {
    float p = clampf(ms, 0.0f, IFX_REVERB_MAX_PRE_MS) * 0.001f * rv->fs;
    float maxP = (float)(rv->pre.len - 1u);
    rv->preDelay = p > maxP ? maxP : p;
}
