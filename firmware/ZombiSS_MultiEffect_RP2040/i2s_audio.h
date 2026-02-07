#ifndef I2S_AUDIO_H
#define I2S_AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Callback type: processes AUDIO_BUFFER_FRAMES stereo frames */
typedef void (*audio_callback_t)(const int32_t *input, int32_t *output, uint32_t frame_count);

void I2SAudio_Init(audio_callback_t callback);
void I2SAudio_Start(void);
void I2SAudio_Stop(void);

/* Stats */
uint32_t I2SAudio_GetFrameCount(void);
uint32_t I2SAudio_GetUnderruns(void);

#ifdef __cplusplus
}
#endif

#endif
