# Zombi SS Multi-Effect DSP - Arduino IDE Flash Guide

## Step 1: Install Arduino IDE

Download and install **Arduino IDE 2.x** from:
https://www.arduino.cc/en/software

(Version 2.3.0 or newer recommended)

---

## Step 2: Install the Arduino-Pico Board Package

This project uses Earle Philhower's **arduino-pico** core which supports RP2350.

1. Open Arduino IDE
2. Go to **File > Preferences**
3. In **"Additional Board Manager URLs"**, paste:
   ```
   https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
   ```
4. Click **OK**
5. Go to **Tools > Board > Boards Manager**
6. Search for **"pico"**
7. Install **"Raspberry Pi Pico/RP2040/RP2350"** by Earle F. Philhower III
   - **Version 4.0.0 or newer** (must include RP2350 support)
8. Wait for installation to complete

---

## Step 3: Open the Sketch

1. Go to **File > Open**
2. Navigate to:
   ```
   IFX-Guitar-DSP/firmware/ZombiSS_MultiEffect/ZombiSS_MultiEffect.ino
   ```
3. Click **Open**

The IDE will show the main `.ino` file with all supporting `.cpp`, `.c`, and `.h`
files visible as tabs.

---

## Step 4: Configure Board Settings

Go to **Tools** menu and set the following:

| Setting | Value |
|---------|-------|
| **Board** | `Raspberry Pi Pico 2` |
| **Flash Size** | `4MB (no FS)` |
| **CPU Speed** | `150 MHz` (or `133 MHz` if 150 is unavailable) |
| **Optimize** | `Optimize Even More (-O2)` |
| **USB Stack** | `Pico SDK` |
| **IP/Bluetooth Stack** | `IPv4 Only` |
| **Upload Method** | `Default (UF2)` |
| **Debug Port** | `Serial` |

> **Important:** Select **Raspberry Pi Pico 2** (NOT the original Pico/RP2040).

---

## Step 5: Increase Wire Library Buffer

The SH1106 OLED needs to send 128+ bytes per I2C transaction.
The default Wire buffer may be too small. Add this to your Arduino-pico
`platform.local.txt` if you encounter display issues:

