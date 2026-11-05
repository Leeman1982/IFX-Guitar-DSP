#ifndef IFX_MIX_H
#define IFX_MIX_H

/*
 * Pedal-style dry/wet law for the time effects (delay, reverb):
 *   mix 0.0 → dry 1, wet 0
 *   mix 0.5 → dry 1, wet 1      (dry never ducks below unity up to here)
 *   mix 1.0 → dry 0, wet 1
 * The old law, (1−mix)·dry + mix·wet, pulled the guitar down as soon as any
 * delay was dialled in.
 */
static inline float IFX_MixDry(float mix) { float d = 2.0f * (1.0f - mix); return d > 1.0f ? 1.0f : d; }
static inline float IFX_MixWet(float mix) { float w = 2.0f * mix;          return w > 1.0f ? 1.0f : w; }

/* sin(π·ph) for ph in [0,1]  (even polynomial in x = π·ph − π/2, |err| < 3e-5) */
static inline float IFX_SinPi01(float ph) {
    float x = ph * 3.14159265f - 1.57079633f;
    float x2 = x * x;
    return 1.0f + x2 * (-0.5f + x2 * (0.041666667f + x2 * (-0.0013888889f + x2 * 0.0000248016f)));
}

/* sin(2π·ph) for ph in [0,1)  (7th-order odd polynomial, |err| < 1e-4) */
static inline float IFX_Sin2Pi01(float ph) {
    float x = ph * 6.28318531f - 3.14159265f;
    if (x >  1.57079633f) x =  3.14159265f - x;
    if (x < -1.57079633f) x = -3.14159265f - x;
    float x2 = x * x;
    return -x * (1.0f + x2 * (-0.16666667f + x2 * (0.0083333f - x2 * 0.00019841f)));
}

static inline float IFX_S16ToF(int16_t s) { return (float)s * (1.0f / 32768.0f); }
/* Truncation toward zero (C cast), not rounding: in a lossy feedback loop
 * rounding leaves dead-band limit cycles (|x| ≤ 0.5/(1−g) LSB never decays —
 * 25 LSB at g = 0.98); magnitude truncation always decays to exact zero. */
#if defined(__ARM_FEATURE_SAT) && __ARM_FEATURE_SAT
#include <arm_acle.h>
/* Cortex-M33/M4/M7: VCVT (truncates, saturates to int32) + SSAT, no branches */
static inline int16_t IFX_FToS16(float x) {
    return (int16_t)__ssat((int32_t)(x * 32768.0f), 16);
}
#else
static inline int16_t IFX_FToS16(float x) {
    if (x >  0.99997f) x =  0.99997f;
    if (x < -0.99997f) x = -0.99997f;
    return (int16_t)(x * 32768.0f);
}
#endif

#endif
