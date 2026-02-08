#include "sh1106_oled.h"
#include <U8g2lib.h>
#include <string.h>

/*
 * U8g2 SH1106 driver using SOFTWARE I2C (bit-bang).
 * This avoids all Wire library issues:
 *   - No dependency on system clock frequency
 *   - No race condition with Core 0 clock change
 *   - No Wire buffer size limitations
 *   - Works with any GPIO pins
 */

#define DISP ((U8G2_SH1106_128X64_NONAME_F_SW_I2C*)_u8g2)

void SH1106::init(uint8_t sda_pin, uint8_t scl_pin, uint8_t addr) {
    _addr = addr;

    /* Create U8g2 display with software I2C
     * Constructor: rotation, clock_pin(SCL), data_pin(SDA), reset_pin */
    _u8g2 = new U8G2_SH1106_128X64_NONAME_F_SW_I2C(
        U8G2_R0, scl_pin, sda_pin, U8X8_PIN_NONE);

    /* Set I2C address (U8g2 uses 8-bit address = 7-bit shifted left by 1) */
    DISP->setI2CAddress(addr * 2);

    /* Initialize - U8g2 handles full SH1106 power-on and init sequence */
    DISP->begin();

    /* y coordinate = top of character (matches our UI layout code) */
    DISP->setFont(u8g2_font_5x7_tr);
    DISP->setFontPosTop();
    DISP->setFontMode(1);   /* Transparent mode */
    DISP->setDrawColor(1);

    Serial.print("OLED init (U8g2 SW_I2C) addr=0x");
    Serial.println(addr, HEX);

    clear();
    flush();
}

void SH1106::clear() {
    if (!_u8g2) return;
    DISP->clearBuffer();
}

void SH1106::flush() {
    if (!_u8g2) return;
    DISP->sendBuffer();
}

void SH1106::setPixel(int x, int y, bool on) {
    if (!_u8g2) return;
    DISP->setDrawColor(on ? 1 : 0);
    DISP->drawPixel(x, y);
    DISP->setDrawColor(1);
}

void SH1106::drawChar(int x, int y, char c, bool invert) {
    if (!_u8g2) return;
    char buf[2] = { c, 0 };
    DISP->setFont(u8g2_font_5x7_tr);
    DISP->setFontPosTop();
    DISP->setDrawColor(invert ? 0 : 1);
    DISP->drawStr(x, y, buf);
    DISP->setDrawColor(1);
}

void SH1106::drawString(int x, int y, const char *str, bool invert) {
    if (!_u8g2) return;
    DISP->setFont(u8g2_font_5x7_tr);
    DISP->setFontPosTop();
    DISP->setDrawColor(invert ? 0 : 1);
    DISP->drawStr(x, y, str);
    DISP->setDrawColor(1);
}

void SH1106::drawStringLarge(int x, int y, const char *str, bool invert) {
    if (!_u8g2) return;
    DISP->setFont(u8g2_font_7x14_tr);
    DISP->setFontPosTop();
    DISP->setDrawColor(invert ? 0 : 1);
    DISP->drawStr(x, y, str);
    DISP->setDrawColor(1);
    /* Restore default small font */
    DISP->setFont(u8g2_font_5x7_tr);
}

void SH1106::fillRect(int x, int y, int w, int h, bool on) {
    if (!_u8g2) return;
    DISP->setDrawColor(on ? 1 : 0);
    DISP->drawBox(x, y, w, h);
    DISP->setDrawColor(1);
}

void SH1106::drawRect(int x, int y, int w, int h) {
    if (!_u8g2) return;
    DISP->drawFrame(x, y, w, h);
}

void SH1106::drawHLine(int x, int y, int w) {
    if (!_u8g2) return;
    DISP->drawHLine(x, y, w);
}

void SH1106::drawVLine(int x, int y, int h) {
    if (!_u8g2) return;
    DISP->drawVLine(x, y, h);
}

void SH1106::drawProgressBar(int x, int y, int w, int h, float fraction) {
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;
    drawRect(x, y, w, h);
    int fill = (int)((w - 2) * fraction);
    fillRect(x + 1, y + 1, fill, h - 2, true);
}

void SH1106::setContrast(uint8_t contrast) {
    if (!_u8g2) return;
    DISP->setContrast(contrast);
}

void SH1106::invertDisplay(bool invert) {
    if (!_u8g2) return;
    /* Send raw SH1106 display inversion command */
    u8x8_cad_StartTransfer(DISP->getU8x8());
    u8x8_cad_SendCmd(DISP->getU8x8(), invert ? 0xA7 : 0xA6);
    u8x8_cad_EndTransfer(DISP->getU8x8());
}
