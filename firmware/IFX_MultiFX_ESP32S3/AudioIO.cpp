#include "AudioIO.h"
#include "config.h"

#include <Arduino.h>
#include <driver/i2s_std.h>

static i2s_chan_handle_t s_txHandle = NULL;
static i2s_chan_handle_t s_rxHandle = NULL;

bool AudioIO_Init(void) {

    /* One port, both directions -> full duplex with shared BCK/WS */
    i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chanCfg.dma_desc_num  = 6;
    chanCfg.dma_frame_num = BLOCK_FRAMES;
    chanCfg.auto_clear    = true;

    if (i2s_new_channel(&chanCfg, &s_txHandle, &s_rxHandle) != ESP_OK) {
        return false;
    }

    i2s_std_config_t stdCfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE_HZ),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = (gpio_num_t) PIN_I2S_MCLK,
            .bclk = (gpio_num_t) PIN_I2S_BCLK,
            .ws   = (gpio_num_t) PIN_I2S_WS,
            .dout = (gpio_num_t) PIN_I2S_DOUT,
            .din  = (gpio_num_t) PIN_I2S_DIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    /* PCM1808 needs a 256fs system clock */
    stdCfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

    if (i2s_channel_init_std_mode(s_txHandle, &stdCfg) != ESP_OK) {
        return false;
    }

    if (i2s_channel_init_std_mode(s_rxHandle, &stdCfg) != ESP_OK) {
        return false;
    }

    if (i2s_channel_enable(s_txHandle) != ESP_OK) {
        return false;
    }

    if (i2s_channel_enable(s_rxHandle) != ESP_OK) {
        return false;
    }

    return true;

}

bool AudioIO_Read(int32_t *buf, size_t frames) {

    size_t bytesRead = 0;
    size_t bytesWanted = frames * 2 * sizeof(int32_t);

    esp_err_t err = i2s_channel_read(s_rxHandle, buf, bytesWanted, &bytesRead, portMAX_DELAY);

    return (err == ESP_OK) && (bytesRead == bytesWanted);

}

bool AudioIO_Write(const int32_t *buf, size_t frames) {

    size_t bytesWritten = 0;
    size_t bytesToWrite = frames * 2 * sizeof(int32_t);

    esp_err_t err = i2s_channel_write(s_txHandle, buf, bytesToWrite, &bytesWritten, portMAX_DELAY);

    return (err == ESP_OK) && (bytesWritten == bytesToWrite);

}
