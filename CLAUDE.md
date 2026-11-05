# IFX Guitar DSP Algorithms — Professional Audio Processing Library

**Project:** ZOMBI SS Multi-Effect DSP Unit  
**Platform:** RP2350 (dual-core ARM Cortex-M33) @ 250 MHz (Arduino Tools → CPU Speed)  
**Audio:** 48 kHz, 24-bit I2S I/O  
**Original DSP:** Philip Salmony (@phils-lab.net) — ported to RP2350 Arduino  

This document describes every effect algorithm, filter design, and coefficient table for reuse in other projects.

---

## Audio Pipeline Overview

```
Guitar Input (PCM1808 ADC, 24-bit I2S)
    ↓
[Boost Factory]  ← core 1 (doorbell IRQ, one block ahead)
    ↓
[Noise Gate] → [Overdrive] → [10-Band EQ] → [Chorus] → [Delay ⇄ Reverb]   ← core 0 (DMA IRQ)
    ↓
Master Volume Smoother (prevents clicks)
    ↓
Output (PCM5102 DAC, 24-bit I2S, mono → stereo L/R)
```

**Sample Rate:** 48 kHz  
**Buffer Size:** 64 frames per half-buffer = ~1.33 ms (+1 block for the core-1 Boost Factory pipeline)  
**Signal Format:** Float [-1.0, +1.0] between effects
**Switching:** every effect crossfades over 6 ms when toggled (no clicks); delay and reverb
have optional trails and a momentary HOLD (freeze) switch — see §6, §6b, §7b.

---

## 1. Boost Factory (circuit-modelled boost / overdrive / distortion / fuzz)

Replaces the former single-curve TS Boost. **Full documentation:
[`firmware/ZombiSS_MultiEffect/BOOST_FACTORY.md`](firmware/ZombiSS_MultiEffect/BOOST_FACTORY.md).**

Nine pedals are simulated at component level from their schematics:
- TS-808, Klon Centaur, MXR Distortion+ / DOD 250, ProCo RAT / Turbo RAT, Boss DS-1;
- Big Muff Pi;
- Fuzz Face in germanium and silicon versions, and the Rangemaster (both with a pickup and
  guitar-volume model).

How the engine works:
- Each pedal is a netlist in `ifx_bf_models.c`.
- `ifx_bf_circuit.c` turns it into a discrete state-space model with nonlinear device ports
  (nodal DK method, trapezoidal discretisation).
- Diode pairs are solved in closed form (Wright omega). Ebers–Moll transistors use Newton
  with SPICE `pnjlim` limiting.
- Clipping stages are oversampled 2×/4× with polyphase IIR half-bands.
- Every stage is validated against ngspice (`tools/boost_factory/`).

| Param | Range | Notes |
|-------|-------|-------|
| Model | 0–8 (enum) | name shown on OLED; `~` suffix = CPU fallback (1× rendering) |
| Voice | per model (enum) | diode swaps (Si / Asym / LED / Ge), Big Muff mids, pickup type |
| Drive | 0–10 | the pedal's gain/distortion/sustain/fuzz pot (real taper) |
| Tone | 0–10 | tone/filter/treble pot; guitar volume on the fuzz models |
| Level | −30…+12 dB | digital output trim (models are loudness-matched) |

```c
IFX_BF_Init(&bf, 48000.0f);            /* builds model 0 */
IFX_BF_SetModel(&bf, 3);               /* UI core */
IFX_BF_SetDrive(&bf, 0.6f);            /* 0..1 */
IFX_BF_Service(&bf);                   /* UI core: rebuild matrices, publish */
IFX_BF_Process(&bf, buf, 64);          /* audio core: in-place block */
```

---

## 2. Noise Gate

### Algorithm
A sophisticated noise gate using moving RMS detection and smoothed gain ramping.

**Key Features:**
- RMS-based signal detection (50 ms window → ~2400 samples @ 48 kHz)
- Separate attack (opening) and release (closing) times with hold duration
- Exponential envelope smoother prevents clicking

### RMS Detection

The gate measures the **moving RMS** of the input signal over a 50 ms window:

```
RMS² = (1/N) Σ x[n]²    where N = 2400 samples
```

If RMS < threshold, the gate closes (gain → 0). If RMS ≥ threshold, the gate opens (gain → 1).

### Gain Smoothing with Hold

