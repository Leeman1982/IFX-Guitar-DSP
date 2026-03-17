#include "ui_system.h"
#include "splash_screen.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

static void format_value(char *buf, size_t sz, float value, const char *unit) {
    if (value >= 1000.0f)      snprintf(buf, sz, "%.1fk%s", value / 1000.0f, unit);
    else if (value >= 100.0f)  snprintf(buf, sz, "%.0f%s", value, unit);
    else if (value >= 10.0f)   snprintf(buf, sz, "%.1f%s", value, unit);
    else if (value >= 0.0f)    snprintf(buf, sz, "%.2f%s", value, unit);
    else if (value > -10.0f)   snprintf(buf, sz, "%.2f%s", value, unit);
    else                       snprintf(buf, sz, "%.1f%s", value, unit);
}

void UISystem::drawChainView() {
    _oled->clear();
    _oled->fillRect(0, 0, 128, 9, true);
    _oled->drawString(1, 1, "ZOMBI SS  FX CHAIN", true);

    /* Show 5 effects at a time; scroll if FX_COUNT > 5 */
    const uint8_t maxVis = 5;
    if (_selectedEffect < _chainScrollOffset)
        _chainScrollOffset = _selectedEffect;
    if (_selectedEffect >= _chainScrollOffset + maxVis)
        _chainScrollOffset = _selectedEffect - maxVis + 1;

    for (uint8_t i = 0; i < maxVis && (i + _chainScrollOffset) < FX_COUNT; i++) {
        uint8_t fi = i + _chainScrollOffset;
        int y = 10 + i * 9;  /* rows at y=10,19,28,37,46; last ends at y=54, footer at y=57 */
        bool sel = (fi == _selectedEffect);
        bool act = _chain->active[fi];
        if (sel) _oled->fillRect(0, y, 128, 9, true);

        char line[22];
        snprintf(line, sizeof(line), "%d %s%-8s", fi+1, act ? "[ON]  " : "[OFF] ", _chain->fxNames[fi]);
        _oled->drawString(2, y+1, line, sel);

        if (act && !sel) _oled->fillRect(122, y+2, 4, 5, true);
    }

    /* Scroll indicators in header right side (invert=true = black on white bar) */
    if (_chainScrollOffset > 0)
        _oled->drawString(112, 1, "^", true);
    if (_chainScrollOffset + maxVis < FX_COUNT)
        _oled->drawString(118, 1, "v", true);

    _oled->drawString(0, 57, "Turn:Sel Push:Edit", false);

    _oled->flush();
}

void UISystem::drawEffectParams() {
    _oled->clear();
    uint8_t fx = _selectedEffect;
    uint8_t count = _chain->paramCount[fx];

    _oled->fillRect(0, 0, 128, 9, true);
    char header[22];
    snprintf(header, sizeof(header), "%s %s PARAMS", _chain->fxNames[fx],
             _chain->active[fx] ? "[ON]" : "[OFF]");
    _oled->drawString(1, 1, header, true);

    uint8_t maxVis = 5;
    if (_selectedParam < _scrollOffset) _scrollOffset = _selectedParam;
    if (_selectedParam >= _scrollOffset + maxVis) _scrollOffset = _selectedParam - maxVis + 1;

    for (uint8_t i = 0; i < maxVis && (i + _scrollOffset) < count; i++) {
        uint8_t pi = i + _scrollOffset;
        int y = 11 + i * 10;
        bool sel = (pi == _selectedParam);
        ParamDesc *p = &_chain->params[fx][pi];
        if (sel) _oled->fillRect(0, y, 128, 10, true);
        _oled->drawString(2, y+1, p->name, sel);

        char val[16];
        format_value(val, sizeof(val), p->value, p->unit);
        int vx = 128 - ((int)strlen(val) * 6) - 2;
        _oled->drawString(vx, y+1, val, sel);
    }

    if (_scrollOffset > 0) _oled->drawString(60, 11, "^", false);
    if (_scrollOffset + maxVis < count) _oled->drawString(60, 56, "v", false);
    _oled->drawString(0, 57, "Turn:Sel Push:Edit", false);
    _oled->flush();
}

