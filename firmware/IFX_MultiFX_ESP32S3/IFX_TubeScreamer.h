/*
*
*   InfiniFX - Tube Screamer style boost
*
*   Models the TS-808 topology:
*     - Variable input high-pass "tight" control
*     - Clipping stage: dry signal + soft-clipped, high-passed (720 Hz)
*       and gained-up signal, i.e. the non-inverting op-amp stage with
*       diodes in the feedback path. Slightly asymmetrical clipping.
*     - First-order low-pass tone control
*     - Output level
*
*   Recommended settings:
*   tightCutoffHz = 250.0f;
*   drive         = 8.0f;    (1 ... ~30)
*   toneCutoffHz  = 2000.0f; (500 ... 5000)
*   level         = 1.0f;    (0 ... 2)
*
*/

#ifndef IFX_TUBESCREAMER_H
#define IFX_TUBESCREAMER_H

#include <math.h>

typedef struct {
	/* Sampling time */
	float T;

	/* Input "tight" high-pass filter (1st order) */
	float hpTightAlpha;
	float hpTightIn;
	float hpTightOut;

	/* Clip-path high-pass filter, fixed 720 Hz (1st order) */
	float hpClipAlpha;
	float hpClipIn;
	float hpClipOut;

	/* Drive gain into the clipper */
	float drive;

	/* Tone low-pass filter (1st order) */
	float lpToneAlpha;
	float lpToneOut;

	/* Output level */
	float level;

	float out;

} IFX_TubeScreamer;

void  IFX_TubeScreamer_Init(IFX_TubeScreamer *ts, float samplingFrequencyHz, float tightCutoffHz, float drive, float toneCutoffHz, float level);
void  IFX_TubeScreamer_SetTight(IFX_TubeScreamer *ts, float tightCutoffHz);
void  IFX_TubeScreamer_SetDrive(IFX_TubeScreamer *ts, float drive);
void  IFX_TubeScreamer_SetTone(IFX_TubeScreamer *ts, float toneCutoffHz);
void  IFX_TubeScreamer_SetLevel(IFX_TubeScreamer *ts, float level);
float IFX_TubeScreamer_Update(IFX_TubeScreamer *ts, float inp);

#endif
