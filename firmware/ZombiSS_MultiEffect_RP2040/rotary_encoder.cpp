#include "rotary_encoder.h"
#include <Arduino.h>

void RotaryEncoder::pushEvent(EncoderEvent evt) {
    uint8_t next = (evtHead + 1) & 0x0F;
    if (next != evtTail) {
        events[evtHead] = evt;
        evtHead = next;
    }
}

void RotaryEncoder::init() {
    position = 0;
    evtHead = 0;
    evtTail = 0;
    swPressed = false;
    swLongDetected = false;
    swPressStart = 0;

    pinMode(ENCODER_PIN_A, INPUT_PULLUP);
    pinMode(ENCODER_PIN_B, INPUT_PULLUP);
    pinMode(ENCODER_PIN_SW, INPUT_PULLUP);

    lastAB = (digitalRead(ENCODER_PIN_A) << 1) | digitalRead(ENCODER_PIN_B);
    swLastTime = millis();

    const uint8_t fxPins[] = {SWITCH_FX1_PIN, SWITCH_FX2_PIN, SWITCH_FX3_PIN,
                               SWITCH_FX4_PIN, SWITCH_FX5_PIN};
    for (int i = 0; i < 5; i++) {
        pinMode(fxPins[i], INPUT_PULLUP);
        fxLastTime[i] = 0;
        fxLastState[i] = true;
    }

    pinMode(SWITCH_BACK_PIN, INPUT_PULLUP);
    backLastTime = 0;
    backLastState = true;

    pinMode(SWITCH_CONFIRM_PIN, INPUT_PULLUP);
    confirmLastTime = 0;
    confirmLastState = true;
}

void RotaryEncoder::poll() {
    uint32_t now = millis();

    /* Quadrature decoding */
    uint8_t a = digitalRead(ENCODER_PIN_A) ? 1 : 0;
    uint8_t b = digitalRead(ENCODER_PIN_B) ? 1 : 0;
    uint8_t ab = (a << 1) | b;

    if (ab != lastAB) {
        static const int8_t encTable[] = {
             0, +1, -1,  0,
            -1,  0,  0, +1,
            +1,  0,  0, -1,
             0, -1, +1,  0
        };
        int8_t delta = encTable[(lastAB << 2) | ab];
        position += delta;
        if (delta > 0)      pushEvent(ENC_EVENT_CW);
        else if (delta < 0) pushEvent(ENC_EVENT_CCW);
        lastAB = ab;
    }

    /* Encoder push button */
    bool swNow = !digitalRead(ENCODER_PIN_SW);
    if (swNow != swPressed && (now - swLastTime) > DEBOUNCE_MS) {
        swLastTime = now;
        swPressed = swNow;
        if (swNow) {
            swPressStart = now;
            swLongDetected = false;
        } else {
            if (!swLongDetected) pushEvent(ENC_EVENT_PRESS);
        }
    }
    if (swPressed && !swLongDetected && (now - swPressStart) > LONG_PRESS_MS) {
        swLongDetected = true;
        pushEvent(ENC_EVENT_LONG_PRESS);
    }

    /* FX momentary switches */
    const uint8_t fxPins[] = {SWITCH_FX1_PIN, SWITCH_FX2_PIN, SWITCH_FX3_PIN,
                               SWITCH_FX4_PIN, SWITCH_FX5_PIN};
    const EncoderEvent fxEvents[] = {ENC_EVENT_FX1_TOGGLE, ENC_EVENT_FX2_TOGGLE,
                                      ENC_EVENT_FX3_TOGGLE, ENC_EVENT_FX4_TOGGLE,
                                      ENC_EVENT_FX5_TOGGLE};
    for (int i = 0; i < 5; i++) {
        bool state = !digitalRead(fxPins[i]);
        if (state != fxLastState[i] && (now - fxLastTime[i]) > DEBOUNCE_MS) {
            fxLastTime[i] = now;
            fxLastState[i] = state;
            if (state) pushEvent(fxEvents[i]);
        }
    }

    /* Back button */
    bool backNow = !digitalRead(SWITCH_BACK_PIN);
    if (backNow != backLastState && (now - backLastTime) > DEBOUNCE_MS) {
        backLastTime = now;
        backLastState = backNow;
        if (backNow) pushEvent(ENC_EVENT_BACK);
    }

    /* Confirm button */
    bool confirmNow = !digitalRead(SWITCH_CONFIRM_PIN);
    if (confirmNow != confirmLastState && (now - confirmLastTime) > DEBOUNCE_MS) {
        confirmLastTime = now;
        confirmLastState = confirmNow;
        if (confirmNow) pushEvent(ENC_EVENT_CONFIRM);
    }
}

EncoderEvent RotaryEncoder::getEvent() {
    if (evtHead == evtTail) return ENC_EVENT_NONE;
    EncoderEvent evt = events[evtTail];
    evtTail = (evtTail + 1) & 0x0F;
    return evt;
}

bool RotaryEncoder::hasEvent() {
    return evtHead != evtTail;
}

int32_t RotaryEncoder::getPosition() {
    return position;
}
