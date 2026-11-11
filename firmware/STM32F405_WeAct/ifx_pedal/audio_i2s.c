#include "audio_i2s.h"
#include "board.h"

/*
 * DMA1 Stream3 / channel 3 = I2S2_EXT_RX, Stream4 / channel 0 = SPI2_TX (RM0090
 * table 42).  32-bit data in 16-bit DR → 2 half-words per channel, MSB half first;
 * a frame is [L hi, L lo, R hi, R lo].  Buffers live in SRAM (DMA cannot reach CCM).
 * Processing is driven by the RX half/complete interrupts; the TX half with the
 * same index is written, so output latency is one block (1.33 ms) + converters.
 */
#define HW_PER_FRAME 4
#define HALF_HW      (AUDIO_FRAMES * HW_PER_FRAME)
static uint16_t s_rx[2 * HALF_HW] __attribute__((section(".sram"), aligned(4)));
static uint16_t s_tx[2 * HALF_HW] __attribute__((section(".sram"), aligned(4)));
static audio_callback_t s_cb;
static volatile uint32_t s_late;

uint32_t audio_late_blocks(void) { return s_late; }

static void process(int half) {
    const uint16_t *rx = &s_rx[half * HALF_HW];
    uint16_t *tx = &s_tx[half * HALF_HW];
    float in[AUDIO_FRAMES], out[AUDIO_FRAMES];
    for (int i = 0; i < AUDIO_FRAMES; i++) {
        int32_t l = (int32_t)(((uint32_t)rx[i * 4] << 16) | rx[i * 4 + 1]);
        in[i] = (float)l * (1.0f / 2147483648.0f);
    }
    s_cb(in, out, AUDIO_FRAMES);
    for (int i = 0; i < AUDIO_FRAMES; i++) {
        float x = out[i];
        if (x != x) x = 0.0f;                         /* NaN guard */
        if (x > 1.0f) x = 1.0f; else if (x < -1.0f) x = -1.0f;
        uint32_t o = (uint32_t)(int32_t)(x * 2147483392.0f);
        tx[i * 4]     = (uint16_t)(o >> 16);
        tx[i * 4 + 1] = (uint16_t)o;
        tx[i * 4 + 2] = (uint16_t)(o >> 16);
        tx[i * 4 + 3] = (uint16_t)o;
    }
}

void DMA1_Stream3_IRQHandler(void) {
    uint32_t f = DMA1->LISR;
    if (f & DMA_LISR_HTIF3) { DMA1->LIFCR = DMA_LIFCR_CHTIF3; process(0); }
    if (f & DMA_LISR_TCIF3) { DMA1->LIFCR = DMA_LIFCR_CTCIF3; process(1); }
    if (f & (DMA_LISR_TEIF3 | DMA_LISR_DMEIF3 | DMA_LISR_FEIF3))
        DMA1->LIFCR = DMA_LIFCR_CTEIF3 | DMA_LIFCR_CDMEIF3 | DMA_LIFCR_CFEIF3;
    /* the next half already arrived while we were busy → overrun */
    if (DMA1->LISR & (DMA_LISR_HTIF3 | DMA_LISR_TCIF3)) s_late++;
}

