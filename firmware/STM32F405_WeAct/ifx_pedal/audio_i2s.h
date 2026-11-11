#ifndef AUDIO_I2S_H
#define AUDIO_I2S_H
/* I2S2 full duplex (SPI2 master TX + I2S2ext slave RX), 48 kHz, 32-bit slots,
 * circular DMA, 64-frame double buffer.  The callback runs in the DMA IRQ. */
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define AUDIO_FRAMES 64
/* in: left-channel samples (PCM1808 LIN) as float ±1; out: written to L and R */
typedef void (*audio_callback_t)(const float *in, float *out, int n);
void     audio_start(audio_callback_t cb);
uint32_t audio_late_blocks(void);     /* blocks not finished before the next one arrived */
#ifdef __cplusplus
}
#endif
#endif
