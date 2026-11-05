# InfiniFX Guitar DSP Platform
 
 Sample code to accompany YouTube DSP tutorials.

## Tools

- `tools/pedalflash/`: choose your STM32 board, then build a CubeIDE project without the IDE
  and flash it over ST-LINK or USB DFU. See its README.
- `tools/dsp_test/run.sh`: host tests for the ZombiSS delay, reverb and effect switching.
  Needs only gcc.
- `tools/boost_factory/`: Boost Factory validation against ngspice.
