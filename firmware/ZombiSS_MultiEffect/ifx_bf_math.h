#ifndef IFX_BF_MATH_H
#define IFX_BF_MATH_H

/*
 * Boost Factory — fast math + oversampling primitives.
 *
 * Everything here is header-inline so the per-sample circuit solvers inline
 * it.  Accuracy targets were checked on the host against libm / scipy:
 *   bf_exp   : rel. error < 3e-7  over [-87, 88]
 *   bf_log   : abs. error < 2e-7  for x > 0 (normal floats)
 *   bf_omega : rel. error < 1e-7  (Wright omega, one Halley step)
 */

#include <stdint.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Hot DSP code runs from SRAM: XIP flash cache misses on a 128 KB+ image
 * would otherwise stall the audio IRQ unpredictably. */
#if defined(ARDUINO_ARCH_RP2040)
#define IFX_RAMFUNC(fn) __attribute__((noinline, section(".time_critical." #fn))) fn
#else
#define IFX_RAMFUNC(fn) fn
#endif

typedef union { float f; int32_t i; uint32_t u; } bf_f32;

/* e^x via 2^(x·log2e): integer part into the exponent, fraction by a
 * degree-5 minimax polynomial on [0,1). */
static inline float bf_exp(float x) {
    if (x > 88.0f)  x = 88.0f;
    if (x < -87.0f) x = -87.0f;
    float t  = x * 1.44269504089f;
    float fl = floorf(t);
    float f  = t - fl;
    float p = 1.8777142e-3f;
    p = p * f + 8.9894800e-3f;
    p = p * f + 5.5825690e-2f;
    p = p * f + 2.4015407e-1f;
    p = p * f + 6.9315296e-1f;
    p = p * f + 9.9999994e-1f;
    bf_f32 r; r.f = p;
    r.i += ((int32_t)fl) << 23;
    return r.f;
}

/* ln(x) for x > 0: exponent split, mantissa mapped to [sqrt(.5), sqrt(2))
 * and an atanh-series in s = (m-1)/(m+1). */
static inline float bf_log(float x) {
    bf_f32 v; v.f = x;
    int32_t e = ((v.i >> 23) & 0xFF) - 127;
    v.i = (v.i & 0x007FFFFF) | 0x3F800000;           /* m in [1,2) */
    float m = v.f;
    if (m > 1.41421356f) { m *= 0.5f; e += 1; }
    float s  = (m - 1.0f) / (m + 1.0f);
    float s2 = s * s;
    float p = 0.29496524f;
    p = p * s2 + 0.3999063f;
    p = p * s2 + 0.6666668f;
    p = p * s2 + 2.0f;
    return (float)e * 0.69314718f + s * p;
}

/* Wright omega ω(x): the solution w of  w + ln(w) = x  (= LambertW(e^x)).
 * Piecewise initial guess + one Halley step on f(w) = w − e^(x−w). */
static inline float bf_omega(float x) {
    float w;
    if (x < -3.0f) {
        float e = bf_exp(x);
        w = e * (1.0f - e);
    } else if (x < 8.0f) {
        w = 5.15217556e-05f;
        w = w * x - 6.95930590e-04f;
        w = w * x - 9.52220946e-04f;
        w = w * x + 6.82535682e-02f;
        w = w * x + 3.62445890e-01f;
        w = w * x + 5.72270223e-01f;
    } else {
        float l = bf_log(x);
        w = x - l + l / x;
    }
    float E  = bf_exp(x - w);
    float f  = w - E;
    float f1 = 1.0f + E;
    w -= 2.0f * f * f1 / (2.0f * f1 * f1 + f * E);
    return w;
}

/* ---------------------------------------------------------------------------
 * 2× polyphase IIR half-band (Valenzuela–Constantinides allpass pair).
 * Stage A (48k↔96k): 5 coefs, transition 0.0834 → −80.7 dB stop band,
 *                    flat to 20 kHz.
 * Stage B (96k↔192k): 2 coefs, transition 0.29  → −70.6 dB stop band.
 * Coefficients computed and verified offline (see BOOST_FACTORY.md).
 * ------------------------------------------------------------------------ */
/* Branch layout: stage A = 3 + 2 allpass sections, stage B = 1 + 1. */
typedef struct {
    float c[5];                 /* A: c0,c2,c4 → branch 0; c1,c3 → branch 1 */
    float x[5], y[5];           /* one-sample history per section            */
    uint8_t stageB;
} bf_halfband;

void bf_halfband_init(bf_halfband *h, int stageB);

/* First-order allpass section in the decimated domain:
 *   y[n] = c·(x[n] − y[n−1]) + x[n−1]                                     */
#define BF_AP(h, k, in) ({ float _o = (h)->c[k] * ((in) - (h)->y[k]) + (h)->x[k]; \
                           (h)->x[k] = (in); (h)->y[k] = _o; _o; })

static inline float bf_hb_branch0(bf_halfband *h, float in) {
    float s = BF_AP(h, 0, in);
    if (h->stageB) return s;
    s = BF_AP(h, 2, s);
    return BF_AP(h, 4, s);
}
static inline float bf_hb_branch1(bf_halfband *h, float in) {
    float s = BF_AP(h, 1, in);
    if (h->stageB) return s;
    return BF_AP(h, 3, s);
}

/* Upsample: one input → two outputs (o[0] earlier, o[1] later). */
static inline void bf_hb_up(bf_halfband *h, float in, float *o) {
    o[0] = bf_hb_branch0(h, in);
    o[1] = bf_hb_branch1(h, in);
}

/* Downsample: two inputs (i[0] earlier) → one output.  Evaluates the odd
 * output phase of H(z) = ½[A0(z²) + z⁻¹·A1(z²)], so no extra delay is needed. */
static inline float bf_hb_down(bf_halfband *h, const float *i) {
    return 0.5f * (bf_hb_branch0(h, i[1]) + bf_hb_branch1(h, i[0]));
}

#ifdef __cplusplus
}
#endif

#endif /* IFX_BF_MATH_H */