void audio_start(audio_callback_t cb) {
    s_cb = cb;
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    (void)RCC->APB1ENR;

    /* pins: PB12 WS, PB13 CK, PB15 SD = AF5; PB14 I2S2ext_SD = AF6; PC6 MCK = AF5 */
    gpio_config(PB(12), GPIO_AF, GPIO_NOPULL, 5, false, 2);
    gpio_config(PB(13), GPIO_AF, GPIO_NOPULL, 5, false, 3);
    gpio_config(PB(15), GPIO_AF, GPIO_NOPULL, 5, false, 2);
    gpio_config(PB(14), GPIO_AF, GPIO_NOPULL, 6, false, 2);
    gpio_config(PC(6),  GPIO_AF, GPIO_NOPULL, 5, false, 3);

    /* I2S2 master TX, Philips, 32-bit data in 32-bit channel, MCK on.
     * fs = 86 MHz / (256 · (2·3 + 1)) = 47 991 Hz */
    SPI2->I2SCFGR = 0;
    I2S2ext->I2SCFGR = 0;
    SPI2->I2SPR = 3u | SPI_I2SPR_ODD | SPI_I2SPR_MCKOE;
    SPI2->I2SCFGR = SPI_I2SCFGR_I2SMOD | SPI_I2SCFGR_I2SCFG_1 | SPI_I2SCFGR_DATLEN_1 | SPI_I2SCFGR_CHLEN;
    I2S2ext->I2SCFGR = SPI_I2SCFGR_I2SMOD | SPI_I2SCFGR_I2SCFG_0 | SPI_I2SCFGR_DATLEN_1 | SPI_I2SCFGR_CHLEN;

    for (unsigned i = 0; i < 2 * HALF_HW; i++) { s_tx[i] = 0; s_rx[i] = 0; }

    /* RX: Stream3 channel 3, periph→mem, 16/16-bit, circular, HT+TC IRQs, very high prio */
    DMA1_Stream3->CR = 0;
    while (DMA1_Stream3->CR & DMA_SxCR_EN) { }
    DMA1->LIFCR = DMA_LIFCR_CTCIF3 | DMA_LIFCR_CHTIF3 | DMA_LIFCR_CTEIF3 | DMA_LIFCR_CDMEIF3 | DMA_LIFCR_CFEIF3;
    DMA1_Stream3->PAR  = (uint32_t)&I2S2ext->DR;
    DMA1_Stream3->M0AR = (uint32_t)s_rx;
    DMA1_Stream3->NDTR = 2 * HALF_HW;
    DMA1_Stream3->FCR  = 0;                                   /* direct mode */
    DMA1_Stream3->CR = (3u << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_PL | DMA_SxCR_MSIZE_0 | DMA_SxCR_PSIZE_0 |
                       DMA_SxCR_MINC | DMA_SxCR_CIRC | DMA_SxCR_HTIE | DMA_SxCR_TCIE;

    /* TX: Stream4 channel 0, mem→periph, same shape, no IRQs */
    DMA1_Stream4->CR = 0;
    while (DMA1_Stream4->CR & DMA_SxCR_EN) { }
    DMA1->HIFCR = DMA_HIFCR_CTCIF4 | DMA_HIFCR_CHTIF4 | DMA_HIFCR_CTEIF4 | DMA_HIFCR_CDMEIF4 | DMA_HIFCR_CFEIF4;
    DMA1_Stream4->PAR  = (uint32_t)&SPI2->DR;
    DMA1_Stream4->M0AR = (uint32_t)s_tx;
    DMA1_Stream4->NDTR = 2 * HALF_HW;
    DMA1_Stream4->FCR  = 0;
    DMA1_Stream4->CR = (0u << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_PL_1 | DMA_SxCR_MSIZE_0 | DMA_SxCR_PSIZE_0 |
                       DMA_SxCR_MINC | DMA_SxCR_CIRC | DMA_SxCR_DIR_0;

    NVIC_SetPriority(DMA1_Stream3_IRQn, 1);
    NVIC_EnableIRQ(DMA1_Stream3_IRQn);

    /* same order as ST's HAL_I2SEx_TransmitReceive_DMA: DMA streams, then
     * the ext (slave) before the master, so both start on the first frame */
    DMA1_Stream3->CR |= DMA_SxCR_EN;
    DMA1_Stream4->CR |= DMA_SxCR_EN;
    I2S2ext->CR2 |= SPI_CR2_RXDMAEN;
    I2S2ext->I2SCFGR |= SPI_I2SCFGR_I2SE;
    SPI2->CR2 |= SPI_CR2_TXDMAEN;
    SPI2->I2SCFGR |= SPI_I2SCFGR_I2SE;
}
