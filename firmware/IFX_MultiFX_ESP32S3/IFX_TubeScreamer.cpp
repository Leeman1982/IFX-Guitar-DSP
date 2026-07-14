#include "IFX_TubeScreamer.h"

/* 1st order filter coefficient from cutoff frequency */
static inline float alphaFromCutoff(float cutoffHz, float T) {
	return expf(-6.28318530718f * cutoffHz * T);
}

void IFX_TubeScreamer_Init(IFX_TubeScreamer *ts, float samplingFrequencyHz, float tightCutoffHz, float drive, float toneCutoffHz, float level) {
	/* Sampling time */
	ts->T = 1.0f / samplingFrequencyHz;

	/* Input "tight" high-pass filter */
	ts->hpTightAlpha = alphaFromCutoff(tightCutoffHz, ts->T);
	ts->hpTightIn  = 0.0f;
	ts->hpTightOut = 0.0f;

	/* Clip-path high-pass filter (720 Hz corner of the TS-808 clipping stage) */
	ts->hpClipAlpha = alphaFromCutoff(720.0f, ts->T);
	ts->hpClipIn  = 0.0f;
	ts->hpClipOut = 0.0f;

	/* Drive */
	ts->drive = drive;

	/* Tone low-pass filter */
	ts->lpToneAlpha = alphaFromCutoff(toneCutoffHz, ts->T);
	ts->lpToneOut = 0.0f;

	/* Output level */
	ts->level = level;

	ts->out = 0.0f;
}

void IFX_TubeScreamer_SetTight(IFX_TubeScreamer *ts, float tightCutoffHz) {

	ts->hpTightAlpha = alphaFromCutoff(tightCutoffHz, ts->T);

}

void IFX_TubeScreamer_SetDrive(IFX_TubeScreamer *ts, float drive) {

	ts->drive = drive;

}

void IFX_TubeScreamer_SetTone(IFX_TubeScreamer *ts, float toneCutoffHz) {

	ts->lpToneAlpha = alphaFromCutoff(toneCutoffHz, ts->T);

}

void IFX_TubeScreamer_SetLevel(IFX_TubeScreamer *ts, float level) {

	ts->level = level;

}

float IFX_TubeScreamer_Update(IFX_TubeScreamer *ts, float inp) {

	/* Input "tight" high-pass filter (removes flub before clipping) */
	float tightOut = ts->hpTightAlpha * (ts->hpTightOut + inp - ts->hpTightIn);
	ts->hpTightIn  = inp;
	ts->hpTightOut = tightOut;

	/* High-pass the clip path at 720 Hz, as in the TS-808 feedback network.
	 * This is what leaves the low end (relatively) clean and gives the
	 * characteristic mid-hump. */
	float clipPath = ts->hpClipAlpha * (ts->hpClipOut + tightOut - ts->hpClipIn);
	ts->hpClipIn  = tightOut;
	ts->hpClipOut = clipPath;

	/* Soft, slightly asymmetrical diode clipping of the gained-up signal */
	float u = ts->drive * clipPath;
	float clipped;

	if (u >= 0.0f) {
		clipped = tanhf(u);
	} else {
		clipped = tanhf(1.15f * u) * (1.0f / 1.15f);
	}

	/* Non-inverting stage: output = input + clipped feedback-path signal.
	 * Scaled by 0.5 to keep headroom. */
	float stageOut = 0.5f * (tightOut + clipped);

	/* Tone low-pass filter */
	ts->lpToneOut = (1.0f - ts->lpToneAlpha) * stageOut + ts->lpToneAlpha * ts->lpToneOut;

	/* Output level */
	ts->out = ts->level * ts->lpToneOut;

	/* Limit output */
	if (ts->out > 1.0f) {
		ts->out = 1.0f;
	} else if (ts->out < -1.0f) {
		ts->out = -1.0f;
	}

	return ts->out;
}