void UISystem::drawParamEdit() {
    _oled->clear();
    uint8_t fx = _selectedEffect;
    uint8_t pi = _selectedParam;
    ParamDesc *p = &_chain->params[fx][pi];

    _oled->fillRect(0, 0, 128, 9, true);
    char header[22];
    snprintf(header, sizeof(header), "%s > %s", _chain->fxNames[fx], p->name);
    _oled->drawString(1, 1, header, true);

    char val[16];
    format_value(val, sizeof(val), p->value, p->unit);
    int tw = (int)strlen(val) * 12;
    int tx = (128 - tw) / 2;
    if (tx < 0) tx = 0;
    _oled->drawStringLarge(tx, 16, val, false);

    float frac = (p->value - p->min) / (p->max - p->min);
    _oled->drawProgressBar(4, 36, 120, 8, frac);

    char minS[10], maxS[10];
    format_value(minS, sizeof(minS), p->min, "");
    format_value(maxS, sizeof(maxS), p->max, "");
    _oled->drawString(4, 47, minS, false);
    int mx = 128 - ((int)strlen(maxS) * 6) - 4;
    _oled->drawString(mx, 47, maxS, false);

    _oled->drawString(0, 57, "Turn:Adj  Push:Done", false);
    _oled->flush();
}

void UISystem::drawMasterVol() {
    _oled->clear();
    _oled->fillRect(0, 0, 128, 9, true);
    _oled->drawString(1, 1, "MASTER VOLUME", true);

    char val[10];
    snprintf(val, sizeof(val), "%d%%", (int)(_chain->masterVolume * 100.0f));
    int tw = (int)strlen(val) * 12;
    int tx = (128 - tw) / 2;
    _oled->drawStringLarge(tx, 18, val, false);

    _oled->drawProgressBar(4, 40, 120, 10, _chain->masterVolume);
    _oled->drawString(0, 57, "Turn:Adj  Push:Back", false);
    _oled->flush();
}

void UISystem::handleChainView(EncoderEvent evt) {
    switch (evt) {
    case ENC_EVENT_CW:  if (_selectedEffect < FX_COUNT-1) _selectedEffect++; _needsRedraw = true; break;
    case ENC_EVENT_CCW: if (_selectedEffect > 0) _selectedEffect--; _needsRedraw = true; break;
    case ENC_EVENT_PRESS:
        _screen = UI_SCREEN_EFFECT_PARAMS; _selectedParam = 0; _scrollOffset = 0; _needsRedraw = true; break;
    case ENC_EVENT_LONG_PRESS: _screen = UI_SCREEN_MASTER_VOL; _needsRedraw = true; break;
    case ENC_EVENT_CONFIRM:
        EffectChain_ToggleEffect(_chain, _selectedEffect); _needsRedraw = true; break;
    default: break;
    }
}

void UISystem::handleEffectParams(EncoderEvent evt) {
    uint8_t count = _chain->paramCount[_selectedEffect];
    switch (evt) {
    case ENC_EVENT_CW:  if (_selectedParam < count-1) _selectedParam++; _needsRedraw = true; break;
    case ENC_EVENT_CCW: if (_selectedParam > 0) _selectedParam--; _needsRedraw = true; break;
    case ENC_EVENT_PRESS: _screen = UI_SCREEN_PARAM_EDIT; _needsRedraw = true; break;
    case ENC_EVENT_BACK: _screen = UI_SCREEN_CHAIN_VIEW; _needsRedraw = true; break;
    case ENC_EVENT_CONFIRM:
        EffectChain_ToggleEffect(_chain, _selectedEffect); _needsRedraw = true; break;
    default: break;
    }
}

