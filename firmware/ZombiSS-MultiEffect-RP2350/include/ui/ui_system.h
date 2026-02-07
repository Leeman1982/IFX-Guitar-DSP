#ifndef UI_SYSTEM_H
#define UI_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include "sh1106_oled.h"
#include "rotary_encoder.h"
#include "effect_chain.h"

/* UI screen modes */
typedef enum {
    UI_SCREEN_SPLASH,
    UI_SCREEN_CHAIN_VIEW,     /* Shows all effects in chain with on/off status */
    UI_SCREEN_EFFECT_PARAMS,  /* Shows parameters for selected effect */
    UI_SCREEN_PARAM_EDIT,     /* Editing a single parameter value */
    UI_SCREEN_MASTER_VOL,     /* Master volume control */
} UIScreen;

typedef struct {
    SH1106 *oled;
    RotaryEncoder *encoder;
    EffectChain *chain;

    UIScreen screen;
    uint8_t selectedEffect;    /* Index in FX_COUNT for chain view */
    uint8_t selectedParam;     /* Index within effect's params */
    bool needsRedraw;

    /* Scrolling state */
    uint8_t scrollOffset;

    /* Animation timer */
    uint32_t lastUpdateMs;
    uint32_t animFrame;
} UISystem;

void UISystem_Init(UISystem *ui, SH1106 *oled, RotaryEncoder *encoder, EffectChain *chain);
void UISystem_Update(UISystem *ui);
void UISystem_ForceRedraw(UISystem *ui);

#endif
