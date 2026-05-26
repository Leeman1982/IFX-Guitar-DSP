#include "ifx_log_audio_pot.h"

float IFX_LogAudioPot_Get(float linVal) {
    if (linVal > 1.0f) linVal = 1.0f;
    else if (linVal < 0.0f) linVal = 0.0f;
    return IFX_LOG_AUDIO_POT_A * (powf(IFX_LOG_AUDIO_POT_B, linVal) - 1.0f);
}