The smoothed gain uses exponential averaging, but respects a **hold time** to prevent chattering:

```
if (signal_below_threshold):
    if (attack_counter > hold_time):
        smoothedGain = attackCoeff × smoothedGain + (1 - attackCoeff) × 0
    else:
        attack_counter += dt
else:
    if (release_counter > hold_time):
        smoothedGain = releaseCoeff × smoothedGain + (1 - releaseCoeff) × 1
    else:
        release_counter += dt

output = input × smoothedGain
```

### Time Constant Coefficient Calculation

For an exponential envelope with desired time constant τ (in milliseconds):

```
coefficient = exp(-2197.22457734 / (fs × τ_ms))

where 2197.22457734 ≈ ln(10000) = natural logarithm of 10000
```

This maps a time constant such that after time τ, the envelope decays to ~0.01% of peak (4.6 time constants of exponential decay).

### Parameters

| Name      | Range        | Unit | Description |
|-----------|-------------|------|-------------|
| Threshold | 0.01–0.5   | —    | RMS level to open gate |
| Attack    | 0.5–50     | ms   | Time for gate to open (RMS above threshold) |
| Release   | 0.5–50     | ms   | Time for gate to close (RMS below threshold) |
| Hold      | 1–200      | ms   | Minimum time before attack/release starts |

### Default Initialization (48 kHz)

```c
threshold = 0.1f
attack = 2.0f ms
release = 2.0f ms
hold = 20.0f ms
```

### C-Style Integration

```c
typedef struct {
    float thresholdSq;      // threshold²
    float holdTimeS;        // hold time in seconds
    float attackCoeff;      // exponential coeff for opening
    float releaseCoeff;     // exponential coeff for closing
    float attackCounter;    // time accumulator (s)
    float releaseCounter;   // time accumulator (s)
    float smoothedGain;     // 0.0 (closed) to 1.0 (open)
    float sampleTimeS;      // = 1/fs
    IFX_MovingRMS mrms;     // 50 ms RMS detector
} IFX_NoiseGate;

void IFX_NoiseGate_Init(IFX_NoiseGate *ng, float threshold, float attackTimeMs,
                        float releaseTimeMs, float holdTimeMs, float sampleRateHz);
float IFX_NoiseGate_Update(IFX_NoiseGate *ng, float inp);
void IFX_NoiseGate_SetThreshold(IFX_NoiseGate *ng, float threshold);
void IFX_NoiseGate_SetAttackRelease(IFX_NoiseGate *ng, float attackTimeMs,
                                    float releaseTimeMs, float sampleRateHz);
void IFX_NoiseGate_SetHoldTime(IFX_NoiseGate *ng, float holdTimeMs);
```

---

## 3. Overdrive/Distortion

### Algorithm
A multi-stage overdrive with cascaded filtering and sophisticated clipping:

**Signal Flow:**
```
Input → [69-Tap FIR LPF @ fs/4] → [1st-Order HPF] → [Soft Clipping] → 
        [3rd-Order Butterworth LPF] → Output
```

### Stage 1: Input Low-Pass Filter (69-tap FIR)

**Specifications:**
- **Type:** Linear-phase FIR (symmetric)
- **Order:** 68 (69 taps = 68th order)
- **Cutoff:** fs/4 (12 kHz @ 48 kHz)
- **Design Method:** Windowed sinc (Hamming window)

**Coefficient Array (69 elements):**

