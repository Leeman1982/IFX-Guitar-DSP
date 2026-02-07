#include "ui_system.h"
#include "splash_screen.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

/* Format float value into string buffer */
static void format_value(char *buf, size_t sz, float value, const char *unit) {
    if (value >= 1000.0f) {
        snprintf(buf, sz, "%.1fk%s", value / 1000.0f, unit);
    } else if (value >= 100.0f) {
        snprintf(buf, sz, "%.0f%s", value, unit);
    } else if (value >= 10.0f) {
        snprintf(buf, sz, "%.1f%s", value, unit);
    } else if (value >= 0.0f) {
        snprintf(buf, sz, "%.2f%s", value, unit);
    } else if (value > -10.0f) {
        snprintf(buf, sz, "%.2f%s", value, unit);
    } else {
        snprintf(buf, sz, "%.1f%s", value, unit);
    }
}

/* ---- CHAIN VIEW: shows all effects with on/off status ---- */
static void draw_chain_view(UISystem *ui) {
    SH1106_Clear(ui->oled);

    /* Header */
    SH1106_FillRect(ui->oled, 0, 0, 128, 9, true);
    SH1106_DrawString(ui->oled, 1, 1, "ZOMBI SS  FX CHAIN", true);

    /* Draw each effect in the chain */
    for (int i = 0; i < FX_COUNT; i++) {
        int y = 12 + i * 10;
        bool selected = (i == ui->selectedEffect);
        bool active = ui->chain->active[i];

        if (selected) {
            SH1106_FillRect(ui->oled, 0, y, 128, 10, true);
        }

        /* Effect number and status indicator */
        char line[22];
        snprintf(line, sizeof(line), "%d %s%-6s",
                 i + 1,
                 active ? "[ON]  " : "[OFF] ",
                 ui->chain->fxNames[i]);
        SH1106_DrawString(ui->oled, 2, y + 1, line, selected);

        /* Small activity bar for active effects */
        if (active && !selected) {
            SH1106_FillRect(ui->oled, 122, y + 2, 4, 6, true);
        }
    }

    /* Footer hint */
    SH1106_DrawString(ui->oled, 0, 57, "Turn:Sel  Push:Edit", false);

    SH1106_Flush(ui->oled);
}

/* ---- EFFECT PARAMS VIEW: shows all params for one effect ---- */
static void draw_effect_params(UISystem *ui) {
    SH1106_Clear(ui->oled);

    uint8_t fx = ui->selectedEffect;
    uint8_t count = ui->chain->paramCount[fx];

    /* Header with effect name */
    SH1106_FillRect(ui->oled, 0, 0, 128, 9, true);
    char header[22];
    snprintf(header, sizeof(header), "%s %s PARAMS",
             ui->chain->fxNames[fx],
             ui->chain->active[fx] ? "[ON]" : "[OFF]");
    SH1106_DrawString(ui->oled, 1, 1, header, true);

    /* Calculate visible range (max 5 params visible at once) */
    uint8_t maxVisible = 5;
    if (ui->selectedParam < ui->scrollOffset) {
        ui->scrollOffset = ui->selectedParam;
    }
    if (ui->selectedParam >= ui->scrollOffset + maxVisible) {
        ui->scrollOffset = ui->selectedParam - maxVisible + 1;
    }

    for (uint8_t i = 0; i < maxVisible && (i + ui->scrollOffset) < count; i++) {
        uint8_t pi = i + ui->scrollOffset;
        int y = 11 + i * 10;
        bool selected = (pi == ui->selectedParam);
        ParamDesc *p = &ui->chain->params[fx][pi];

        if (selected) {
            SH1106_FillRect(ui->oled, 0, y, 128, 10, true);
        }

        /* Parameter name */
        SH1106_DrawString(ui->oled, 2, y + 1, p->name, selected);

        /* Parameter value */
        char val[16];
        format_value(val, sizeof(val), p->value, p->unit);
        int vx = 128 - ((int)strlen(val) * 6) - 2;
        SH1106_DrawString(ui->oled, vx, y + 1, val, selected);
    }

    /* Scroll indicators */
    if (ui->scrollOffset > 0) {
        SH1106_DrawString(ui->oled, 60, 11, "^", false);
    }
    if (ui->scrollOffset + maxVisible < count) {
        SH1106_DrawString(ui->oled, 60, 56, "v", false);
    }

    /* Footer */
    SH1106_DrawString(ui->oled, 0, 57, "Turn:Sel Push:Edit", false);

    SH1106_Flush(ui->oled);
}

