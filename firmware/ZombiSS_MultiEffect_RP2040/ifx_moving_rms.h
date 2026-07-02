#ifndef IFX_MOVING_RMS_H
#define IFX_MOVING_RMS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Reduced from 2400 to 1600 for RP2040 264KB SRAM */
#define IFX_MOVING_RMS_MAX_BUF 1600

typedef struct {
    uint16_t M;
    uint16_t count;
    float invM;
    float in_sq_M[IFX_MOVING_RMS_MAX_BUF];
    float out_sq;
} IFX_MovingRMS;

void  IFX_MovingRMS_Init(IFX_MovingRMS *mrms, uint16_t M);
float IFX_MovingRMS_Update(IFX_MovingRMS *mrms, float in);

#ifdef __cplusplus
}
#endif

#endif
