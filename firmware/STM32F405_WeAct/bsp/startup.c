/* Startup for STM32F405 in C: vector table, Reset_Handler, fault handler.
 * Replaces ST's startup_stm32f405xx.s so the project needs no assembler file. */
#include "stm32f4xx.h"
#include "board.h"

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _ssram, _esram, _estack;
extern int main(void);
extern void __libc_init_array(void);

void Reset_Handler(void);
void Default_Handler(void);
void HardFault_Handler(void);

/* Every handler defaults to Default_Handler; define one with the CMSIS name
 * (e.g. DMA1_Stream3_IRQHandler) anywhere and the linker picks it. */
#define WEAK __attribute__((weak, alias("Default_Handler")))
void NMI_Handler(void) WEAK;          void MemManage_Handler(void) WEAK;
void BusFault_Handler(void) WEAK;     void UsageFault_Handler(void) WEAK;
void SVC_Handler(void) WEAK;          void DebugMon_Handler(void) WEAK;
void PendSV_Handler(void) WEAK;       void SysTick_Handler(void) WEAK;
void EXTI0_IRQHandler(void) WEAK;     void EXTI1_IRQHandler(void) WEAK;
void EXTI2_IRQHandler(void) WEAK;     void EXTI3_IRQHandler(void) WEAK;
void EXTI4_IRQHandler(void) WEAK;     void EXTI9_5_IRQHandler(void) WEAK;
void EXTI15_10_IRQHandler(void) WEAK;
void DMA1_Stream0_IRQHandler(void) WEAK; void DMA1_Stream1_IRQHandler(void) WEAK;
void DMA1_Stream2_IRQHandler(void) WEAK; void DMA1_Stream3_IRQHandler(void) WEAK;
void DMA1_Stream4_IRQHandler(void) WEAK; void DMA1_Stream5_IRQHandler(void) WEAK;
void DMA1_Stream6_IRQHandler(void) WEAK; void DMA1_Stream7_IRQHandler(void) WEAK;
void SPI2_IRQHandler(void) WEAK;      void I2C1_EV_IRQHandler(void) WEAK;
void I2C1_ER_IRQHandler(void) WEAK;   void USART1_IRQHandler(void) WEAK;
void TIM2_IRQHandler(void) WEAK;      void OTG_FS_IRQHandler(void) WEAK;

#define IRQ(n) [16 + (n)]
__attribute__((section(".isr_vector"), used))
void (*const g_vectors[16 + 82])(void) = {
    [0]  = (void (*)(void))(&_estack),
    [1]  = Reset_Handler,
    [2]  = NMI_Handler,
    [3]  = HardFault_Handler,
    [4]  = MemManage_Handler,
    [5]  = BusFault_Handler,
    [6]  = UsageFault_Handler,
    [11] = SVC_Handler,
    [12] = DebugMon_Handler,
    [14] = PendSV_Handler,
    [15] = SysTick_Handler,
    IRQ(EXTI0_IRQn) = EXTI0_IRQHandler,         IRQ(EXTI1_IRQn) = EXTI1_IRQHandler,
    IRQ(EXTI2_IRQn) = EXTI2_IRQHandler,         IRQ(EXTI3_IRQn) = EXTI3_IRQHandler,
    IRQ(EXTI4_IRQn) = EXTI4_IRQHandler,         IRQ(EXTI9_5_IRQn) = EXTI9_5_IRQHandler,
    IRQ(EXTI15_10_IRQn) = EXTI15_10_IRQHandler,
    IRQ(DMA1_Stream0_IRQn) = DMA1_Stream0_IRQHandler, IRQ(DMA1_Stream1_IRQn) = DMA1_Stream1_IRQHandler,
    IRQ(DMA1_Stream2_IRQn) = DMA1_Stream2_IRQHandler, IRQ(DMA1_Stream3_IRQn) = DMA1_Stream3_IRQHandler,
    IRQ(DMA1_Stream4_IRQn) = DMA1_Stream4_IRQHandler, IRQ(DMA1_Stream5_IRQn) = DMA1_Stream5_IRQHandler,
    IRQ(DMA1_Stream6_IRQn) = DMA1_Stream6_IRQHandler, IRQ(DMA1_Stream7_IRQn) = DMA1_Stream7_IRQHandler,
    IRQ(SPI2_IRQn) = SPI2_IRQHandler,           IRQ(I2C1_EV_IRQn) = I2C1_EV_IRQHandler,
    IRQ(I2C1_ER_IRQn) = I2C1_ER_IRQHandler,     IRQ(USART1_IRQn) = USART1_IRQHandler,
    IRQ(TIM2_IRQn) = TIM2_IRQHandler,           IRQ(OTG_FS_IRQn) = OTG_FS_IRQHandler,
};

/* Unlisted slots are 0: an interrupt nobody enabled would fetch address 0
 * and hard-fault, which ends in the same LED pattern as Default_Handler. */

void Reset_Handler(void) {
    SCB->CPACR |= (3u << 20) | (3u << 22);           /* FPU before any float code */
    /* Entered from the ROM DFU bootloader ("dfu-util … :leave") the vector
     * table may still point at system memory and the bootloader's USB IRQs
     * may be enabled: take over cleanly. */
    __disable_irq();
    SCB->VTOR = (uint32_t)g_vectors;
    for (int i = 0; i < 8; i++) { NVIC->ICER[i] = 0xFFFFFFFFu; NVIC->ICPR[i] = 0xFFFFFFFFu; }
    SysTick->CTRL = 0;
    __DSB(); __ISB();
    __enable_irq();
    for (uint32_t *s = &_sidata, *d = &_sdata; d < &_edata;) *d++ = *s++;
    for (uint32_t *d = &_sbss; d < &_ebss;) *d++ = 0;
    for (uint32_t *d = &_ssram; d < &_esram;) *d++ = 0;
    __libc_init_array();
    main();
    for (;;) { }
}

/* Unexpected interrupt / fault: blink the LED in a fast double pattern so a
 * crash is visible without a debugger. */
static void fault_blink(void) {
    __disable_irq();
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    GPIOB->MODER = (GPIOB->MODER & ~(3u << 4)) | (1u << 4);   /* PB2 output */
    for (;;) {
        for (int k = 0; k < 4; k++) {
            GPIOB->ODR ^= 1u << 2;
            for (volatile uint32_t i = 0; i < 600000u; i++) { }
        }
        for (volatile uint32_t i = 0; i < 6000000u; i++) { }
    }
}
void Default_Handler(void)   { fault_blink(); }
void HardFault_Handler(void) { fault_blink(); }
