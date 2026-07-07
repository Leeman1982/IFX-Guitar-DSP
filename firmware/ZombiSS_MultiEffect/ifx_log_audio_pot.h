#ifndef IFX_LOG_AUDIO_POT_H
#define IFX_LOG_AUDIO_POT_H

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IFX_LOG_AUDIO_POT_A 0.0125f
#define IFX_LOG_AUDIO_POT_B 81.0f

float IFX_LogAudioPot_Get(float linVal);

#ifdef __cplusplus
}
#endif

#endif
