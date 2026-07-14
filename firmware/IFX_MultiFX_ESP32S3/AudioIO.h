/*
*
*   InfiniFX Multi-FX - ESP32-S3 audio I/O
*
*   One full-duplex I2S port (master):
*     RX <- PCM1808 (24-bit data in 32-bit I2S slots, needs 256fs MCLK)
*     TX -> PCM5102 (32-bit I2S slots)
*
*   Both converters share BCK and LRCK.
*
*   Requires arduino-esp32 core 3.x (ESP-IDF 5 I2S "std" driver).
*
*/

#ifndef AUDIO_IO_H
#define AUDIO_IO_H

#include <stdint.h>
#include <stddef.h>

/* Returns true on success */
bool AudioIO_Init(void);

/* Blocking read/write of interleaved stereo frames (L, R, L, R, ...) */
bool AudioIO_Read(int32_t *buf, size_t frames);
bool AudioIO_Write(const int32_t *buf, size_t frames);

/* 24-bit audio (in upper bits of 32-bit slots) <-> float */
static inline float AudioIO_SampleToFloat(int32_t s) {
    return (float) (s >> 8) * (1.0f / 8388608.0f);
}

static inline int32_t AudioIO_FloatToSample(float f) {
    if (f > 1.0f) {
        f = 1.0f;
    } else if (f < -1.0f) {
        f = -1.0f;
    }
    return ((int32_t) (f * 8388607.0f)) << 8;
}

#endif
