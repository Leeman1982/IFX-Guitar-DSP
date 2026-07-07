#ifndef IFX_10BAND_EQ_H
#define IFX_10BAND_EQ_H

/*
 * 10-Band Graphic Equaliser
 *
 * 10 cascaded 2nd-order IIR peaking (biquad) filters at standard
 * octave-band centre frequencies:
 *   31, 63, 125, 250, 500, 1k, 2k, 4k, 8k, 16k Hz
 *
 * Each band has Q = 1.41 (octave bandwidth).
 * Gain is controlled in dB (-15 to +15 dB per band).
 *
 * Default preset: Metallica "scooped mid" rhythm tone
 *   31Hz:+6  63Hz:+5  125Hz:+3  250Hz:-2  500Hz:-4
 *   1kHz:-6  2kHz:-5  4kHz:-3   8kHz:+4   16kHz:+3
 */

#include <math.h>
#include <stdint.h>
#include "ifx_peaking_filter.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EQ10_BANDS 10

typedef struct {
    IFX_PeakingFilter bands[EQ10_BANDS];
    float gainDb[EQ10_BANDS];
} IFX_10BandEQ;

/* Standard octave-band centre frequencies */
extern const float IFX_EQ10_FREQS[EQ10_BANDS];

/* Default Metallica preset gains in dB (index = band 0..9) */
extern const float IFX_EQ10_METALLICA_PRESET[EQ10_BANDS];

void  IFX_10BandEQ_Init(IFX_10BandEQ *eq, float sampleRate_Hz);
void  IFX_10BandEQ_SetBand(IFX_10BandEQ *eq, uint8_t band, float gainDb);
float IFX_10BandEQ_Update(IFX_10BandEQ *eq, float in);

#ifdef __cplusplus
}
#endif

#endif /* IFX_10BAND_EQ_H */
