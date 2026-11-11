#pragma once
/* Bit-banged I2C master with the Arduino TwoWire API (open-drain GPIO,
 * internal pull-ups enabled as a fallback; OLED modules carry their own).
 * Bytes are sent as they are written; endTransmission() sends STOP and
 * returns 0 = ok, 2 = address NACK, 3 = data NACK, 4 = bus stuck. */
#include "Arduino.h"

class TwoWire {
public:
    void setSDA(uint8_t pin) { _sda = pin; }
    void setSCL(uint8_t pin) { _scl = pin; }
    void setClock(uint32_t hz);
    void begin();
    void beginTransmission(uint8_t addr);
    size_t write(uint8_t b);
    size_t write(const uint8_t *p, size_t n) { size_t k = 0; while (n--) k += write(*p++); return k; }
    uint8_t endTransmission(bool stop = true);
private:
    bool bit(bool b);
    bool sendByte(uint8_t b);   /* true = ACK */
    void hold();
    uint8_t _sda = 0, _scl = 0;
    uint32_t _halfCycles = 210;  /* 168 MHz / (2·400 kHz) */
    uint8_t _err = 0;
};
extern TwoWire Wire;
