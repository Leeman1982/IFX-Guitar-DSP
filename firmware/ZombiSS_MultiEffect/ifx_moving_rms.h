#ifndef IFX_MOVING_RMS_H
#define IFX_MOVING_RMS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef IFX_MOVING_RMS_MAX_BUF
#define IFX_MOVING_RMS_MAX_BUF 2400   /* 50 ms @ 48 kHz; small-RAM targets may shrink it */
#endif

typedef struct {
    uint16_t M;
    uint16_t count;
    float invM;
    float in_sq_M[IFX_MOVING_RMS_MAX_BUF];
    float out_sq;
    float fresh;      /* exact sum of the current window, rebuilt each wrap */
} IFX_MovingRMS;

void  IFX_MovingRMS_Init(IFX_MovingRMS *mrms, uint16_t M);
float IFX_MovingRMS_Update(IFX_MovingRMS *mrms, float in);

#ifdef __cplusplus
}
#endif

#endif
