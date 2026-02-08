#include "sh1106_oled.h"
#include "font5x7.h"
#include <Wire.h>
#include <string.h>

void SH1106::sendCmd(uint8_t cmd) {
    Wire.beginTransmission(_addr);
    Wire.write(0x00);   /* Co=0, D/C#=0 (command) */
    Wire.write(cmd);
    Wire.endTransmission();
}

void SH1106::init(uint8_t sda_pin, uint8_t scl_pin, uint8_t addr) {
    _addr = addr;

    Wire.setSDA(sda_pin);
    Wire.setSCL(scl_pin);
    Wire.setClock(OLED_I2C_FREQ);
    Wire.begin();

    delay(100);

    /* Auto-detect I2C address: scan 0x3C and 0x3D */
    bool found = false;
    uint8_t tryAddrs[] = { addr, 0x3C, 0x3D };
    for (int i = 0; i < 3; i++) {
        Wire.beginTransmission(tryAddrs[i]);
        uint8_t err = Wire.endTransmission();
        if (err == 0) {
            _addr = tryAddrs[i];
            found = true;
            Serial.print("OLED found at 0x");
            Serial.println(_addr, HEX);
            break;
        }
    }

    if (!found) {
        Serial.println("OLED NOT FOUND! Check wiring. Trying 0x3C anyway.");
        _addr = 0x3C;
    }

    delay(10);

    sendCmd(0xAE);  /* Display OFF */
    sendCmd(0xD5); sendCmd(0x80);  /* Clock div */
    sendCmd(0xA8); sendCmd(0x3F);  /* Multiplex 64 */
    sendCmd(0xD3); sendCmd(0x00);  /* Display offset 0 */
    sendCmd(0x40);  /* Start line 0 */
    sendCmd(0xAD); sendCmd(0x8B);  /* DC-DC ON (internal charge pump) */
    sendCmd(0x32);  /* Pump voltage 8.0V */
    sendCmd(0xA1);  /* Segment remap (flip horizontal) */
    sendCmd(0xC8);  /* COM scan direction (flip vertical) */
    sendCmd(0xDA); sendCmd(0x12);  /* COM pins config */
    sendCmd(0x81); sendCmd(0xFF);  /* Contrast MAX */
    sendCmd(0xD9); sendCmd(0xF1);  /* Pre-charge period */
    sendCmd(0xDB); sendCmd(0x40);  /* VCOMH deselect level */
    sendCmd(0xA4);  /* Display from RAM */
    sendCmd(0xA6);  /* Normal (not inverted) */

    delay(10);

    sendCmd(0xAF);  /* Display ON */

    delay(100);  /* Let display stabilize */

    clear();
    flush();
}

void SH1106::clear() {
    memset(_framebuf, 0, sizeof(_framebuf));
}

void SH1106::flush() {
    for (uint8_t page = 0; page < SH1106_PAGES; page++) {
        sendCmd(0xB0 | page);
        sendCmd(0x00 | (SH1106_COL_OFFSET & 0x0F));
        sendCmd(0x10 | (SH1106_COL_OFFSET >> 4));

        /* Send pixel data in 32-byte chunks to avoid Wire buffer issues */
        uint16_t offset = page * SH1106_WIDTH;
        for (uint8_t chunk = 0; chunk < SH1106_WIDTH; chunk += 32) {
            Wire.beginTransmission(_addr);
            Wire.write(0x40);  /* Co=0, D/C#=1 (data) */
            uint8_t len = 32;
            if (chunk + len > SH1106_WIDTH) len = SH1106_WIDTH - chunk;
            Wire.write(&_framebuf[offset + chunk], len);
            Wire.endTransmission();
        }
    }
}

void SH1106::setPixel(int x, int y, bool on) {
    if (x < 0 || x >= SH1106_WIDTH || y < 0 || y >= SH1106_HEIGHT) return;
    uint16_t idx = (y / 8) * SH1106_WIDTH + x;
    if (on) _framebuf[idx] |= (1 << (y & 7));
    else    _framebuf[idx] &= ~(1 << (y & 7));
}

void SH1106::drawChar(int x, int y, char c, bool invert) {
    if (c < 32 || c > 126) c = '?';
    const uint8_t *glyph = &font5x7_data[(c - 32) * 5];
    for (int col = 0; col < 5; col++) {
        uint8_t line = glyph[col];
        if (invert) line = ~line;
        for (int row = 0; row < 7; row++) {
            setPixel(x + col, y + row, (line >> row) & 1);
        }
    }
    for (int row = 0; row < 7; row++) {
        setPixel(x + 5, y + row, invert);
    }
}

void SH1106::drawString(int x, int y, const char *str, bool invert) {
    while (*str) {
        drawChar(x, y, *str, invert);
        x += 6;
        str++;
    }
}

void SH1106::drawStringLarge(int x, int y, const char *str, bool invert) {
    while (*str) {
        if (*str < 32 || *str > 126) { str++; x += 12; continue; }
        const uint8_t *glyph = &font5x7_data[(*str - 32) * 5];
        for (int col = 0; col < 5; col++) {
            uint8_t line = glyph[col];
            if (invert) line = ~line;
            for (int row = 0; row < 7; row++) {
                bool on = (line >> row) & 1;
                setPixel(x + col*2,     y + row*2,     on);
                setPixel(x + col*2 + 1, y + row*2,     on);
                setPixel(x + col*2,     y + row*2 + 1, on);
                setPixel(x + col*2 + 1, y + row*2 + 1, on);
            }
        }
        for (int row = 0; row < 14; row++) {
            setPixel(x + 10, y + row, invert);
            setPixel(x + 11, y + row, invert);
        }
        x += 12;
        str++;
    }
}

void SH1106::fillRect(int x, int y, int w, int h, bool on) {
    for (int i = x; i < x + w; i++)
        for (int j = y; j < y + h; j++)
            setPixel(i, j, on);
}

void SH1106::drawRect(int x, int y, int w, int h) {
    drawHLine(x, y, w);
    drawHLine(x, y + h - 1, w);
    drawVLine(x, y, h);
    drawVLine(x + w - 1, y, h);
}

void SH1106::drawHLine(int x, int y, int w) {
    for (int i = x; i < x + w; i++) setPixel(i, y, true);
}

void SH1106::drawVLine(int x, int y, int h) {
    for (int j = y; j < y + h; j++) setPixel(x, j, true);
}

void SH1106::drawProgressBar(int x, int y, int w, int h, float fraction) {
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;
    drawRect(x, y, w, h);
    int fill = (int)((w - 2) * fraction);
    fillRect(x + 1, y + 1, fill, h - 2, true);
}

void SH1106::setContrast(uint8_t contrast) {
    sendCmd(0x81);
    sendCmd(contrast);
}

void SH1106::invertDisplay(bool invert) {
    sendCmd(invert ? 0xA7 : 0xA6);
}
