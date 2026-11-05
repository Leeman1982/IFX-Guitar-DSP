/* Host tests for the ZombiSS time effects + effect-chain switching.
 *   ./run.sh            (gcc, no other deps)
 * Every check prints PASS/FAIL; exit status = number of failures. */
#include "effect_chain.h"
#include "ifx_mix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define FS 48000
static int fails = 0;
static void check(int ok, const char *what, double v) {
    printf("%s  %-58s %g\n", ok ? "PASS" : "FAIL", what, v);
    if (!ok) fails++;
}

static EffectChain ec;
static IFX_Delay dl;
static IFX_Reverb rv;

static void chain(float *x, int n) { for (int i = 0; i < n; i += 64) EffectChain_ProcessBlock(&ec, x + i, (n - i) < 64 ? (n - i) : 64); }
static double maxstep(const float *x, int n) { double m = 0; for (int i = 1; i < n; i++) { double d = fabs(x[i] - x[i-1]); if (d > m) m = d; } return m; }
static double rms(const float *x, int n) { double a = 0; for (int i = 0; i < n; i++) a += (double)x[i] * x[i]; return sqrt(a / n); }
static int finite_all(const float *x, int n) { for (int i = 0; i < n; i++) if (!isfinite(x[i])) return 0; return 1; }
/* frequency from positive-going zero crossings */
static double zc_freq(const float *x, int n) {
    int c = 0, first = -1, last = -1;
    for (int i = 1; i < n; i++) if (x[i-1] < 0 && x[i] >= 0) { if (first < 0) first = i; last = i; c++; }
    return c > 1 ? (double)(c - 1) * FS / (last - first) : 0;
}

static float big[FS * 12];

