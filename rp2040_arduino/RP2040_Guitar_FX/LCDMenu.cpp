#include "LCDMenu.h"
#include <stdio.h>
#include <string.h>

// Param.value is volatile float* – no cast needed; taking the address of a
// volatile member gives volatile float* directly, which the compiler treats
// correctly on both cores (no register caching, correct read/write ordering).

LCDMenu::LCDMenu(uint8_t i2c_addr, uint8_t cols, uint8_t rows)
    : _lcd(i2c_addr, cols, rows),
      _state(MS_HOME), _effectSel(0), _paramIdx(0), _dirty(true),
      _distParamCount(5), _chorusParamCount(4), _eqParamCount(5)
{}

void LCDMenu::begin()
{
    _lcd.init();
    _lcd.backlight();
    buildParamTables();
    _dirty = true;
}

// ── Build parameter descriptor tables ────────────────────────────────────────
void LCDMenu::buildParamTables()
{
    // DISTORTION – 5 parameters
    _distParams[0] = { "DRIVE",    &g_dist.gain,       1.0f,   500.0f,  5.0f,  false, false, true  };
    _distParams[1] = { "HPF",      &g_dist.hpf_freq,   80.0f,  500.0f,  10.0f, true,  false, false };
    _distParams[2] = { "TONE",     &g_dist.tone_freq,  500.0f, 10000.f, 100.f, true,  false, false };
    _distParams[3] = { "BODY",     &g_dist.tone_damp,  0.5f,   2.0f,    0.1f,  false, false, false };
    _distParams[4] = { "LEVEL",    &g_dist.level,      0.0f,   1.0f,    0.01f, false, false, true  };

    // CHORUS – 4 parameters
    _chorusParams[0] = { "RATE",   &g_chorus.rate,     0.1f,   8.0f,    0.1f,  false, false, false };
    _chorusParams[1] = { "DEPTH",  &g_chorus.depth,    5.0f,   80.0f,   1.0f,  false, false, false };
    _chorusParams[2] = { "MIX",    &g_chorus.mix,      0.0f,   1.0f,    0.01f, false, false, true  };
    _chorusParams[3] = { "DELAY",  &g_chorus.delay_ms, 5.0f,   30.0f,   0.5f,  false, false, false };

    // EQ – 5 parameters
    _eqParams[0] = { "BASS",    &g_eq.bass_db,    -12.0f,  12.0f,  0.5f,  false, true,  false };
    _eqParams[1] = { "MID",     &g_eq.mid_db,     -12.0f,  12.0f,  0.5f,  false, true,  false };
    _eqParams[2] = { "TREBLE",  &g_eq.treb_db,    -12.0f,  12.0f,  0.5f,  false, true,  false };
    _eqParams[3] = { "MID FRQ", &g_eq.mid_freq,   200.0f, 5000.f, 50.0f, true,  false, false };
    _eqParams[4] = { "MID BW",  &g_eq.mid_bw,     100.0f, 2000.f, 25.0f, true,  false, false };
}

// ── Main update called from Core 0 loop ──────────────────────────────────────
void LCDMenu::update(int8_t encDelta, bool encPressed, bool forceRedraw)
{
    if (forceRedraw) _dirty = true;

    switch (_state) {

    // ── HOME ─────────────────────────────────────────────────────────────
    case MS_HOME:
        if (encPressed) {
            _state    = MS_EFFECT_SELECT;
            _effectSel = 0;
            _dirty    = true;
        }
        if (_dirty) drawHome();
        break;

    // ── EFFECT SELECT ────────────────────────────────────────────────────
    case MS_EFFECT_SELECT:
        if (encDelta != 0) {
            int8_t sel = (int8_t)_effectSel + (encDelta > 0 ? 1 : -1);
            if (sel < 0) sel = 2;
            if (sel > 2) sel = 0;
            _effectSel = (uint8_t)sel;
            _dirty     = true;
        }
        if (encPressed) {
            _paramIdx = 0;
            switch (_effectSel) {
                case 0: _state = MS_DIST_PARAM;   break;
                case 1: _state = MS_CHORUS_PARAM;  break;
                case 2: _state = MS_EQ_PARAM;      break;
            }
            _dirty = true;
        }
        if (_dirty) drawEffectSelect();
        break;

    // ── DISTORTION PARAMS ────────────────────────────────────────────────
    case MS_DIST_PARAM:
        if (encDelta != 0) {
            applyDelta(_distParams[_paramIdx], encDelta);
            __sync_synchronize();           // ensure value write reaches Core 1 before flag
            g_dist.needs_update = true;
            _dirty = true;
        }
        if (encPressed) {
            _paramIdx++;
            if (_paramIdx >= _distParamCount) {
                _paramIdx = 0;
                _state    = MS_EFFECT_SELECT;
            }
            _dirty = true;
        }
        if (_dirty) drawParamEdit(_distParams, _distParamCount, "DIST");
        break;

    // ── CHORUS PARAMS ────────────────────────────────────────────────────
    case MS_CHORUS_PARAM:
        if (encDelta != 0) {
            applyDelta(_chorusParams[_paramIdx], encDelta);
            __sync_synchronize();
            g_chorus.needs_update = true;
            _dirty = true;
        }
        if (encPressed) {
            _paramIdx++;
            if (_paramIdx >= _chorusParamCount) {
                _paramIdx = 0;
                _state    = MS_EFFECT_SELECT;
            }
            _dirty = true;
        }
        if (_dirty) drawParamEdit(_chorusParams, _chorusParamCount, "CHORUS");
        break;

    // ── EQ PARAMS ────────────────────────────────────────────────────────
    case MS_EQ_PARAM:
        if (encDelta != 0) {
            applyDelta(_eqParams[_paramIdx], encDelta);
            __sync_synchronize();
            g_eq.needs_update = true;
            _dirty = true;
        }
        if (encPressed) {
            _paramIdx++;
            if (_paramIdx >= _eqParamCount) {
                _paramIdx = 0;
                _state    = MS_EFFECT_SELECT;
            }
            _dirty = true;
        }
        if (_dirty) drawParamEdit(_eqParams, _eqParamCount, "EQ");
        break;
    }

    _dirty = false;
}