**Location (varies by OS):**
- **Linux:** `~/.arduino15/packages/rp2040/hardware/rp2040/<version>/`
- **macOS:** `~/Library/Arduino15/packages/rp2040/hardware/rp2040/<version>/`
- **Windows:** `%LOCALAPPDATA%\Arduino15\packages\rp2040\hardware\rp2040\<version>\`

Create or edit `platform.local.txt` and add:
```
compiler.cpp.extra_flags=-DWIRE_BUFFER_SIZE=256
compiler.c.extra_flags=-DWIRE_BUFFER_SIZE=256
```

> **Note:** The arduino-pico core typically has a 256-byte Wire buffer by default
> on RP2350, so this step may not be needed.

---

## Step 6: Compile (Verify)

1. Click the **checkmark button** (Verify) or press **Ctrl+R**
2. Wait for compilation to complete
3. Verify output shows:
   ```
   Sketch uses XXXXXX bytes (XX%) of program storage space.
   Global variables use XXXXXX bytes (XX%) of dynamic memory.
   ```

### Common Build Errors and Fixes

| Error | Fix |
|-------|-----|
| `'pio_sm_config' was not declared` | Board not set to Pico 2 - check Tools > Board |
| `'Wire' was not declared` | Board package not installed - redo Step 2 |
| `undefined reference to 'IFX_...'` | .c files not compiling - ensure all files are in sketch folder |
| `multiple definition of...` | Duplicate file - check for copies in sketch folder |

---

## Step 7: Flash via USB (BOOTSEL Method)

### First Time / Fresh Board

1. **Hold the BOOTSEL button** on the Pico 2
2. **Plug in the USB cable** while holding the button
3. **Release BOOTSEL** - the board mounts as a USB drive `RP2350`
4. In Arduino IDE, select the port:
   - **Tools > Port > UF2 Board**
   - (It may show as `UF2 Board` or a COM/serial port)
5. Click the **right-arrow button** (Upload) or press **Ctrl+U**
6. The IDE uploads and the board reboots automatically

### Subsequent Uploads (Board Already Running)

Once the arduino-pico bootloader is installed, you do NOT need BOOTSEL:

1. Connect USB
2. **Tools > Port** - select the serial port that appears:
   - **Linux:** `/dev/ttyACM0`
   - **macOS:** `/dev/cu.usbmodem*`
   - **Windows:** `COM3` (or similar)
3. Click **Upload** (Ctrl+U)
4. The IDE automatically resets the board into bootloader and uploads

> If the port doesn't appear, the board may be in a crash loop.
> Use the BOOTSEL method to recover.

---

## Step 8: Verify Operation

After upload completes:

1. **OLED splash screen** appears:
   - Zombi SS logo with skull
   - "MULTI-EFFECT DSP"
   - ":: RP2350 ::"
   - Flash animation
2. **Chain view** appears showing all 5 effects:
   ```
   ZOMBI SS  FX CHAIN
   1 [OFF] GATE
   2 [ON]  DRIVE    ■
   3 [OFF] EQ
   4 [OFF] CHORUS
   5 [OFF] DELAY
   Turn:Sel  Push:Edit
   ```
3. **Audio** passes through with Overdrive active

---

## Wiring Reference

### PCM5102 DAC (Output)
| Pin | Connect to | Notes |
|-----|-----------|-------|
| VIN | 3V3 | board has its own regulator |
| GND | GND | |
| DIN | GP16 | serial data |
| BCK | GP17 | bit clock |
| LCK/LRCK | GP18 | word select |
| SCK | GND | selects internal PLL |
| XSMT | GP0 (or 3.3V) | firmware drives GP0 HIGH = unmuted |
| FMT | GND | I2S Philips format |

### PCM1808 ADC (Input) — slave mode, RP2350 supplies ALL clocks
| Pin | Connect to | Notes |
|-----|-----------|-------|
| **5V/VCC** | **5V** | **REQUIRED — analog supply. Total silence without it!** |
| GND | GND | |
| OUT | GP19 | serial data to RP2350 |
| **SCKI** | **GP22** | 12.5 MHz master clock from PWM — required |
| BCK | GP17 | shared with DAC BCK |
| LRC | GP18 | shared with DAC LRCK |
| FMT | GND | I2S Philips, 24-bit |
| MD0 | GND | MD0=MD1=GND → **slave mode** |
| MD1 | GND | (3.3V here would fight the RP2350's clocks) |

### Bridge wires (required)
The input PIO reads the clocks on its own pins, so jumper:
```
GP17 ──→ GP21   (BCK)
GP18 ──→ GP20   (LRCK)
```

### Estardyn OLED+Encoder Module
| Pin | GPIO |
|-----|------|
| SDA | GP4 |
| SCL | GP5 |
| TRA (Enc A) | GP10 |
| TRB (Enc B) | GP11 |
| PSH (Enc SW) | GP12 |
| BAK (Back) | GP14 |
| CON (Confirm) | GP15 |
| VCC | 3.3V |
| GND | GND |

### Effect Bypass Switches (momentary, to GND)
| Switch | GPIO | Effect |
|--------|------|--------|
| SW1 | GP6 | TS Boost |
| SW2 | GP7 | Noise Gate |
| SW3 | GP8 | Overdrive |
| SW4 | GP9 | 10-Band EQ |
| SW5 | GP13 | Chorus |

(Delay has no footswitch — toggle it from the OLED menu with CONFIRM.)

---

## Serial Monitor Debug

1. **Tools > Serial Monitor** (or Ctrl+Shift+M)
2. Set baud rate to **115200**
3. You can add debug prints in `loop()` on Core 0

---

## Updating the Firmware

To make changes and re-flash:

1. Edit files in the Arduino IDE (tabs show all source files)
2. Click **Verify** to check for errors
3. Click **Upload** to flash
4. Board resets automatically with new firmware

---

## Project Structure

```
ZombiSS_MultiEffect/
├── ZombiSS_MultiEffect.ino  ← Main sketch (setup/loop/setup1/loop1)
├── config.h                  ← Pin definitions and audio config
├── i2s_pio_programs.h        ← Pre-assembled PIO I2S programs
├── i2s_audio.h / .cpp        ← PIO+DMA I2S driver
├── sh1106_oled.h / .cpp      ← OLED display driver (Wire library)
├── rotary_encoder.h / .cpp   ← Encoder + button input
├── font5x7.h / .cpp          ← 5x7 pixel font data
├── splash_screen.h / .cpp    ← Zombi SS splash screen
├── ui_system.h / .cpp        ← Full menu/parameter UI
├── effect_chain.h / .c       ← Effect chain manager
├── ifx_overdrive.h / .c      ← Overdrive effect
├── ifx_noisegate.h / .c      ← Noise gate effect
├── ifx_delay.h / .c          ← Delay effect
├── ifx_chorus.h / .c         ← Chorus effect
├── ifx_peaking_filter.h / .c ← Peaking EQ effect
├── ifx_moving_rms.h / .c     ← Moving RMS utility
├── ifx_delay_line.h / .c     ← Delay line utility
└── ifx_log_audio_pot.h / .c  ← Logarithmic pot mapping
```

---

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| Compilation fails with RP2350 errors | Update arduino-pico to v4.0.0+ |
| "No port selected" | Hold BOOTSEL + plug USB, or install drivers |
| OLED shows nothing | Check I2C: SDA=GP4, SCL=GP5. Try addr 0x3D |
| No boot tone (first 2 s) | DAC side: XSMT high (GP0=3.3V), DIN/BCK/LCK wiring, FMT=GND |
| Boot tone OK but no guitar | ADC side: **PCM1808 5V pin connected?** SCKI on GP22? Bridges GP17→GP21, GP18→GP20? MD0=MD1=GND? |
| Upload fails repeatedly | Hold BOOTSEL, plug USB, try UF2 upload |
| Crackling audio | Verify bridge wires are short and solid |

### Scope bring-up checks (if you have a scope/logic analyzer)
1. GP18 → clean 48.0 kHz square wave (LRCK)
2. GP17 → ~3.1 MHz bit clock (BCK)
3. GP22 → 12.5 MHz (SCKI)
4. GP0 → sits at 3.3 V after boot (XSMT unmuted)
5. PCM1808 5V pin reads ~5 V with common GND
| Encoder jumpy | Normal for cheap EC11 - debounce is in firmware |
