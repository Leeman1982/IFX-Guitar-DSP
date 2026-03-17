#ifndef SH1106_OLED_H
#define SH1106_OLED_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#define SH1106_WIDTH       128
#define SH1106_HEIGHT       64
#define SH1106_PAGES        (SH1106_HEIGHT / 8)
#define SH1106_COL_OFFSET   0   /* SSD1306 has no column offset (SH1106 uses 2) */

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
    void sendCmd(uint8_t cmd);
    uint8_t _addr;
    uint8_t _framebuf[SH1106_WIDTH * SH1106_PAGES];
};

#endif
