# InfiniFX Guitar DSP Platform
 
 Sample code to accompany YouTube DSP tutorials.

## Firmware

- `firmware/InfiniFX-Reloaded-TikiDrive_Firmware/` — original STM32H7 (STM32CubeIDE) project.
- `firmware/IFX_MultiFX_ESP32S3/` — guitar multi-effects unit for ESP32-S3 (Arduino IDE):
  PCM1808 in / PCM5102 out, SH1106 OLED, 4 pots, per-effect footswitches.
  Chain: Noise Gate → TS Boost → Overdrive → Chorus → Delay.
  See its [README](firmware/IFX_MultiFX_ESP32S3/README.md) for wiring and setup.
