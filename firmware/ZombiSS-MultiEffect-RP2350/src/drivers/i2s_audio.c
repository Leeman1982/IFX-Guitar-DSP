#include "i2s_audio.h"
#include "i2s_out.pio.h"
#include "i2s_in.pio.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include <string.h>

I2SAudio g_audio;

static void dma_irq_handler(void) {
    I2SAudio_DMA_IRQ_Handler();
}

void I2SAudio_Init(audio_callback_t callback) {
    memset(&g_audio, 0, sizeof(g_audio));
    g_audio.callback = callback;

    /* --- I2S Output (DAC) on PIO0 --- */
    g_audio.pio_out = pio0;
    g_audio.sm_out = pio_claim_unused_sm(g_audio.pio_out, true);
    uint offset_out = pio_add_program(g_audio.pio_out, &i2s_out_program);
    i2s_out_program_init(g_audio.pio_out, g_audio.sm_out, offset_out,
                         I2S_OUT_DIN_PIN, I2S_OUT_BCK_PIN);
    /* Pause until start */
    pio_sm_set_enabled(g_audio.pio_out, g_audio.sm_out, false);

    /* --- I2S Input (ADC) on PIO1 --- */
    g_audio.pio_in = pio1;
    g_audio.sm_in = pio_claim_unused_sm(g_audio.pio_in, true);
    uint offset_in = pio_add_program(g_audio.pio_in, &i2s_in_program);
    i2s_in_program_init(g_audio.pio_in, g_audio.sm_in, offset_in,
                        I2S_IN_DOUT_PIN, I2S_IN_LRCK_PIN, I2S_IN_BCK_PIN);
    pio_sm_set_enabled(g_audio.pio_in, g_audio.sm_in, false);

    /* --- DMA for TX (output to DAC) --- */
    for (int i = 0; i < 2; i++) {
        g_audio.dma_out_ch[i] = dma_claim_unused_channel(true);
    }

    /* Configure TX DMA channel 0 */
    dma_channel_config c_tx0 = dma_channel_get_default_config(g_audio.dma_out_ch[0]);
    channel_config_set_transfer_data_size(&c_tx0, DMA_SIZE_32);
    channel_config_set_read_increment(&c_tx0, true);
    channel_config_set_write_increment(&c_tx0, false);
    channel_config_set_dreq(&c_tx0, pio_get_dreq(g_audio.pio_out, g_audio.sm_out, true));
    channel_config_set_chain_to(&c_tx0, g_audio.dma_out_ch[1]);
    dma_channel_configure(g_audio.dma_out_ch[0], &c_tx0,
                          &g_audio.pio_out->txf[g_audio.sm_out],
                          g_audio.tx_buf[0],
                          AUDIO_BUFFER_TOTAL, false);

    /* Configure TX DMA channel 1 */
    dma_channel_config c_tx1 = dma_channel_get_default_config(g_audio.dma_out_ch[1]);
    channel_config_set_transfer_data_size(&c_tx1, DMA_SIZE_32);
    channel_config_set_read_increment(&c_tx1, true);
    channel_config_set_write_increment(&c_tx1, false);
    channel_config_set_dreq(&c_tx1, pio_get_dreq(g_audio.pio_out, g_audio.sm_out, true));
    channel_config_set_chain_to(&c_tx1, g_audio.dma_out_ch[0]);
    dma_channel_configure(g_audio.dma_out_ch[1], &c_tx1,
                          &g_audio.pio_out->txf[g_audio.sm_out],
                          g_audio.tx_buf[1],
                          AUDIO_BUFFER_TOTAL, false);

    /* --- DMA for RX (input from ADC) --- */
    for (int i = 0; i < 2; i++) {
        g_audio.dma_in_ch[i] = dma_claim_unused_channel(true);
    }

    /* Configure RX DMA channel 0 */
    dma_channel_config c_rx0 = dma_channel_get_default_config(g_audio.dma_in_ch[0]);
    channel_config_set_transfer_data_size(&c_rx0, DMA_SIZE_32);
    channel_config_set_read_increment(&c_rx0, false);
    channel_config_set_write_increment(&c_rx0, true);
    channel_config_set_dreq(&c_rx0, pio_get_dreq(g_audio.pio_in, g_audio.sm_in, false));
    channel_config_set_chain_to(&c_rx0, g_audio.dma_in_ch[1]);
    dma_channel_configure(g_audio.dma_in_ch[0], &c_rx0,
                          g_audio.rx_buf[0],
                          &g_audio.pio_in->rxf[g_audio.sm_in],
                          AUDIO_BUFFER_TOTAL, false);

    /* Configure RX DMA channel 1 */
    dma_channel_config c_rx1 = dma_channel_get_default_config(g_audio.dma_in_ch[1]);
    channel_config_set_transfer_data_size(&c_rx1, DMA_SIZE_32);
    channel_config_set_read_increment(&c_rx1, false);
    channel_config_set_write_increment(&c_rx1, true);
    channel_config_set_dreq(&c_rx1, pio_get_dreq(g_audio.pio_in, g_audio.sm_in, false));
    channel_config_set_chain_to(&c_rx1, g_audio.dma_in_ch[0]);
    dma_channel_configure(g_audio.dma_in_ch[1], &c_rx1,
                          g_audio.rx_buf[1],
                          &g_audio.pio_in->rxf[g_audio.sm_in],
                          AUDIO_BUFFER_TOTAL, false);

    /* Enable IRQ on TX DMA completion (both channels) */
    dma_channel_set_irq0_enabled(g_audio.dma_out_ch[0], true);
    dma_channel_set_irq0_enabled(g_audio.dma_out_ch[1], true);

    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}

