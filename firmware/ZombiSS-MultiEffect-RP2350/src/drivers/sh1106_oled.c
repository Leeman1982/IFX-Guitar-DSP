#include "sh1106_oled.h"
#include "font5x7.h"
#include <string.h>

static void sh1106_cmd(SH1106 *oled, uint8_t cmd) {
    uint8_t buf[2] = {0x00, cmd};  /* Co=0, D/C#=0 */
    i2c_write_blocking(oled->i2c, SH1106_I2C_ADDR, buf, 2, false);
}

void SH1106_Init(SH1106 *oled, i2c_inst_t *i2c, uint sda_pin, uint scl_pin) {
    oled->i2c = i2c;

    /* Init I2C at 400kHz */
    i2c_init(i2c, 400 * 1000);
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(sda_pin);
    gpio_pull_up(scl_pin);

    sleep_ms(100);

    /* SH1106 init sequence */
    sh1106_cmd(oled, 0xAE);  /* Display OFF */
    sh1106_cmd(oled, 0xD5);  /* Set display clock div */
    sh1106_cmd(oled, 0x80);
    sh1106_cmd(oled, 0xA8);  /* Set multiplex ratio */
    sh1106_cmd(oled, 0x3F);  /* 64 rows */
    sh1106_cmd(oled, 0xD3);  /* Set display offset */
    sh1106_cmd(oled, 0x00);
    sh1106_cmd(oled, 0x40);  /* Set start line = 0 */
    sh1106_cmd(oled, 0xAD);  /* DC-DC control */
    sh1106_cmd(oled, 0x8B);  /* DC-DC ON */
    sh1106_cmd(oled, 0x32);  /* Set pump voltage 8.0V */
    sh1106_cmd(oled, 0xA1);  /* Segment remap (flip horizontal) */
    sh1106_cmd(oled, 0xC8);  /* COM output scan direction (flip vertical) */
    sh1106_cmd(oled, 0xDA);  /* Set COM pins config */
    sh1106_cmd(oled, 0x12);
    sh1106_cmd(oled, 0x81);  /* Set contrast */
    sh1106_cmd(oled, 0xCF);
    sh1106_cmd(oled, 0xD9);  /* Set pre-charge period */
    sh1106_cmd(oled, 0xF1);
    sh1106_cmd(oled, 0xDB);  /* Set VCOMH deselect level */
    sh1106_cmd(oled, 0x40);
    sh1106_cmd(oled, 0xA4);  /* Display from RAM */
    sh1106_cmd(oled, 0xA6);  /* Normal display (not inverted) */
    sh1106_cmd(oled, 0xAF);  /* Display ON */

    SH1106_Clear(oled);
    SH1106_Flush(oled);
}

void SH1106_Clear(SH1106 *oled) {
    memset(oled->framebuf, 0, sizeof(oled->framebuf));
    oled->dirty = true;
}

void SH1106_Flush(SH1106 *oled) {
    /* Write framebuffer page-by-page (SH1106 doesn't support horizontal addressing mode) */
    for (uint8_t page = 0; page < SH1106_PAGES; page++) {
        sh1106_cmd(oled, 0xB0 | page);                          /* Set page address */
        sh1106_cmd(oled, 0x00 | (SH1106_COL_OFFSET & 0x0F));   /* Set lower column */
        sh1106_cmd(oled, 0x10 | (SH1106_COL_OFFSET >> 4));     /* Set upper column */

        uint8_t buf[SH1106_WIDTH + 1];
        buf[0] = 0x40;  /* Co=0, D/C#=1 (data) */
        memcpy(&buf[1], &oled->framebuf[page * SH1106_WIDTH], SH1106_WIDTH);
        i2c_write_blocking(oled->i2c, SH1106_I2C_ADDR, buf, SH1106_WIDTH + 1, false);
    }
    oled->dirty = false;
}

