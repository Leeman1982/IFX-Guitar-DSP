#ifndef IFX_TSBOOST_H
#define IFX_TSBOOST_H

/*
 * Tube Screamer (TS-808/TS-9) style boost
 *
 * Signal path:
 *   Input → 1st-order HPF (~720 Hz) → tanh soft-clip → 1st-order LPF tone → Level → Output
 *
 * The HPF before clipping is the key TS character: it removes bass so only
 * mids/highs are clipped, producing the signature "mid hump" overdrive sound.
 * tanh() closely approximates the dual Si-diode clipper in the real pedal.
 *
 * Parameters:
 *   Drive  1–100  (dimensionless gain into tanh)
 *   Tone   0.0–1.0  (maps HPF cutoff: 0→500Hz, 1→5000Hz)
 *   Level  0.0–2.0  (output volume)
 */

#include <math.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float T;

    /* 1st-order bilinear HPF (removes bass before clipping) */
    float hpf_b0, hpf_b1, hpf_a1;
    float hpf_x1, hpf_y1;

    /* Drive into tanh */
    float drive;

    /* 1st-order bilinear LPF (tone control) */
    float lpf_b0, lpf_b1, lpf_a1;
    float lpf_x1, lpf_y1;

    /* Output level */
    float level;

    float out;
} IFX_TSBoost;

void  IFX_TSBoost_Init(IFX_TSBoost *ts, float sampleRate_Hz);
void  IFX_TSBoost_SetDrive(IFX_TSBoost *ts, float drive);
void  IFX_TSBoost_SetTone(IFX_TSBoost *ts, float tone_0to1);
void  IFX_TSBoost_SetLevel(IFX_TSBoost *ts, float level);
float IFX_TSBoost_Update(IFX_TSBoost *ts, float inp);

#ifdef __cplusplus
}
#endif

#endif /* IFX_TSBOOST_H */