void UISystem::handleParamEdit(EncoderEvent evt) {
    ParamDesc *p = &_chain->params[_selectedEffect][_selectedParam];
    switch (evt) {
    case ENC_EVENT_CW:
        EffectChain_SetParam(_chain, _selectedEffect, _selectedParam, p->value + p->step);
        _needsRedraw = true; break;
    case ENC_EVENT_CCW:
        EffectChain_SetParam(_chain, _selectedEffect, _selectedParam, p->value - p->step);
        _needsRedraw = true; break;
    case ENC_EVENT_PRESS: case ENC_EVENT_BACK: case ENC_EVENT_CONFIRM:
        _screen = UI_SCREEN_EFFECT_PARAMS; _needsRedraw = true; break;
    default: break;
    }
}

void UISystem::handleMasterVol(EncoderEvent evt) {
    switch (evt) {
    case ENC_EVENT_CW:
        EffectChain_SetMasterVolume(_chain, _chain->masterVolume + 0.02f); _needsRedraw = true; break;
    case ENC_EVENT_CCW:
        EffectChain_SetMasterVolume(_chain, _chain->masterVolume - 0.02f); _needsRedraw = true; break;
    case ENC_EVENT_PRESS: case ENC_EVENT_BACK:
        _screen = UI_SCREEN_CHAIN_VIEW; _needsRedraw = true; break;
    default: break;
    }
}

void UISystem::handleFxToggle(EncoderEvent evt) {
    switch (evt) {
    case ENC_EVENT_FX1_TOGGLE: EffectChain_ToggleEffect(_chain, FX_TSBOOST);   break;
    case ENC_EVENT_FX2_TOGGLE: EffectChain_ToggleEffect(_chain, FX_NOISEGATE); break;
    case ENC_EVENT_FX3_TOGGLE: EffectChain_ToggleEffect(_chain, FX_OVERDRIVE); break;
    case ENC_EVENT_FX4_TOGGLE: EffectChain_ToggleEffect(_chain, FX_EQ);        break;
    case ENC_EVENT_FX5_TOGGLE: EffectChain_ToggleEffect(_chain, FX_CHORUS);    break;
    /* FX_DELAY has no footswitch — toggled via OLED menu CONFIRM button */
    default: break;
    }
    _needsRedraw = true;
}

void UISystem::init(SH1106 *oled, RotaryEncoder *encoder, EffectChain *chain) {
    _oled = oled;
    _encoder = encoder;
    _chain = chain;
    _screen = UI_SCREEN_SPLASH;
    _selectedEffect = 0;
    _selectedParam = 0;
    _scrollOffset = 0;
    _chainScrollOffset = 0;
    _needsRedraw = true;
}

void UISystem::update() {
    if (_screen == UI_SCREEN_SPLASH) {
        Splash_Show(_oled);
        _screen = UI_SCREEN_CHAIN_VIEW;
        _needsRedraw = true;
        return;
    }

    _encoder->poll();

    EncoderEvent evt;
    while ((evt = _encoder->getEvent()) != ENC_EVENT_NONE) {
        if (evt >= ENC_EVENT_FX1_TOGGLE && evt <= ENC_EVENT_FX5_TOGGLE) {
            handleFxToggle(evt);
            continue;
        }
        switch (_screen) {
        case UI_SCREEN_CHAIN_VIEW:    handleChainView(evt); break;
        case UI_SCREEN_EFFECT_PARAMS: handleEffectParams(evt); break;
        case UI_SCREEN_PARAM_EDIT:    handleParamEdit(evt); break;
        case UI_SCREEN_MASTER_VOL:    handleMasterVol(evt); break;
        default: break;
        }
    }

    if (_needsRedraw) {
        switch (_screen) {
        case UI_SCREEN_CHAIN_VIEW:    drawChainView(); break;
        case UI_SCREEN_EFFECT_PARAMS: drawEffectParams(); break;
        case UI_SCREEN_PARAM_EDIT:    drawParamEdit(); break;
        case UI_SCREEN_MASTER_VOL:    drawMasterVol(); break;
        default: break;
        }
        _needsRedraw = false;
    }
}

void UISystem::forceRedraw() { _needsRedraw = true; }
