#include "IFX_NoiseGate.h"

void IFX_NoiseGate_Init(IFX_NoiseGate *ng, float sampleRateHz) {
    // Store sample time
    ng->sampleTimeS = 1.0f / sampleRateHz;

    // Default threshold -40dB
    ng->thresholdSq = 0.01f * 0.01f;  // 0.01 = 10^(-40/20)

    // Default hold time 10ms
    ng->holdTimeS = 0.010f;

    // Default attack and release times (1ms attack, 100ms release)
    ng->attackCoeff = expf(-2197.22457734f / (sampleRateHz * 1.0f));
    ng->releaseCoeff = expf(-2197.22457734f / (sampleRateHz * 100.0f));

    // Reset counters
    ng->attackCounter = 0.0f;
    ng->releaseCounter = 0.0f;

    // Reset smoothed gain value
    ng->smoothedGain = 0.0f;

    // Initialize moving RMS filter
    IFX_MovingRMS_Init(&ng->mrms, (uint16_t)(sampleRateHz * IFX_NOISEGATE_RMS_HORIZON_MS * 0.001f));
}

float IFX_NoiseGate_Process(IFX_NoiseGate *ng, float inp) {
    // [1] Estimate (RMS)^2 of input
    float inRMSSq = IFX_MovingRMS_Update(&ng->mrms, inp);

    // [2] Gain computer (static gain characteristic)
    float gain = 1.0f;

    if (inRMSSq < ng->thresholdSq) {
        gain = 0.0f;
    }

    // [3] Gain smoothing
    if (gain <= ng->smoothedGain) { // ATTACK
        if (ng->attackCounter > ng->holdTimeS) {
            ng->smoothedGain = ng->attackCoeff * ng->smoothedGain + (1.0f - ng->attackCoeff) * gain;
        } else {
            ng->attackCounter += ng->sampleTimeS;
        }
        ng->releaseCounter = 0.0f;
    } else if (gain > ng->smoothedGain) { // RELEASE
        if (ng->releaseCounter > ng->holdTimeS) {
            ng->smoothedGain = ng->releaseCoeff * ng->smoothedGain + (1.0f - ng->releaseCoeff) * gain;
        } else {
            ng->releaseCounter += ng->sampleTimeS;
        }
        ng->attackCounter = 0.0f;
    }

    return (inp * ng->smoothedGain);
}

void IFX_NoiseGate_SetThreshold(IFX_NoiseGate *ng, float thresholdDB) {
    // Convert dB to linear (threshold = 10^(dB/20))
    float threshold = powf(10.0f, thresholdDB / 20.0f);
    ng->thresholdSq = threshold * threshold;
}

void IFX_NoiseGate_SetAttackTime(IFX_NoiseGate *ng, float attackTimeMs) {
    float sampleRateHz = 1.0f / ng->sampleTimeS;
    ng->attackCoeff = expf(-2197.22457734f / (sampleRateHz * attackTimeMs));
}

void IFX_NoiseGate_SetReleaseTime(IFX_NoiseGate *ng, float releaseTimeMs) {
    float sampleRateHz = 1.0f / ng->sampleTimeS;
    ng->releaseCoeff = expf(-2197.22457734f / (sampleRateHz * releaseTimeMs));
}

void IFX_NoiseGate_SetHoldTime(IFX_NoiseGate *ng, float holdTimeMs) {
    ng->holdTimeS = 0.001f * holdTimeMs;
}