/* ---- PARAM EDIT VIEW: editing a single parameter ---- */
static void draw_param_edit(UISystem *ui) {
    SH1106_Clear(ui->oled);

    uint8_t fx = ui->selectedEffect;
    uint8_t pi = ui->selectedParam;
    ParamDesc *p = &ui->chain->params[fx][pi];

    /* Header */
    SH1106_FillRect(ui->oled, 0, 0, 128, 9, true);
    char header[22];
    snprintf(header, sizeof(header), "%s > %s", ui->chain->fxNames[fx], p->name);
    SH1106_DrawString(ui->oled, 1, 1, header, true);

    /* Large value display */
    char val[16];
    format_value(val, sizeof(val), p->value, p->unit);
    /* Center the large text */
    int tw = (int)strlen(val) * 12;
    int tx = (128 - tw) / 2;
    if (tx < 0) tx = 0;
    SH1106_DrawStringLarge(ui->oled, tx, 16, val, false);

    /* Progress bar showing position in range */
    float fraction = (p->value - p->min) / (p->max - p->min);
    SH1106_DrawProgressBar(ui->oled, 4, 36, 120, 8, fraction);

    /* Min/Max labels */
    char minStr[10], maxStr[10];
    format_value(minStr, sizeof(minStr), p->min, "");
    format_value(maxStr, sizeof(maxStr), p->max, "");
    SH1106_DrawString(ui->oled, 4, 47, minStr, false);
    int mx = 128 - ((int)strlen(maxStr) * 6) - 4;
    SH1106_DrawString(ui->oled, mx, 47, maxStr, false);

    /* Footer */
    SH1106_DrawString(ui->oled, 0, 57, "Turn:Adj  Push:Done", false);

    SH1106_Flush(ui->oled);
}

/* ---- MASTER VOLUME VIEW ---- */
static void draw_master_vol(UISystem *ui) {
    SH1106_Clear(ui->oled);

    SH1106_FillRect(ui->oled, 0, 0, 128, 9, true);
    SH1106_DrawString(ui->oled, 1, 1, "MASTER VOLUME", true);

    /* Big percentage */
    char val[10];
    snprintf(val, sizeof(val), "%d%%", (int)(ui->chain->masterVolume * 100.0f));
    int tw = (int)strlen(val) * 12;
    int tx = (128 - tw) / 2;
    SH1106_DrawStringLarge(ui->oled, tx, 18, val, false);

    /* Progress bar */
    SH1106_DrawProgressBar(ui->oled, 4, 40, 120, 10, ui->chain->masterVolume);

    SH1106_DrawString(ui->oled, 0, 57, "Turn:Adj  Push:Back", false);

    SH1106_Flush(ui->oled);
}

/* ---- Event handling ---- */
static void handle_chain_view(UISystem *ui, EncoderEvent evt) {
    switch (evt) {
    case ENC_EVENT_CW:
        if (ui->selectedEffect < FX_COUNT - 1) ui->selectedEffect++;
        ui->needsRedraw = true;
        break;
    case ENC_EVENT_CCW:
        if (ui->selectedEffect > 0) ui->selectedEffect--;
        ui->needsRedraw = true;
        break;
    case ENC_EVENT_PRESS:
        /* Enter effect params */
        ui->screen = UI_SCREEN_EFFECT_PARAMS;
        ui->selectedParam = 0;
        ui->scrollOffset = 0;
        ui->needsRedraw = true;
        break;
    case ENC_EVENT_LONG_PRESS:
        /* Enter master volume */
        ui->screen = UI_SCREEN_MASTER_VOL;
        ui->needsRedraw = true;
        break;
    case ENC_EVENT_CONFIRM:
        /* Toggle selected effect */
        EffectChain_ToggleEffect(ui->chain, ui->selectedEffect);
        ui->needsRedraw = true;
        break;
    default:
        break;
    }
}

static void handle_effect_params(UISystem *ui, EncoderEvent evt) {
    uint8_t count = ui->chain->paramCount[ui->selectedEffect];
    switch (evt) {
    case ENC_EVENT_CW:
        if (ui->selectedParam < count - 1) ui->selectedParam++;
        ui->needsRedraw = true;
        break;
    case ENC_EVENT_CCW:
        if (ui->selectedParam > 0) ui->selectedParam--;
        ui->needsRedraw = true;
        break;
    case ENC_EVENT_PRESS:
        /* Enter param edit */
        ui->screen = UI_SCREEN_PARAM_EDIT;
        ui->needsRedraw = true;
        break;
    case ENC_EVENT_BACK:
        /* Back to chain view */
        ui->screen = UI_SCREEN_CHAIN_VIEW;
        ui->needsRedraw = true;
        break;
    case ENC_EVENT_CONFIRM:
        /* Toggle this effect */
        EffectChain_ToggleEffect(ui->chain, ui->selectedEffect);
        ui->needsRedraw = true;
        break;
    default:
        break;
    }
}

