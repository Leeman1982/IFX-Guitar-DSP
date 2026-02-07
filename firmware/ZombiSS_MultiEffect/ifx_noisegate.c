#include "ifx_noisegate.h"

void IFX_NoiseGate_Init(IFX_NoiseGate *ng, float threshold, float attackTimeMs,
                         float releaseTimeMs, float holdTimeMs, float sampleRateHz) {
    ng->thresholdSq = threshold * threshold;
    ng->holdTimeS = 0.001f * holdTimeMs;
    IFX_NoiseGate_SetAttackRelease(ng, attackTimeMs, releaseTimeMs, sampleRateHz);
    ng->sampleTimeS = 1.0f / sampleRateHz;
    ng->attackCounter  = 0.0f;
    ng->releaseCounter = 0.0f;
    ng->smoothedGain = 0.0f;
    IFX_MovingRMS_Init(&ng->mrms, (uint16_t)(sampleRateHz * IFX_NOISEGATE_RMS_HORIZON_MS * 0.001f));
}

float IFX_NoiseGate_Update(IFX_NoiseGate *ng, float inp) {
    float inRMSSq = IFX_MovingRMS_Update(&ng->mrms, inp);

    float gain = 1.0f;
    if (inRMSSq < ng->thresholdSq) {
        gain = 0.0f;
    }

    if (gain <= ng->smoothedGain) {
        /* ATTACK */
        if (ng->attackCounter > ng->holdTimeS) {
            ng->smoothedGain = ng->attackCoeff * ng->smoothedGain + (1.0f - ng->attackCoeff) * gain;
        } else {
            ng->attackCounter += ng->sampleTimeS;
        }
        ng->releaseCounter = 0.0f;
    } else {
        /* RELEASE */
        if (ng->releaseCounter > ng->holdTimeS) {
            ng->smoothedGain = ng->releaseCoeff * ng->smoothedGain + (1.0f - ng->releaseCoeff) * gain;
        } else {
            ng->releaseCounter += ng->sampleTimeS;
        }
        ng->attackCounter = 0.0f;
    }

    return inp * ng->smoothedGain;
}

void IFX_NoiseGate_SetThreshold(IFX_NoiseGate *ng, float threshold) {
    ng->thresholdSq = threshold * threshold;
}

void IFX_NoiseGate_SetAttackRelease(IFX_NoiseGate *ng, float attackTimeMs,
                                     float releaseTimeMs, float sampleRateHz) {
    ng->attackCoeff  = expf(-2197.22457734f / (sampleRateHz * attackTimeMs));
    ng->releaseCoeff = expf(-2197.22457734f / (sampleRateHz * releaseTimeMs));
}

void IFX_NoiseGate_SetHoldTime(IFX_NoiseGate *ng, float holdTimeMs) {
    ng->holdTimeS = 0.001f * holdTimeMs;
}
