#ifndef I2S_PIO_PROGRAMS_H
#define I2S_PIO_PROGRAMS_H

/*
 * Pre-assembled PIO programs for I2S I/O
 * (Arduino IDE cannot compile .pio files directly)
 *
 * These are hand-assembled from the original .pio sources.
 */

#include "hardware/pio.h"
#include "hardware/clocks.h"

/* ============================================================
 * I2S OUTPUT (PCM5102 DAC) — MASTER, 48 kHz, 32 BCK/channel (64 fs)
 *
 * Side-set bit 0 = BCK  (bit clock)  -> side-set BASE   = GP17
 * Side-set bit 1 = LRCK (word select)-> side-set BASE+1 = GP18
 * Out pin 0      = DIN  (serial data)                   = GP16
 *
 * Side-set value notation below is 0b<LRCK><BCK> (bit1=LRCK, bit0=BCK).
 *
 * .side_set 2
 * .wrap_target
 *   set x, 30       side 0b00    ; 0: LEFT start (LRCK=0, BCK=0), load count
 * left_loop:
 *   out pins, 1     side 0b00    ; 1: data on BCK low
 *   jmp x-- left    side 0b01    ; 2: BCK rising edge (receiver samples)
 *   out pins, 1     side 0b00    ; 3: 32nd left bit, BCK low
 *   nop             side 0b01    ; 4: BCK rising edge
 *   set x, 30       side 0b10    ; 5: RIGHT start (LRCK->1, BCK=0)
 * right_loop:
 *   out pins, 1     side 0b10    ; 6: data on BCK low
 *   jmp x-- right   side 0b11    ; 7: BCK rising edge
 *   out pins, 1     side 0b10    ; 8: 32nd right bit, BCK low
 *   nop             side 0b11    ; 9: BCK rising edge
 * .wrap                          ; -> 0: LRCK->0 (back to left)
 *
 * NOTE: opcodes verified against the pico-SDK encoder (pio_encode_*).
 *   Two fatal bugs were fixed vs. the earlier hand-assembly:
 *     1. jmp condition was 0b110 (PIN) instead of 0b010 (X--). With the
 *        default JMP_PIN = GP0 (= XSMT, held HIGH), every jump was always
 *        taken -> infinite loops -> no valid I2S -> silence.
 *     2. side-set bit order was swapped (BCK on bit1/GP18 instead of
 *        bit0/GP17), conflicting with config.h, the bridge wires and the
 *        PCM5102/PCM1808 pin map.
 * ============================================================ */

static const uint16_t i2s_out_program_instructions[] = {
    0xE03E, /* 0: set x, 30       side 0b00 */
    0x6001, /* 1: out pins, 1     side 0b00 */
    0x0841, /* 2: jmp x--, 1      side 0b01 */
    0x6001, /* 3: out pins, 1     side 0b00 */
    0xA842, /* 4: nop             side 0b01 */
    0xF03E, /* 5: set x, 30       side 0b10 */
    0x7001, /* 6: out pins, 1     side 0b10 */
    0x1846, /* 7: jmp x--, 6      side 0b11 */
    0x7001, /* 8: out pins, 1     side 0b10 */
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

    /* Wrap */
    sm_config_set_wrap(&c, offset + 0, offset + 9);

    /* OUT pin: serial data */
    sm_config_set_out_pins(&c, data_pin, 1);
    /* Side-set: BCK and LRCK (2 pins, not optional) */
    sm_config_set_sideset_pins(&c, bck_pin);
    sm_config_set_sideset(&c, 2, false, false);

    /* Configure GPIO */
    pio_gpio_init(pio, data_pin);
    pio_gpio_init(pio, bck_pin);
    pio_gpio_init(pio, bck_pin + 1);
    pio_sm_set_consecutive_pindirs(pio, sm, data_pin, 1, true);
    pio_sm_set_consecutive_pindirs(pio, sm, bck_pin, 2, true);

    /* 32-bit MSB-first autopull */
    sm_config_set_out_shift(&c, false, true, 32);

    /* Join TX FIFO for deeper buffer */
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    /* Clock divider: this program executes 130 PIO cycles per stereo frame
     * (65 per channel: 1 set-x + 31x(out+jmp) + out + nop = 65), NOT 128.
     * target = 48000 * 65 * 2 makes LRCK exactly 48 kHz, BCK = 3.072 MHz. */
    float sys_clk = (float)clock_get_hz(clk_sys);
    float target = 48000.0f * 65.0f * 2.0f;
    sm_config_set_clkdiv(&c, sys_clk / target);

    pio_sm_init(pio, sm, offset, &c);
}


