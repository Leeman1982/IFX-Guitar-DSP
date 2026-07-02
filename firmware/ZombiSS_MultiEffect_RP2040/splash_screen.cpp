#include "splash_screen.h"
#include <Arduino.h>

static void draw_skull(SH1106 *oled, int x, int y) {
    for (int i = 3; i <= 8; i++) oled->setPixel(x+i, y, true);
    for (int i = 2; i <= 9; i++) oled->setPixel(x+i, y+1, true);
    for (int i = 1; i <= 10; i++) oled->setPixel(x+i, y+2, true);
    for (int i = 1; i <= 10; i++) oled->setPixel(x+i, y+3, true);
    oled->setPixel(x+3, y+3, false); oled->setPixel(x+4, y+3, false);
    oled->setPixel(x+7, y+3, false); oled->setPixel(x+8, y+3, false);
    for (int i = 1; i <= 10; i++) oled->setPixel(x+i, y+4, true);
    oled->setPixel(x+3, y+4, false); oled->setPixel(x+4, y+4, false);
    oled->setPixel(x+7, y+4, false); oled->setPixel(x+8, y+4, false);
    for (int i = 2; i <= 9; i++) oled->setPixel(x+i, y+5, true);
    oled->setPixel(x+5, y+5, false); oled->setPixel(x+6, y+5, false);
    for (int i = 2; i <= 9; i++) oled->setPixel(x+i, y+6, true);
    for (int i = 3; i <= 8; i++) oled->setPixel(x+i, y+7, true);
    oled->setPixel(x+4, y+7, false); oled->setPixel(x+6, y+7, false);
    for (int i = 3; i <= 8; i++) oled->setPixel(x+i, y+8, true);
    oled->setPixel(x+5, y+8, false); oled->setPixel(x+7, y+8, false);
    for (int i = 4; i <= 7; i++) oled->setPixel(x+i, y+9, true);
}

static void draw_ss_rune(SH1106 *oled, int x, int y) {
    /* Stylised lightning bolt rune */
    int pts[][2] = {{0,0},{1,0},{2,0},{3,0},{0,1},{1,1},{1,2},{2,2},
                    {2,3},{3,3},{3,4},{4,4},{1,5},{2,5},{3,5},{4,5}};
    for (int i = 0; i < 16; i++) {
        oled->setPixel(x + pts[i][0], y + pts[i][1], true);
        oled->setPixel(x + pts[i][0] + 6, y + pts[i][1], true);  /* Second bolt */
    }
}

void Splash_Show(SH1106 *oled) {
    oled->clear();

    oled->drawRect(0, 0, 128, 64);
    oled->drawRect(2, 2, 124, 60);

    draw_skull(oled, 58, 5);

    /* "ZOMBI SS" branding - 7x14 large font */
    oled->drawStringLarge(20, 18, "ZOMBI", false);
    draw_ss_rune(oled, 60, 20);

    oled->drawHLine(4, 36, 120);
    oled->drawHLine(4, 38, 120);

    oled->drawString(10, 42, "MULTI-EFFECT DSP", false);
    oled->drawString(28, 52, "::  RP2040 ::", false);

    oled->flush();
    delay(800);

    oled->invertDisplay(true);
    delay(100);
    oled->invertDisplay(false);
    delay(100);
    oled->invertDisplay(true);
    delay(80);
    oled->invertDisplay(false);
    delay(1200);
}
