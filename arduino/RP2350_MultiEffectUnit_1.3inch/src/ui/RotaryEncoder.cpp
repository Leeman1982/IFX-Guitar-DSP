#include "RotaryEncoder.h"

RotaryEncoderUI::RotaryEncoderUI(uint8_t clkPin, uint8_t dtPin, uint8_t swPin)
    : pinCLK(clkPin), pinDT(dtPin), pinSW(swPin),
      position(0), buttonPressed(false), buttonClicked(false),
      lastCLK(0), lastDT(0), lastButtonTime(0) {
}

void RotaryEncoderUI::begin() {
    pinMode(pinCLK, INPUT_PULLUP);
    pinMode(pinDT, INPUT_PULLUP);
    pinMode(pinSW, INPUT_PULLUP);

    lastCLK = digitalRead(pinCLK);
    lastDT = digitalRead(pinDT);
}

void RotaryEncoderUI::update() {
    // Read current state
    uint8_t clkState = digitalRead(pinCLK);
    uint8_t dtState = digitalRead(pinDT);

    // Check for rotation
    if (clkState != lastCLK) {
        // CLK changed
        if (clkState == LOW) {
            // Falling edge of CLK
            if (dtState == HIGH) {
                position++;  // Clockwise
            } else {
                position--;  // Counter-clockwise
            }
        }
    }

    lastCLK = clkState;
    lastDT = dtState;

    // Check button
    uint32_t now = millis();
    uint8_t swState = digitalRead(pinSW);

    if (swState == LOW && !buttonPressed) {
        // Button just pressed
        if (now - lastButtonTime > DEBOUNCE_TIME) {
            buttonPressed = true;
            buttonClicked = true;
            lastButtonTime = now;
        }
    } else if (swState == HIGH && buttonPressed) {
        // Button released
        if (now - lastButtonTime > DEBOUNCE_TIME) {
            buttonPressed = false;
            lastButtonTime = now;
        }
    }
}

bool RotaryEncoderUI::wasClicked() {
    if (buttonClicked) {
        buttonClicked = false;
        return true;
    }
    return false;
}

int8_t RotaryEncoderUI::getDelta() {
    static int32_t lastPosition = 0;
    int32_t delta = position - lastPosition;
    lastPosition = position;

    if (delta > 127) delta = 127;
    if (delta < -127) delta = -127;

    return (int8_t)delta;
}
