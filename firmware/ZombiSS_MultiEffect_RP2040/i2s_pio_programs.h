#ifndef I2S_PIO_PROGRAMS_H
#define I2S_PIO_PROGRAMS_H

/*
 * Pre-assembled PIO programs for I2S I/O (RP2040 version)
 * PIO hardware is identical between RP2040 and RP2350.
 * Only the clock divider changes (133MHz sys_clk, 32kHz sample rate).
 */

#include "hardware/pio.h"
#include "hardware/clocks.h"

/* ============================================================
 * I2S OUTPUT (PCM5102 DAC) - same program as RP2350
 * ============================================================ */

static const uint16_t i2s_out_program_instructions[] = {
    0xE03E, /* 0: set x, 30       side 0b00 */
    0x6001, /* 1: out pins, 1     side 0b00 */
    0x10C1, /* 2: jmp x--, 1      side 0b10 */
    0x6001, /* 3: out pins, 1     side 0b00 */
    0xB042, /* 4: nop             side 0b10 */
    0xE83E, /* 5: set x, 30       side 0b01 */
    0x6801, /* 6: out pins, 1     side 0b01 */
    0x18C6, /* 7: jmp x--, 6      side 0b11 */
    0x6801, /* 8: out pins, 1     side 0b01 */
    0xB842, /* 9: nop             side 0b11 */
};

static const struct pio_program i2s_out_program = {
    .instructions = i2s_out_program_instructions,
    .length = 10,
    .origin = -1,
};

static inline void i2s_out_program_init(PIO pio, uint sm, uint offset,
                                         uint data_pin, uint bck_pin) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + 0, offset + 9);
    sm_config_set_out_pins(&c, data_pin, 1);
    sm_config_set_sideset_pins(&c, bck_pin);
    sm_config_set_sideset(&c, 2, false, false);

    pio_gpio_init(pio, data_pin);
    pio_gpio_init(pio, bck_pin);
    pio_gpio_init(pio, bck_pin + 1);
    pio_sm_set_consecutive_pindirs(pio, sm, data_pin, 1, true);
    pio_sm_set_consecutive_pindirs(pio, sm, bck_pin, 2, true);

    sm_config_set_out_shift(&c, false, true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    /* RP2040: 133MHz sys_clk, 32kHz sample rate
     * BCK = 32000 * 64 = 2.048 MHz, PIO = 2 * BCK = 4.096 MHz
     * clkdiv = 133000000 / 4096000 = 32.47 */
    float sys_clk = (float)clock_get_hz(clk_sys);
    float target = 32000.0f * 64.0f * 2.0f;
    sm_config_set_clkdiv(&c, sys_clk / target);

    pio_sm_init(pio, sm, offset, &c);
}


/* ============================================================
 * I2S INPUT (PCM1808 ADC) - same program as RP2350
 * ============================================================ */

static const uint16_t i2s_in_program_instructions[] = {
    0x2021, /*  0: wait 0 pin 1       */
    0xE03F, /*  1: set x, 31          */
    0x2022, /*  2: wait 0 pin 2       */
    0x20A2, /*  3: wait 1 pin 2       */
    0x4001, /*  4: in pins, 1         */
    0x00C2, /*  5: jmp x--, 2         */
    0x8020, /*  6: push block         */
    0x20A1, /*  7: wait 1 pin 1       */
    0xE03F, /*  8: set x, 31          */
    0x2022, /*  9: wait 0 pin 2       */
    0x20A2, /* 10: wait 1 pin 2       */
    0x4001, /* 11: in pins, 1         */
    0x00C8, /* 12: jmp x--, 8         */
    0x8020, /* 13: push block         */
    0x0000, /* 14: jmp 0              */
};

static const struct pio_program i2s_in_program = {
    .instructions = i2s_in_program_instructions,
    .length = 15,
    .origin = -1,
};

static inline void i2s_in_program_init(PIO pio, uint sm, uint offset,
                                        uint data_pin, uint lrck_pin, uint bck_pin) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + 14, offset + 14);
    sm_config_set_in_pins(&c, data_pin);
    sm_config_set_in_shift(&c, false, false, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

    pio_sm_set_consecutive_pindirs(pio, sm, data_pin, 3, false);
    pio_gpio_init(pio, data_pin);
    pio_gpio_init(pio, lrck_pin);
    pio_gpio_init(pio, bck_pin);

    sm_config_set_clkdiv(&c, 1.0f);
    pio_sm_init(pio, sm, offset, &c);
}

#endif
