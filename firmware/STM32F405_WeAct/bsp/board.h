#ifndef BOARD_H
#define BOARD_H
/*
 * WeAct Studio "STM32F4 Core Board", STM32F405RGT6 (64-pin, USB-C, 8 MHz HSE).
 * Bare-metal board support: clocks, SysTick, GPIO.  CMSIS registers only — no HAL.
 *
 * On-board:
 *   LED   PB2   (active high)            KEY  PC13 (to GND, needs pull-up)
 *   BOOT0 button "B0", reset button "NR", SWD header 3V3 / DIO / CLK / GND
 *   USB-C on PA11/PA12 (ROM DFU bootloader: hold B0, tap NR, release B0)
 * Avoid PA4–PA7 (footprint for the optional SPI flash on the back) and
 * PA13/PA14 (SWD).
 */
#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pin = port·16 + bit.  These numbers are also the "Arduino pin numbers" of
 * the small Arduino-API shim, so ZombiSS code that does pinMode(PIN_PB2…)
 * works unchanged. */
#define PIN(port, bit)   ((uint8_t)(((port) << 4) | (bit)))
#define PORT_A 0
#define PORT_B 1
#define PORT_C 2
#define PORT_D 3
#define PA(n) PIN(PORT_A, n)
#define PB(n) PIN(PORT_B, n)
#define PC(n) PIN(PORT_C, n)
#define PD(n) PIN(PORT_D, n)

#define BOARD_LED_PIN    PB(2)
#define BOARD_KEY_PIN    PC(13)

#define BOARD_HSE_HZ     8000000u
#define BOARD_SYSCLK_HZ  168000000u

typedef enum { GPIO_IN = 0, GPIO_OUT = 1, GPIO_AF = 2, GPIO_ANALOG = 3 } gpio_mode_t;
typedef enum { GPIO_NOPULL = 0, GPIO_PULLUP = 1, GPIO_PULLDOWN = 2 } gpio_pull_t;

GPIO_TypeDef *gpio_port(uint8_t pin);
void  gpio_config(uint8_t pin, gpio_mode_t mode, gpio_pull_t pull, uint8_t af,
                  bool open_drain, uint8_t speed /* 0..3 */);
static inline void gpio_write(uint8_t pin, bool v) {
    gpio_port(pin)->BSRR = v ? (1u << (pin & 15)) : (1u << ((pin & 15) + 16));
}
static inline bool gpio_read(uint8_t pin) { return (gpio_port(pin)->IDR >> (pin & 15)) & 1u; }
static inline void gpio_toggle(uint8_t pin) { gpio_port(pin)->ODR ^= 1u << (pin & 15); }

/* 168 MHz from the 8 MHz crystal, 48 MHz USB clock, PLLI2S = 86 MHz
 * (→ fs = 47 991 Hz with MCLK = 256·fs).  Returns false if the HSE did not
 * start (then the chip keeps running on the 16 MHz HSI and the LED blinks fast). */
bool  board_clock_init(void);
void  board_init(void);              /* clocks + SysTick 1 kHz + DWT + LED */
uint32_t board_millis(void);
void  board_delay_ms(uint32_t ms);
void  board_delay_us(uint32_t us);   /* DWT busy-wait */
static inline uint32_t board_cycles(void) { return DWT->CYCCNT; }

/* Optional 1 kHz hook, called from SysTick (the Arduino shim uses it). */
extern void (*board_tick_hook)(void);

#ifdef __cplusplus
}
#endif
#endif
