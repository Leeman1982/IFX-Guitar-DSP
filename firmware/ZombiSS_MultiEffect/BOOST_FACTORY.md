# Boost Factory: circuit-level pedal models

The Boost Factory replaces the old single-curve "TS Boost" in slot FX1. It runs nine classic
boost, overdrive, distortion and fuzz pedals as component-level circuit simulations:

- every resistor, capacitor, pot, op-amp, diode and transistor from the original schematic
  is in the model;
- the circuit is solved every sample with the same equations SPICE uses;
- the result is checked against ngspice.

| # | Model | Original circuit | Voices (Voice param) | Drive knob | Tone knob |
|---|-------|------------------|----------------------|------------|-----------|
| 0 | TS808 | Ibanez TS-808 Tube Screamer | Si (stock 1N914 pair), Asym (1:2 diodes), Si x2, LED | Drive 500 kA | Tone 20 k |
| 1 | KLON | Klon Centaur | Ge (stock 1N34A), Si | Gain (dual-gang 100 kB) | Treble 10 kB |
| 2 | DIST+ | MXR Distortion+ / DOD 250 | Ge MXR (1N34A), Si DOD (1N914), LED | Distortion 1 MC | – (no tone control) |
| 3 | RAT | ProCo RAT | Si (1N914), LED Turbo, Ge | Distortion 100 kA | Filter 100 kA |
| 4 | DS-1 | Boss DS-1 | Si, LED, Si x2 | Dist 100 kB | Tone 25 kB |
| 5 | BIG MUFF | Electro-Harmonix Big Muff Pi | Stock, Mids (tone-stack caps swapped) | Sustain 100 kB | Tone 100 kB |
| 6 | FUZZFACE Ge | Dallas-Arbiter Fuzz Face, germanium PNP | Single-coil, Humbucker, Buffer | Fuzz 1 kC | Guitar volume 250 kA |
| 7 | FUZZFACE Si | Fuzz Face, silicon NPN (BC108) | Single-coil, Humbucker, Buffer | Fuzz 1 kC | Guitar volume 250 kA |
| 8 | RANGEMSTR | Dallas Rangemaster (OC44) | Single-coil, Humbucker, Buffer | Input level (−12…+12 dB) | Guitar volume 250 kA |

The fuzz models include the guitar itself: pickup resistance, inductance and capacitance plus
a 250 k audio-taper volume pot. A Fuzz Face's low input impedance interacts with the pickup, and
that interaction is part of its sound. Rolling **Tone** (guitar volume) back cleans the fuzz up
the way it does on a real rig. **Buffer** removes the pickup (as if a buffer pedal came first).

**Level** is a digital output trim in dB. Each model has a make-up gain so that every model
comes out at roughly the same loudness at its default settings. Selecting a model loads its
default Drive and Tone settings.

## Why not the 30 models in Boost Factory v4?

v4 was a single generic chain (high-pass → mid boost → waveshaper → filters) with numbers
chosen per model. That isn't circuit modelling, and it also had real bugs:

- `0xAUD10` isn't a valid hex literal, so the sketch doesn't compile.
- The 69-tap FIR placed *before* the clipper does nothing to stop aliasing, because the
  clipper creates the harmonics after it.
- `clipExponential` calls `fastExpNeg()` with the wrong sign.
- `clipFoldback` can loop forever on a NaN.
- Its 16-bit AudioTools I2S pinout doesn't match this hardware.

I kept only pedals with fully published, cross-checked schematics, because those are the only
ones I can model honestly. Dropped:
- amp-in-a-box designs whose circuits I couldn't verify (Friedman BE-OD, Suhr Riot, Revv G3,
  "Mesa preamp", "Sabbra Cadabra");
- circuits whose behaviour depends on hand-trimmed bias or self-oscillation (Fuzz Factory);
- circuits I didn't have time to transcribe and validate to the same standard (HM-2, MT-2,
  BD-2, OD-1, SD-1 as a separate model, Timmy, Morning Glory, Zendrive, Bluesbreaker, OCD,
  Crunch Box, Tone Bender, ODR-1).

The Voice parameter recovers several of those flavours with the real diode substitutions:
- the TS808 asymmetric voice is the SD-1 clipping arrangement;
- DIST+ Si is a DOD 250;
- RAT LED is a Turbo RAT.

## How it works

### 1. Netlists, not curves
Each pedal is a list of components with their schematic values (`ifx_bf_models.c`, with
designators in the comments). Values were transcribed from the Aion FX build documents for
Stratus (TS-808), Helios (RAT), Refractor (Klon), Aphelion (Dist+/DOD 250), Comet (DS-1),
Halo (Big Muff), Proteus (Fuzz Face) and Radian (Rangemaster). Those documents reproduce the
original circuits with the mods switchable. Stock values were used throughout, for example
51 pF in the TS-808 instead of Aion's 47 pF, and 33 k / 8k2 / 470 Ω in the Fuzz Face instead
of bias trimmers.

