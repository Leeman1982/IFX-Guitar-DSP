# Zombi SS Multi-Effect DSP Unit - Build & Flash Guide

## Hardware Requirements

### Microcontroller
- **Raspberry Pi Pico 2** (RP2350-based) or compatible RP2350 board

### Audio Components
| Component | Description | Connection |
|-----------|-------------|------------|
| **PCM1808** | 24-bit stereo ADC (I2S slave mode) | Guitar input signal |
| **PCM5102** | 32-bit stereo DAC (I2S slave mode) | Amplifier/speaker output |

### UI Components (Estardyn Module)
| Component | Description |
|-----------|-------------|
| **SH1106 1.3" OLED** | 128x64 white/blue I2C display |
| **EC11 Rotary Encoder** | Built into the Estardyn module |
| **BACK button** | Built into the Estardyn module |
| **CONFIRM button** | Built into the Estardyn module |

### Additional Components
| Component | Qty | Description |
|-----------|-----|-------------|
| Momentary push switches | 5 | One per effect bypass (normally open, active low) |
| 3.3V regulator | 1 | If powering from higher voltage supply |
| Decoupling capacitors | 4 | 100nF ceramic, one per IC VCC |
| Bulk capacitors | 2 | 10uF electrolytic on power rails |

---

## Wiring Diagram

### PCM5102 DAC (Output to Amp)

| PCM5102 Pin | Connect To | Notes |
|-------------|-----------|-------|
| VCC | 3.3V | |
| GND | GND | |
| BCK | GP17 | Bit clock from RP2350 PIO0 |
| LRCK (WS) | GP18 | Word select from RP2350 PIO0 |
| DIN | GP16 | Serial audio data from RP2350 |
| FMT | GND | I2S standard format |
| DEMP | GND | De-emphasis off |
| XSMT | 3.3V | Soft mute off (unmuted) |
| FLT | GND | Normal latency filter |
| SCK | GND | System clock from internal PLL |

### PCM1808 ADC (Guitar Input)

| PCM1808 Pin | Connect To | Notes |
|-------------|-----------|-------|
| VCC | 3.3V | |
| GND | GND | |
| DOUT | GP19 | Serial audio data to RP2350 |
| BCK | GP21 | Wire directly from GP17 (shared clock) |
| LRCK | GP20 | Wire directly from GP18 (shared clock) |
| FMT0 | GND | I2S standard format |
| FMT1 | GND | |
| MD0 | GND | Slave mode |
| MD1 | 3.3V | |
| SCKI | 12.288MHz or GND* | See note below |
| VINL | Guitar signal via preamp | See input circuit |
| VINR | GND (or bridge to VINL) | Mono input |

> **\*SCKI Note:** The PCM1808 typically needs a system clock (256fs = 12.288MHz
> for 48kHz). In slave mode you can either:
> 1. Feed a 12.288MHz crystal oscillator to SCKI
> 2. Generate it from RP2350 using a PWM pin at 12.288MHz (see Optional section)
> 3. Some modules have an onboard oscillator

### Estardyn OLED+Encoder Module

| Module Pin | Connect To | Notes |
|------------|-----------|-------|
| CON (GND) | GND | Common ground |
| SDA | GP4 | I2C0 SDA (with pull-up on module) |
| SCL | GP5 | I2C0 SCL (with pull-up on module) |
| PSH (Encoder Push) | GP12 | Active low, internal pull-up enabled |
| TRA (Encoder A) | GP10 | Internal pull-up enabled |
| TRB (Encoder B) | GP11 | Internal pull-up enabled |
| BAK (Back Button) | GP14 | Active low, internal pull-up enabled |
| GND | GND | |
| VCC | 3.3V | |

### Effect Bypass Switches

Wire 5 momentary push-buttons between the GPIO pin and GND.
Internal pull-ups are enabled in firmware - no external resistors needed.

| Switch | GPIO | Effect |
|--------|------|--------|
| SW1 | GP6 | Noise Gate ON/OFF |
| SW2 | GP7 | Overdrive ON/OFF |
| SW3 | GP8 | EQ ON/OFF |
| SW4 | GP9 | Chorus ON/OFF |
| SW5 | GP13 | Delay ON/OFF |

### Guitar Input Preamp Circuit (Recommended)

```
Guitar Tip ──┬── 1M Ohm ── GND (input impedance)
             │
             ├── 100nF cap ── PCM1808 VINL
             │
Guitar Sleeve── GND
```

For a hotter signal, add an op-amp buffer (e.g., TL072) before the ADC.

---

## Software Build Instructions

### 1. Install Prerequisites

#### Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi \
    build-essential libstdc++-arm-none-eabi-newlib git
```

#### macOS
```bash
brew install cmake arm-none-eabi-gcc
```

#### Windows
1. Install [ARM GCC toolchain](https://developer.arm.com/downloads/-/gnu-rm)
2. Install [CMake](https://cmake.org/download/)
3. Install [Git for Windows](https://git-scm.com/download/win)
4. Install [MinGW or MSYS2](https://www.msys2.org/) for make

### 2. Install Pico SDK

```bash
# Clone the Pico SDK (must be v2.0+ for RP2350 support)
cd ~
git clone https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git submodule update --init

# Set environment variable (add to .bashrc/.zshrc for persistence)
export PICO_SDK_PATH=~/pico-sdk
```

### 3. Clone This Repository

```bash
git clone https://github.com/Leeman1982/IFX-Guitar-DSP.git
cd IFX-Guitar-DSP
```

### 4. Build the Firmware

```bash
cd firmware/ZombiSS-MultiEffect-RP2350

# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build
make -j$(nproc)
```

On success you will see:
```
[100%] Built target zombi_ss_multi_fx
```

Build outputs in the `build/` directory:
- `zombi_ss_multi_fx.uf2` - **This is the file you flash**
- `zombi_ss_multi_fx.elf` - Debug binary
- `zombi_ss_multi_fx.bin` - Raw binary
- `zombi_ss_multi_fx.hex` - Intel HEX format

---

## Flashing the RP2350

### Method 1: USB Mass Storage (BOOTSEL - Easiest)

1. **Hold the BOOTSEL button** on the Pico 2
2. **While holding**, connect the USB cable to your computer
3. **Release BOOTSEL** - the Pico 2 appears as a USB drive called `RP2350`
4. **Copy the UF2 file** to the drive:

```bash
# Linux
cp build/zombi_ss_multi_fx.uf2 /media/$USER/RP2350/

# macOS
cp build/zombi_ss_multi_fx.uf2 /Volumes/RP2350/

# Windows
copy build\zombi_ss_multi_fx.uf2 E:\
```
> Replace `E:\` with the actual drive letter shown in File Explorer.

5. The Pico 2 will **automatically reboot** and begin running the firmware.
   The OLED should show the Zombi SS splash screen within 1 second.

### Method 2: picotool (No Button Press Required)

```bash
# Install picotool
sudo apt install picotool   # Ubuntu 24.04+
# OR build from source:
# git clone https://github.com/raspberrypi/picotool.git
# cd picotool && mkdir build && cd build && cmake .. && make && sudo make install

# Flash while Pico is running (forces reboot into bootloader)
picotool load build/zombi_ss_multi_fx.uf2 --force
picotool reboot
```

### Method 3: SWD Debug Probe (For Development)

Using a Raspberry Pi Debug Probe or another Pico as debugger:

**Wiring (3 wires):**
| Debug Probe | Target Pico 2 |
|-------------|--------------|
| SWCLK | SWCLK (test pad) |
| SWDIO | SWDIO (test pad) |
| GND | GND |

```bash
# Install OpenOCD with RP2350 support
sudo apt install openocd

# Flash via SWD
openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg \
    -c "adapter speed 5000" \
    -c "program build/zombi_ss_multi_fx.elf verify reset exit"
```

### Method 4: Using VS Code + Raspberry Pi Pico Extension

1. Install VS Code
2. Install the "Raspberry Pi Pico" extension
3. Open the `firmware/ZombiSS-MultiEffect-RP2350/` folder
4. Click "Build" in the status bar
5. Click "Flash" to upload via BOOTSEL or SWD

---

## First Boot Verification

After flashing, the unit should:

1. **OLED shows splash screen** - Zombi SS logo with skull, lightning bolts,
   "MULTI-EFFECT DSP" subtitle, flash animation (~2 seconds)
2. **Chain view appears** - Shows all 5 effects with ON/OFF status:
   ```
   ┌──────────────────────┐
   │ ZOMBI SS  FX CHAIN   │
   │ 1 [OFF] GATE         │
   │ 2 [ON]  DRIVE    ■   │
   │ 3 [OFF] EQ           │
   │ 4 [OFF] CHORUS       │
   │ 5 [OFF] DELAY        │
   │ Turn:Sel  Push:Edit   │
   └──────────────────────┘
   ```
3. **Audio passes through** - With only Overdrive active by default

### If Nothing Appears on OLED
- Check I2C wiring (SDA=GP4, SCL=GP5)
- Verify 3.3V power to the Estardyn module
- Try alternate I2C address: some SH1106 modules use 0x3D instead of 0x3C.
  Edit `SH1106_I2C_ADDR` in `include/drivers/sh1106_oled.h` if needed.

### If No Audio
- Verify BCK and LRCK are reaching both the DAC and ADC
- Check PCM1808 has its system clock (SCKI)
- Verify PCM5102 XSMT pin is HIGH (unmuted)
- Check PCM1808 MD0=LOW, MD1=HIGH (slave mode)

---

## UI Operating Instructions

### Navigation
| Action | Control |
|--------|---------|
| Move through effects/params | Rotate encoder |
| Enter effect parameters | Press encoder |
| Edit parameter value | Press encoder on parameter |
| Adjust value while editing | Rotate encoder |
| Confirm / save edit | Press encoder or CONFIRM button |
| Go back one screen | BACK button |
| Master volume | Long-press encoder from chain view |
| Toggle any effect | Press corresponding FX switch (SW1-SW5) |

### Screen Flow
```
Splash Screen
    │
    ▼
