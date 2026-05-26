#include "rotary_encoder.h"
#include <Arduino.h>
#include "hardware/sync.h"   /* save_and_disable_interrupts / restore_interrupts */

/* ================================================================
 * Interrupt-driven A/B quadrature state
 * These are file-scope (not class members) so the static ISR can
 * access them without a global class pointer.
 * ================================================================ */

static volatile int32_t s_enc_pos    = 0;
static volatile uint8_t s_enc_lastAB = 0;

/* Gray-code transition table: index = (prevAB << 2) | curAB → step delta */
static const int8_t ENC_TABLE[16] = {
     0, +1, -1,  0,
    -1,  0,  0, +1,
    +1,  0,  0, -1,
     0, -1, +1,  0
};

/* ISR — fires on CHANGE of either ENCODER_PIN_A or ENCODER_PIN_B.
 * Registered on Core 1 (called from setup1 via attachInterrupt) so it
 * executes on Core 1 only; no cross-core synchronisation required here. */
static void enc_isr() {
    uint8_t a  = (digitalRead(ENCODER_PIN_A) != 0) ? 1u : 0u;
    uint8_t b  = (digitalRead(ENCODER_PIN_B) != 0) ? 1u : 0u;
    uint8_t ab = (uint8_t)((a << 1u) | b);
    s_enc_pos += ENC_TABLE[(s_enc_lastAB << 2u) | ab];
    s_enc_lastAB = ab;
}

/* ================================================================
 * MUX helpers
 * ================================================================ */

static inline void mux_select(uint8_t ch) {
    digitalWrite(MUX_S0_PIN, (ch >> 0) & 1);
    digitalWrite(MUX_S1_PIN, (ch >> 1) & 1);
    digitalWrite(MUX_S2_PIN, (ch >> 2) & 1);
    digitalWrite(MUX_S3_PIN, (ch >> 3) & 1);
}

bool RotaryEncoder::readMuxChannel(uint8_t ch) {
    mux_select(ch);
    delayMicroseconds(2);                    /* 74HC4067 max prop delay ~540 ns @ 2 V; well within 2 µs @ 3.3 V */
    return digitalRead(MUX_SIG_PIN) == LOW; /* active LOW: button pressed = SIG pulled to GND */
}

/* ================================================================
 * Initialisation
 * ================================================================ */

void RotaryEncoder::init() {
    dispatchedPos = 0;
    evtHead       = 0;
    evtTail       = 0;
    swPressStart  = 0;
    swLongFired   = false;

    /* Encoder A/B — attach GPIO interrupts for zero-latency edge capture.
     * Must be called from Core 1 (setup1) so the ISR fires on Core 1. */
    pinMode(ENCODER_PIN_A, INPUT_PULLUP);
    pinMode(ENCODER_PIN_B, INPUT_PULLUP);

    s_enc_lastAB = (uint8_t)(((digitalRead(ENCODER_PIN_A) != 0) ? 2u : 0u) |
                              ((digitalRead(ENCODER_PIN_B) != 0) ? 1u : 0u));
    s_enc_pos    = 0;

    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), enc_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), enc_isr, CHANGE);

    /* MUX select lines */
    pinMode(MUX_S0_PIN, OUTPUT);
    pinMode(MUX_S1_PIN, OUTPUT);
    pinMode(MUX_S2_PIN, OUTPUT);
    pinMode(MUX_S3_PIN, OUTPUT);
    mux_select(0);

    /* MUX signal — internal pull-up so unpressed channel reads HIGH */
    pinMode(MUX_SIG_PIN, INPUT_PULLUP);

    uint32_t now = millis();
    for (uint8_t i = 0; i < 8; i++) {
        muxState[i]      = false;
        muxLastChange[i] = now;
    }
}

/* ================================================================
 * Event queue
 * ================================================================ */

void RotaryEncoder::pushEvent(EncoderEvent evt) {
    uint8_t next = (evtHead + 1) & 0x0F;
    if (next != evtTail) {
        events[evtHead] = evt;
        evtHead = next;
    }
}

/* ================================================================
 * Polling — called from loop1() at ~60 Hz
 * ================================================================ */

void RotaryEncoder::poll() {
    uint32_t now = millis();

    /* --- Encoder rotation (ISR-maintained position) -------------- */
    /* Briefly disable interrupts on Core 1 to atomically snapshot s_enc_pos.
     * The window is one load instruction — negligible impact on ISR latency. */
    uint32_t irq_save = save_and_disable_interrupts();
    int32_t  cur_pos  = s_enc_pos;
    restore_interrupts(irq_save);

    int32_t delta = cur_pos - dispatchedPos;
    dispatchedPos = cur_pos;
    while (delta > 0) { pushEvent(ENC_EVENT_CW);  delta--; }
    while (delta < 0) { pushEvent(ENC_EVENT_CCW); delta++; }

    /* --- MUX button scanning (CH0..CH7) -------------------------- */
    for (uint8_t ch = 0; ch < 8; ch++) {
        bool raw  = readMuxChannel(ch);
        bool prev = muxState[ch];

        if (raw == prev) continue;
        if ((now - muxLastChange[ch]) < (uint32_t)DEBOUNCE_MS) continue;

        muxLastChange[ch] = now;
        muxState[ch]      = raw;

        if (raw) {
            /* Press edge */
            if      (ch <= 4u)              pushEvent((EncoderEvent)((uint8_t)ENC_EVENT_FX1_TOGGLE + ch));
            else if (ch == MUX_CH_BACK)     pushEvent(ENC_EVENT_BACK);
            else if (ch == MUX_CH_CONFIRM)  pushEvent(ENC_EVENT_CONFIRM);
            else if (ch == MUX_CH_ENC_SW) { swPressStart = now; swLongFired = false; }
        } else {
            /* Release edge */
            if (ch == MUX_CH_ENC_SW && !swLongFired)
                pushEvent(ENC_EVENT_PRESS);
        }
    }

    /* Long-press: checked every poll tick while encoder SW held */
    if (muxState[MUX_CH_ENC_SW] && !swLongFired &&
        (now - swPressStart) >= (uint32_t)LONG_PRESS_MS) {
        swLongFired = true;
        pushEvent(ENC_EVENT_LONG_PRESS);
    }
}

/* ================================================================
 * Public accessors
 * ================================================================ */

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
    uint32_t save = save_and_disable_interrupts();
    int32_t  p    = s_enc_pos;
    restore_interrupts(save);
    return p;
}