### 2. Nodal DK method
When a knob moves, the UI core does the following (`ifx_bf_circuit.c`):
- builds the circuit's modified nodal analysis (MNA) matrix in double precision;
- discretises capacitors and inductors with the trapezoidal rule (SPICE's default);
- eliminates every linear node.

What remains is a small state-space system, plus a few nonlinear *ports* for the diodes and
transistors. The audio core only evaluates the precomputed matrices.

### 3. Nonlinear solve per sample
- **Diode pairs and forward-active transistors (1 port):** an exact closed-form solution via
  the Wright omega function. It is used as a fallback behind a warm-started one- or two-step
  Newton fast path.
- **Ebers–Moll BJTs (2 or 4 ports):** Newton's method using the following:
  - SPICE's `pnjlim` junction limiting;
  - a linear predictor for the starting point;
  - a Schur-complement 4×4 solve for the two-transistor Fuzz Face;
  - up to 12 iterations. Most samples need 1–2; the cap matters only on hard switching edges.
- **Diode models:** 1N914 (Is 2.52 nA, N 1.752), 1N34A (Is 200 nA, N 1.3) and red LED
  (Is 1.2e-19 A, N 1.9).
- **BJT models:** 2N3904 and 2N5088 SPICE parameters, plus estimated germanium parameters (see
  Assumptions).

### 4. Operating point
On a model change the DC bias is solved first, with capacitors open and source stepping, and
every capacitor starts charged to its real bias voltage. So there is no thump and no
multi-second settling of 22–47 µF caps. A 20 ms fade-in covers the rest.

### 5. Oversampling only where the nonlinearity is
Stages with clipping run at 2× or 4×, using polyphase IIR half-band filters with
80 dB / 70 dB stop-band rejection. Linear stages run at 48 kHz:
- TS-808, RAT and Dist+: 4×.
- Klon, DS-1, Big Muff, Fuzz Faces and Rangemaster: 2×.

Each pedal is cut into stages only where the real circuit is buffered:
- at an op-amp output;
- or at a transistor collector, represented by its Thevenin source resistance in the next
  stage.

The Klon's IC1B gain stage, for example, is solved separately at 48 kHz, because nothing
downstream can load it.

### 6. Op-amp realism
- Single-pole gain-bandwidth limits: JRC4558 3 MHz, LM308 1 MHz, µA741 1 MHz, TL072 3 MHz.
- Output-swing soft limits (rails).
- Slew-rate limits: LM308 0.3 V/µs, which is part of the RAT sound; 741 0.5 V/µs.

## Validation against ngspice

Each stage was simulated in ngspice from the same netlist (`tools/boost_factory/validate.sh
spice …`), with identical device models, and compared with the C engine running at the
stage's own sample rate. Errors are RMS differences relative to the signal (lower is better):

| Stage | Error |
|-------|-------|
| TS808 clip / tone | −68 / −71 dB |
| Klon gain / clipper + summing / treble | −72 / −72 (Si voice −76) / −70 dB |
| Dist+ gain / clipper | −70 / −58 dB |
| RAT gain / clipper + filter | −66 / −66 dB |
| DS-1 booster / gain / clipper + tone | −60 / −82 / −67 dB |
| Big Muff input / clip 1 / clip 2 + tone / output | −74…−77 / −62 / −54 / −69 dB |
| Fuzz Face Ge (Fuzz 7, 0.1 V) | −63 dB |
| Fuzz Face Si (Fuzz 7, 0.1 V) | −59 dB |
| Rangemaster | −48 dB, first 10 harmonics within 0.5 dB |

At full fuzz the transistors switch so abruptly that sample-level timing dominates the
waveform difference (−28 dB). Even there, the first 10 harmonics match ngspice within
0.1–0.8 dB.

These comparisons show that the C engine solves the stated circuit correctly
**[Certain, for the tested settings]**. They don't show that the netlist and device
parameters equal any particular physical pedal. See Assumptions.

## Aliasing (non-harmonic energy, 20 Hz–16 kHz)

Measured relative to the harmonic energy, at the output, for a sine input
(`validate.sh alias`):

| Model | Drive 5, 70 mV: 330 Hz | 660 Hz | 1.3 kHz | Drive 8, 300 mV: 330 Hz | 1.3 kHz |
|-------|------------|--------|---------|-----------|---------|
| TS808 | −92 | −83 | −74 | −88 | −71 |
| KLON | −92 | −86 | −78 | −90 | −60 |
| DIST+ | −81 | −75 | −70 | −74 | −61 |
| RAT | −76 | −74 | −46 | −46 | −26 |
| DS-1 | −76 | −64 | −41 | −34 | −28 |
| BIG MUFF | −78 | −68 | −63 | −66 | −46 |
| FUZZFACE Ge | −52 | −55 | −50 | −38 | −28 |
| FUZZFACE Si | −42 | −36 | −30 | −30 | −23 |
| RANGEMSTR | −79 | −79 | −79 | −72 | −57 |

The hard-clipping models (RAT, DS-1 and especially the silicon Fuzz Face) alias audibly on
high notes at high gain. The silicon Fuzz Face's analog edges really are that fast: ngspice
sampled at the same rate shows the same non-harmonic level. More oversampling is the only fix;
see `BF_FUZZ_OSFACTOR` below.

## CPU and the dual-core pipeline

The Boost Factory runs on **core 1**, triggered by an RP2350 SIO doorbell interrupt once per
64-sample block and pipelined one block ahead of core 0. That adds 1.33 ms of latency. Core 0
runs the rest of the chain. Core 1 also runs the UI and the matrix rebuilds; the doorbell
interrupt preempts both.

Arm doesn't publish Cortex-M33 FPU instruction timings, so cost is given two ways
(Cortex-M33 in QEMU, `-O2`, per 48 kHz sample; budget at 250 MHz = 5208 cycles):

| Model | Instructions | Pessimistic cycles (VFMA 3, VLDR 2, VDIV 14) | % of core 1 at 250 MHz |
|-------|-------------|------------------|-------------------|
| TS808 | 976 | 2800 | 19–54 % |
| KLON | 1047 | 3160 | 20–61 % |
| DIST+ | 937 | 2280 | 18–44 % |
| RAT | 1093 | 3120 | 21–60 % |
| DS-1 | 1340 | 3560 | 26–68 % |
| BIG MUFF | 2278 | 5400 | 44–104 % |
| FUZZFACE Ge | 2077 | 4730 | 40–91 % |
| FUZZFACE Si | 1870 | 4770 | 36–92 % |
| RANGEMSTR | 966 | 2490 | 19–48 % |
| Rest of chain on core 0 (gate+OD+EQ+chorus+delay all on) | ~850 | ~2180 | 16–42 % of core 0 |

The real figure lies between those bounds **[Likely]**. Three safeguards cover the uncertain end:

- **Load meters.** The chain screen shows `CPU c0/c1` in % of the block period, and the
  serial monitor prints both cores' load, their peaks and missed deadlines every 2 s.
- **Automatic fallback.** If core 1 stays above 90 % for 0.5 s, or misses a deadline, the
  current model drops to 1× (no oversampling). The OLED then shows the model name with a
  trailing `~`. Selecting another model re-arms full quality.
- **Overrun guard.** If core 1 is ever late, core 0 waits at most a quarter of a block and
  then plays that block dry rather than glitching the DMA.

## Assumptions and limitations (read before judging the sound)

- **Input calibration.** `BF_IN_FS_VOLTS = 1.5` maps digital full scale to ±1.5 V at the
  pedal input. That assumes the PCM1808's ~3 Vpp full scale at 5 V **[Likely]**, fed by a
  unity-gain buffer. If your front end has gain, the pedals see a hotter guitar than you
  have, so lower this value (or raise it if the front end attenuates). The fuzz and
  boost models are very sensitive to input level, just like the real ones.
- **Germanium transistors.** NKT275 and OC44 parameters (Is 1 µA, β 75/115/80, βR 5) are
  plausible estimates **[Guessing]**. Real Ge parts vary by ±50 % and leak; leakage isn't
  modelled. Junction capacitance is modelled only on the Rangemaster, at an estimated 20 pF
  **[Guessing]**.
- **Silicon Fuzz Face.** BC108 with β 120/200 **[Likely]** typical values.
- **1N34A / 1N270 germanium diodes and the red LED.** Single-exponential fits to typical
  forward voltage **[Likely]**, with no series resistance.
- **LM308 gain-bandwidth in the RAT.** 1 MHz with the 30 pF compensation cap
  **[Likely]**.
- **Not modelled:**
  - transistor Early effect and series resistances;
  - op-amp input bias currents;
  - the TS-808's and DS-1's input and output buffer transistors (treated as ideal buffers);
  - the Klon's charge pump. Its op-amps are treated as unclipped at ±9 V, which holds below
    extreme settings **[Likely]**.
- **Big Muff.** Q2–Q4 use a forward-active transistor model. Big Muff Q2/Q3 can't saturate
  (their diodes clamp the collector) and Q4 has 2.7 k of degeneration. Big Muff stage 2,
  forward-active vs full Ebers–Moll in ngspice: −62 dB. The stages are cut at transistor
  collectors with a Thevenin source, which ignores how 470 k of feedback changes under load:
  a sub-1 % effect **[Likely]**.
- **Fuzz Face output.** The 500 k volume load is moved after the 10 nF output cap, a 0.14 dB
  effect.

## Tuning knobs (`config.h`)

| Define | Default | Meaning |
|--------|---------|---------|
| `BF_FUZZ_OSFACTOR` | 2 | Fuzz Face oversampling, 2 or 4. Use 4 only if the core-1 meter shows < 45 % at 2×. |
| `BF_IN_FS_VOLTS` / `BF_OUT_FS_VOLTS` (`ifx_boost_factory.h`) | 1.5 | Analog scaling (see above). |

## Re-running the checks

```
cd tools/boost_factory
./validate.sh build
./validate.sh smoke                                   # all models: silence/DC, level, speed
./validate.sh spice 3 1 0.5 0.5 0 192000 2.5 440      # RAT clipper stage vs ngspice
./validate.sh alias 7 660 0.5 0.07                    # Fuzz Face Si aliasing
```