int main(void) {
    /* ---------------- memory ---------------- */
    IFX_Reverb_Init(&rv, FS);
    uint32_t used = rv.dlR2.off + rv.dlR2.len;
    printf("sizeof EffectChain %zu B  (delay %zu, reverb %zu; reverb pool used %u / %u)\n",
           sizeof(EffectChain), sizeof(IFX_Delay), sizeof(IFX_Reverb), used, IFX_REVERB_POOL);
    check(used <= IFX_REVERB_POOL, "reverb lines fit the int16 pool", used);
    check(sizeof(EffectChain) < 300000, "EffectChain < 300 KB (RP2350 has 520 KB SRAM)", (double)sizeof(EffectChain));

    /* ---------------- delay: impulse, normal mode ---------------- */
    IFX_Delay_Init(&dl, 300.0f, 0.5f, 0.5f, FS);
    IFX_Delay_SetTone(&dl, 20000.0f); IFX_Delay_SetMod(&dl, 0.0f);
    int n = FS;
    memset(big, 0, sizeof(float) * n); big[0] = 0.5f;
    IFX_Delay_ProcessWet(&dl, big, big, n);
    int pk = 0; for (int i = 1; i < 20000; i++) if (fabsf(big[i]) > fabsf(big[pk])) pk = i;
    check(abs(pk - 14400) <= 1, "first repeat at 300 ms (sample 14400)", pk);
    double e1 = rms(big + 14390, 20), e2 = rms(big + 28790, 20);
    check(fabs(e2 / e1 - 0.5) < 0.05, "second repeat = feedback x first (energy ratio)", e2 / e1);

    /* ---------------- delay: time change glides (no jump) ---------------- */
    IFX_Delay_Init(&dl, 200.0f, 0.5f, 0.0f, FS); IFX_Delay_SetMod(&dl, 0.0f);
    n = FS * 2;
    for (int i = 0; i < n; i++) big[i] = 0.5f * sinf(2 * M_PI * 220 * i / FS);
    double inStep = maxstep(big, n);
    IFX_Delay_ProcessWet(&dl, big, big, FS);
    IFX_Delay_SetLength(&dl, 900.0f, FS);                        /* big knob jump */
    IFX_Delay_ProcessWet(&dl, big + FS, big + FS, FS);
    double st = maxstep(big + FS - 10, FS + 10);
    check(st < 1.6 * inStep, "time 200->900 ms: glide, no discontinuity (step vs input)", st / inStep);
    IFX_Delay_SetLength(&dl, 150.0f, FS);                        /* and back down */
    for (int i = 0; i < FS; i++) big[i] = 0.5f * sinf(2 * M_PI * 220 * (i + 2 * FS) / FS);
    IFX_Delay_ProcessWet(&dl, big, big, FS);
    st = maxstep(big, FS);
    check(st < 1.6 * inStep, "time 900->150 ms: glide, no discontinuity (step vs input)", st / inStep);

    /* ---------------- delay: octave mode pitch ---------------- */
    IFX_Delay_Init(&dl, 300.0f, 1.0f, 0.0f, FS);
    IFX_Delay_SetMode(&dl, IFX_DLY_OCTAVE); IFX_Delay_SetMod(&dl, 0.0f); IFX_Delay_SetTone(&dl, 12000.0f);
    n = FS * 2;
    for (int i = 0; i < n; i++) big[i] = 0.5f * sinf(2 * M_PI * 220 * i / FS);
    IFX_Delay_ProcessWet(&dl, big, big, n);
    double f = zc_freq(big + FS, FS);
    check(fabs(f - 440) < 15, "OCTAVE: 220 Hz in -> ~440 Hz out", f);

    /* ---------------- delay: reverse mode ---------------- */
    IFX_Delay_Init(&dl, 400.0f, 1.0f, 0.3f, FS);
    IFX_Delay_SetMode(&dl, IFX_DLY_REVERSE);
    /* rising-amplitude 330 Hz tone: reversed it must decay within each grain */
    n = FS * 2;
    for (int i = 0; i < n; i++) big[i] = 0.5f * sinf(2 * M_PI * 330 * i / FS);
    IFX_Delay_ProcessWet(&dl, big, big, n);
    f = zc_freq(big + FS, FS / 2);
    check(finite_all(big, n) && fabs(f - 330) < 20, "REVERSE: stable, pitch preserved", f);

    /* ---------------- delay: freeze holds ---------------- */
    IFX_Delay_Init(&dl, 250.0f, 1.0f, 0.5f, FS); IFX_Delay_SetMod(&dl, 0.0f);
    n = FS;
    for (int i = 0; i < n; i++) big[i] = 0.3f * sinf(2 * M_PI * 196 * i / FS);
    IFX_Delay_ProcessWet(&dl, big, big, n);
    double r0 = rms(big + n - 12000, 12000);
    IFX_Delay_SetFreeze(&dl, 1);
    memset(big, 0, sizeof(float) * FS * 5);
    IFX_Delay_ProcessWet(&dl, big, big, FS * 5);
    double r5 = rms(big + FS * 5 - 12000, 12000);
    check(r5 > 0.8 * r0, "FREEZE: 5 s later the loop still holds its level", r5 / r0);

    /* ---------------- reverb ---------------- */
    IFX_Reverb_Init(&rv, FS);
    IFX_Reverb_SetDecay(&rv, 0.6f); IFX_Reverb_SetPreDelay(&rv, 0.0f);
    n = FS * 4;
    memset(big, 0, sizeof(float) * n); big[0] = 0.9f;
    IFX_Reverb_ProcessWet(&rv, big, big, n);
    /* RT60 from the envelope (50 ms rms windows) */
    double e0 = 0; int t60 = -1;
    for (int w = 0; w + 2400 <= n; w += 2400) {
        double e = rms(big + w, 2400);
        if (w == 2400) e0 = e;
        if (w > 2400 && t60 < 0 && e < e0 * 1e-3) t60 = w;
    }
    printf("      reverb decay 0.6: RT60 ~ %.2f s\n", t60 / (double)FS);
    check(t60 > 0.5 * FS && t60 < 4 * FS, "reverb decay 0.6: RT60 between 0.5 and 4 s", t60 / (double)FS);

    IFX_Reverb_Init(&rv, FS);
    IFX_Reverb_SetDecay(&rv, 0.98f);
    n = FS * 10;
    srand(1);
    for (int i = 0; i < n; i++) big[i] = (i < FS * 6) ? 0.9f * sinf(2 * M_PI * 110 * i / FS) : 0.0f;
    IFX_Reverb_ProcessWet(&rv, big, big, n);
    double pkv = 0; for (int i = 0; i < n; i++) if (fabs(big[i]) > pkv) pkv = fabs(big[i]);
    check(finite_all(big, n) && pkv < 4.0, "reverb decay 0.98, 0.9 FS input: bounded", pkv);
    double tail = rms(big + n - 4800, 4800);
    check(tail > 1e-4, "reverb decay 0.98: long tail still present 4 s after input stops", tail);

    /* ---------------- chain: click-free toggle ---------------- */
    EffectChain_Init(&ec);
    ec.active[FX_EQ] = false; ec.fade[FX_EQ] = 0;          /* isolate the overdrive */
    n = FS;
    for (int i = 0; i < n; i++) big[i] = 0.2f * sinf(2 * M_PI * 110 * i / FS);
    chain(big, FS / 2);
    EffectChain_ToggleEffect(&ec, FX_OVERDRIVE);            /* OD on -> off mid-signal */
    chain(big + FS / 2, FS / 2);
    double s1 = maxstep(big + FS / 2 - 64, 512);
    double sOD = maxstep(big + 4800, FS / 2 - 4800);
    check(s1 <= 1.05 * sOD, "overdrive OFF toggle: no step larger than the signal's own", s1 / sOD);

    /* ---------------- chain: delay trails then stop ---------------- */
    EffectChain_Init(&ec);
    ec.active[FX_OVERDRIVE] = ec.active[FX_EQ] = false; ec.fade[FX_OVERDRIVE] = ec.fade[FX_EQ] = 0;
    EffectChain_ToggleEffect(&ec, FX_DELAY);
    n = FS * 12;
    memset(big, 0, sizeof(float) * n);
    for (int i = 0; i < FS / 4; i++) big[i] = 0.4f * sinf(2 * M_PI * 330 * i / FS);
    chain(big, FS / 2);
    EffectChain_ToggleEffect(&ec, FX_DELAY);                /* off: trails ring on */
    chain(big + FS / 2, n - FS / 2);
    double trail = rms(big + FS / 2 + 14000, 4800);        /* next repeat after switch-off */
    check(trail > 1e-3, "delay trails: repeats continue after switch-off", trail);
    check(!ec.dRun, "delay trails: processing stops once the tail is silent", ec.dRun);

    /* trails OFF: tail muted at once */
    EffectChain_Init(&ec);
    ec.active[FX_OVERDRIVE] = ec.active[FX_EQ] = false; ec.fade[FX_OVERDRIVE] = ec.fade[FX_EQ] = 0;
    EffectChain_SetParam(&ec, FX_DELAY, DL_TRAILS, 0);
    EffectChain_ToggleEffect(&ec, FX_DELAY);
    memset(big, 0, sizeof(float) * FS * 2);
    for (int i = 0; i < FS / 4; i++) big[i] = 0.4f * sinf(2 * M_PI * 330 * i / FS);
    chain(big, FS / 2);
    EffectChain_ToggleEffect(&ec, FX_DELAY);
    chain(big + FS / 2, FS * 3 / 2);
    trail = rms(big + FS / 2 + 14000, 4800);
    check(trail < 1e-6, "trails OFF: nothing heard after switch-off", trail);

    /* ---------------- chain: hold ---------------- */
    EffectChain_Init(&ec);
    ec.active[FX_OVERDRIVE] = ec.active[FX_EQ] = false; ec.fade[FX_OVERDRIVE] = ec.fade[FX_EQ] = 0;
    EffectChain_ToggleEffect(&ec, FX_REVERB);
    n = FS * 6;
    for (int i = 0; i < n; i++) big[i] = (i < FS) ? 0.3f * sinf(2 * M_PI * 261.6 * i / FS) : 0.0f;
    chain(big, FS);
    EffectChain_SetHold(&ec, true);
    chain(big + FS, n - FS);
    double held = rms(big + n - 4800, 4800);
    check(held > 0.01, "hold: reverb sustains 5 s after the note", held);

    /* ---------------- speed (host, relative only) ---------------- */
    EffectChain_Init(&ec);
    for (int fx = 0; fx < FX_COUNT; fx++) if (fx != FX_BOOST) { ec.active[fx] = true; ec.fade[fx] = 1; }
    n = FS * 4;
    for (int i = 0; i < n; i++) big[i] = 0.2f * sinf(2 * M_PI * 110 * i / FS);
    clock_t t0 = clock(); chain(big, n);
    double us = (double)(clock() - t0) / CLOCKS_PER_SEC * 1e6 / n;
    ec.active[FX_REVERB] = false; ec.fade[FX_REVERB] = 0; ec.rRun = false;
    ec.active[FX_DELAY] = false; ec.fade[FX_DELAY] = 0; ec.dRun = false;
    t0 = clock(); chain(big, n);
    double us2 = (double)(clock() - t0) / CLOCKS_PER_SEC * 1e6 / n;
    printf("      host: core-0 chain all on %.3f us/sample, without delay+reverb %.3f us/sample\n", us, us2);

    printf("%d failure(s)\n", fails);
    return fails;
}
