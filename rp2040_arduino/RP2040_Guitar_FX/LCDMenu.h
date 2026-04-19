#pragma once
// =============================================================================
// LCDMenu – hierarchical menu for 16×2 HD44780 via PCF8574 I2C backpack
//
// Navigation:
//   Encoder rotate  →  move cursor / change value
//   Encoder press   →  enter / next parameter
//   Footswitches    →  toggle effect bypass (any screen)
//
// Menu tree:
//   HOME       : effect status overview  →  press = EFFECT_SELECT
//   EFFECT_SELECT : pick DIST / CHORUS / EQ  →  press = PARAM_EDIT (first param)
//   PARAM_EDIT   : rotate changes value, press advances to next param,
//                  wrapping back to EFFECT_SELECT after last param
// =============================================================================
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "effect_params.h"
#include "config.h"

enum MenuState {
    MS_HOME,
    MS_EFFECT_SELECT,
    MS_DIST_PARAM,
    MS_CHORUS_PARAM,
    MS_EQ_PARAM
};

// Parameter descriptor – one per editable field
struct Param {
    const char     *label;       // up to 8 chars
    volatile float *value;       // direct pointer into g_dist/g_chorus/g_eq – keeps volatile
    float           minVal;
    float           maxVal;
    float           step;
    bool            isFreq;      // display as Hz/kHz
    bool            isDb;        // display with dB suffix
    bool            isPct;       // display as 0-100%
};

class LCDMenu {
public:
    LCDMenu(uint8_t i2c_addr, uint8_t cols, uint8_t rows);
    void begin();

    // Call from Core 0 loop
    void update(int8_t encDelta, bool encPressed, bool forceRedraw = false);

    // Called externally when a footswitch fires (just triggers a redraw)
    void notifyBypassChange();

private:
    LiquidCrystal_I2C _lcd;
    MenuState         _state;
    uint8_t           _effectSel;   // 0=DIST, 1=CHORUS, 2=EQ
    uint8_t           _paramIdx;    // index within current effect's param list
    bool              _dirty;

    // ── Parameter tables ─────────────────────────────────────────────────
    Param _distParams[5];
    Param _chorusParams[4];
    Param _eqParams[5];

    uint8_t _distParamCount;
    uint8_t _chorusParamCount;
    uint8_t _eqParamCount;

    void buildParamTables();

    // Screen renderers
    void drawHome();
    void drawEffectSelect();
    void drawParamEdit(Param *params, uint8_t count, const char *effectName);

    // Value formatting helpers
    void formatValue(char *buf, const Param &p);
    void applyDelta (Param &p, int8_t delta);
    void markNeedsUpdate(MenuState s);
};
