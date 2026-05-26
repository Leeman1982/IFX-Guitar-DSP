#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

typedef enum {
    ENC_EVENT_NONE = 0,
    ENC_EVENT_CW,
    ENC_EVENT_CCW,
    ENC_EVENT_PRESS,
    ENC_EVENT_LONG_PRESS,
    ENC_EVENT_FX1_TOGGLE,
    ENC_EVENT_FX2_TOGGLE,
    ENC_EVENT_FX3_TOGGLE,
    ENC_EVENT_FX4_TOGGLE,
    ENC_EVENT_FX5_TOGGLE,
    ENC_EVENT_BACK,
    ENC_EVENT_CONFIRM,
} EncoderEvent;

/*
 * EC11 rotary encoder + CD74HC4067 MUX button scanner.
 *
 * A/B quadrature: decoded in a GPIO interrupt service routine (fires on
 * CHANGE of either pin) for zero-latency edge capture, then consumed by
 * poll() which generates CW/CCW events.
 *
 * Buttons (CH0-7 on MUX): polled at the UI refresh rate in poll().
 * All events share the same 16-entry ring queue; read with getEvent().
 */
class RotaryEncoder {
public:
    void init();
    void poll();
    EncoderEvent getEvent();
    bool hasEvent();
    int32_t getPosition();   /* absolute step count from ISR */

private:
    void pushEvent(EncoderEvent evt);
    bool readMuxChannel(uint8_t ch);

    /* Tracks the last position that has been converted to events.
     * The absolute position counter lives in the file-scope ISR state. */
    int32_t dispatchedPos;

    /* MUX button debounce — one entry per channel (CH0..CH7) */
    bool     muxState[8];
    uint32_t muxLastChange[8];

    /* Encoder SW long-press tracking */
    uint32_t swPressStart;
    bool     swLongFired;

    EncoderEvent events[16];
    uint8_t evtHead;
    uint8_t evtTail;
};

#endif
