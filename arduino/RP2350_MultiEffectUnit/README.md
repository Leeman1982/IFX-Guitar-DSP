# RP2350 Multi-Effect Guitar Unit

A professional-grade guitar multi-effects processor based on the RP2350 microcontroller (Raspberry Pi Pico 2), featuring high-quality DSP algorithms ported from the InfiniFX platform.

## Features

- **4 High-Quality Effects:**
  - Noise Gate - Dynamic noise reduction with adjustable threshold and release
  - Overdrive - Tube-style asymmetrical clipping with tone control
  - Chorus - Stereo chorus with dual LFO modulation
  - Delay - Classic delay/echo with feedback control

- **Interactive UI:**
  - 0.93" OLED display (128x64)
  - Rotary encoder with push button
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
2. **PCM1808 ADC Module** - 24-bit Stereo Audio ADC
3. **PCM5102 DAC Module** - 24-bit Stereo Audio DAC
4. **0.93" OLED Display** - 128x64 I2C (SSD1306)
5. **Rotary Encoder** - KY-040 or similar with push button
6. **Power Supply** - 5V USB or regulated supply

### Wiring Diagram

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

#### RP2350 to OLED Display
```
RP2350 Pin    | OLED Pin    | Description
--------------|-------------|------------------
GPIO 4 (SDA)  | SDA         | I2C Data
GPIO 5 (SCL)  | SCL         | I2C Clock
3.3V          | VCC         | Power
GND           | GND         | Ground
```

#### RP2350 to Rotary Encoder
```
RP2350 Pin    | Encoder Pin | Description
--------------|-------------|------------------
GPIO 6        | CLK         | Encoder A
GPIO 7        | DT          | Encoder B
GPIO 8        | SW          | Push Button
GND           | GND         | Ground
```

## Software Setup

### Prerequisites

- [PlatformIO](https://platformio.org/) installed
- USB cable for programming
- Git (for cloning the repository)

### Building and Uploading

1. **Clone the repository:**
   ```bash
   cd arduino/RP2350_MultiEffectUnit
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

### Main Menu

The main menu displays the effect chain. Each effect can be enabled or disabled:

- **Rotate encoder** - Navigate through effects
- **Short press** - Toggle effect on/off (indicated by [X] or [ ])
- **Long press** - Enter parameter adjustment mode

### Parameter Menu

When in parameter mode for a selected effect:

- **Rotate encoder** - Adjust parameter value
- **Click** - Exit to main menu

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
- **Bit Depth:** 16-bit processing (24-bit I/O)
- **Latency:** <10ms (buffer-dependent)
- **Processing:** Real-time floating-point DSP

## Troubleshooting

### No Audio Output
- Check all I2S connections (BCK, LRCK, DIN/DOUT)
- Verify power supply to ADC/DAC modules
- Ensure PCM5102 FMT pin is HIGH for I2S mode

### Display Not Working
- Verify I2C address (default 0x3C)
- Check SDA/SCL connections
- Try adjusting I2C pull-up resistors

### Encoder Not Responding
- Check CLK, DT, and SW connections
- Ensure pull-up resistors are enabled
- Verify ground connection

### Distorted Audio
- Reduce input gain
- Check for clipping (input level too high)
- Adjust effect parameters (especially overdrive gain)

## Credits

- **Original DSP Algorithms:** Philip Salmony ([phils-lab.net](https://phils-lab.net))
- **InfiniFX Platform:** [IFX-Guitar-DSP Repository](https://github.com/pms67/IFX-Guitar-DSP)
- **Arduino Audio Tools:** Phil Schatzmann
- **RP2350 Port:** Community contribution

## License

This project inherits the MIT License from the original InfiniFX platform.

## Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.

## Future Enhancements

- [ ] Preset save/load functionality
- [ ] Additional effects (reverb, flanger, phaser)
- [ ] MIDI control support
- [ ] USB audio interface mode
- [ ] Expression pedal input
- [ ] Tap tempo for delay

## References

- [InfiniFX GitHub Repository](https://github.com/pms67/IFX-Guitar-DSP)
- [RP2350 Datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf)
- [PCM1808 Datasheet](https://www.ti.com/product/PCM1808)
- [PCM5102 Datasheet](https://www.ti.com/product/PCM5102)
