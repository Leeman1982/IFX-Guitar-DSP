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
 * Handles EC11 rotary encoder (A/B quadrature on direct GPIO) plus
 * all 8 buttons routed through the CD74HC4067 MUX:
 *   CH0-4 = FX1-5 footswitches
 *   CH5   = BACK nav button
 *   CH6   = CONFIRM nav button
 *   CH7   = encoder push (with long-press detection)
 * All events are queued and read via getEvent().
 */
class RotaryEncoder {
public:
    void init();
    void poll();
    EncoderEvent getEvent();
    bool hasEvent();
    int32_t getPosition();

private:
    void pushEvent(EncoderEvent evt);
    bool readMuxChannel(uint8_t ch);

    int32_t position;
    uint8_t lastAB;

    /* MUX button debounce state — one entry per channel (CH0..CH7) */
    bool     muxState[8];       /* debounced: true = pressed          */
    uint32_t muxLastChange[8];  /* millis() when state last changed   */

    /* Encoder push long-press tracking */
    uint32_t swPressStart;
    bool     swLongFired;

    EncoderEvent events[16];
    uint8_t evtHead;
    uint8_t evtTail;
};

#endif
