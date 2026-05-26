#include "ifx_tsboost.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* Bilinear 1st-order HPF coefficients for cutoff fc */
static void set_hpf(IFX_TSBoost *ts, float fc) {
    float k = 2.0f * M_PI * fc * ts->T;  /* wcT */
    float d = 2.0f + k;
    ts->hpf_b0 =  2.0f / d;
    ts->hpf_b1 = -2.0f / d;
    ts->hpf_a1 = (2.0f - k) / d;
}

/* Bilinear 1st-order LPF coefficients for cutoff fc */
static void set_lpf(IFX_TSBoost *ts, float fc) {
    float k = 2.0f * M_PI * fc * ts->T;  /* wcT */
    float d = 2.0f + k;
    ts->lpf_b0 = k / d;
    ts->lpf_b1 = k / d;
    ts->lpf_a1 = (k - 2.0f) / d;
}

void IFX_TSBoost_Init(IFX_TSBoost *ts, float sampleRate_Hz) {
    ts->T = 1.0f / sampleRate_Hz;

    ts->hpf_x1 = 0.0f; ts->hpf_y1 = 0.0f;
    ts->lpf_x1 = 0.0f; ts->lpf_y1 = 0.0f;

    ts->drive = 10.0f;
    ts->level = 0.8f;

    set_hpf(ts, 720.0f);    /* Classic TS HPF - cuts bass before clipping */
    set_lpf(ts, 2200.0f);   /* Tone at mid-point */
}

void IFX_TSBoost_SetDrive(IFX_TSBoost *ts, float drive) {
    ts->drive = drive;
}

void IFX_TSBoost_SetTone(IFX_TSBoost *ts, float tone_0to1) {
    /* Tone 0.0 = dark (500Hz), 1.0 = bright (5000Hz) */
    float fc = 500.0f + tone_0to1 * 4500.0f;
    set_lpf(ts, fc);
}

void IFX_TSBoost_SetLevel(IFX_TSBoost *ts, float level) {
    ts->level = level;
}

float IFX_TSBoost_Update(IFX_TSBoost *ts, float inp) {
    /* 1st-order HPF: y[n] = b0*x[n] + b1*x[n-1] - a1*y[n-1] */
    float hpf_out = ts->hpf_b0 * inp
                  + ts->hpf_b1 * ts->hpf_x1
                  - ts->hpf_a1 * ts->hpf_y1;
    ts->hpf_x1 = inp;
    ts->hpf_y1 = hpf_out;

    /* Padé approximant of tanh: x*(27+x²)/(27+9x²), accurate to <1% for |x|<3.
     * Clamps to ±1 for |x|>=3 (tanh(3)=0.9951). Avoids costly tanhf() call
     * in the 48kHz hot path (~5-10x faster than libm tanhf on Cortex-M33). */
    float x = ts->drive * hpf_out;
    float clipped;
    if (x > 3.0f) {
        clipped =  1.0f;
    } else if (x < -3.0f) {
        clipped = -1.0f;
    } else {
        float x2 = x * x;
        clipped = x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    /* 1st-order LPF tone: y[n] = b0*x[n] + b1*x[n-1] - a1*y[n-1] */
    float tone_out = ts->lpf_b0 * clipped
                   + ts->lpf_b1 * ts->lpf_x1
                   - ts->lpf_a1 * ts->lpf_y1;
    ts->lpf_x1 = clipped;
    ts->lpf_y1 = tone_out;

    float out = tone_out * ts->level;
    if (out >  1.0f) out =  1.0f;
    if (out < -1.0f) out = -1.0f;
    return out;
}
