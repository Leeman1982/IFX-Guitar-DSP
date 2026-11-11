#ifndef IFX_DELAY_H
#define IFX_DELAY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Feedback delay — v2.
 *
 * Ideas taken from the GuitarML STM32 pedal (github.com/GuitarML/GuitarPedalSTM32)
 * and re-implemented here in C:
 *   - int16 storage: half the RAM of float, so 1.5 s fits in 144 KB (the
 *     float line was 660 ms in 128 KB);
 *   - smoothed delay time: turning Time glides the read head (tape-style
 *     pitch bend) instead of jumping and clicking;
 *   - modes: NORMAL / OCTAVE (+12 st repeats that climb each pass) / REVERSE;
 *   - low-pass "tone" filter inside the feedback loop;
 *   - wow & flutter from smooth gradient noise (tape modulator);
 *   - feed control, used for trails (the tail keeps ringing after the delay
 *     is switched off) and for freeze (feedback = 1, input muted).
 *
 * Signal:  line ← feed·in + fb·LPF(read)    wet = LPF(read)
 * The block API returns the wet signal only; the effect chain does the
 * dry/wet mixing (and the delay ↔ reverb routing).
 */

/* Line length is a build option: small-RAM targets (STM32F405) override it. */
#ifndef IFX_DELAY_MAX_LINE_LENGTH
#define IFX_DELAY_MAX_LINE_LENGTH 72000u   /* 1.5 s @ 48 kHz, int16  */
#endif
/* Longest Time setting that fits, in ms at 48 kHz (rounded down to 10 ms). */
#define IFX_DELAY_MAX_MS  ((float)(((IFX_DELAY_MAX_LINE_LENGTH - 64u) / 480u) * 10u))
#define IFX_DELAY_OCT_WINDOW      3072.0f  /* octave grain, samples  */

enum { IFX_DLY_NORMAL = 0, IFX_DLY_OCTAVE, IFX_DLY_REVERSE, IFX_DLY_MODE_COUNT };

typedef struct {
    int16_t  line[IFX_DELAY_MAX_LINE_LENGTH];
    uint32_t w;                 /* write index                              */
    float    fs;

    float    timeTarget;        /* samples                                  */
    float    timeCur;           /* smoothed                                 */
    float    timeCoef;          /* one-pole coefficient (~80 ms)            */

    float    feedback;          /* user feedback 0..0.95                    */
    float    mix;               /* user mix, used by IFX_Delay_Update only  */
    float    feed, feedTarget;  /* input gain into the line (ramped)        */
    uint8_t  mode;
    uint8_t  freeze;

    float    toneCoef, toneZ;   /* one-pole LPF in the loop                 */

    float    headPh;            /* grain phase 0..1 (OCTAVE / REVERSE)      */

    /* tape modulator: two gradient-noise generators, evaluated every 32
     * samples and linearly interpolated in between */
    float    modDepth;          /* 0..1                                     */
    float    wowX, flutX;
    float    modCur, modStep;
    uint8_t  modCount;

    float    out;
} IFX_Delay;

void  IFX_Delay_Init(IFX_Delay *dly, float delayTime_ms, float mix, float feedback, float sampleRate_Hz);
/* One sample, dry/wet mixed with the pedal mix law (see ifx_mix.h). */
float IFX_Delay_Update(IFX_Delay *dly, float inp);
/* Block form: in-place, dry/wet mixed. */
void  IFX_Delay_Process(IFX_Delay *dly, float *buf, int n);
/* Block form used by the effect chain: wet only (in and wet may alias). */
void  IFX_Delay_ProcessWet(IFX_Delay *dly, const float *in, float *wet, int n);

void  IFX_Delay_SetLength(IFX_Delay *dly, float delayTime_ms, float sampleRate_Hz);
void  IFX_Delay_SetMix(IFX_Delay *dly, float mix);
void  IFX_Delay_SetFeedback(IFX_Delay *dly, float feedback);
void  IFX_Delay_SetMode(IFX_Delay *dly, uint8_t mode);
void  IFX_Delay_SetTone(IFX_Delay *dly, float cutoff_Hz);
void  IFX_Delay_SetMod(IFX_Delay *dly, float depth01);
void  IFX_Delay_SetFeed(IFX_Delay *dly, float target01);   /* 1 = normal, 0 = tail only */
void  IFX_Delay_SetFreeze(IFX_Delay *dly, uint8_t on);
void  IFX_Delay_Clear(IFX_Delay *dly);
const char *IFX_Delay_ModeName(uint8_t mode);

#ifdef __cplusplus
}
#endif

#endif