Chain View ──(long press)──▶ Master Volume
    │                            │
    │ (press)                    │ (press/back)
    ▼                            ▼
Effect Params ◀──────────── Chain View
    │
    │ (press)
    ▼
Param Edit
    │
    │ (press/back/confirm)
    ▼
Effect Params
```

### Default Parameter Values

**Noise Gate** (OFF by default)
- Threshold: 0.10, Attack: 2.0ms, Release: 2.0ms, Hold: 20.0ms

**Overdrive** (ON by default)
- Gain: 40, Boost: 0, HPF: 150Hz, LPF: 5000Hz, Damping: 1.0, Clip Q: -0.20

**Peaking EQ** (OFF by default)
- Freq: 1000Hz, BW: 500Hz, Gain: 1.0x

**Chorus** (OFF by default)
- Delay A: 10ms, Delay B: 15ms, Depth A/B: 10, Rate A: 1.0Hz, Rate B: 1.3Hz,
  Gain A/B: 0.25, Mix: 0.50

**Delay** (OFF by default)
- Time: 300ms, Mix: 0.35, Feedback: 0.40

---

## Optional: Generate 12.288MHz System Clock for PCM1808

If your PCM1808 module doesn't have an onboard oscillator, you can generate
the required system clock from the RP2350 using a PWM output.

Add this to `main.c` before `I2SAudio_Init()`:

```c
#include "hardware/pwm.h"

// Generate 12.288MHz MCLK on GP22 for PCM1808
#define MCLK_PIN 22

void mclk_init(void) {
    gpio_set_function(MCLK_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(MCLK_PIN);

    // 150MHz / 12.288MHz ≈ 12.207 → use wrap=11, duty=6 for ~50% at 12.5MHz
    // Close enough for PCM1808 tolerance (±2%)
    pwm_set_wrap(slice, 11);
    pwm_set_chan_level(slice, pwm_gpio_to_channel(MCLK_PIN), 6);
    pwm_set_enabled(slice, true);
}
```

Then wire GP22 to PCM1808 SCKI pin.

---

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| OLED blank | Wrong I2C address or wiring | Check SDA/SCL, try 0x3D |
| No audio output | DAC muted | Ensure XSMT = 3.3V |
| Distorted/clipping input | Input too hot | Add voltage divider before ADC |
| Crackling audio | Clock mismatch | Verify BCK/LRCK shared between ADC and DAC |
| Encoder skips values | Noisy encoder | Already debounced in firmware, check wiring |
| Effects don't toggle | Wrong switch pin | Verify GP6-9, GP13 wiring |
| Build fails: "RP2350 not supported" | Old Pico SDK | Update to SDK v2.0+ |
| UF2 won't copy | Wrong bootloader | Hold BOOTSEL, re-plug USB |

---

## Technical Specifications

| Parameter | Value |
|-----------|-------|
| MCU | RP2350 dual-core ARM Cortex-M33 @ 150MHz |
| Audio sample rate | 48 kHz |
| Audio bit depth | 24-bit (in 32-bit I2S frames) |
| Audio buffer | 64 frames, double-buffered |
| Processing latency | ~2.67 ms (input to output) |
| I2S interface | PIO-based (jitter-free hardware clocking) |
| Display | SH1106 128x64 OLED, I2C @ 400kHz |
| Effects | 5 (Noise Gate, Overdrive, EQ, Chorus, Delay) |
| Parameters | 25 adjustable + Master Volume |
| Bypass switches | 5 dedicated momentary (hardware debounced) |
| Core allocation | Core 0 = DSP, Core 1 = UI |
| Power | USB 5V or external 3.3V |
