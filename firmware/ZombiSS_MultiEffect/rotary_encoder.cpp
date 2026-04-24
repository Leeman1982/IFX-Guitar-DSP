#include "rotary_encoder.h"
#include <Arduino.h>

/* ---- MUX helpers ------------------------------------------------ */

static inline void mux_select(uint8_t ch) {
    digitalWrite(MUX_S0_PIN, (ch >> 0) & 1);
    digitalWrite(MUX_S1_PIN, (ch >> 1) & 1);
    digitalWrite(MUX_S2_PIN, (ch >> 2) & 1);
    digitalWrite(MUX_S3_PIN, (ch >> 3) & 1);
}

bool RotaryEncoder::readMuxChannel(uint8_t ch) {
    mux_select(ch);
    delayMicroseconds(2);                      /* 74HC4067 max prop: ~540 ns at 2V, faster at 3.3V */
    return digitalRead(MUX_SIG_PIN) == LOW;   /* Active LOW: button pressed = LOW */
}

/* ---- Initialisation --------------------------------------------- */

void RotaryEncoder::init() {
    position     = 0;
    evtHead      = 0;
    evtTail      = 0;
    swPressStart = 0;
    swLongFired  = false;

    /* Encoder A/B — quadrature needs fast direct reads */
    pinMode(ENCODER_PIN_A, INPUT_PULLUP);
    pinMode(ENCODER_PIN_B, INPUT_PULLUP);
    lastAB = ((digitalRead(ENCODER_PIN_A) ? 1 : 0) << 1) |
              (digitalRead(ENCODER_PIN_B) ? 1 : 0);

    /* MUX select pins */
    pinMode(MUX_S0_PIN, OUTPUT);
    pinMode(MUX_S1_PIN, OUTPUT);
    pinMode(MUX_S2_PIN, OUTPUT);
    pinMode(MUX_S3_PIN, OUTPUT);
    mux_select(0);

    /* MUX signal — pull-up so unpressed = HIGH */
    pinMode(MUX_SIG_PIN, INPUT_PULLUP);

    /* Initialise all button state as not-pressed */
    uint32_t now = millis();
    for (uint8_t i = 0; i < 8; i++) {
        muxState[i]      = false;
        muxLastChange[i] = now;
    }
}

/* ---- Event queue ------------------------------------------------- */

void RotaryEncoder::pushEvent(EncoderEvent evt) {
    uint8_t next = (evtHead + 1) & 0x0F;
    if (next != evtTail) {        /* drop if full */
        events[evtHead] = evt;
        evtHead = next;
    }
}

/* ---- Polling ----------------------------------------------------- */

void RotaryEncoder::poll() {
    uint32_t now = millis();

    /* --- Quadrature decoding (EC11 A/B direct GPIO) --- */
    uint8_t a  = digitalRead(ENCODER_PIN_A) ? 1 : 0;
    uint8_t b  = digitalRead(ENCODER_PIN_B) ? 1 : 0;
    uint8_t ab = (a << 1) | b;

    if (ab != lastAB) {
        /* Gray-code transition table: [prev_AB << 2 | cur_AB] → delta */
        static const int8_t encTable[16] = {
             0, +1, -1,  0,
            -1,  0,  0, +1,
            +1,  0,  0, -1,
             0, -1, +1,  0
        };
        int8_t delta = encTable[(lastAB << 2) | ab];
        position += delta;
        if      (delta > 0) pushEvent(ENC_EVENT_CW);
        else if (delta < 0) pushEvent(ENC_EVENT_CCW);
        lastAB = ab;
    }

    /* --- MUX button scanning (all 8 channels) --- */
    for (uint8_t ch = 0; ch < 8; ch++) {
        bool raw  = readMuxChannel(ch);
        bool prev = muxState[ch];

        /* Debounce: ignore transitions shorter than DEBOUNCE_MS */
        if (raw == prev) continue;
        if ((now - muxLastChange[ch]) < DEBOUNCE_MS) continue;

        /* State has changed and held past debounce window — accept it */
        muxLastChange[ch] = now;
        muxState[ch]      = raw;

        if (raw) {
            /* --- Press edge --- */
            if      (ch <= 4) pushEvent((EncoderEvent)((uint8_t)ENC_EVENT_FX1_TOGGLE + ch));
            else if (ch == MUX_CH_BACK)    pushEvent(ENC_EVENT_BACK);
            else if (ch == MUX_CH_CONFIRM) pushEvent(ENC_EVENT_CONFIRM);
            else if (ch == MUX_CH_ENC_SW) {
                swPressStart = now;
                swLongFired  = false;
            }
        } else {
            /* --- Release edge --- */
            if (ch == MUX_CH_ENC_SW && !swLongFired) {
                pushEvent(ENC_EVENT_PRESS);   /* Short press on release */
            }
        }
    }

    /* Long-press detection for encoder SW (checked every poll tick) */
    if (muxState[MUX_CH_ENC_SW] && !swLongFired &&
        (now - swPressStart) >= LONG_PRESS_MS) {
        swLongFired = true;
        pushEvent(ENC_EVENT_LONG_PRESS);
    }
}

/* ---- Public accessors -------------------------------------------- */

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
