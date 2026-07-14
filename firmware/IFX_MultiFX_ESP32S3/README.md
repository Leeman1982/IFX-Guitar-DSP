# InfiniFX Multi-FX — ESP32-S3 (Arduino IDE)

Guitar multi-effects unit built from the InfiniFX DSP modules, ported from the
STM32H7 firmware to an **ESP32-S3-WROOM** and buildable in the **Arduino IDE**.

## Signal chain

```
Guitar -> (external buffer/preamp) -> PCM1808 ADC
   -> Noise Gate -> TS Boost -> Overdrive -> Chorus -> Delay
   -> PCM5102 DAC -> Amp
```

Every effect has its own momentary footswitch and can be toggled on/off
independently. The **TS Boost** is a Tube Screamer-style soft-clipping
boost placed before the overdrive — use it to push the overdrive harder or
as a standalone light drive.

- The guitar signal must be **buffered/preamplified externally** and biased to
  the PCM1808 input range (the PCM1808 input is line level, ~2 Vrms full scale).
- Guitar goes into the PCM1808 **left** channel. The same processed signal is
  sent to both DAC channels.
- Audio runs at 44.1 kHz, 24-bit, block size 64 samples (~1.5 ms), processed in
  a dedicated task on core 1. UI runs on core 0.

## Arduino IDE setup

1. Install the **esp32 board package by Espressif, version 3.0 or newer**
   (Boards Manager). The audio driver uses the ESP-IDF 5 I2S API, so 2.x cores
   will not compile.
2. Install the **U8g2** library by olikraus (Library Manager) for the SH1106 OLED.
3. Open `IFX_MultiFX_ESP32S3.ino`.
4. Board settings:
   - Board: **ESP32S3 Dev Module**
   - PSRAM: enable if your module has it (e.g. **OPI PSRAM** for N16R8).
     With PSRAM the maximum delay time is 1.5 s, without it 400 ms.
   - USB CDC On Boot: **Enabled** (for the serial monitor)
5. Upload.

## Wiring

All pins can be changed in `config.h`.

### I2S audio (PCM1808 + PCM5102, shared bit/word clocks)

| ESP32-S3 | PCM1808 | PCM5102 |
|----------|---------|---------|
| GPIO 14 (MCLK) | SCKI | — (tie PCM5102 SCK to GND, it uses its internal PLL) |
| GPIO 15 (BCK)  | BCK  | BCK |
| GPIO 16 (WS)   | LRC  | LCK |
| GPIO 17 (DOUT) | —    | DIN |
| GPIO 18 (DIN)  | DOUT | — |

PCM1808 configuration pins: `FMT` low (I2S format), `MD1`/`MD0` low/low
(slave mode, 256fs system clock).

PCM5102 configuration pins: `FMT` low (I2S), `XSMT` high (unmute),
`FLT` low, `DEMP` low, `SCK` to GND.

Use short wires for BCK/LRCK/MCLK, common ground, and clean 3.3 V analog
supplies for both converters.

### OLED (SH1106 128x64, I2C)

| ESP32-S3 | OLED |
|----------|------|
| GPIO 8 | SDA |
| GPIO 9 | SCL |

### Potentiometers (10k linear, 3V3 — wiper — GND)

| ESP32-S3 | Pot |
|----------|-----|
| GPIO 4 | Pot 1 |
| GPIO 5 | Pot 2 |
| GPIO 6 | Pot 3 |
| GPIO 7 | Pot 4 |

### Footswitches (momentary, switch pin to GND)

| ESP32-S3 | Effect |
|----------|--------|
| GPIO 10 | Noise Gate |
| GPIO 11 | TS Boost |
| GPIO 12 | Overdrive |
| GPIO 13 | Chorus |
| GPIO 21 | Delay |

## Using it

- **Short press** a footswitch: toggle that effect on/off.
- **Long press** (≥ 0.5 s) a footswitch: select that effect for editing.
  The OLED shows the selected effect and its four parameters; the four pots
  now control them.
- After selecting a different effect, each pot is **locked** until you move it
  (shown as a dot next to the parameter bar). This prevents parameter jumps
  when switching between effects.

### Parameters per effect

| Effect | Pot 1 | Pot 2 | Pot 3 | Pot 4 |
|--------|-------|-------|-------|-------|
| Noise Gate | Threshold | Attack | Release | Hold |
| TS Boost | Drive | Tone | Level | Tight (input HPF) |
| Overdrive | Gain | Voice (input HPF) | Tone (output LPF) | Level |
| Chorus | Rate | Depth | Mix | Level |
| Delay | Time | Feedback | Mix | Level |

## Files

| File | Description |
|------|-------------|
| `IFX_MultiFX_ESP32S3.ino` | Main sketch: audio task, UI, parameter mapping |
| `config.h` | Pin map, sample rate, UI tuning |
| `AudioIO.*` | Full-duplex I2S driver (PCM1808 in / PCM5102 out) |
| `IFX_TubeScreamer.*` | New Tube Screamer-style boost |
| `IFX_Overdrive.*` | Ported unchanged from the STM32 firmware |
| `IFX_NoiseGate.*`, `IFX_MovingRMS.*` | Ported unchanged |
| `IFX_Chorus.*` | Reworked with interpolated modulated taps |
| `IFX_Delay.*` | Ported; delay line allocated in PSRAM when available |
