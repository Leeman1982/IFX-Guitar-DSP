#include "i2s_audio.h"
#include "i2s_pio_programs.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include <string.h>

/* Internal state */
static PIO pio_out;
static uint sm_out;
static PIO pio_in;
static uint sm_in;

static int dma_out_ch[2];
static int dma_in_ch[2];

static int32_t tx_buf[2][AUDIO_BUFFER_TOTAL];
static int32_t rx_buf[2][AUDIO_BUFFER_TOTAL];

static audio_callback_t audio_cb = NULL;
static volatile uint32_t frame_count = 0;
static volatile uint32_t underruns = 0;

/* DMA IRQ handler */
static void __isr __time_critical_func(dma_irq_handler)(void) {
    for (int i = 0; i < 2; i++) {
        if (dma_channel_get_irq0_status(dma_out_ch[i])) {
            dma_channel_acknowledge_irq0(dma_out_ch[i]);

            if (audio_cb) {
                audio_cb(rx_buf[i], tx_buf[i], AUDIO_BUFFER_FRAMES);
            }

            frame_count += AUDIO_BUFFER_FRAMES;

            /* Re-configure for next cycle */
            dma_channel_set_read_addr(dma_out_ch[i], tx_buf[i], false);
            dma_channel_set_write_addr(dma_in_ch[i], rx_buf[i], false);
        }
    }
}

void I2SAudio_Init(audio_callback_t callback) {
    audio_cb = callback;
    memset(tx_buf, 0, sizeof(tx_buf));
    memset(rx_buf, 0, sizeof(rx_buf));

    /* --- I2S Output (DAC) on PIO0 --- */
    pio_out = pio0;
    sm_out = pio_claim_unused_sm(pio_out, true);
    uint offset_out = pio_add_program(pio_out, &i2s_out_program);
    i2s_out_program_init(pio_out, sm_out, offset_out,
                         I2S_OUT_DIN_PIN, I2S_OUT_BCK_PIN);

    /* --- I2S Input (ADC) on PIO1 --- */
    pio_in = pio1;
    sm_in = pio_claim_unused_sm(pio_in, true);
    uint offset_in = pio_add_program(pio_in, &i2s_in_program);
    i2s_in_program_init(pio_in, sm_in, offset_in,
                        I2S_IN_DOUT_PIN, I2S_IN_LRCK_PIN, I2S_IN_BCK_PIN);

    /* --- DMA TX (output to DAC) --- */
    for (int i = 0; i < 2; i++) {
        dma_out_ch[i] = dma_claim_unused_channel(true);
    }

    dma_channel_config c0 = dma_channel_get_default_config(dma_out_ch[0]);
    channel_config_set_transfer_data_size(&c0, DMA_SIZE_32);
    channel_config_set_read_increment(&c0, true);
    channel_config_set_write_increment(&c0, false);
    channel_config_set_dreq(&c0, pio_get_dreq(pio_out, sm_out, true));
    channel_config_set_chain_to(&c0, dma_out_ch[1]);
    dma_channel_configure(dma_out_ch[0], &c0,
                          &pio_out->txf[sm_out], tx_buf[0],
                          AUDIO_BUFFER_TOTAL, false);

    dma_channel_config c1 = dma_channel_get_default_config(dma_out_ch[1]);
    channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);
    channel_config_set_read_increment(&c1, true);
    channel_config_set_write_increment(&c1, false);
    channel_config_set_dreq(&c1, pio_get_dreq(pio_out, sm_out, true));
    channel_config_set_chain_to(&c1, dma_out_ch[0]);
    dma_channel_configure(dma_out_ch[1], &c1,
                          &pio_out->txf[sm_out], tx_buf[1],
                          AUDIO_BUFFER_TOTAL, false);

    /* --- DMA RX (input from ADC) --- */
    for (int i = 0; i < 2; i++) {
        dma_in_ch[i] = dma_claim_unused_channel(true);
    }

    dma_channel_config r0 = dma_channel_get_default_config(dma_in_ch[0]);
    channel_config_set_transfer_data_size(&r0, DMA_SIZE_32);
    channel_config_set_read_increment(&r0, false);
    channel_config_set_write_increment(&r0, true);
    channel_config_set_dreq(&r0, pio_get_dreq(pio_in, sm_in, false));
    channel_config_set_chain_to(&r0, dma_in_ch[1]);
    dma_channel_configure(dma_in_ch[0], &r0,
                          rx_buf[0], &pio_in->rxf[sm_in],
                          AUDIO_BUFFER_TOTAL, false);

    dma_channel_config r1 = dma_channel_get_default_config(dma_in_ch[1]);
    channel_config_set_transfer_data_size(&r1, DMA_SIZE_32);
    channel_config_set_read_increment(&r1, false);
    channel_config_set_write_increment(&r1, true);
    channel_config_set_dreq(&r1, pio_get_dreq(pio_in, sm_in, false));
    channel_config_set_chain_to(&r1, dma_in_ch[0]);
    dma_channel_configure(dma_in_ch[1], &r1,
                          rx_buf[1], &pio_in->rxf[sm_in],
                          AUDIO_BUFFER_TOTAL, false);

    /* IRQ on TX DMA completion */
    dma_channel_set_irq0_enabled(dma_out_ch[0], true);
    dma_channel_set_irq0_enabled(dma_out_ch[1], true);

    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}

void I2SAudio_Start(void) {
    memset(tx_buf, 0, sizeof(tx_buf));

    dma_channel_start(dma_out_ch[0]);
    dma_channel_start(dma_in_ch[0]);

    pio_sm_set_enabled(pio_out, sm_out, true);
    pio_sm_set_enabled(pio_in, sm_in, true);
}

void I2SAudio_Stop(void) {
    pio_sm_set_enabled(pio_out, sm_out, false);
    pio_sm_set_enabled(pio_in, sm_in, false);

    for (int i = 0; i < 2; i++) {
        dma_channel_abort(dma_out_ch[i]);
        dma_channel_abort(dma_in_ch[i]);
    }
}

uint32_t I2SAudio_GetFrameCount(void) { return frame_count; }
uint32_t I2SAudio_GetUnderruns(void)  { return underruns; }