/* ============================================================
 * I2S INPUT (PCM1808 ADC)
 * PIO program: slaves off BCK/LRCK driven by output PIO
 *
 * In-base pin 0 = DOUT (serial data from ADC)
 * In-base pin 1 = LRCK (from output PIO, directly wired)
 * In-base pin 2 = BCK  (from output PIO, directly wired)
 *
 * No side-set, runs at system clock to catch BCK edges.
 *
 * NOTE: opcodes verified against the pico-SDK encoder. The two jmp x--
 *   instructions previously decoded as jmp PIN (cond 0b110, always taken
 *   with JMP_PIN = GP0 held HIGH) -> infinite loop -> no captured samples.
 *
 * wait_left:
 *   wait 0 pin 1        ; 0:  Wait for LRCK low  (left channel)
 *   set x, 31           ; 1:  32 bits to read
 * left_loop:
 *   wait 0 pin 2        ; 2:  Wait for BCK low
 *   wait 1 pin 2        ; 3:  Wait for BCK high (rising edge)
 *   in pins, 1          ; 4:  Sample DOUT
 *   jmp x-- left_loop   ; 5:  Next bit
 *   push block           ; 6:  Push 32-bit left sample
 * wait_right:
 *   wait 1 pin 1        ; 7:  Wait for LRCK high (right channel)
 *   set x, 31           ; 8:  32 bits to read
 * right_loop:
 *   wait 0 pin 2        ; 9:  Wait for BCK low
 *   wait 1 pin 2        ; 10: Wait for BCK high (rising edge)
 *   in pins, 1          ; 11: Sample DOUT
 *   jmp x-- right_loop  ; 12: Next bit (target=9, NOT 8)
 *   push block           ; 13: Push 32-bit right sample
 * .wrap_target
 *   jmp wait_left       ; 14: Loop
 * .wrap
 * ============================================================ */

static const uint16_t i2s_in_program_instructions[] = {
    0x2021, /*  0: wait 0 pin 1       */
    0xE03F, /*  1: set x, 31          */
    0x2022, /*  2: wait 0 pin 2       */
    0x20A2, /*  3: wait 1 pin 2       */
    0x4001, /*  4: in pins, 1         */
    0x0042, /*  5: jmp x--, 2         */
    0x8020, /*  6: push block         */
    0x20A1, /*  7: wait 1 pin 1       */
    0xE03F, /*  8: set x, 31          */
    0x2022, /*  9: wait 0 pin 2       */
    0x20A2, /* 10: wait 1 pin 2       */
    0x4001, /* 11: in pins, 1         */
    0x0049, /* 12: jmp x--, 9         */
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

    /* Wrap: target=14, wrap=14 */
    sm_config_set_wrap(&c, offset + 14, offset + 14);

    /* IN base pin: DOUT (data_pin), then LRCK (data_pin+1), BCK (data_pin+2) */
    sm_config_set_in_pins(&c, data_pin);

    /* 32-bit MSB-first, manual push (we push explicitly) */
    sm_config_set_in_shift(&c, false, false, 32);

    /* Join RX FIFO for deeper buffer */
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

    /* All inputs */
    pio_sm_set_consecutive_pindirs(pio, sm, data_pin, 3, false);
    pio_gpio_init(pio, data_pin);
    pio_gpio_init(pio, lrck_pin);
    pio_gpio_init(pio, bck_pin);

    /* Run at system clock (fast enough to catch BCK edges) */
    sm_config_set_clkdiv(&c, 1.0f);

    pio_sm_init(pio, sm, offset, &c);
}

#endif /* I2S_PIO_PROGRAMS_H */
