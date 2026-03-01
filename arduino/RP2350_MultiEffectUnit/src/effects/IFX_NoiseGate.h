#ifndef IFX_NOISE_GATE_H
#define IFX_NOISE_GATE_H

#include <Arduino.h>
#include <math.h>
#include "IFX_MovingRMS.h"

#define IFX_NOISEGATE_RMS_HORIZON_MS 50.0f

typedef struct {
    float thresholdSq;
    float holdTimeS;
    float attackCoeff;
    float releaseCoeff;
    float attackCounter;
    float releaseCounter;
    float smoothedGain;
    float sampleTimeS;
    IFX_MovingRMS mrms;
} IFX_NoiseGate;

void IFX_NoiseGate_Init(IFX_NoiseGate *ng, float sampleRateHz);
float IFX_NoiseGate_Process(IFX_NoiseGate *ng, float inp);
void IFX_NoiseGate_SetThreshold(IFX_NoiseGate *ng, float thresholdDB);
void IFX_NoiseGate_SetAttackTime(IFX_NoiseGate *ng, float attackTimeMs);
void IFX_NoiseGate_SetReleaseTime(IFX_NoiseGate *ng, float releaseTimeMs);
void IFX_NoiseGate_SetHoldTime(IFX_NoiseGate *ng, float holdTimeMs);

#endif