```c
float IFX_OD_LPF_INP_COEF[69] = {
    -0.00020692388031130378f,
    -0.0005449777163912186f,
    -0.0010648637855421347f,
    -0.0016364252077762365f,
    -0.002012399024754339f,
    -0.001863732890917493f,
    -0.000893772118525287f,
     0.0010039250791041001f,
     0.003603402015637201f,
     0.006295503824772816f,
     0.008189064906492259f,
     0.00837592349490335f,
     0.006298513670065863f,
     0.00209212864464293f,
    -0.003255494658311472f,
    -0.008051092706911124f,
    -0.010376200403688028f,
    -0.008792118107324508f,
    -0.003037554751423106f,
     0.005568054109378603f,
     0.014251053145796779f,
     0.019515960562734174f,
     0.01833642159152087f,
     0.009447624551409354f,
    -0.005725703771431606f,
    -0.022919941654740428f,
    -0.03585932375996474f,
    -0.03793377289247158f,
    -0.024280160492278255f,
     0.006380286560875572f,
     0.050788580809754395f,
     0.10148403327365461f,
     0.1484439587786635f,
     0.18160934822751035f,    // CENTER (tap 34 of 69)
     0.19357173063571662f,    // PEAK
     0.18160934822751035f,
     0.1484439587786635f,
     0.10148403327365461f,
     0.050788580809754395f,
     0.006380286560875572f,
    -0.024280160492278255f,
    -0.03793377289247158f,
    -0.03585932375996474f,
    -0.022919941654740428f,
    -0.005725703771431606f,
     0.009447624551409354f,
     0.01833642159152087f,
     0.019515960562734174f,
     0.014251053145796779f,
     0.005568054109378603f,
    -0.003037554751423106f,
    -0.008792118107324508f,
    -0.010376200403688028f,
    -0.008051092706911124f,
    -0.003255494658311472f,
     0.00209212864464293f,
     0.006298513670065863f,
     0.00837592349490335f,
     0.008189064906492259f,
     0.006295503824772816f,
     0.003603402015637201f,
     0.0010039250791041001f,
    -0.000893772118525287f,
    -0.001863732890917493f,
    -0.002012399024754339f,
    -0.0016364252077762365f,
    -0.0010648637855421347f,
    -0.0005449777163912186f,
    -0.00020692388031130378f
};
```

**FIR Implementation:**
```c
// Circular buffer convolution (69 tap positions)
float result = 0.0f;
for (int k = 0; k < 69; k++) {
    int idx = (lpfInpBufIndex + k) % 69;
    result += lpfInpBuf[idx] * IFX_OD_LPF_INP_COEF[k];
}
lpfInpBuf[lpfInpBufIndex] = input;
lpfInpBufIndex = (lpfInpBufIndex + 1) % 69;
lpfInpOut = result;
```

### Stage 2: Input High-Pass Filter (1st-Order Bilinear)

User-configurable cutoff (default 150 Hz) using bilinear transformation:

```
k = 2π × fc × (1/fs)

HPF Coefficients:
  b0 =  2 / (2 + k)
  b1 = -2 / (2 + k)
  a1 = (2 - k) / (2 + k)

y[n] = b0·x[n] + b1·x[n-1] - a1·y[n-1]
```

### Stage 3: Soft Clipping

Simple threshold-based clipping:
```
threshold = 1/3 ≈ 0.333
clipped_output = tanh(preGain × hpf_out / threshold)
```

The `tanh()` function smoothly saturates around ±1, modeling vacuum tube behavior.

### Stage 4: Output Low-Pass Filter (3rd-Order Butterworth)

User-configurable cutoff (default 5 kHz) and damping factor.

**Butterworth 3rd-Order State-Space:**
```
The 3rd-order filter is implemented as cascaded biquad sections.
ωc = 2π × fc, ζ = damping (typical 1.0 = critical damping)

Difference equations use state variables:
  x[3] = input history
  y[3] = output history
  a[3], b[3] = computed coefficients based on ωc·T and ζ
```

### Parameters

| Name | Range | Unit | Description |
|------|-------|------|-------------|
| Gain | 1–200 | — | Pre-clipping gain |
| Boost | 0–100 | — | Additional post-HPF gain |
| HPF Cutoff | 50–2000 | Hz | High-pass before clipping |
| LPF Cutoff | 500–20000 | Hz | Low-pass after clipping |
| LPF Damping | 0.1–2.0 | — | Butterworth Q-factor (1.0=critical) |
| Q Clip | -0.5 to -0.01 | — | Soft-clip threshold scalar |

### Default Initialization (48 kHz)

```c
hpf_cutoff = 150 Hz
pre_gain = 40
boost_gain = 0
lpf_cutoff = 5000 Hz
lpf_damping = 1.0f
Q_clip = -0.2f
threshold = 1/3 ≈ 0.333
```

### C-Style Integration

