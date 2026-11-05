# pedalflash — pick your STM32 board, build, flash over USB

One Python script with no dependencies (standard library only, Python 3.9+). Windows, macOS
and Linux.

```
python3 pedalflash.py                 # wizard: choose a board → project → build → flash
python3 pedalflash.py setup           # one time: download the compiler + flash tools
python3 pedalflash.py go ~/GuitarPedalSTM32 --board guitarml
python3 pedalflash.py go firmware/InfiniFX-Reloaded-TikiDrive_Firmware --board infinifx --method dfu
```

| Command | What it does |
|---------|--------------|
| `boards` | List the board database (`boards.json`). Add your own board there. |
| `setup` | Download pinned xPack `arm-none-eabi-gcc` 14.2.1, xPack OpenOCD 0.12.0-7 and dfu-util 0.11 (plus Zadig on Windows) into `~/.pedalflash`. |
| `doctor` | Show the installed tools, the ST-LINK / DFU devices plugged in, and permission problems. |
| `build PROJECT` | Build an **STM32CubeIDE project without CubeIDE** (see below), or a `make` project. |
| `flash FILE --board B` | Flash an `.elf` / `.hex` / `.bin` over ST-LINK (OpenOCD) or USB DFU (dfu-util). |
| `go PROJECT --board B` | `build` + `flash`. |
| `drivers` | Linux: udev rules so you don't need sudo. Windows: fetch Zadig and walk you through binding WinUSB. macOS: nothing needed. |

## How "build without the IDE" works

The script reads the project's `.cproject` (the chosen configuration, `Debug` by default):
- the MCU, FPU and float ABI;
- defines and include paths;
- the optimisation level and C / C++ standard;
- the linker script and source folders.

It also reads `.project` for *linked folders*. Those often hold absolute paths from the
author's PC (`C:/Users/…/GuitarPedalSTM32/software/util`), so each one is found again by
matching its trailing path components next to the project. Missing git submodules (DaisySP
for the GuitarML pedal) are fetched with `git submodule update --init --recursive`.

Builds are incremental and parallel. Output goes to `<project>/build/pedalflash/<Config>/`:
`.elf`, `.bin`, `.hex` and `.map`.

Checked here:
- **InfiniFX TikiDrive:** 51 files. 78 600 B of text against 78 596 B from CubeIDE's own
  committed build, with the same symbols and the same linker flags. The 4-byte difference is
  GCC 14.2 vs CubeIDE's 12.3.
- **GuitarML pedal:** 73 files, including DaisySP through the two absolute-path linked folders.
  55.6 KB of flash, 489 KB of RAM.

**Not checked:** flashing on real hardware. No probe or board was attached, so only the error
paths were exercised.

## Why it does not "scrape the web for drivers"

Searching for and running whatever turns up is how you get malware. Every download here is one
fixed official release URL with its **SHA-256 written into the script**. If the file changes,
it is refused. What each OS actually needs:

- **Linux:** no drivers, only USB permissions (`pedalflash drivers`).
- **macOS:** nothing.
- **Windows:** the WinUSB driver bound to the ST-LINK or "STM32 BOOTLOADER" device. Zadig does
  that in two clicks, and `pedalflash drivers` downloads the pinned Zadig and tells you which
  entry to pick. ST's own ST-LINK driver also works if CubeIDE / CubeProgrammer is already
  installed.

## Board facts that decide *how* you can flash

- **GuitarML STM32 pedal:** no USB at all. PA11 drives an LED. You need an **ST-LINK V3**,
  which is itself a USB device, on the 10-pin header, with the pedal on 9 V. Asking for
  `--method dfu` is refused with that explanation.
- **InfiniFX Reloaded:** USB OTG HS on PA11/PA12, so ROM DFU works if BOOT0 can be held high
  at reset on your revision. Not verified on hardware.
- **BluePill F103:** the ROM bootloader is UART-only, so ST-LINK only.
- **BlackPill F401 / F411, Daisy Seed:** DFU with the BOOT button.

RP2040/RP2350 boards (the ZombiSS) are not STM32. Use the Arduino IDE / `arduino-cli`, or drag a
`.uf2` onto the BOOTSEL drive.

## Adding a board

Copy an entry in `boards.json`:
- `mcu` is the CubeMX part name, used only if the project has no `.cproject`;
- `openocd_target` is a file from OpenOCD's `scripts/target/`;
- `methods` lists only what the PCB supports, default first;
- `dfu_entry` explains how to get into the bootloader.
