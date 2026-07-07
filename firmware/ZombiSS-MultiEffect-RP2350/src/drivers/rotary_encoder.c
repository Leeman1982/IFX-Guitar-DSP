#include "rotary_encoder.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

static void push_event(RotaryEncoder *enc, EncoderEvent evt) {
    uint8_t next = (enc->evt_head + 1) & 0x0F;
    if (next != enc->evt_tail) {
        enc->events[enc->evt_head] = evt;
        enc->evt_head = next;
    }
}

void RotaryEncoder_Init(RotaryEncoder *enc) {
    enc->position = 0;
    enc->evt_head = 0;
    enc->evt_tail = 0;
    enc->sw_pressed = false;
    enc->sw_long_detected = false;
    enc->sw_press_start = 0;

    /* Init encoder pins with pull-ups */
    gpio_init(ENCODER_PIN_A);
    gpio_set_dir(ENCODER_PIN_A, GPIO_IN);
    gpio_pull_up(ENCODER_PIN_A);

    gpio_init(ENCODER_PIN_B);
    gpio_set_dir(ENCODER_PIN_B, GPIO_IN);
    gpio_pull_up(ENCODER_PIN_B);

    gpio_init(ENCODER_PIN_SW);
    gpio_set_dir(ENCODER_PIN_SW, GPIO_IN);
    gpio_pull_up(ENCODER_PIN_SW);

    /* Read initial state */
    enc->last_ab = (gpio_get(ENCODER_PIN_A) << 1) | gpio_get(ENCODER_PIN_B);
    enc->sw_last_time = to_ms_since_boot(get_absolute_time());

    /* Init FX momentary switches */
    const uint8_t fx_pins[] = {SWITCH_FX1_PIN, SWITCH_FX2_PIN, SWITCH_FX3_PIN,
                                SWITCH_FX4_PIN, SWITCH_FX5_PIN};
    for (int i = 0; i < 5; i++) {
        gpio_init(fx_pins[i]);
        gpio_set_dir(fx_pins[i], GPIO_IN);
        gpio_pull_up(fx_pins[i]);
        enc->fx_last_time[i] = 0;
        enc->fx_last_state[i] = true; /* Pull-up: idle = high */
    }

    /* Back and Confirm buttons */
    gpio_init(SWITCH_BACK_PIN);
    gpio_set_dir(SWITCH_BACK_PIN, GPIO_IN);
    gpio_pull_up(SWITCH_BACK_PIN);
    enc->back_last_time = 0;
    enc->back_last_state = true;

    gpio_init(SWITCH_CONFIRM_PIN);
    gpio_set_dir(SWITCH_CONFIRM_PIN, GPIO_IN);
    gpio_pull_up(SWITCH_CONFIRM_PIN);
    enc->confirm_last_time = 0;
    enc->confirm_last_state = true;
}

void RotaryEncoder_Poll(RotaryEncoder *enc) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    /* --- Rotary encoder quadrature decoding --- */
    uint8_t a = gpio_get(ENCODER_PIN_A) ? 1 : 0;
    uint8_t b = gpio_get(ENCODER_PIN_B) ? 1 : 0;
    uint8_t ab = (a << 1) | b;

    if (ab != enc->last_ab) {
        /* Gray code state table for reliable decoding */
        /* States: 00=0, 01=1, 11=3, 10=2 */
        /* CW:  00->01->11->10->00 */
        /* CCW: 00->10->11->01->00 */
        static const int8_t enc_table[] = {
             0, +1, -1,  0,
            -1,  0,  0, +1,
            +1,  0,  0, -1,
             0, -1, +1,  0
        };
        int8_t delta = enc_table[(enc->last_ab << 2) | ab];
        enc->position += delta;

        if (delta > 0) push_event(enc, ENC_EVENT_CW);
        else if (delta < 0) push_event(enc, ENC_EVENT_CCW);

        enc->last_ab = ab;
    }

    /* --- Encoder push button --- */
    bool sw_now = !gpio_get(ENCODER_PIN_SW); /* Active low */
    if (sw_now != enc->sw_pressed && (now - enc->sw_last_time) > DEBOUNCE_MS) {
        enc->sw_last_time = now;
        enc->sw_pressed = sw_now;
        if (sw_now) {
            enc->sw_press_start = now;
            enc->sw_long_detected = false;
        } else {
            if (!enc->sw_long_detected) {
                push_event(enc, ENC_EVENT_PRESS);
            }
        }
    }
    /* Long press detection */
    if (enc->sw_pressed && !enc->sw_long_detected && (now - enc->sw_press_start) > 500) {
        enc->sw_long_detected = true;
        push_event(enc, ENC_EVENT_LONG_PRESS);
    }

    /* --- FX momentary switches (toggle on press) --- */
    const uint8_t fx_pins[] = {SWITCH_FX1_PIN, SWITCH_FX2_PIN, SWITCH_FX3_PIN,
                                SWITCH_FX4_PIN, SWITCH_FX5_PIN};
    const EncoderEvent fx_events[] = {ENC_EVENT_FX1_TOGGLE, ENC_EVENT_FX2_TOGGLE,
                                       ENC_EVENT_FX3_TOGGLE, ENC_EVENT_FX4_TOGGLE,
                                       ENC_EVENT_FX5_TOGGLE};
    for (int i = 0; i < 5; i++) {
        bool state = !gpio_get(fx_pins[i]); /* Active low */
        if (state != enc->fx_last_state[i] && (now - enc->fx_last_time[i]) > DEBOUNCE_MS) {
            enc->fx_last_time[i] = now;
            enc->fx_last_state[i] = state;
            if (state) { /* On press (falling edge) */
                push_event(enc, fx_events[i]);
            }
        }
    }

    /* --- Back button --- */
    bool back_now = !gpio_get(SWITCH_BACK_PIN);
    if (back_now != enc->back_last_state && (now - enc->back_last_time) > DEBOUNCE_MS) {
        enc->back_last_time = now;
        enc->back_last_state = back_now;
        if (back_now) push_event(enc, ENC_EVENT_BACK);
    }

    /* --- Confirm button --- */
    bool confirm_now = !gpio_get(SWITCH_CONFIRM_PIN);
    if (confirm_now != enc->confirm_last_state && (now - enc->confirm_last_time) > DEBOUNCE_MS) {
        enc->confirm_last_time = now;
        enc->confirm_last_state = confirm_now;
        if (confirm_now) push_event(enc, ENC_EVENT_CONFIRM);
    }
}

EncoderEvent RotaryEncoder_GetEvent(RotaryEncoder *enc) {
    if (enc->evt_head == enc->evt_tail) return ENC_EVENT_NONE;
    EncoderEvent evt = enc->events[enc->evt_tail];
    enc->evt_tail = (enc->evt_tail + 1) & 0x0F;
    return evt;
}

bool RotaryEncoder_HasEvent(RotaryEncoder *enc) {
    return enc->evt_head != enc->evt_tail;
}

int32_t RotaryEncoder_GetPosition(RotaryEncoder *enc) {
    return enc->position;
}
