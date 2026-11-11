#pragma once
/*
 * Tiny Arduino-API shim for the bare-metal STM32F405 port: just enough for
 * the ZombiSS UI files (sh1106_oled, rotary_encoder, ui_system, splash) to
 * compile unchanged.  Pin numbers are board.h's PIN(port, bit) codes.
 */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>
#include "board.h"

#define HIGH 1
#define LOW  0
#define INPUT        0
#define OUTPUT       1
#define INPUT_PULLUP 2
#define INPUT_PULLDOWN 3
#define CHANGE  1
#define RISING  2
#define FALLING 3
#define HEX 16
#define DEC 10

#ifdef __cplusplus
extern "C" {
#endif
void     pinMode(uint8_t pin, uint8_t mode);
void     digitalWrite(uint8_t pin, uint8_t v);
int      digitalRead(uint8_t pin);
uint32_t millis(void);
uint32_t micros(void);
void     delay(uint32_t ms);
void     delayMicroseconds(uint32_t us);
/* any pin 0..15 of any port (one pin per EXTI line); runs at NVIC priority 12 */
void     attachInterrupt(uint8_t pin, void (*isr)(void), int mode);
static inline uint8_t digitalPinToInterrupt(uint8_t pin) { return pin; }
static inline void tight_loop_contents(void) { }
#ifdef __cplusplus
}

/* Serial: there is no console on this port — calls compile and do nothing. */
class NullSerial {
public:
    void begin(unsigned long) {}
    template <typename T> size_t print(const T &) { return 0; }
    template <typename T> size_t print(const T &, int) { return 0; }
    template <typename T> size_t println(const T &) { return 0; }
    template <typename T> size_t println(const T &, int) { return 0; }
    size_t println() { return 0; }
    int printf(const char *, ...) { return 0; }
};
extern NullSerial Serial;
#endif
