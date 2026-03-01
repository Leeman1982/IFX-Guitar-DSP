# RP2350 Multi-Effect Guitar Unit - 1.3" OLED Module Version

A professional-grade guitar multi-effects processor based on the RP2350 microcontroller (Raspberry Pi Pico 2), featuring high-quality DSP algorithms and a 1.3" OLED display module with integrated rotary encoder.

## Features

- **4 High-Quality Effects:**
  - Noise Gate - Dynamic noise reduction with adjustable threshold and release
  - Overdrive - Tube-style asymmetrical clipping with tone control
  - Chorus - Stereo chorus with dual LFO modulation
  - Delay - Classic delay/echo with feedback control

- **1.3" OLED Module UI:**
  - SH1106 controller (128x64 resolution)
  - Integrated rotary encoder for navigation
  - BACK and CONFIRM buttons
  - Real-time parameter adjustment
  - Visual feedback of effect chain state

- **Professional Audio I/O:**
  - PCM1808 24-bit ADC for audio input
  - PCM5102 24-bit DAC for audio output
  - 48kHz sample rate
  - Stereo processing

## Hardware Requirements

### Components

1. **Raspberry Pi Pico 2 (RP2350)**
2. **1.3" OLED Module** - SH1106 128x64 with Integrated Rotary Encoder
   - Model: EC11 Rotary Encoder Module
   - Includes rotary encoder, BACK button, CONFIRM button
   - I2C interface (address 0x3C)
   - Available on AliExpress/Amazon (search "1.3 inch OLED EC11 rotary encoder")
3. **PCM1808 ADC Module** - 24-bit Stereo Audio ADC
4. **PCM5102 DAC Module** - 24-bit Stereo Audio DAC
5. **Power Supply** - 5V USB or regulated supply

### Pin Connections

#### RP2350 to 1.3" OLED Module

The 1.3" OLED module has the following pins (typical pinout):

```
Module Pin    | RP2350 Pin  | Description
--------------|-------------|------------------
GND           | GND         | Ground
VCC           | 3.3V        | Power (3.3V)
SCL           | GPIO 5      | I2C Clock
SDA           | GPIO 4      | I2C Data
TRA           | GPIO 6      | Rotary Encoder A
TRB           | GPIO 7      | Rotary Encoder B
TRS           | GPIO 8      | Rotary Encoder Switch
BACK          | GPIO 9      | Back Button
CONFIRM       | GPIO 10     | Confirm Button
```

**Note:** Some modules may have different pin labels. TRA/TRB are the rotary encoder quadrature outputs, TRS is the encoder push switch.

#### RP2350 to PCM1808 (ADC Input)
```
RP2350 Pin    | PCM1808 Pin | Description
--------------|-------------|------------------
GPIO 26       | DOUT        | Audio Data Input
GPIO 21       | LRCK        | Left/Right Clock
GPIO 20       | BCK         | Bit Clock
3.3V          | VCC         | Power
GND           | GND         | Ground
```

#### RP2350 to PCM5102 (DAC Output)
```
RP2350 Pin    | PCM5102 Pin | Description
--------------|-------------|------------------
GPIO 22       | DIN         | Audio Data Output
GPIO 21       | LRCK/WS     | Left/Right Clock (Shared)
GPIO 20       | BCK         | Bit Clock (Shared)
3.3V          | VCC         | Power
GND           | GND         | Ground
GND           | SCK         | System Clock (tied to GND for auto)
3.3V          | FMT         | Format (I2S = HIGH)
GND           | DEMP        | De-emphasis off
3.3V or GND   | XSMT        | Soft mute control (optional)
```

### Complete Pin Summary

```
RP2350 GPIO   | Function
--------------|------------------
GPIO 4        | I2C SDA (Display)
GPIO 5        | I2C SCL (Display)
GPIO 6        | Encoder A (TRA)
GPIO 7        | Encoder B (TRB)
GPIO 8        | Encoder Switch
GPIO 9        | BACK Button
GPIO 10       | CONFIRM Button
GPIO 20       | I2S BCK (Audio)
GPIO 21       | I2S LRCK (Audio)
GPIO 22       | I2S DOUT (DAC)
GPIO 26       | I2S DIN (ADC)
```

## Software Setup

### Prerequisites

