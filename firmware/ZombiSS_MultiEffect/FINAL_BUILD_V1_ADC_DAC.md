# Final Build v1 — ADC / DAC Verification & Wiring

This build was audited end-to-end with special focus on the audio I/O path
(PCM1808 ADC + PCM5102 DAC). Below is exactly what was wrong, what was fixed,
and the wiring you must have for signal to pass.

---

## Bugs found & fixed in the audio engine

| # | Where | Bug | Effect | Fix |
|---|-------|-----|--------|-----|
| 1 | `i2s_pio_programs.h` | All `jmp x--` opcodes were encoded as `jmp PIN` (cond `0b110` not `0b010`). Default JMP_PIN = GP0 (held HIGH) so every jump was always taken. | PIO bit-loops never exit → infinite loop → **no valid I2S clocks at all**. | Re-encoded the 4 loop jumps to true `X--` (verified with the pico-SDK encoder). |
| 2 | `i2s_pio_programs.h` | One input jump also targeted `set x,31` instead of the loop body. | Counter reset every pass → infinite loop. | Re-targeted to the loop body (addr 9). |
| 3 | `i2s_pio_programs.h` | Output side-set bit order swapped: BCK ended up on GP18, LRCK on GP17. | BCK/LRCK swapped vs. config, bridges and the PCM chips. | side-set bit0 = BCK (GP17), bit1 = LRCK (GP18). |
| 4 | `i2s_pio_programs.h` | clkdiv target used `48000*64*2`, but the program runs **130** PIO cycles/frame. | LRCK ≈ 47.26 kHz (1.5 % flat). | target = `48000*65*2` → exact 48 kHz. |
| 5 | `ZombiSS_MultiEffect.ino` | **No SCKI generated** for the PCM1808. | ADC modulator has no clock → **silence**. | GP22 PWM @ 12.5 MHz in `setup()`. |
| 6 | `ZombiSS_MultiEffect.ino` | **XSMT never driven** for the PCM5102. | DAC stays soft-muted → **silence**. | GP0 driven HIGH in `setup()`. |
| 7 | `ZombiSS_MultiEffect.ino` | Sample word was left-justified (audio in bits 31:8) but the chips are I2S Philips. | Every sample off by one bit (≈6 dB low + LSB garbage). | Format B: input `((u32)raw<<1)>>8`, output `((u32)a24 & 0xFFFFFF)<<7`. |

Items 1–4 mean the PIO never produced correct I2S regardless of hardware.
Items 5–6 mean both chips were dead/muted. Item 7 would corrupt audio even
once clocks ran. All are fixed in this branch.

---

## The hardware cause you found (keep it!)

The PCM1808 module's **analog supply (5V / VCC pin) was not connected**.
With only the 3.3 V logic powered, the sigma-delta modulator does not run and
you get complete silence even when the digital I2S looks perfect. Always power
**both** the board's `5V`/`VCC` and `GND`.

---

## Wiring — PCM5102 DAC (RP2350 is I2S master)

| PCM5102 pin | Connect to | Notes |
|-------------|-----------|-------|
| VIN | 3V3 | onboard regulator |
| GND | GND | |
| DIN | GP16 | serial data |
| BCK | GP17 | bit clock |
| LCK | GP18 | word select (LRCK/WS) |
| SCK | GND | selects internal PLL |
| XSMT | GP0 | HIGH = unmuted (firmware drives it; or tie to 3V3) |
| FMT | GND | I2S Philips |

## Wiring — PCM1808 ADC (slave; RP2350 supplies all clocks)

| PCM1808 pin | Connect to | Notes |
|-------------|-----------|-------|
| **5V / VCC** | **5V** | **REQUIRED — analog supply (the missing piece)** |
| GND | GND | |
| OUT | GP19 | serial data to RP2350 |
| SCKI | GP22 | 12.5 MHz master clock (PWM) — required |
| BCK | GP17 | shared with DAC BCK |
| LRC | GP18 | shared with DAC LRCK |
| FMT | GND | I2S Philips, 24-bit |
| MD0 | GND | MD0=MD1=0 → slave mode |
| MD1 | GND | |

## Bridge wires (required)

The input PIO reads BCK/LRCK on GP21/GP20, so jumper:

```
GP17 ──→ GP21   (BCK)
GP18 ──→ GP20   (LRCK)
```

So GP17 fans out to: PCM5102 BCK, PCM1808 BCK, and GP21.
And GP18 fans out to: PCM5102 LCK, PCM1808 LRC, and GP20.

---

## Clocking summary (sys clock = 150 MHz)

| Signal | Frequency | Source |
|--------|-----------|--------|
| SCKI (PCM1808) | 12.5 MHz | PWM 150 MHz ÷ 12 (~256 fs, +1.7 %) |
| LRCK / WS | 48.000 kHz | output PIO (130 cyc/frame, clkdiv target 65×2) |
| BCK | 3.072 MHz | output PIO (64 × fs) |

## Quick bring-up checks
1. Scope GP18 → should be a clean 48 kHz square wave (LRCK).
2. Scope GP17 → 3.072 MHz bit clock (BCK).
3. Scope GP22 → 12.5 MHz (SCKI).
4. Confirm GP0 sits at 3.3 V after boot (XSMT unmuted).
5. Confirm the PCM1808 **5V** pin reads ~5 V and its GND is common.