static void handle_param_edit(UISystem *ui, EncoderEvent evt) {
    uint8_t fx = ui->selectedEffect;
    uint8_t pi = ui->selectedParam;
    ParamDesc *p = &ui->chain->params[fx][pi];

    switch (evt) {
    case ENC_EVENT_CW:
        EffectChain_SetParam(ui->chain, fx, pi, p->value + p->step);
        ui->needsRedraw = true;
        break;
    case ENC_EVENT_CCW:
        EffectChain_SetParam(ui->chain, fx, pi, p->value - p->step);
        ui->needsRedraw = true;
        break;
    case ENC_EVENT_PRESS:
    case ENC_EVENT_BACK:
        /* Done editing, back to params */
        ui->screen = UI_SCREEN_EFFECT_PARAMS;
        ui->needsRedraw = true;
        break;
    case ENC_EVENT_CONFIRM:
        /* Also confirm = done */
        ui->screen = UI_SCREEN_EFFECT_PARAMS;
        ui->needsRedraw = true;
        break;
    default:
        break;
    }
}

static void handle_master_vol(UISystem *ui, EncoderEvent evt) {
    switch (evt) {
    case ENC_EVENT_CW:
        EffectChain_SetMasterVolume(ui->chain, ui->chain->masterVolume + 0.02f);
        ui->needsRedraw = true;
        break;
    case ENC_EVENT_CCW:
        EffectChain_SetMasterVolume(ui->chain, ui->chain->masterVolume - 0.02f);
        ui->needsRedraw = true;
        break;
    case ENC_EVENT_PRESS:
    case ENC_EVENT_BACK:
        ui->screen = UI_SCREEN_CHAIN_VIEW;
        ui->needsRedraw = true;
        break;
    default:
        break;
    }
}

/* ---- FX toggle events (from momentary switches) ---- */
static void handle_fx_toggle(UISystem *ui, EncoderEvent evt) {
    switch (evt) {
    case ENC_EVENT_FX1_TOGGLE:
        EffectChain_ToggleEffect(ui->chain, FX_NOISEGATE);
        ui->needsRedraw = true;
        break;
    case ENC_EVENT_FX2_TOGGLE:
        EffectChain_ToggleEffect(ui->chain, FX_OVERDRIVE);
        ui->needsRedraw = true;
        break;
    case ENC_EVENT_FX3_TOGGLE:
        EffectChain_ToggleEffect(ui->chain, FX_EQ);
        ui->needsRedraw = true;
        break;
    case ENC_EVENT_FX4_TOGGLE:
        EffectChain_ToggleEffect(ui->chain, FX_CHORUS);
        ui->needsRedraw = true;
        break;
    case ENC_EVENT_FX5_TOGGLE:
        EffectChain_ToggleEffect(ui->chain, FX_DELAY);
        ui->needsRedraw = true;
        break;
    default:
        break;
    }
}

/* ---- Public API ---- */

void UISystem_Init(UISystem *ui, SH1106 *oled, RotaryEncoder *encoder, EffectChain *chain) {
    ui->oled = oled;
    ui->encoder = encoder;
    ui->chain = chain;
    ui->screen = UI_SCREEN_SPLASH;
    ui->selectedEffect = 0;
    ui->selectedParam = 0;
    ui->scrollOffset = 0;
    ui->needsRedraw = true;
    ui->lastUpdateMs = 0;
    ui->animFrame = 0;
}

void UISystem_Update(UISystem *ui) {
    /* Handle splash screen */
    if (ui->screen == UI_SCREEN_SPLASH) {
        Splash_Show(ui->oled);
        ui->screen = UI_SCREEN_CHAIN_VIEW;
        ui->needsRedraw = true;
        return;
    }

    /* Poll encoder and buttons */
    RotaryEncoder_Poll(ui->encoder);

    /* Process all queued events */
    EncoderEvent evt;
    while ((evt = RotaryEncoder_GetEvent(ui->encoder)) != ENC_EVENT_NONE) {
        /* FX toggle switches are global - work in any screen */
        if (evt >= ENC_EVENT_FX1_TOGGLE && evt <= ENC_EVENT_FX5_TOGGLE) {
            handle_fx_toggle(ui, evt);
            continue;
        }

        switch (ui->screen) {
        case UI_SCREEN_CHAIN_VIEW:
            handle_chain_view(ui, evt);
            break;
        case UI_SCREEN_EFFECT_PARAMS:
            handle_effect_params(ui, evt);
            break;
        case UI_SCREEN_PARAM_EDIT:
            handle_param_edit(ui, evt);
            break;
        case UI_SCREEN_MASTER_VOL:
            handle_master_vol(ui, evt);
            break;
        default:
            break;
        }
    }

    /* Redraw if needed */
    if (ui->needsRedraw) {
        switch (ui->screen) {
        case UI_SCREEN_CHAIN_VIEW:
            draw_chain_view(ui);
            break;
        case UI_SCREEN_EFFECT_PARAMS:
            draw_effect_params(ui);
            break;
        case UI_SCREEN_PARAM_EDIT:
            draw_param_edit(ui);
            break;
        case UI_SCREEN_MASTER_VOL:
            draw_master_vol(ui);
            break;
        default:
            break;
        }
        ui->needsRedraw = false;
    }
}

void UISystem_ForceRedraw(UISystem *ui) {
    ui->needsRedraw = true;
}