void SH1106_SetPixel(SH1106 *oled, int x, int y, bool on) {
    if (x < 0 || x >= SH1106_WIDTH || y < 0 || y >= SH1106_HEIGHT) return;
    uint16_t idx = (y / 8) * SH1106_WIDTH + x;
    if (on) {
        oled->framebuf[idx] |= (1 << (y & 7));
    } else {
        oled->framebuf[idx] &= ~(1 << (y & 7));
    }
}

void SH1106_DrawChar(SH1106 *oled, int x, int y, char c, bool invert) {
    if (c < 32 || c > 126) c = '?';
    const uint8_t *glyph = &font5x7[(c - 32) * 5];
    for (int col = 0; col < 5; col++) {
        uint8_t line = glyph[col];
        if (invert) line = ~line;
        for (int row = 0; row < 7; row++) {
            SH1106_SetPixel(oled, x + col, y + row, (line >> row) & 1);
        }
    }
    /* 1-pixel gap between characters */
    for (int row = 0; row < 7; row++) {
        SH1106_SetPixel(oled, x + 5, y + row, invert);
    }
}

void SH1106_DrawString(SH1106 *oled, int x, int y, const char *str, bool invert) {
    while (*str) {
        SH1106_DrawChar(oled, x, y, *str, invert);
        x += 6;
        str++;
    }
}

void SH1106_DrawStringLarge(SH1106 *oled, int x, int y, const char *str, bool invert) {
    /* 2x scaled font rendering */
    while (*str) {
        if (*str < 32 || *str > 126) { str++; x += 12; continue; }
        const uint8_t *glyph = &font5x7[(*str - 32) * 5];
        for (int col = 0; col < 5; col++) {
            uint8_t line = glyph[col];
            if (invert) line = ~line;
            for (int row = 0; row < 7; row++) {
                bool on = (line >> row) & 1;
                SH1106_SetPixel(oled, x + col * 2,     y + row * 2,     on);
                SH1106_SetPixel(oled, x + col * 2 + 1, y + row * 2,     on);
                SH1106_SetPixel(oled, x + col * 2,     y + row * 2 + 1, on);
                SH1106_SetPixel(oled, x + col * 2 + 1, y + row * 2 + 1, on);
            }
        }
        /* 2-pixel gap */
        for (int row = 0; row < 14; row++) {
            SH1106_SetPixel(oled, x + 10, y + row, invert);
            SH1106_SetPixel(oled, x + 11, y + row, invert);
        }
        x += 12;
        str++;
    }
}

void SH1106_FillRect(SH1106 *oled, int x, int y, int w, int h, bool on) {
    for (int i = x; i < x + w; i++) {
        for (int j = y; j < y + h; j++) {
            SH1106_SetPixel(oled, i, j, on);
        }
    }
}

void SH1106_DrawRect(SH1106 *oled, int x, int y, int w, int h) {
    SH1106_DrawHLine(oled, x, y, w);
    SH1106_DrawHLine(oled, x, y + h - 1, w);
    SH1106_DrawVLine(oled, x, y, h);
    SH1106_DrawVLine(oled, x + w - 1, y, h);
}

void SH1106_DrawHLine(SH1106 *oled, int x, int y, int w) {
    for (int i = x; i < x + w; i++) {
        SH1106_SetPixel(oled, i, y, true);
    }
}

void SH1106_DrawVLine(SH1106 *oled, int x, int y, int h) {
    for (int j = y; j < y + h; j++) {
        SH1106_SetPixel(oled, x, j, true);
    }
}

void SH1106_DrawProgressBar(SH1106 *oled, int x, int y, int w, int h, float fraction) {
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;
    SH1106_DrawRect(oled, x, y, w, h);
    int fill = (int)((w - 2) * fraction);
    SH1106_FillRect(oled, x + 1, y + 1, fill, h - 2, true);
}

void SH1106_SetContrast(SH1106 *oled, uint8_t contrast) {
    sh1106_cmd(oled, 0x81);
    sh1106_cmd(oled, contrast);
}

void SH1106_InvertDisplay(SH1106 *oled, bool invert) {
    sh1106_cmd(oled, invert ? 0xA7 : 0xA6);
}
