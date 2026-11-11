#include "Arduino.h"
#include "Wire.h"

NullSerial Serial;
TwoWire Wire;

/* ---------------- GPIO / time ---------------- */
extern "C" void pinMode(uint8_t pin, uint8_t mode) {
    switch (mode) {
    case OUTPUT:         gpio_config(pin, GPIO_OUT, GPIO_NOPULL, 0, false, 1); break;
    case INPUT_PULLUP:   gpio_config(pin, GPIO_IN, GPIO_PULLUP, 0, false, 0); break;
    case INPUT_PULLDOWN: gpio_config(pin, GPIO_IN, GPIO_PULLDOWN, 0, false, 0); break;
    default:             gpio_config(pin, GPIO_IN, GPIO_NOPULL, 0, false, 0); break;
    }
}
extern "C" void digitalWrite(uint8_t pin, uint8_t v) { gpio_write(pin, v != 0); }
extern "C" int  digitalRead(uint8_t pin) { return gpio_read(pin) ? HIGH : LOW; }
extern "C" uint32_t millis(void) { return board_millis(); }
extern "C" uint32_t micros(void) { return DWT->CYCCNT / (SystemCoreClock / 1000000u); }
extern "C" void delay(uint32_t ms) { board_delay_ms(ms); }
extern "C" void delayMicroseconds(uint32_t us) { board_delay_us(us); }

/* ---------------- EXTI ---------------- */
static void (*s_isr[16])(void);

extern "C" void attachInterrupt(uint8_t pin, void (*isr)(void), int mode) {
    uint32_t line = pin & 15u, port = pin >> 4;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    (void)RCC->APB2ENR;
    s_isr[line] = isr;
    volatile uint32_t *cr = &SYSCFG->EXTICR[line >> 2];
    *cr = (*cr & ~(0xFu << ((line & 3u) * 4u))) | (port << ((line & 3u) * 4u));
    if (mode == RISING || mode == CHANGE)  EXTI->RTSR |= 1u << line; else EXTI->RTSR &= ~(1u << line);
    if (mode == FALLING || mode == CHANGE) EXTI->FTSR |= 1u << line; else EXTI->FTSR &= ~(1u << line);
    EXTI->PR = 1u << line;
    EXTI->IMR |= 1u << line;
    /* EXTI0..4 have their own vectors (and their IRQn values are not
     * consecutive with the rest), 5..9 and 10..15 share one each */
    static const IRQn_Type low[5] = { EXTI0_IRQn, EXTI1_IRQn, EXTI2_IRQn, EXTI3_IRQn, EXTI4_IRQn };
    IRQn_Type irq = line <= 4 ? low[line] : line <= 9 ? EXTI9_5_IRQn : EXTI15_10_IRQn;
    NVIC_SetPriority(irq, 12);
    NVIC_EnableIRQ(irq);
}

static void exti_dispatch(uint32_t first, uint32_t last) {
    uint32_t pr = EXTI->PR;
    for (uint32_t l = first; l <= last; l++) {
        if (pr & (1u << l)) {
            EXTI->PR = 1u << l;
            if (s_isr[l]) s_isr[l]();
        }
    }
}
extern "C" void EXTI0_IRQHandler(void)     { exti_dispatch(0, 0); }
extern "C" void EXTI1_IRQHandler(void)     { exti_dispatch(1, 1); }
extern "C" void EXTI2_IRQHandler(void)     { exti_dispatch(2, 2); }
extern "C" void EXTI3_IRQHandler(void)     { exti_dispatch(3, 3); }
extern "C" void EXTI4_IRQHandler(void)     { exti_dispatch(4, 4); }
extern "C" void EXTI9_5_IRQHandler(void)   { exti_dispatch(5, 9); }
extern "C" void EXTI15_10_IRQHandler(void) { exti_dispatch(10, 15); }

/* ---------------- bit-banged I2C ---------------- */
void TwoWire::setClock(uint32_t hz) {
    if (hz < 10000u) hz = 10000u;
    if (hz > 400000u) hz = 400000u;
    _halfCycles = SystemCoreClock / (2u * hz);
}
void TwoWire::hold() {
    uint32_t t0 = DWT->CYCCNT;
    while ((uint32_t)(DWT->CYCCNT - t0) < _halfCycles) { }
}
void TwoWire::begin() {
    gpio_write(_sda, 1); gpio_write(_scl, 1);
    gpio_config(_sda, GPIO_OUT, GPIO_PULLUP, 0, true, 1);
    gpio_config(_scl, GPIO_OUT, GPIO_PULLUP, 0, true, 1);
    /* free a slave stuck mid-byte: 9 clocks, then STOP */
    for (int i = 0; i < 9 && !gpio_read(_sda); i++) { gpio_write(_scl, 0); hold(); gpio_write(_scl, 1); hold(); }
    gpio_write(_sda, 0); hold(); gpio_write(_scl, 1); hold(); gpio_write(_sda, 1); hold();
}
bool TwoWire::bit(bool b) {             /* clock one bit out (b) and read SDA back */
    gpio_write(_sda, b); hold();
    gpio_write(_scl, 1);
    uint32_t t0 = millis();
    while (!gpio_read(_scl)) { if (millis() - t0 > 2) { _err = 4; break; } }   /* clock stretching */
    hold();
    bool r = gpio_read(_sda);
    gpio_write(_scl, 0);
    return r;
}
bool TwoWire::sendByte(uint8_t b) {
    for (int i = 7; i >= 0; i--) bit((b >> i) & 1u);
    return !bit(true);                  /* release SDA, ACK = low */
}
void TwoWire::beginTransmission(uint8_t addr) {
    _err = 0;
    gpio_write(_sda, 1); gpio_write(_scl, 1); hold();
    gpio_write(_sda, 0); hold();         /* START */
    gpio_write(_scl, 0);
    if (!sendByte((uint8_t)(addr << 1))) _err = 2;
}
size_t TwoWire::write(uint8_t b) {
    if (_err) return 0;
    if (!sendByte(b)) { _err = 3; return 0; }
    return 1;
}
uint8_t TwoWire::endTransmission(bool stop) {
    if (stop) {
        gpio_write(_sda, 0); hold();
        gpio_write(_scl, 1); hold();
        gpio_write(_sda, 1); hold();     /* STOP */
    }
    return _err;
}
