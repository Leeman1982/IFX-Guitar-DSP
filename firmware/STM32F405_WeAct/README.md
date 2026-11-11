# ZombiSS effects on the WeAct STM32F405RGT6 core board

Bare-metal port of the ZombiSS multi-effect for the WeAct Studio "STM32F4 Core Board".
It uses CMSIS registers only: no HAL, no Arduino core, no CubeIDE. It is built and flashed with
`tools/pedalflash`.

The DSP and UI code are **the same files** as the RP2350 build
(`../ZombiSS_MultiEffect`). Nothing is copied. A small Arduino-API shim
(`bsp/arduino_shim`) lets the OLED, encoder and menu code compile unchanged.

| Folder | What it is |
|--------|------------|
| `blinky/` | Board smoke test. **Flash this first.** |
| `ifx_pedal/` | The effects unit: Boost Factory, gate, overdrive, 10-band EQ, chorus, delay, plate reverb, OLED UI. |
| `bsp/` | Startup (C), linker script, 168 MHz clock and PLLI2S, GPIO, syscalls, Arduino shim. |
| `cmsis/` | ST + ARM register headers, vendored so a ZIP download builds offline (see `cmsis/README.md`). |

## Flash it (USB-C, no ST-Link needed)

```
python3 tools/pedalflash/pedalflash.py setup                                  # once: compiler + dfu-util
python3 tools/pedalflash/pedalflash.py go firmware/STM32F405_WeAct/blinky
python3 tools/pedalflash/pedalflash.py go firmware/STM32F405_WeAct/ifx_pedal
```

The board is taken from each folder's `pedalflash.json`, so `--board` is not needed.

When the tool asks for DFU mode:
1. Keep USB-C plugged in.
2. **Hold B0, tap NR, release B0.**
3. Press Enter.

The board appears as *STM32 BOOTLOADER* (USB ID 0483:df11). After flashing, dfu-util's `:leave`
starts the new firmware.

On Windows, run `pedalflash drivers` once to bind WinUSB to "STM32 BOOTLOADER" with Zadig.

### Blinky
- 1 Hz on the PB2 LED: the crystal, the 168 MHz PLL and flashing all work.
- 5 Hz while KEY is held: GPIO input works.
- A fast flicker that never stops: the 8 MHz crystal didn't start.
- 4 quick flashes then a pause, repeating: a fault handler ran.

## Wiring (effects unit)

| F405 pin | Goes to | Notes |
|----------|---------|-------|
| PB12 | PCM1808 LRC, PCM5102 LCK | I2S2 WS (word select) |
| PB13 | PCM1808 BCK, PCM5102 BCK | 3.072 MHz bit clock |
| PB15 | PCM5102 DIN | audio out |
| PB14 | PCM1808 OUT | audio in (I2S2ext) |
| PC6 | PCM1808 SCK | MCLK 256·fs = 12.288 MHz |
| PC7 | PCM5102 XSMT | driven high (unmute) |
| PB6 / PB7 | OLED SCL / SDA | SH1106/SSD1306, optional |
| PC0 / PC1 | encoder A / B | optional |
| PC8–PC11 / PC12 | CD74HC4067 S0–S3 / SIG | optional; same channel map as the RP2350 build, CH8 = HOLD |
| 5V, 3V3, GND | modules | PCM1808 FMT/MD0/MD1 to GND (slave, I2S) |

- The PCM5102 module's SCK goes to GND, the same as in the RP2350 build. It derives its clock
  from BCK.
- This is the same I2S2 pinout as the earlier overdrive-only port on
  `claude/stm32-overdrive-effect-port-ssz0ee`.
- Avoid PA4–PA7 (the optional SPI-flash footprint), PA11/PA12 (USB), PA13/PA14 (SWD) and
  PB3/PB4 (JTAG).

**No UI hardware needed to try it.** The board boots into drive + EQ, and the on-board KEY
(PC13) cycles three presets:
1. Drive + EQ.
2. Clean, with chorus + delay + reverb.
3. Everything off.

The LED shows a 1 Hz heartbeat, and goes fast while the DSP is overloaded.

## What is different from the RP2350 build, and why

The F405 has 128 KB of DMA-capable SRAM plus 64 KB of CCM that only the CPU can use, and a
single 168 MHz Cortex-M4. The RP2350 has 520 KB and two 250 MHz cores. So:

| | RP2350 | F405 |
|---|---|---|
| Delay max | 1.5 s (0.75 s reverse) | **450 ms** (225 ms reverse) |
| Reverb tank | 48 kHz | **24 kHz** (`IFX_REVERB_HALF_RATE`: half the RAM and CPU, same decay; wet audio rolls off around 10 kHz) |
| Gate RMS window | 50 ms | 25 ms |
| Boost Factory | own core, one block ahead | same IRQ as the rest of the chain |
| Sample rate | 48 000 Hz | 47 991 Hz (PLLI2S), inaudible |

All of these are compile-time `defines` in `ifx_pedal/pedalflash.json`. The host test suite
passes with them (`tools/dsp_test`, see CLAUDE.md).

**Memory (linker output):**
- SRAM 126.8 / 128 KB: the effect chain plus the DMA buffers.
- CCM 53.7 / 64 KB: the Boost Factory's matrix-rebuild scratch, stack and heap.
- Flash 108 KB / 1 MB.

**CPU — not measured, estimated from instruction counts:**
- Without the Boost Factory, everything on is ~1400 instructions per sample against a budget of
  3500 cycles, probably 40–60 % [Guessing].
- The Boost Factory adds 940–2280 instructions. The diode models will be near the limit, and
  Fuzz Face / Big Muff will not fit.
- So the firmware protects itself. If the load stays above 92 % for 0.5 s (or a DMA block is
  missed), it first renders the boost at 1× (`~` after the model name). If that's still too
  much, it switches the boost off.
- The OLED chain view shows the CPU load as rest/boost percentages.

**If you want the whole chain with no compromises, use the STM32H723 board** (550 MHz M7,
564 KB RAM). It's in `pedalflash boards` as `weact-h723`, but has no port yet.

## Status

- Builds with zero warnings (`-Wall`) using the pinned xPack GCC 14.2.
- The vector table and memory map were checked in the ELF.
- **Not yet run on hardware.** Flash `blinky` first, then `ifx_pedal`. If the audio is silent
  or noisy, check the PCM1808 FMT/MD pins and the BCK/LRCK wiring before suspecting the code.