```c
typedef struct {
    float T;  // = 1/fs
    // Input FIR LPF
    float lpfInpBuf[69];
    uint8_t lpfInpBufIndex;
    float lpfInpOut;
    // Input HPF (1st order)
    float hpfInpBufIn[2], hpfInpBufOut[2];
    float hpfInpWcT;  // ωc·T = 2π·fc/fs
    float hpfInpOut;
    // Clipping
    float preGain, boostGain, threshold;
    // Output LPF (3rd order)
    float lpfOutBufIn[3], lpfOutBufOut[3];
    float lpfOutWcT, lpfOutDamp;
    float lpfOutOut;
    float out, Q;
} IFX_Overdrive;

void IFX_Overdrive_Init(IFX_Overdrive *od, float fs_Hz,
                        float hpf_Hz, float pre_gain,
                        float lpf_Hz, float lpf_damp);
float IFX_Overdrive_Update(IFX_Overdrive *od, float inp);
void IFX_Overdrive_SetGain(IFX_Overdrive *od, float gain);
void IFX_Overdrive_SetBoost(IFX_Overdrive *od, float boost);
void IFX_Overdrive_SetHPF(IFX_Overdrive *od, float hpf_Hz);
void IFX_Overdrive_SetLPF(IFX_Overdrive *od, float lpf_Hz, float damp);
void IFX_Overdrive_SetQ(IFX_Overdrive *od, float Q);
```

---

## 4. 10-Band Parametric EQ

### Algorithm
Ten cascaded 2nd-order IIR **peaking filters** (biquads) at standard octave-band center frequencies.

**Bands (octave spacing):**
```
31 Hz, 63 Hz, 125 Hz, 250 Hz, 500 Hz,
1 kHz, 2 kHz, 4 kHz, 8 kHz, 16 kHz
```

Each band:
- **Bandwidth:** 1 octave (Q ≈ 1.41)
- **Gain Range:** −15 to +15 dB
- **Type:** Peaking (bell) filter

### Peaking Filter Design (Bilinear Biquad)

For a desired center frequency `fc` and bandwidth `BW`:

```
ωc = 2π × fc
α = tan(ωc·T/2) / (2·Q)    where Q = fc/BW ≈ 1.41 for octave

For boost/cut with linear gain G:

b0 =  1 + α·G
b1 = -2·cos(ωc·T)
b2 =  1 - α·G
a0 =  1 + α/G
a1 = -2·cos(ωc·T)
a2 =  1 - α/G

Normalize: divide all by a0

Difference Equation:
  y[n] = b0·x[n] + b1·x[n-1] + b2·x[n-2]
         - a1·y[n-1] - a2·y[n-2]
```

### Metallica Preset (Default Gains in dB)

The firmware ships with this iconic "scooped mid" rhythm tone:

```c
const float EQ10_METALLICA_PRESET[10] = {
    +6.0f,   // 31 Hz   — big low-end scoop start
    +5.0f,   // 63 Hz
    +3.0f,   // 125 Hz
    -2.0f,   // 250 Hz  — mid scoop begins
    -4.0f,   // 500 Hz  — deep mid scoop
    -6.0f,   // 1 kHz   — deepest scoop (mids are scooped)
    -5.0f,   // 2 kHz
    -3.0f,   // 4 kHz
    +4.0f,   // 8 kHz   — mid-high presence peak
    +3.0f    // 16 kHz  — high-end boost
};
```

This creates the characteristic Metallica tone: huge lows, scooped mids, emphasized highs.

### Parameters

| Name | Range | Unit |
|------|-------|------|
| 31 Hz Gain | −15 to +15 | dB |
| 63 Hz Gain | −15 to +15 | dB |
| 125 Hz Gain | −15 to +15 | dB |
| 250 Hz Gain | −15 to +15 | dB |
| 500 Hz Gain | −15 to +15 | dB |
| 1 kHz Gain | −15 to +15 | dB |
| 2 kHz Gain | −15 to +15 | dB |
| 4 kHz Gain | −15 to +15 | dB |
| 8 kHz Gain | −15 to +15 | dB |
| 16 kHz Gain | −15 to +15 | dB |

### C-Style Integration

```c
typedef struct {
    IFX_PeakingFilter bands[10];  // Each band is a biquad
    float gainDb[10];              // User-set gains
} IFX_10BandEQ;

void IFX_10BandEQ_Init(IFX_10BandEQ *eq, float sampleRate_Hz);
float IFX_10BandEQ_Update(IFX_10BandEQ *eq, float inp);
void IFX_10BandEQ_SetBand(IFX_10BandEQ *eq, uint8_t band, float gainDb);
```

**Individual Peaking Filter:**

