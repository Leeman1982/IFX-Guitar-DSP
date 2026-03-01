#include "IFX_Delay.h"

static float g_sampleRate = 48000.0f;

void IFX_Delay_Init(IFX_Delay *dly, float sampleRateHz) {
    g_sampleRate = sampleRateHz;

    // Default settings
    dly->mix = 0.3f;
    dly->feedback = 0.4f;
    dly->lineLength = (uint32_t)(0.25f * sampleRateHz); // 250ms default

    // Clear delay line circular buffer, reset index
    dly->lineIndex = 0;

    for (uint32_t n = 0; n < IFX_DELAY_MAX_LINE_LENGTH; n++) {
        dly->line[n] = 0.0f;
    }

    // Clear output
    dly->out = 0.0f;
}

float IFX_Delay_Process(IFX_Delay *dly, float inp) {
    // Get current delay line output
    float delayLineOutput = dly->line[dly->lineIndex];

    // Compute current delay line input
    float delayLineInput = inp + dly->feedback * delayLineOutput;

    // Store in delay line circular buffer
    dly->line[dly->lineIndex] = delayLineInput;

    // Increment delay line index
    dly->lineIndex++;
    if (dly->lineIndex >= dly->lineLength) {
        dly->lineIndex = 0;
    }

    // Mix dry and wet signals to compute output
    dly->out = (1.0f - dly->mix) * inp + dly->mix * delayLineOutput;

    // Limit output
    if (dly->out > 1.0f) {
        dly->out = 1.0f;
    } else if (dly->out < -1.0f) {
        dly->out = -1.0f;
    }

    // Return current output
    return dly->out;
}

void IFX_Delay_SetDelayTime(IFX_Delay *dly, float delayTimeMs) {
    dly->lineLength = (uint32_t)(0.001f * delayTimeMs * g_sampleRate);

    if (dly->lineLength > IFX_DELAY_MAX_LINE_LENGTH) {
        dly->lineLength = IFX_DELAY_MAX_LINE_LENGTH;
    }
}

void IFX_Delay_SetFeedback(IFX_Delay *dly, float feedback) {
    dly->feedback = feedback;
}

void IFX_Delay_SetMix(IFX_Delay *dly, float mix) {
    dly->mix = mix;
}
