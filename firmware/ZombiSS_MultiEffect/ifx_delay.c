#include "ifx_delay.h"
#include "pico/platform.h"

void IFX_Delay_Init(IFX_Delay *dly, float delayTime_ms, float mix, float feedback, float sampleRate_Hz) {
    IFX_Delay_SetLength(dly, delayTime_ms, sampleRate_Hz);
    dly->mix = mix;
    dly->feedback = feedback;
    dly->lineIndex = 0;
    for (uint32_t n = 0; n < IFX_DELAY_MAX_LINE_LENGTH; n++) {
        dly->line[n] = 0.0f;
    }
    dly->out = 0.0f;
}

float __not_in_flash_func(IFX_Delay_Update)(IFX_Delay *dly, float inp) {
    float delayLineOutput = dly->line[dly->lineIndex];
    float delayLineInput  = inp + dly->feedback * delayLineOutput;
    dly->line[dly->lineIndex] = delayLineInput;

    dly->lineIndex++;
    if (dly->lineIndex >= dly->lineLength) {
        dly->lineIndex = 0;
    }

    dly->out = (1.0f - dly->mix) * inp + dly->mix * delayLineOutput;

    if (dly->out > 1.0f) dly->out = 1.0f;
    else if (dly->out < -1.0f) dly->out = -1.0f;

    return dly->out;
}

void IFX_Delay_SetLength(IFX_Delay *dly, float delayTime_ms, float sampleRate_Hz) {
    dly->lineLength = (uint32_t)(0.001f * delayTime_ms * sampleRate_Hz);
    if (dly->lineLength > IFX_DELAY_MAX_LINE_LENGTH) dly->lineLength = IFX_DELAY_MAX_LINE_LENGTH;
    if (dly->lineLength < 1) dly->lineLength = 1;
}

void IFX_Delay_SetMix(IFX_Delay *dly, float mix) {
    if (mix > 1.0f) mix = 1.0f;
    if (mix < 0.0f) mix = 0.0f;
    dly->mix = mix;
}

void IFX_Delay_SetFeedback(IFX_Delay *dly, float feedback) {
    if (feedback > 0.99f) feedback = 0.99f;
    if (feedback < 0.0f) feedback = 0.0f;
    dly->feedback = feedback;
}
