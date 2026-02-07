#include "splash_screen.h"
#include "pico/stdlib.h"

/* 128x64 bitmap logo for Zombi SS - stored as page-oriented data */
/* Skull/lightning bolt motif with "ZOMBI SS" text */
static const uint8_t zombi_logo_top[] = {
    /* Row 0-7: "ZOMBI SS" large text centered */
    /* Each byte is a column of 8 vertical pixels */
};

/* Draw the SS lightning bolt runes */
static void draw_ss_rune(SH1106 *oled, int x, int y) {
    /* Stylised SS rune - two lightning bolts */
    /* First bolt */
    SH1106_SetPixel(oled, x+0, y+0, true);
    SH1106_SetPixel(oled, x+1, y+0, true);
    SH1106_SetPixel(oled, x+2, y+0, true);
    SH1106_SetPixel(oled, x+3, y+0, true);
    SH1106_SetPixel(oled, x+0, y+1, true);
    SH1106_SetPixel(oled, x+1, y+1, true);
    SH1106_SetPixel(oled, x+1, y+2, true);
    SH1106_SetPixel(oled, x+2, y+2, true);
    SH1106_SetPixel(oled, x+2, y+3, true);
    SH1106_SetPixel(oled, x+3, y+3, true);
    SH1106_SetPixel(oled, x+3, y+4, true);
    SH1106_SetPixel(oled, x+4, y+4, true);
    SH1106_SetPixel(oled, x+1, y+5, true);
    SH1106_SetPixel(oled, x+2, y+5, true);
    SH1106_SetPixel(oled, x+3, y+5, true);
    SH1106_SetPixel(oled, x+4, y+5, true);

    /* Second bolt (offset) */
    int ox = 6;
    SH1106_SetPixel(oled, x+ox+0, y+0, true);
    SH1106_SetPixel(oled, x+ox+1, y+0, true);
    SH1106_SetPixel(oled, x+ox+2, y+0, true);
    SH1106_SetPixel(oled, x+ox+3, y+0, true);
    SH1106_SetPixel(oled, x+ox+0, y+1, true);
    SH1106_SetPixel(oled, x+ox+1, y+1, true);
    SH1106_SetPixel(oled, x+ox+1, y+2, true);
    SH1106_SetPixel(oled, x+ox+2, y+2, true);
    SH1106_SetPixel(oled, x+ox+2, y+3, true);
    SH1106_SetPixel(oled, x+ox+3, y+3, true);
    SH1106_SetPixel(oled, x+ox+3, y+4, true);
    SH1106_SetPixel(oled, x+ox+4, y+4, true);
    SH1106_SetPixel(oled, x+ox+1, y+5, true);
    SH1106_SetPixel(oled, x+ox+2, y+5, true);
    SH1106_SetPixel(oled, x+ox+3, y+5, true);
    SH1106_SetPixel(oled, x+ox+4, y+5, true);
}

/* Draw a small skull icon */
static void draw_skull(SH1106 *oled, int x, int y) {
    /* Skull outline 12x10 */
    /* Top of skull */
    for (int i = 3; i <= 8; i++) SH1106_SetPixel(oled, x+i, y, true);
    for (int i = 2; i <= 9; i++) SH1106_SetPixel(oled, x+i, y+1, true);
    for (int i = 1; i <= 10; i++) SH1106_SetPixel(oled, x+i, y+2, true);
    for (int i = 1; i <= 10; i++) SH1106_SetPixel(oled, x+i, y+3, true);
    /* Eyes */
    SH1106_SetPixel(oled, x+3, y+3, false);
    SH1106_SetPixel(oled, x+4, y+3, false);
    SH1106_SetPixel(oled, x+7, y+3, false);
    SH1106_SetPixel(oled, x+8, y+3, false);
    SH1106_SetPixel(oled, x+3, y+4, false);
    SH1106_SetPixel(oled, x+4, y+4, false);
    SH1106_SetPixel(oled, x+7, y+4, false);
    SH1106_SetPixel(oled, x+8, y+4, false);
    /* Mid face */
    for (int i = 1; i <= 10; i++) SH1106_SetPixel(oled, x+i, y+4, true);
    SH1106_SetPixel(oled, x+3, y+4, false);
    SH1106_SetPixel(oled, x+4, y+4, false);
    SH1106_SetPixel(oled, x+7, y+4, false);
    SH1106_SetPixel(oled, x+8, y+4, false);
    /* Nose */
    for (int i = 2; i <= 9; i++) SH1106_SetPixel(oled, x+i, y+5, true);
    SH1106_SetPixel(oled, x+5, y+5, false);
    SH1106_SetPixel(oled, x+6, y+5, false);
    /* Jaw */
    for (int i = 2; i <= 9; i++) SH1106_SetPixel(oled, x+i, y+6, true);
    /* Teeth */
    for (int i = 3; i <= 8; i++) SH1106_SetPixel(oled, x+i, y+7, true);
    SH1106_SetPixel(oled, x+4, y+7, false);
    SH1106_SetPixel(oled, x+6, y+7, false);
    for (int i = 3; i <= 8; i++) SH1106_SetPixel(oled, x+i, y+8, true);
    SH1106_SetPixel(oled, x+5, y+8, false);
    SH1106_SetPixel(oled, x+7, y+8, false);
    for (int i = 4; i <= 7; i++) SH1106_SetPixel(oled, x+i, y+9, true);
}

void Splash_Show(SH1106 *oled) {
    /* Phase 1: Fade in with scan lines */
    SH1106_Clear(oled);

    /* Draw border */
    SH1106_DrawRect(oled, 0, 0, 128, 64);
    SH1106_DrawRect(oled, 2, 2, 124, 60);

    /* Draw skull centered at top */
    draw_skull(oled, 58, 6);

    /* Draw "ZOMBI" in large text */
    SH1106_DrawStringLarge(oled, 5, 20, "ZOMBI", false);

    /* Draw SS lightning runes */
    draw_ss_rune(oled, 72, 22);

    /* Draw decorative lines */
    SH1106_DrawHLine(oled, 4, 38, 120);
    SH1106_DrawHLine(oled, 4, 40, 120);

    /* Subtitle */
    SH1106_DrawString(oled, 10, 44, "MULTI-EFFECT DSP", false);
    SH1106_DrawString(oled, 28, 54, ":: RP2350 ::", false);

    SH1106_Flush(oled);
    sleep_ms(800);

    /* Phase 2: Flash effect */
    SH1106_InvertDisplay(oled, true);
    sleep_ms(100);
    SH1106_InvertDisplay(oled, false);
    sleep_ms(100);
    SH1106_InvertDisplay(oled, true);
    sleep_ms(80);
    SH1106_InvertDisplay(oled, false);
    sleep_ms(1200);
}
