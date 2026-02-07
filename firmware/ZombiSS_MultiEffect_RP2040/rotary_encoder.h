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

class RotaryEncoder {
public:
    void init();
    void poll();
    EncoderEvent getEvent();
    bool hasEvent();
    int32_t getPosition();

private:
    void pushEvent(EncoderEvent evt);

    int32_t position;
    uint8_t lastAB;

    uint32_t swLastTime;
    bool swPressed;
    bool swLongDetected;
    uint32_t swPressStart;

    uint32_t fxLastTime[5];
    bool fxLastState[5];

    uint32_t backLastTime;
    bool backLastState;
    uint32_t confirmLastTime;
    bool confirmLastState;

    EncoderEvent events[16];
    uint8_t evtHead;
    uint8_t evtTail;
};

#endif
