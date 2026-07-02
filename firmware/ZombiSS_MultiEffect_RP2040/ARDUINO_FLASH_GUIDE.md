# Zombi SS Multi-Effect DSP - RP2040 Arduino IDE Flash Guide

## RP2040 vs RP2350 Differences

| Spec | RP2040 (this version) | RP2350 |
|------|----------------------|--------|
| CPU | Dual Cortex-M0+ | Dual Cortex-M33 |
| FPU | None (software float) | Hardware FPU |
| Clock | 133 MHz | 150 MHz |
| SRAM | 264 KB | 520 KB |
| Sample Rate | 32 kHz | 48 kHz |
| Max Delay | 375 ms | 660 ms |
| Buffer Frames | 128 (~4ms) | 64 (~1.3ms) |
| Chorus Buffer | 1024 samples/line | 2048 samples/line |
| Total Latency | ~8 ms | ~2.67 ms |

The RP2040 version trades sample rate and delay time for compatibility
with the original Pico's smaller memory and lack of hardware FPU.
The 32kHz rate matches the original STM32H7 version.

---

## Step 1: Install Arduino IDE 2.x

Download from: https://www.arduino.cc/en/software

---

## Step 2: Install Arduino-Pico Board Package

1. **File > Preferences**
2. In **"Additional Board Manager URLs"** paste:
   ```
   https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
   ```
3. **Tools > Board > Boards Manager**
4. Search **"pico"**, install **"Raspberry Pi Pico/RP2040/RP2350"** by Earle Philhower

---

## Step 3: Open the Sketch

**File > Open** → navigate to:
```
IFX-Guitar-DSP/firmware/ZombiSS_MultiEffect_RP2040/ZombiSS_MultiEffect_RP2040.ino
```

---

## Step 4: Board Settings

| Setting | Value |
|---------|-------|
| **Board** | `Raspberry Pi Pico` |
| **Flash Size** | `2MB (no FS)` |
| **CPU Speed** | `133 MHz` |
| **Optimize** | `Optimize Even More (-O2)` |
| **USB Stack** | `Pico SDK` |
| **Upload Method** | `Default (UF2)` |

> **Critical:** Select **Raspberry Pi Pico** (the original RP2040 board),
> NOT "Pico 2" or "Pico W".

---

## Step 5: Upload

### First Time (BOOTSEL)
1. **Hold BOOTSEL** button on the Pico
2. **Plug in USB cable** while holding
3. **Release BOOTSEL** — board mounts as `RPI-RP2` drive
4. In Arduino IDE: **Tools > Port > UF2 Board**
5. Click **Upload** (right arrow button)

### Subsequent Uploads (Auto-Reset)
1. Just connect USB
2. **Tools > Port** → select the serial port (`/dev/ttyACM0`, `COM3`, etc.)
3. Click **Upload** — board auto-resets and flashes

---

## Step 6: Verify

After upload:
1. Zombi SS splash screen appears on OLED
2. Chain view shows 5 effects (Overdrive ON by default)
3. Audio passes through at 32kHz

---

## Wiring (Identical to RP2350 Version)

### PCM5102 DAC
| Pin | GPIO |
|-----|------|
| DIN | GP16 |
| BCK | GP17 |
| LRCK | GP18 |
| SCK | GND |
| XSMT | 3.3V |

### PCM1808 ADC
| Pin | GPIO |
|-----|------|
| DOUT | GP19 |
| LRCK | GP20 (wire from GP18) |
| BCK | GP21 (wire from GP17) |
| MD0 | GND, MD1 | 3.3V |

### Estardyn OLED Module
| Pin | GPIO |
|-----|------|
| SDA | GP4 |
| SCL | GP5 |
| TRA | GP10 |
| TRB | GP11 |
| PSH | GP12 |
| BAK | GP14 |
| CON | GP15 |

### FX Bypass Switches (to GND)
| SW | GPIO | Effect |
|----|------|--------|
| 1 | GP6 | Gate |
| 2 | GP7 | Drive |
| 3 | GP8 | EQ |
| 4 | GP9 | Chorus |
| 5 | GP13 | Delay |

---

## MCLK for PCM1808

The PCM1808 needs a system clock. At 32kHz, MCLK = 256 × 32000 = 8.192 MHz.

Add to `setup()` before `I2SAudio_Start()`:
```cpp
#include "hardware/pwm.h"

// Generate ~8.3MHz on GP22 (133MHz / 16 = 8.3125MHz, within PCM1808 tolerance)
void mclk_init() {
    gpio_set_function(22, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(22);
    pwm_set_wrap(slice, 15);    // 133MHz / 16 = 8.3125MHz
    pwm_set_chan_level(slice, pwm_gpio_to_channel(22), 8);  // 50% duty
    pwm_set_enabled(slice, true);
}
```

Wire GP22 → PCM1808 SCKI.

---

## Memory Budget (264KB SRAM)

| Component | Size |
|-----------|------|
| Delay buffer | 48 KB (12000 × 4 bytes) |
| Chorus buffers | 8 KB (2 × 1024 × 4) |
| MovingRMS buffer | 6.4 KB (1600 × 4) |
| Overdrive FIR | 0.3 KB |
| DMA audio buffers | 4 KB |
| OLED framebuffer | 1 KB |
| Stack (both cores) | 16 KB |
| Code data/BSS | ~16 KB |
| **Total** | **~100 KB** |
| **Free** | **~164 KB** |

---

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| Build error: "RP2350" | Wrong board selected — choose "Raspberry Pi Pico" (not Pico 2) |
| Drive shows as `RP2350` | Wrong board — you have a Pico 2, use the RP2350 sketch instead |
| Audio crackling | Too many effects active. Try disabling Chorus or Delay |
| No audio | Check PCM1808 SCKI clock, XSMT=3.3V on PCM5102 |
| Out of memory crash | Reduce delay time or disable effects at startup |
| Slow UI response | Normal — M0+ is slower. UI runs at 60Hz on Core 1 |
