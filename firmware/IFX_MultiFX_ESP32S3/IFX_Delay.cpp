#include "IFX_Delay.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

uint8_t IFX_Delay_Init(IFX_Delay *dly, float maxDelayTime_ms, float delayTime_ms, float mix, float feedback, float sampleRate_Hz) {

    // Allocate delay line, preferring PSRAM
    dly->maxLineLength = (uint32_t) (0.001f * maxDelayTime_ms * sampleRate_Hz);

    dly->line = (float *) heap_caps_malloc(dly->maxLineLength * sizeof(float), MALLOC_CAP_SPIRAM);

    if (dly->line == NULL) {
        dly->line = (float *) heap_caps_malloc(dly->maxLineLength * sizeof(float), MALLOC_CAP_8BIT);
    }

    if (dly->line == NULL) {
        dly->maxLineLength = 0;
        dly->lineLength    = 0;
        return 0;
    }

    // Set delay line length
    IFX_Delay_SetLength(dly, delayTime_ms, sampleRate_Hz);

    // Store delay setting
    dly->mix = mix;
    dly->feedback = feedback;

    // Clear delay line circular buffer, reset index
    dly->lineIndex = 0;

    for (uint32_t n = 0; n < dly->maxLineLength; n++) {
        dly->line[n] = 0.0f;
    }

    // Clear output
    dly->out = 0.0f;

    return 1;

}

float IFX_Delay_Update(IFX_Delay *dly, float inp) {

	// Get current delay line output
	float delayLineOutput = dly->line[dly->lineIndex];

	// Compute current delay line input
	float delayLineInput  = inp + dly->feedback * delayLineOutput;

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

void IFX_Delay_SetLength(IFX_Delay *dly, float delayTime_ms, float sampleRate_Hz) {

    uint32_t newLength = (uint32_t) (0.001f * delayTime_ms * sampleRate_Hz);

    if (newLength > dly->maxLineLength) {

        newLength = dly->maxLineLength;

    }

    if (newLength < 1) {

        newLength = 1;

    }

    dly->lineLength = newLength;

    // Keep index in range if the line just got shorter (audio task reads this concurrently)
    if (dly->lineIndex >= newLength) {

        dly->lineIndex = 0;

    }

}

void IFX_Delay_SetMix(IFX_Delay *dly, float mix) {

    dly->mix = mix;

}

void IFX_Delay_SetFeedback(IFX_Delay *dly, float feedback) {

    dly->feedback = feedback;

}