- [PlatformIO](https://platformio.org/) installed
- USB cable for programming
- Git (for cloning the repository)

### Libraries Used

- **U8g2** - Graphics library for SH1106 OLED display
- **Arduino Audio Tools** - I2S audio handling framework
- **Wire** - I2C communication (built-in)

### Building and Uploading

1. **Navigate to the project directory:**
   ```bash
   cd arduino/RP2350_MultiEffectUnit_1.3inch
   ```

2. **Build the project:**
   ```bash
   pio run
   ```

3. **Upload to RP2350:**
   ```bash
   pio run --target upload
   ```

4. **Monitor serial output (optional):**
   ```bash
   pio device monitor
   ```

## User Interface

### Controls

- **Rotary Encoder:** Navigate and adjust values
- **Encoder Press:** Toggle effects on/off in main menu
- **CONFIRM Button:** Enter parameter menu for selected effect
- **BACK Button:** Return to main menu

### Main Menu

The main menu displays the effect chain. Each effect can be enabled or disabled:

- **Rotate encoder** - Navigate through effects
- **Press encoder** - Toggle effect on/off (indicated by [X] or [ ])
- **Press CONFIRM** - Enter parameter adjustment mode for selected effect

### Parameter Menu

When in parameter mode for a selected effect:

- **Rotate encoder** - Adjust parameter value
- **Press BACK** - Return to main menu

### Effect Parameters

#### Noise Gate
- `Threshold` (-60dB to 0dB) - Level below which signal is muted
- `Release` (10ms to 500ms) - Time to fade out when signal drops

#### Overdrive
- `PreGain` (0 to 255) - Input gain/drive amount
- `Tone` (1000Hz to 8000Hz) - Output low-pass filter cutoff

#### Chorus
- `Rate` (0.1Hz to 5Hz) - LFO speed
- `Depth` (0.0 to 1.0) - Modulation depth
- `Mix` (0.0 to 1.0) - Wet/dry balance

#### Delay
- `Time` (10ms to 1000ms) - Delay time
- `Feedback` (0.0 to 0.95) - Number of repeats
- `Mix` (0.0 to 1.0) - Wet/dry balance

## Signal Chain

The effects are processed in the following order:

```
Input → Noise Gate → Overdrive → Chorus → Delay → Output
```

Each effect can be bypassed independently without affecting the signal flow.

## Audio Performance

- **Sample Rate:** 48kHz
- **Bit Depth:** 16-bit processing (24-bit I/O capable)
- **Latency:** <10ms (buffer-dependent)
- **Processing:** Real-time floating-point DSP
- **Channels:** Stereo

## Display Information

The 1.3" OLED module uses the **SH1106** controller (not SSD1306). Key differences:

- **Resolution:** 128x64 pixels
- **Controller:** SH1106 (U8g2 library support)
- **Interface:** I2C (address 0x3C)
- **Features:** Better contrast and viewing angle than SSD1306
- **Library:** U8g2 (optimized for embedded systems)

## Troubleshooting

### Display Issues

**Display not working:**
- Verify I2C address (default 0x3C, some modules use 0x3D)
- Check SDA/SCL connections
- Ensure 3.3V power supply
- Try adjusting I2C pull-up resistors (4.7kΩ recommended)

**Display shows garbage:**
- Wrong controller - this module uses SH1106, not SSD1306
- Check if module requires initialization sequence
- Verify I2C bus speed (default 100kHz works best)

**Display is dim:**
- Adjust contrast in U8g2 initialization
- Check power supply voltage (should be 3.3V)

### Encoder Issues

**Encoder not responding:**
- Check TRA, TRB, and TRS connections
- Verify pull-up resistors are enabled (INPUT_PULLUP)
- Test encoder independently with serial output

**Encoder direction reversed:**
- Swap TRA and TRB pins in code or physically

**Encoder skips or jumps:**
- This is normal encoder behavior
- Software debouncing is implemented
- Hardware filtering (capacitors) can help

### Button Issues

**Buttons not working:**
- Check BACK and CONFIRM pin connections
- Verify buttons are wired to pull LOW when pressed
- Adjust debounce delay if needed (default 50ms)

### Audio Issues

**No audio output:**
- Check all I2S connections (BCK, LRCK, DIN/DOUT)
- Verify power supply to ADC/DAC modules
- Ensure PCM5102 FMT pin is HIGH for I2S mode

**Distorted audio:**
- Reduce input gain
- Check for clipping (input level too high)
- Adjust effect parameters (especially overdrive gain)

**Latency too high:**
- Reduce BUFFER_SIZE in code (trade-off with stability)
- Current setting: 256 samples ≈ 5.3ms @ 48kHz

## Module Variants

There are several versions of the 1.3" OLED encoder module available:

1. **White OLED** - Most common, better readability
2. **Blue OLED** - Alternative color option
3. **Different button layouts** - Some have buttons on top, others on side
4. **Pin header variations** - Check your module's pinout diagram

**Important:** Always verify your specific module's pinout before connecting!

## Wiring Tips

1. **Use short I2C wires** - Keep SDA/SCL connections under 15cm for reliability
2. **Add decoupling capacitors** - 100nF ceramic caps near VCC/GND on each module
3. **Separate analog and digital grounds** - Connect at a single point near the power supply
4. **Use shielded cable for audio** - Reduces noise pickup
5. **Twisted pair for I2S** - Helps reduce crosstalk

## Credits

- **Original DSP Algorithms:** Philip Salmony ([phils-lab.net](https://phils-lab.net))
- **InfiniFX Platform:** [IFX-Guitar-DSP Repository](https://github.com/pms67/IFX-Guitar-DSP)
- **Arduino Audio Tools:** Phil Schatzmann
- **U8g2 Library:** Oliver Kraus
- **RP2350 Port:** Community contribution

## License

This project inherits the MIT License from the original InfiniFX platform.

## References

- [U8g2 Library Documentation](https://github.com/olikraus/u8g2)
- [Arduino Audio Tools](https://github.com/pschatzmann/arduino-audio-tools)
- [RP2350 Datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf)
- [SH1106 Controller Datasheet](https://www.velleman.eu/downloads/29/infosheets/sh1106_datasheet.pdf)
- [PCM1808 Datasheet](https://www.ti.com/product/PCM1808)
- [PCM5102 Datasheet](https://www.ti.com/product/PCM5102)

## Version Comparison

### 1.3" OLED Module Version (This Version)
- ✅ Integrated rotary encoder and buttons
- ✅ Larger display (better readability)
- ✅ Fewer wiring connections
- ✅ Better contrast (SH1106 vs SSD1306)
- ✅ Professional look with mounted encoder
- ⚠️ Slightly higher cost

### 0.93" Separate Components Version
- ✅ More flexible component placement
- ✅ Lower cost
- ✅ Easier to source components separately
- ⚠️ More wiring required
- ⚠️ Smaller display

Both versions share the same DSP algorithms and audio quality!
