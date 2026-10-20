#include "ifx_moving_rms.h"
#include "ifx_bf_math.h"   /* IFX_RAMFUNC */

void IFX_MovingRMS_Init(IFX_MovingRMS *mrms, uint16_t M) {
    if (M > IFX_MOVING_RMS_MAX_BUF) M = IFX_MOVING_RMS_MAX_BUF;
    if (M < 1) M = 1;
    mrms->M = M;
    mrms->invM = 1.0f / (float)M;
    mrms->count = 0;
    for (uint16_t n = 0; n < M; n++) {
        mrms->in_sq_M[n] = 0.0f;
    }
    mrms->out_sq = 0.0f;
    mrms->fresh = 0.0f;
}

/* Running mean of x² over M samples.  The incremental add/subtract form
 * accumulates float rounding error without bound (it can even go negative
 * after hours); so once per window the running value is replaced by an
 * exact sum of the window that just completed. */
float IFX_RAMFUNC(IFX_MovingRMS_Update)(IFX_MovingRMS *mrms, float in) {
    float in_sq = in * in;
    float old = mrms->in_sq_M[mrms->count];
    mrms->in_sq_M[mrms->count] = in_sq;
    mrms->out_sq += mrms->invM * (in_sq - old);
    mrms->fresh += in_sq;
    if (++mrms->count >= mrms->M) {
        mrms->count = 0;
        mrms->out_sq = mrms->fresh * mrms->invM;
        mrms->fresh = 0.0f;
    }
    return mrms->out_sq;
}
