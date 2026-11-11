/* Board smoke test for the WeAct STM32F405 core board.
 *   LED PB2 blinks at 1 Hz (crystal + 168 MHz PLL OK).
 *   Hold KEY (PC13): blinks at 5 Hz (GPIO input OK).
 *   Fast flicker that never stops: the 8 MHz crystal did not start.
 *   4 quick flashes + pause, repeating: a fault handler ran.
 * Flash it first: if this works, the toolchain, pedalflash and DFU are fine. */
#include "board.h"

int main(void) {
    board_init();
    for (;;) {
        bool key = !gpio_read(BOARD_KEY_PIN);          /* pressed = low */
        gpio_toggle(BOARD_LED_PIN);
        board_delay_ms(key ? 100 : 500);
    }
}