void I2SAudio_Start(void) {
    /* Pre-fill TX buffers with silence */
    memset(g_audio.tx_buf, 0, sizeof(g_audio.tx_buf));
    g_audio.tx_buf_idx = 0;
    g_audio.rx_buf_idx = 0;

    /* Start DMA transfers */
    dma_channel_start(g_audio.dma_out_ch[0]);
    dma_channel_start(g_audio.dma_in_ch[0]);

    /* Enable PIO state machines */
    pio_sm_set_enabled(g_audio.pio_out, g_audio.sm_out, true);
    pio_sm_set_enabled(g_audio.pio_in, g_audio.sm_in, true);
}

void I2SAudio_Stop(void) {
    pio_sm_set_enabled(g_audio.pio_out, g_audio.sm_out, false);
    pio_sm_set_enabled(g_audio.pio_in, g_audio.sm_in, false);

    dma_channel_abort(g_audio.dma_out_ch[0]);
    dma_channel_abort(g_audio.dma_out_ch[1]);
    dma_channel_abort(g_audio.dma_in_ch[0]);
    dma_channel_abort(g_audio.dma_in_ch[1]);
}

void I2SAudio_DMA_IRQ_Handler(void) {
    /* Check which TX DMA channel completed */
    for (int i = 0; i < 2; i++) {
        if (dma_channel_get_irq0_status(g_audio.dma_out_ch[i])) {
            dma_channel_acknowledge_irq0(g_audio.dma_out_ch[i]);

            /* The buffer that just finished playing is now free to fill.
               The other buffer is currently being played by DMA.
               Process audio: read from corresponding RX buffer, write to this TX buffer. */
            uint8_t buf_to_fill = i;

            if (g_audio.callback) {
                g_audio.processing = true;
                g_audio.callback(g_audio.rx_buf[buf_to_fill],
                                g_audio.tx_buf[buf_to_fill],
                                AUDIO_BUFFER_FRAMES);
                g_audio.processing = false;
            }

            g_audio.frame_count += AUDIO_BUFFER_FRAMES;

            /* Re-configure the completed channel's read address for next cycle */
            dma_channel_set_read_addr(g_audio.dma_out_ch[i],
                                      g_audio.tx_buf[i], false);
            dma_channel_set_write_addr(g_audio.dma_in_ch[i],
                                       g_audio.rx_buf[i], false);
        }
    }
}
