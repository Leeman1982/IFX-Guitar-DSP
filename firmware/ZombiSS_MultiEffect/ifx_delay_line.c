#include "ifx_delay_line.h"

void IFX_DelayLine_Init(IFX_DelayLine *dlyLn, float delayTime_ms, float sampleRate_Hz) {
    IFX_DelayLine_SetLength(dlyLn, delayTime_ms, sampleRate_Hz);
    dlyLn->index = 0;
    for (uint32_t n = 0; n < IFX_DELAYLINE_MAXLENGTH; n++) {
        dlyLn->memory[n] = 0.0f;
    }
}

float IFX_DelayLine_Update(IFX_DelayLine *dlyLn, float inp) {
    float out = dlyLn->memory[dlyLn->index];
    dlyLn->memory[dlyLn->index] = inp;
    dlyLn->index++;
    if (dlyLn->index >= dlyLn->length) {
        dlyLn->index = 0;
    }
    return out;
}

void IFX_DelayLine_SetLength(IFX_DelayLine *dlyLn, float delayTime_ms, float sampleRate_Hz) {
    dlyLn->length = (uint32_t)(0.001f * delayTime_ms * sampleRate_Hz);
    if (dlyLn->length > IFX_DELAYLINE_MAXLENGTH) {
        dlyLn->length = IFX_DELAYLINE_MAXLENGTH;
    }
    if (dlyLn->length < 1) {
        dlyLn->length = 1;
    }
}