void LCDMenu::notifyBypassChange() { _dirty = true; }

// ── Screen: HOME ──────────────────────────────────────────────────────────────
//  Line 1:  "DIST CHOR  EQ  "
//  Line 2:  "[ON] [ON] [OFF]"
void LCDMenu::drawHome()
{
    _lcd.setCursor(0, 0);
    _lcd.print("DIST CHOR  EQ   ");
    _lcd.setCursor(0, 1);

    char buf[17];
    snprintf(buf, sizeof(buf), "[%s] [%s] [%s]  ",
             g_dist.enabled   ? "ON" : "--",
             g_chorus.enabled ? "ON" : "--",
             g_eq.enabled     ? "ON" : "--");
    _lcd.print(buf);
}

// ── Screen: EFFECT SELECT ─────────────────────────────────────────────────────
//  Line 1:  "> DISTORTION    "
//  Line 2:  "  CHORUS        "
static const char *EFF_NAMES[3] = { "DISTORTION", "CHORUS    ", "EQ        " };

void LCDMenu::drawEffectSelect()
{
    uint8_t a = _effectSel;
    uint8_t b = (_effectSel + 1) % 3;

    char l1[17], l2[17];
    snprintf(l1, sizeof(l1), ">%-15s", EFF_NAMES[a]);
    snprintf(l2, sizeof(l2), " %-15s", EFF_NAMES[b]);

    _lcd.setCursor(0, 0); _lcd.print(l1);
    _lcd.setCursor(0, 1); _lcd.print(l2);
}

// ── Screen: PARAM EDIT ────────────────────────────────────────────────────────
//  Line 1:  "DIST > DRIVE    "
//  Line 2:  "          50%   "
void LCDMenu::drawParamEdit(Param *params, uint8_t count, const char *effectName)
{
    char valBuf[10];
    formatValue(valBuf, params[_paramIdx]);

    char l1[17], l2[17];
    snprintf(l1, sizeof(l1), "%-5s>%-10s", effectName, params[_paramIdx].label);
    snprintf(l2, sizeof(l2), "      %10s", valBuf);

    _lcd.setCursor(0, 0); _lcd.print(l1);
    _lcd.setCursor(0, 1); _lcd.print(l2);

    // Small param counter at far right of line 1: "3/5"
    char ctr[4];
    snprintf(ctr, sizeof(ctr), "%d/%d", _paramIdx + 1, count);
    _lcd.setCursor(16 - strlen(ctr), 0);
    _lcd.print(ctr);
}

// ── Value formatter ───────────────────────────────────────────────────────────
void LCDMenu::formatValue(char *buf, const Param &p)
{
    float v = *p.value;

    if (p.isPct) {
        // Normalise to 0-100% using param range
        float pct = 100.0f * (v - p.minVal) / (p.maxVal - p.minVal);
        snprintf(buf, 10, "%3.0f%%", pct);
    } else if (p.isDb) {
        snprintf(buf, 10, "%+.1fdB", v);
    } else if (p.isFreq) {
        if (v >= 1000.0f)
            snprintf(buf, 10, "%.1fkHz", v / 1000.0f);
        else
            snprintf(buf, 10, "%.0fHz", v);
    } else {
        snprintf(buf, 10, "%.2f", v);
    }
}

// ── Apply encoder delta to parameter, clamped to [min, max] ─────────────────
void LCDMenu::applyDelta(Param &p, int8_t delta)
{
    float v = *p.value + delta * p.step;
    if (v < p.minVal) v = p.minVal;
    if (v > p.maxVal) v = p.maxVal;
    *p.value = v;
}
