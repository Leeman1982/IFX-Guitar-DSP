#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

#include <stdint.h>
#include <stdbool.h>

/* Pin assignments for rotary encoder (EC11 on Estardyn module) */
#define ENCODER_PIN_A      10
#define ENCODER_PIN_B      11
#define ENCODER_PIN_SW     12   /* Push button (active low) */

/* Pins for additional momentary effect bypass switches */
#define SWITCH_FX1_PIN     6    /* Noise Gate toggle */
#define SWITCH_FX2_PIN     7    /* Overdrive toggle */
#define SWITCH_FX3_PIN     8    /* EQ toggle */
#define SWITCH_FX4_PIN     9    /* Chorus toggle */
#define SWITCH_FX5_PIN     13   /* Delay toggle */

/* Back button on the module */
#define SWITCH_BACK_PIN    14
/* Confirm button on the module */
#define SWITCH_CONFIRM_PIN 15

/* Debounce time in ms */
#define DEBOUNCE_MS        5

typedef enum {
    ENC_EVENT_NONE = 0,
    ENC_EVENT_CW,           /* Clockwise rotation (increment) */
    ENC_EVENT_CCW,          /* Counter-clockwise rotation (decrement) */
    ENC_EVENT_PRESS,        /* Push button pressed */
    ENC_EVENT_LONG_PRESS,   /* Push button held >500ms */
    ENC_EVENT_FX1_TOGGLE,
    ENC_EVENT_FX2_TOGGLE,
    ENC_EVENT_FX3_TOGGLE,
    ENC_EVENT_FX4_TOGGLE,
    ENC_EVENT_FX5_TOGGLE,
    ENC_EVENT_BACK,
    ENC_EVENT_CONFIRM,
} EncoderEvent;

typedef struct {
    /* Encoder state */
    int32_t position;
    uint8_t last_ab;

    /* Button states with debouncing */
    uint32_t sw_last_time;
    bool sw_pressed;
    bool sw_long_detected;
    uint32_t sw_press_start;

    /* FX switch debounce */
    uint32_t fx_last_time[5];
    bool fx_last_state[5];

    /* Back/Confirm debounce */
    uint32_t back_last_time;
    bool back_last_state;
    uint32_t confirm_last_time;
    bool confirm_last_state;

    /* Event queue (simple ring buffer) */
    EncoderEvent events[16];
    uint8_t evt_head;
    uint8_t evt_tail;
} RotaryEncoder;

void         RotaryEncoder_Init(RotaryEncoder *enc);
void         RotaryEncoder_Poll(RotaryEncoder *enc);
EncoderEvent RotaryEncoder_GetEvent(RotaryEncoder *enc);
bool         RotaryEncoder_HasEvent(RotaryEncoder *enc);
int32_t      RotaryEncoder_GetPosition(RotaryEncoder *enc);

#endif
