#ifndef ROTARY_ENCODER_UI_H
#define ROTARY_ENCODER_UI_H

#include <Arduino.h>

class RotaryEncoderUI {
public:
    RotaryEncoderUI(uint8_t clkPin, uint8_t dtPin, uint8_t swPin);

    void begin();
    void update();

    int32_t getPosition() const { return position; }
    void setPosition(int32_t pos) { position = pos; }
    void resetPosition() { position = 0; }

    bool wasClicked();
    bool isPressed() const { return buttonPressed; }

    int8_t getDelta();  // Returns change since last call

private:
    uint8_t pinCLK;
    uint8_t pinDT;
    uint8_t pinSW;

    volatile int32_t position;
    volatile bool buttonPressed;
    volatile bool buttonClicked;

    uint8_t lastCLK;
    uint8_t lastDT;
    uint32_t lastButtonTime;

    static const uint32_t DEBOUNCE_TIME = 50; // ms
};

#endif
