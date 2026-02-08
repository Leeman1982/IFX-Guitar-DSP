#ifndef SH1106_OLED_H
#define SH1106_OLED_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

/*
 * SH1106 OLED wrapper using U8g2 library.
 *
 * REQUIRES: Install "U8g2" library in Arduino IDE:
 *   Sketch > Include Library > Manage Libraries > search "U8g2" > Install
 *
 * Uses software I2C (bit-bang) to avoid Wire/clock race conditions
 * between Core 0 (system clock change) and Core 1 (OLED).
 */

#define SH1106_WIDTH       128
#define SH1106_HEIGHT       64

class SH1106 {
public:
    void init(uint8_t sda_pin, uint8_t scl_pin, uint8_t addr = OLED_I2C_ADDR);
    void clear();
    void flush();
    void setPixel(int x, int y, bool on);
    void drawChar(int x, int y, char c, bool invert = false);
    void drawString(int x, int y, const char *str, bool invert = false);
    void drawStringLarge(int x, int y, const char *str, bool invert = false);
    void fillRect(int x, int y, int w, int h, bool on);
    void drawRect(int x, int y, int w, int h);
    void drawHLine(int x, int y, int w);
    void drawVLine(int x, int y, int h);
    void drawProgressBar(int x, int y, int w, int h, float fraction);
    void setContrast(uint8_t contrast);
    void invertDisplay(bool invert);

private:
    void *_u8g2;     /* U8G2 display object (avoids header dependency) */
    uint8_t _addr;
};

#endif