```c
typedef struct {
    float sampleTime_s;  // = 1/fs
    float x[3];          // Input state [n-2, n-1, n]
    float y[3];          // Output state [n-2, n-1, n]
    float a[3];          // Denominator (a0=1 after norm)
    float b[3];          // Numerator
} IFX_PeakingFilter;

void IFX_PeakingFilter_SetParameters(IFX_PeakingFilter *filt,
                                     float centerFrequency_Hz,
                                     float bandwidth_Hz,
                                     float boostCut_linear);  // 10^(dB/20)
```

---

## 5. Chorus Effect

### Algorithm
Dual-delay chorus using sinusoidal modulation of delay time for the classic "ensemble" sound.

**Signal Flow:**
```
Input → [Delay Line A (modulated)] ┐
         [Delay Line B (modulated)] ├→ [Wet/Dry Mix] → Output
                                   ↓
```

Two independent delay lines with separate:
- Base delay times (typically 10–15 ms each)
- Modulation depths
- LFO (sinewave) rates

### Delay Line Implementation

Both voices read **one shared circular buffer** (2048 samples) at a fractional, sinusoidally
modulated delay with linear interpolation:

```c
d = base + depth · sin(2π · phase);      // phase: accumulator, += rate/fs per sample
rp = writeIndex − d  (wrapped);           // fractional read position
y = buf[i0] + frac · (buf[i0+1] − buf[i0]);
```

The LFO is a phase accumulator with a 7th-order polynomial sine (|err| < 1e-4), so there is
no `sinf()` per sample. (Before the optimisation pass the chorus modulated the buffer's
*wrap length* instead, which clicked whenever the length changed.)

### Parameters

| Name | Range | Unit | Description |
|------|-------|------|-------------|
| Delay A | 1–40 | ms | Base delay of line A |
| Delay B | 1–40 | ms | Base delay of line B |
| Depth A | 0–20 | — | Modulation depth (samples) |
| Depth B | 0–20 | — | Modulation depth (samples) |
| Rate A | 0.1–10 | Hz | LFO frequency for line A |
| Rate B | 0.1–10 | Hz | LFO frequency for line B |
| Gain A | 0.0–1.0 | — | Output level of line A |
| Gain B | 0.0–1.0 | — | Output level of line B |
| Mix | 0.0–1.0 | — | Wet/dry blending (0=dry, 1=fully wet) |

### Default Initialization

```c
delayA = 10 ms, depthA = 10, rateA = 1.0 Hz, gainA = 0.25
delayB = 15 ms, depthB = 10, rateB = 1.3 Hz, gainB = 0.25
mix = 0.5
```

### Memory Usage

```
1 shared delay line × 2048 samples = 8 KB
```

### C-Style Integration

```c
typedef struct {
    float delayLine[2048];        /* shared by both voices */
    uint16_t writeIndex;
    float baseA, baseB;           /* base delay, samples   */
    float depthA, depthB;         /* LFO depth, samples    */
    float rateA, rateB, incA, incB, phaseA, phaseB;
    float gainA, gainB, mix;
    float sampleRate, out;
} IFX_Chorus;

void IFX_Chorus_Init(IFX_Chorus *cho,
                     float delayTimeMsA, float delayTimeMsB,
                     float depthA, float depthB,
                     float gainA, float gainB,
                     float rateA, float rateB,
                     float mix, float sampleRate_Hz);
float IFX_Chorus_Update(IFX_Chorus *cho, float inp);
```

---

## 6. Delay Effect (v2)

Several ideas here come from the GuitarML STM32 pedal (`Leeman1982/GuitarPedalSTM32`,
`software/util/Delays`, `tape_modulator`), rewritten in C for this chain.

### Signal flow
```
line ← feed·in + fb·LPF(read)          (int16 storage, truncating, saturating)
wet  = LPF(read)                        read = tap(T + wow/flutter)        NORMAL
                                        read = 2 grains @ 2× speed         OCTAVE  (+12 st, climbs per repeat)
                                        read = 2 grains running backwards  REVERSE (grain = Time)
out  = dry·MixDry(mix) + wet·MixWet(mix)   (effect chain, see "mix law")
```
- **int16 line:** 72 000 samples = 1.5 s in 144 KB. The float line held 660 ms in 128 KB.
  Values are truncated toward zero, not rounded: in a lossy feedback loop, rounding leaves
  dead-band limit cycles that never decay.
