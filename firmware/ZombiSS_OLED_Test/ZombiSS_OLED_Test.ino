/*
 * OLED Test - Bare minimum SH1106 test sketch
 *
 * This tests ONLY the OLED display. No DSP, no effects, no dual-core.
 * If this doesn't show anything, it's a wiring or hardware issue.
 *
 * REQUIRED: Install "U8g2" library in Arduino IDE:
 *   Sketch > Include Library > Manage Libraries > search "U8g2" > Install
 *
 * Board: Raspberry Pi Pico (in Arduino IDE)
 *
 * WIRING:
 *   OLED SDA  -> GP4  (physical pin 6)
 *   OLED SCL  -> GP5  (physical pin 7)
 *   OLED VCC  -> 3V3  (physical pin 36)
 *   OLED GND  -> GND  (physical pin 38)
 */

#include <U8g2lib.h>

/* Try ALL common OLED types - uncomment one at a time if needed */

/* Option 1: SH1106 128x64 (most common 1.3" OLED) */
U8G2_SH1106_128X64_NONAME_F_SW_I2C display(U8G2_R0, /* clock=*/ 5, /* data=*/ 4, /* reset=*/ U8X8_PIN_NONE);

/* Option 2: If Option 1 doesn't work, comment it out and try SSD1306: */
// U8G2_SSD1306_128X64_NONAME_F_SW_I2C display(U8G2_R0, /* clock=*/ 5, /* data=*/ 4, /* reset=*/ U8X8_PIN_NONE);

/* Option 3: SH1106 alternate (VCOMH0 variant): */
// U8G2_SH1106_128X64_VCOMH0_F_SW_I2C display(U8G2_R0, /* clock=*/ 5, /* data=*/ 4, /* reset=*/ U8X8_PIN_NONE);

/* Option 4: SH1107 (some 1.3" modules use this instead): */
// U8G2_SH1107_128X64_F_SW_I2C display(U8G2_R0, /* clock=*/ 5, /* data=*/ 4, /* reset=*/ U8X8_PIN_NONE);

int frame = 0;

void setup() {
    Serial.begin(115200);
    delay(2000);  /* Wait for serial monitor */

    Serial.println("=== OLED TEST START ===");
    Serial.println("Pins: SDA=GP4, SCL=GP5");
    Serial.println();

    /* Scan ALL possible I2C addresses using bit-bang */
    Serial.println("I2C Address Scan (software bit-bang):");
    bool anyFound = false;

    /* Quick manual I2C scan using U8x8 */
    uint8_t addrs[] = { 0x3C, 0x3D, 0x78, 0x7A };
    for (int i = 0; i < 4; i++) {
        Serial.print("  Trying 0x");
        Serial.print(addrs[i], HEX);
        Serial.print("... ");

        /* Try to init display with this address */
        display.setI2CAddress(addrs[i] <= 0x3D ? addrs[i] * 2 : addrs[i]);
        Serial.println("(will try during begin())");
    }

    Serial.println();
    Serial.println("Attempting display.begin() with default address...");

    /* Try default address first (0x3C = 0x78 in 8-bit) */
    display.setI2CAddress(0x78);
    display.begin();

    Serial.println("display.begin() completed!");
    Serial.println();

    /* Draw something obvious */
    display.clearBuffer();
    display.setFont(u8g2_font_ncenB14_tr);
    display.drawStr(10, 30, "ZOMBI SS");
    display.setFont(u8g2_font_5x7_tr);
    display.drawStr(20, 50, "OLED TEST OK!");
    display.sendBuffer();

    Serial.println("Frame sent to display.");
    Serial.println("If you see 'ZOMBI SS' on screen, the display works!");
    Serial.println();
    Serial.println("If screen is STILL blank:");
    Serial.println("  1. Check wiring: SDA=GP4(pin6) SCL=GP5(pin7)");
    Serial.println("  2. Try swapping SDA and SCL wires");
    Serial.println("  3. Check VCC is 3.3V not 5V");
    Serial.println("  4. Uncomment a different display type in the sketch");
    Serial.println("     (SSD1306, SH1106_VCOMH0, or SH1107)");
}

void loop() {
    delay(1000);
    frame++;

    /* Keep updating so we can see if it ever starts working */
    display.clearBuffer();
    display.setFont(u8g2_font_ncenB14_tr);
    display.drawStr(10, 30, "ZOMBI SS");
    display.setFont(u8g2_font_5x7_tr);

    char buf[32];
    snprintf(buf, sizeof(buf), "Frame: %d", frame);
    display.drawStr(20, 50, buf);

    /* Draw a moving pixel line to prove the display is refreshing */
    int barX = (frame * 4) % 128;
    display.drawVLine(barX, 55, 9);

    display.sendBuffer();

    Serial.print("Frame ");
    Serial.println(frame);
}
