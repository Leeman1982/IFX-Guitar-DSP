# TikiDrive Overdrive — Arduino port for the WeAct STM32F405RGT6 Core Board

A standalone Arduino IDE port of the InfiniFX / TikiDrive **overdrive** effect
(`IFX_Overdrive` by Philip Salmony @ phils-lab.net — the DSP code in this folder is
copied unchanged from `firmware/InfiniFX-Reloaded-TikiDrive_Firmware/Core`).

Instead of the original H7 + CS4270 codec hardware, this port runs on cheap modules:

| Part | Role |
|---|---|
| WeAct STM32F405RGT6 Core Board | MCU, 168 MHz, FPU |
| PCM1808 module | audio ADC (I2S in) |
| UDA1334A module | audio DAC (I2S out) |
| TL071 buffer (9 V, 4.5 V vBias) | guitar input conditioning before the PCM1808 |
| 4 × 10k linear pots | Drive / Bias / Tone / Level |
| 1 × momentary footswitch | effect on/off (toggle) |

Audio runs at **48 kHz, 24-bit**, mono in (PCM1808 left channel), same signal on
both DAC outputs. The STM32 is I2S master and generates the 256·fs master clock
the PCM1808 needs; the UDA1334A recovers its clocks from BCLK via its own PLL,
so all three chips share one clock domain.

## Wiring

### Audio (I2S2 full duplex — these pins are fixed)

| STM32 pin | Signal | PCM1808 | UDA1334A |
|---|---|---|---|
| PB12 | LRCK / word select | LRC | WSEL |
| PB13 | Bit clock | BCK | BCLK |
| PB15 | Data out (to DAC) | — | DIN |
| PB14 | Data in (from ADC) | OUT | — |
| PC6 | Master clock 256·fs | SCK | — (not needed) |
| 5V | analog supply | +5V | VIN |
| 3V3 | digital supply | 3.3 | — |
| GND | ground | GND | GND |

PCM1808 module mode pins: leave **FMT (FMY), MD1, MD0** unconnected or tied to
GND — the module's pull-downs select *slave mode, I2S format*, which is exactly
what this sketch expects.

Guitar signal path: guitar → TL071 buffer (9 V supply, input biased to 4.5 V,
100 nF in / 10 µF out coupling caps) → PCM1808 **LIN**. Leave RIN unconnected
(or ground it through a cap).

UDA1334A **LOUT/ROUT** both carry the processed signal — take either to your
amp. The module's SF0/SF1 defaults already select I2S format.

### Controls

| STM32 pin | Control | Range |
|---|---|---|
| PA0 | **Drive** pot | pre-gain 10…110 |
| PA1 | **Bias** pot | clipping asymmetry / character (Q = −0.05…−1.05) |
| PA2 | **Tone** pot | output low-pass 500 Hz…16.5 kHz |
| PA3 | **Level** pot | output volume 0…1 |
| PC13 | Footswitch | momentary to GND, toggles effect (internal pull-up). This is also the onboard WeAct **KEY** button, handy for testing. |
| PA8 | Status LED | high when effect engaged — LED + ~1 kΩ to GND |

Pots: outer lugs to **3V3** and **GND**, wiper to the pin. All pins are
`#define`s at the top of the sketch if you want to move them — but avoid
PA4…PA7 (used by the onboard SPI flash on the WeAct board) and the I2S pins
above. The input high-pass is fixed at 150 Hz (the original firmware's fifth
pot); change `OD_HPF_CUTOFF_HZ` if you want it tighter or looser.

When the effect is off the dry signal is passed through digitally (~2.7 ms
converter+buffer latency), with a 10 ms crossfade so switching doesn't click.

## Building in the Arduino IDE

1. Install the STM32 core: **File → Preferences → Additional boards manager
   URLs**, add
   `https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json`
   then install **STM32 MCU based boards** from the Boards Manager.
2. Open `TikiDrive_Overdrive_F405.ino`.
3. Select under **Tools**:
   - Board: **Generic STM32F4 series**
   - Board part number: **Generic F405RGTx**
   - USB support: *None* (or CDC if you want a serial port)
   - Upload method: **STM32CubeProgrammer (DFU)** — or SWD if you have an ST-Link
4. For DFU upload: hold **BOOT0**, tap **NRST**, release BOOT0, then Upload
   (STM32CubeProgrammer must be installed for the IDE to flash).

No changes to the STM32 core are required — the stock `Generic F405RGTx`
variant is used, and the sketch overrides the default clock config to run the
board's 8 MHz crystal up to 168 MHz (needed for accurate I2S audio clocks;
actual sample rate is 47.991 kHz).

## How it maps to the original firmware

- `IFX_Overdrive.c/.h` — byte-for-byte the original DSP: 69-tap anti-aliasing
  FIR, variable input HPF, asymmetric exponential clipper, variable 2nd-order
  output LPF.
- The original ran on SAI + CS4270 at 32.552 kHz with 5 pots; this port uses
  full-duplex I2S2 (I2S2 master TX + I2S2_ext RX) with circular DMA,
  double-buffered 64-frame blocks, and folds the controls onto 4 pots
  (HPF fixed at the recommended 150 Hz).
- Pot smoothing/thresholding mirrors the original control loop
  (EMA α = 0.5 with a change deadband) to avoid zipper noise.