- **Time glide:** an 80 ms one-pole, slew-limited to ±0.5 samples/sample. Turning Time bends
  the pitch like tape instead of jumping and clicking.
- **Grains (OCTAVE / REVERSE):** two read heads half a period apart with sin² windows (they
  sum to 1). The octave grain is 3072 samples. A reverse grain equals Time, so REVERSE goes up
  to 0.75 s.
- **Wow & flutter:** smooth gradient noise (Perlin-style, hashed lattice, period 4096). Wow is
  0.9 Hz with 2 octaves; flutter is 7 Hz. At depth 1 the excursion is ±40 / ±5 samples. It is
  evaluated every 32 samples and interpolated.
- **Feed:** a 10 ms ramp on the input gain. Feed = 0 gives trails (the tail rings on) and
  freeze (fb = 1, loop filter bypassed).

### Mix law (delay and reverb)
`MixDry = min(1, 2(1−mix))`, `MixWet = min(1, 2·mix)`. Dry stays at unity up to mix 0.5. The
old `(1−mix)·dry + mix·wet` law lowered the guitar as soon as any delay was dialled in.

### Parameters
| Name | Range | Default |
|------|-------|---------|
| Time | 10–1500 ms | 300 |
| Mix | 0–1 | 0.25 |
| Feedback | 0–0.95 | 0.4 |
| Mode | NORMAL / OCTAVE / REVERSE | NORMAL |
| Tone | 500–12000 Hz (LPF in the loop) | 6000 |
| Wow/Flut | 0–1 | 0.15 |
| Trails | OFF / ON | ON |

```c
IFX_Delay_Init(&dly, 300.0f, 0.25f, 0.4f, 48000.0f);
IFX_Delay_SetMode(&dly, IFX_DLY_REVERSE);
IFX_Delay_ProcessWet(&dly, in, wet, 64);   /* wet only; IFX_Delay_Process() = mixed, in place */
```

---

## 6b. Plate Reverb (new)

The figure-of-eight tank from Dattorro, *Effect Design Part 1* (JAES 1997), written from the
paper (`ifx_reverb.c`, MIT).

The GuitarML pedal uses DaisySP's ReverbSc, which is **LGPL-2.1**. Copying it into this MIT
repo would bring LGPL relinking obligations to the firmware, so it was not copied. Only its
int16-storage idea is reused: all 13 lines share one int16 pool of 41 137 samples (82 KB)
instead of ~164 KB as float.

- Predelay (0–100 ms) → bandwidth LPF → 4 input allpasses (0.75, 0.75, 0.625, 0.625).
- Tank: modulated allpass (−0.70, ±26 samples, 1 Hz, quadrature L/R) → delay → damping LPF →
  ×decay → allpass (decay + 0.15, clamped to 0.25…0.5) → delay, cross-coupled.
- Output: the 14 taps of the paper's table, L + R summed (mono out). The input is scaled by
  0.5 for int16 headroom.
- Freeze: decay 1, damping off, input muted.

| Name | Range | Default |
|------|-------|---------|
| Mix | 0–1 | 0.3 |
| Decay | 0–0.98 | 0.6 (RT60 ≈ 1.45 s) |
| Tone | 1–16 kHz (tank damping) | 6000 |
| PreDelay | 0–100 ms | 10 |
| Routing | DLY>VERB / PARALLEL / VERB>DLY | DLY>VERB |
| Trails | OFF / ON | ON |

---

## 7. Master Volume & Smoothing

### Algorithm

The master volume is applied **after** the entire effect chain to prevent DSP-generated clicks when the user adjusts volume mid-signal.

**Volume Smoother:**
```
smoothedVol = smoothedVol + (0.002 × sign(target - smoothedVol))
output = audioSample × smoothedVol
```

**Key Parameters:**
- **Ramp rate:** 0.002 per sample
- **Full-scale transition:** ~500 samples ≈ 10 ms @ 48 kHz
- **Location:** Core 0 audio callback (no cross-core sync needed)

This is critical for zero-click operation when the user adjusts volume via the rotary encoder.

---

## 7b. Switching, Trails and Hold

- **Crossfaded toggles:** each effect has a 0…1 fade (6 ms). While fading, the dry block is
  kept and blended, so toggling a 40× overdrive no longer clicks. A fully-off effect is still
  skipped (no CPU).
