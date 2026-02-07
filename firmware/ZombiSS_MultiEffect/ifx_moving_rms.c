#include "ifx_moving_rms.h"

void IFX_MovingRMS_Init(IFX_MovingRMS *mrms, uint16_t M) {
    if (M > IFX_MOVING_RMS_MAX_BUF) M = IFX_MOVING_RMS_MAX_BUF;
    mrms->M = M;
    mrms->invM = 1.0f / (float)M;
    mrms->count = 0;
    for (uint16_t n = 0; n < M; n++) {
        mrms->in_sq_M[n] = 0.0f;
    }
    mrms->out_sq = 0.0f;
}

float IFX_MovingRMS_Update(IFX_MovingRMS *mrms, float in) {
    float in_sq = in * in;
    mrms->in_sq_M[mrms->count] = in_sq;
    if (mrms->count == (mrms->M - 1)) {
        mrms->count = 0;
    } else {
        mrms->count++;
    }
    mrms->out_sq = mrms->out_sq + mrms->invM * (in_sq - mrms->in_sq_M[mrms->count]);
    return mrms->out_sq;
}
