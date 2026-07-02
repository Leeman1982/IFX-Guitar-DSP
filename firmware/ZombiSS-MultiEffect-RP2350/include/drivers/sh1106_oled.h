#ifndef SH1106_OLED_H
#define SH1106_OLED_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/i2c.h"

#define SH1106_WIDTH       128
#define SH1106_HEIGHT       64
#define SH1106_PAGES        (SH1106_HEIGHT / 8)
#define SH1106_I2C_ADDR    0x3C
#define SH1106_COL_OFFSET   2   /* SH1106 has 132 columns, display starts at col 2 */

typedef struct {
    i2c_inst_t *i2c;
    uint8_t framebuf[SH1106_WIDTH * SH1106_PAGES];
    bool dirty;
} SH1106;

void SH1106_Init(SH1106 *oled, i2c_inst_t *i2c, uint sda_pin, uint scl_pin);
void SH1106_Clear(SH1106 *oled);
void SH1106_Flush(SH1106 *oled);
void SH1106_SetPixel(SH1106 *oled, int x, int y, bool on);
void SH1106_DrawChar(SH1106 *oled, int x, int y, char c, bool invert);
void SH1106_DrawString(SH1106 *oled, int x, int y, const char *str, bool invert);
void SH1106_DrawStringLarge(SH1106 *oled, int x, int y, const char *str, bool invert);
void SH1106_FillRect(SH1106 *oled, int x, int y, int w, int h, bool on);
void SH1106_DrawRect(SH1106 *oled, int x, int y, int w, int h);
void SH1106_DrawHLine(SH1106 *oled, int x, int y, int w);
void SH1106_DrawVLine(SH1106 *oled, int x, int y, int h);
void SH1106_DrawProgressBar(SH1106 *oled, int x, int y, int w, int h, float fraction);
void SH1106_SetContrast(SH1106 *oled, uint8_t contrast);
void SH1106_InvertDisplay(SH1106 *oled, bool invert);

#endif