- **Time effects when switched off:** they keep running with their input muted until the tail
  has been below −66 dBFS for longer than their longest internal delay. Then they stop.
  - Trails ON: you hear that tail.
  - Trails OFF: the wet gain ramps to 0 in 10 ms.
  - Either way, re-enabling never replays stale audio.
- **HOLD (MUX CH8, momentary):** freezes the active delay and reverb while held (GuitarML's aux
  footswitch). The OLED header shows `*HOLD*`.
- **MUX CH9 / CH10** toggle delay and reverb. Unwired channels read "not pressed" thanks to
  the SIG pull-up.

---

## 8. I2S Audio Specifications

### Input (PCM1808 ADC)

```
Format: I2S Philips (1-clock delay)
Word Size: 32-bit per channel
Audio Data: bits 30–7 (24-bit signed, B23=MSB at bit 30)
Delay Bit: bit 31 = 0 (ignored)
Padding: bits 6–0 = 0

Conversion to float [-1.0, +1.0]:
  raw_24bit = ((raw_i32 << 1) >> 8)  // Extract 24-bit, arithmetic shift
  float = raw_24bit / 8388608.0f
```

### Output (PCM5102A DAC)

```
Format: I2S Philips (FMT=GND internally)
Word Size: 32-bit per channel
Audio Data: bits 30–7 (24-bit signed, B23=MSB at bit 30)
Delay Bit: bit 31 = 0 (unused)
Padding: bits 6–0 = 0

Conversion from float [-1.0, +1.0]:
  audio24 = (int32_t)(float × 8388607.0f)
  raw_word = ((uint32_t)audio24 & 0x00FFFFFF) << 7
```

### Clock Specifications

- **System Clock:** set by Arduino Tools → CPU Speed (250 MHz recommended); not overridden in code
- **MCLK/SCKI (PCM1808):** PWM divider chosen at boot for the closest 256/384/512·fs ratio —
  250 MHz / 20 = 12.5 MHz = 260.4·fs (+1.7 %); 240 MHz / 13 = 18.46 MHz = 384.6·fs (+0.16 %);
  150 MHz / 12 = 12.5 MHz (+1.7 %)
- **LRCK (word select):** 48 kHz (exact)
- **BCK (bit clock):** 3.072 MHz (48 kHz × 64 bits/frame)

---

## 9. Integration Into Other Projects

### Minimum Requirements

1. **C99 or C++ compiler** with `<math.h>` support
2. **32-bit floating-point** math (float)
3. **Sample rate flexibility** (init functions take `sampleRate_Hz` as parameter)
4. **RAM:** `sizeof(EffectChain)` = 273 KB (delay 144 KB, reverb 84 KB, boost 22 KB, chorus 8 KB, gate 10 KB)

### Pseudocode Integration

```c
#include "ifx_boost_factory.h"
#include "ifx_noisegate.h"
#include "ifx_overdrive.h"
#include "ifx_10band_eq.h"
#include "ifx_chorus.h"
#include "ifx_delay.h"

// Initialize all effects (sample rate = 48000 Hz)
IFX_BoostFactory boost;
IFX_NoiseGate noisegate;
IFX_Overdrive overdrive;
IFX_10BandEQ eq;
IFX_Chorus chorus;
IFX_Delay delay;

void init_effects(void) {
    IFX_BF_Init(&boost, 48000.0f);          /* TS808 model by default */
    IFX_NoiseGate_Init(&noisegate, 0.1f, 2.0f, 2.0f, 20.0f, 48000.0f);
    IFX_Overdrive_Init(&overdrive, 48000.0f, 150.0f, 40.0f, 5000.0f, 1.0f);
    IFX_10BandEQ_Init(&eq, 48000.0f);
    IFX_Chorus_Init(&chorus, 10.0f, 15.0f, 10.0f, 10.0f,
                    0.25f, 0.25f, 1.0f, 1.3f, 0.5f, 48000.0f);
    IFX_Delay_Init(&delay, 300.0f, 0.35f, 0.4f, 48000.0f);
}

// Process one sample through the entire chain
float process_sample(float input) {
    float sig = input;
    
    // Optional: enable/disable per effect
    /* Boost Factory is block-based: run IFX_BF_Process(&boost, buf, n) on the
     * whole block before this per-sample loop (see ProcessBlock in effect_chain.c) */
    sig = IFX_NoiseGate_Update(&noisegate, sig);
    sig = IFX_Overdrive_Update(&overdrive, sig);
    sig = IFX_10BandEQ_Update(&eq, sig);
    sig = IFX_Chorus_Update(&chorus, sig);
    sig = IFX_Delay_Update(&delay, sig);
    
    // Soft clip to [-1, +1]
    if (sig >  1.0f) sig =  1.0f;
    if (sig < -1.0f) sig = -1.0f;
    
    return sig;
}

// Real-time parameter adjustment
void set_boost_tone(float tone_0to1) {
    IFX_BF_SetTone(&boost, tone_0to1);     /* then IFX_BF_Service() on the UI core */
}

void set_overdrive_gain(float gain_1to200) {
    IFX_Overdrive_SetGain(&overdrive, gain_1to200);
}

void set_eq_band(uint8_t band_index, float gain_db) {
    IFX_10BandEQ_SetBand(&eq, band_index, gain_db);
}
```

