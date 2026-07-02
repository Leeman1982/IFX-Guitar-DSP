#ifndef I2S_AUDIO_H
#define I2S_AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/pio.h"
#include "hardware/dma.h"

/* Audio configuration */
#define AUDIO_SAMPLE_RATE   48000
#define AUDIO_BUFFER_FRAMES 64      /* Frames per half-buffer (low latency ~1.3ms) */
#define AUDIO_BUFFER_TOTAL  (AUDIO_BUFFER_FRAMES * 2)  /* Stereo: L + R per frame */
#define AUDIO_DMA_BUFFERS   2       /* Double buffering */

/* Pin assignments for PCM5102 DAC output */
#define I2S_OUT_DIN_PIN     16
#define I2S_OUT_BCK_PIN     17      /* BCK=17, LRCK=18 (consecutive) */

/* Pin assignments for PCM1808 ADC input */
/* DOUT=19, LRCK=20, BCK=21 (consecutive for PIO in-base) */
#define I2S_IN_DOUT_PIN     19
#define I2S_IN_LRCK_PIN     20
#define I2S_IN_BCK_PIN      21

/* Callback type: processes AUDIO_BUFFER_FRAMES stereo frames */
typedef void (*audio_callback_t)(const int32_t *input, int32_t *output, uint32_t frame_count);

typedef struct {
    PIO pio_out;
    uint sm_out;
    PIO pio_in;
    uint sm_in;

    /* DMA channels */
    int dma_out_ch[2];
    int dma_in_ch[2];

    /* Double buffers */
    int32_t tx_buf[AUDIO_DMA_BUFFERS][AUDIO_BUFFER_TOTAL];
    int32_t rx_buf[AUDIO_DMA_BUFFERS][AUDIO_BUFFER_TOTAL];

    /* Current buffer index (0 or 1) */
    volatile uint8_t tx_buf_idx;
    volatile uint8_t rx_buf_idx;

    /* Processing callback */
    audio_callback_t callback;

    /* Stats */
    volatile uint32_t underruns;
    volatile uint32_t frame_count;
    volatile bool processing;
} I2SAudio;

extern I2SAudio g_audio;

void I2SAudio_Init(audio_callback_t callback);
void I2SAudio_Start(void);
void I2SAudio_Stop(void);

/* Called from DMA IRQ - do not call directly */
void I2SAudio_DMA_IRQ_Handler(void);

#endif
