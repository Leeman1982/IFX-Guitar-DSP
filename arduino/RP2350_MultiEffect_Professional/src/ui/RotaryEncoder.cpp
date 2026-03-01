#include "RotaryEncoder.h"

RotaryEncoderUI::RotaryEncoderUI(uint8_t clkPin, uint8_t dtPin, uint8_t swPin)
    : pinCLK(clkPin), pinDT(dtPin), pinSW(swPin),
      position(0), lastPosition(0), buttonPressed(false),
      buttonClicked(false), buttonLongPress(false),
      lastCLK(HIGH), lastDT(HIGH), lastButtonTime(0), buttonPressTime(0) {
}

void RotaryEncoderUI::begin() {
    pinMode(pinCLK, INPUT_PULLUP);
    pinMode(pinDT, INPUT_PULLUP);
    pinMode(pinSW, INPUT_PULLUP);

    lastCLK = digitalRead(pinCLK);
    lastDT = digitalRead(pinDT);
    position = 0;
    lastPosition = 0;
}

void RotaryEncoderUI::update() {
    // Read current state
    uint8_t currentCLK = digitalRead(pinCLK);
    uint8_t currentDT = digitalRead(pinDT);
    uint8_t currentSW = digitalRead(pinSW);

    // Encoder rotation detection
    if (currentCLK != lastCLK) {
        if (currentCLK == LOW) {
            // CLK changed from HIGH to LOW
            if (currentDT == HIGH) {
                position++; // Clockwise
            } else {
                position--; // Counter-clockwise
            }
        }
    }

    lastCLK = currentCLK;
    lastDT = currentDT;

    // Button press detection with debouncing
    uint32_t now = millis();

    if (currentSW == LOW && !buttonPressed) {
        // Button just pressed
        if (now - lastButtonTime > DEBOUNCE_TIME) {
            buttonPressed = true;
            buttonPressTime = now;
            lastButtonTime = now;
        }
    } else if (currentSW == HIGH && buttonPressed) {
        // Button just released
        if (now - lastButtonTime > DEBOUNCE_TIME) {
            buttonPressed = false;
            lastButtonTime = now;

            // Check if it was a long press
            if (now - buttonPressTime >= LONG_PRESS_TIME) {
                buttonLongPress = true;
            } else {
                buttonClicked = true;
            }
        }
    }

    // Check for long press while still pressed
    if (buttonPressed && !buttonLongPress) {
        if (now - buttonPressTime >= LONG_PRESS_TIME) {
            buttonLongPress = true;
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

bool RotaryEncoderUI::wasLongPress() {
    if (buttonLongPress) {
        buttonLongPress = false;
        return true;
    }
    return false;
}

int8_t RotaryEncoderUI::getDelta() {
    int32_t delta = position - lastPosition;
    lastPosition = position;

    // Clamp to int8_t range
    if (delta > 127) delta = 127;
    if (delta < -128) delta = -128;

    return (int8_t)delta;
}