### Real-Time Parameter Updates

All `SetParam()` functions are safe to call from interrupt handlers or UI threads — they update struct members that are read atomically by the audio callback. No locks required.

### Data Alignment

For best performance on Cortex-M33:
- Keep effect structs **4-byte aligned** (malloc/new do this)
- Keep audio buffers **8-byte aligned** (DMA may require it)

---

## 10. Performance Characteristics (RP2350 @ 250 MHz)

Measured as Cortex-M33 instructions per sample in QEMU (`-O2`, hot code in SRAM). Arm doesn't
publish M33 FPU cycle timings, so the cycles column is a *pessimistic* bound (VFMA 3 cycles,
VLDR 2, VDIV 14). Real cost is between the two. Budget: 250 MHz / 48 kHz = **5208 cycles per
sample per core**.

| Effect (core 0) | Instructions | Pessimistic cycles |
|-----------------|--------------|--------------------|
| Noise gate + overdrive + 10-band EQ + chorus + old delay (all on) | ~850 | ~2180 |
| + delay v2 (NORMAL) | ~+150 (static estimate) | — |
| + plate reverb | ~+740 (static estimate) | — |

The two new rows are **not measured**. They come from the instruction count of `dly_tick` /
`rv_tick` compiled for `-mcpu=cortex-m33 -O2`; the reverb tick is almost branch-free, at 770
static instructions. With everything on, core 0 is estimated at ~1750 instructions/sample, about
45–85 % of the 5208-cycle budget depending on real cycles per instruction. Check the core-0
figure on the OLED before relying on it.

| Boost Factory model (core 1) | Instructions | Pessimistic cycles |
|-----------------------------|--------------|--------------------|
| TS808 / KLON / DIST+ / RAT / RANGEMSTR | 940–1090 | 2280–3160 |
| DS-1 | 1340 | 3560 |
| FUZZFACE Ge / Si | 2080 / 1870 | 4730 / 4770 |
| BIG MUFF | 2280 | 5400 |

The firmware measures its own load with the DWT cycle counter on both cores (OLED chain view
and serial). If the Boost Factory keeps core 1 above 90 %, it falls back to 1× rendering for
the current model.

Optimisation-pass changes:
- the whole chain is block-based, with hot code in SRAM;
- overdrive: folded symmetric FIR (35 MACs instead of 69), coefficients precomputed, fast exp;
- EQ: normalised TDF-II biquads with 0 dB bands skipped — measured closer to a double-precision
  reference than the old DF-I;
- moving-RMS drift fixed;
- chorus rewritten (see §5).

---

## 11. References

- **Original Author:** Philip Salmony (@phils-lab.net)
- **Guitar Effect Theory:** "Guitar Effects Processors" (JSFX documentation)
- **Bilinear Transform:** Oppenheim & Schafer, "Discrete-Time Signal Processing"
- **IIR Filter Design:** Stolzl & Morawski, "The Bilinear Transform"
- **Tube Amp Modeling:** Yeh & Smith, "Nonlinear Digital Audio Effect Synthesis"

---

## License & Attribution

These algorithms are adapted from Philip Salmony's open-source guitar DSP library. Use freely in commercial/non-commercial projects with attribution.

**Attribution:** "DSP algorithms by Philip Salmony (@phils-lab.net), ported to RP2350 Arduino"

