#ifndef UI_SYSTEM_H
#define UI_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include "sh1106_oled.h"
#include "rotary_encoder.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "effect_chain.h"
#ifdef __cplusplus
}
#endif

typedef enum {
    UI_SCREEN_SPLASH,
    UI_SCREEN_CHAIN_VIEW,
    UI_SCREEN_EFFECT_PARAMS,
    UI_SCREEN_PARAM_EDIT,
    UI_SCREEN_MASTER_VOL,
} UIScreen;

class UISystem {
public:
    void init(SH1106 *oled, RotaryEncoder *encoder, EffectChain *chain);
    void update();
    void forceRedraw();

private:
    void drawChainView();
    void drawEffectParams();
    void drawParamEdit();
    void drawMasterVol();

    void handleChainView(EncoderEvent evt);
    void handleEffectParams(EncoderEvent evt);
    void handleParamEdit(EncoderEvent evt);
    void handleMasterVol(EncoderEvent evt);
    void handleFxToggle(EncoderEvent evt);

    SH1106 *_oled;
    RotaryEncoder *_encoder;
    EffectChain *_chain;

    UIScreen _screen;
    uint8_t _selectedEffect;
    uint8_t _selectedParam;
    uint8_t _scrollOffset;
    bool _needsRedraw;
};

#endif
