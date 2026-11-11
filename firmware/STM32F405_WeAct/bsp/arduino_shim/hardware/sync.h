#pragma once
/* pico-sdk hardware/sync.h subset used by rotary_encoder.cpp */
#include "board.h"
static inline uint32_t save_and_disable_interrupts(void) { uint32_t p = __get_PRIMASK(); __disable_irq(); return p; }
static inline void restore_interrupts(uint32_t p) { __set_PRIMASK(p); }
