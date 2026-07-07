cmake_minimum_required(VERSION 3.13)

# Pull in Pico SDK
set(PICO_PLATFORM rp2350)
set(PICO_BOARD pico2)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(zombi_ss_multi_fx C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(zombi_ss_multi_fx
    src/main.c
    src/dsp/ifx_overdrive.c
    src/dsp/ifx_noisegate.c
    src/dsp/ifx_delay.c
    src/dsp/ifx_chorus.c
    src/dsp/ifx_peaking_filter.c
    src/dsp/ifx_moving_rms.c
    src/dsp/ifx_delay_line.c
    src/dsp/ifx_log_audio_pot.c
    src/dsp/effect_chain.c
    src/drivers/i2s_audio.c
    src/drivers/sh1106_oled.c
    src/drivers/rotary_encoder.c
    src/ui/ui_system.c
    src/ui/splash_screen.c
    src/ui/font5x7.c
)

target_include_directories(zombi_ss_multi_fx PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/include/dsp
    ${CMAKE_CURRENT_LIST_DIR}/include/drivers
    ${CMAKE_CURRENT_LIST_DIR}/include/ui
)

pico_generate_pio_header(zombi_ss_multi_fx ${CMAKE_CURRENT_LIST_DIR}/pio/i2s_out.pio)
pico_generate_pio_header(zombi_ss_multi_fx ${CMAKE_CURRENT_LIST_DIR}/pio/i2s_in.pio)

target_link_libraries(zombi_ss_multi_fx
    pico_stdlib
    pico_multicore
    hardware_pio
    hardware_dma
    hardware_i2c
    hardware_gpio
    hardware_irq
    hardware_clocks
    hardware_timer
    pico_float
)

# Use both float implementations optimized for RP2350
target_compile_definitions(zombi_ss_multi_fx PRIVATE
    PICO_FLOAT_SUPPORT_ROM_V1=1
    PICO_FLOAT_SUPPORT_ROM_V2=1
)

# Aggressive optimization for DSP
target_compile_options(zombi_ss_multi_fx PRIVATE
    -O2
    -ffast-math
    -funroll-loops
    -ffunction-sections
    -fdata-sections
)

target_link_options(zombi_ss_multi_fx PRIVATE
    -Wl,--gc-sections
)

pico_add_extra_outputs(zombi_ss_multi_fx)

# Optionally enable USB stdio for debugging
pico_enable_stdio_usb(zombi_ss_multi_fx 0)
pico_enable_stdio_uart(zombi_ss_multi_fx 1)
