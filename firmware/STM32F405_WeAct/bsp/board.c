#include "board.h"

uint32_t SystemCoreClock = 16000000u;      /* CMSIS global; HSI until clocks are set */
static volatile uint32_t s_ms;
void (*board_tick_hook)(void);

GPIO_TypeDef *gpio_port(uint8_t pin) {
    return (GPIO_TypeDef *)(GPIOA_BASE + (uint32_t)(pin >> 4) * (GPIOB_BASE - GPIOA_BASE));
}

void gpio_config(uint8_t pin, gpio_mode_t mode, gpio_pull_t pull, uint8_t af,
                 bool open_drain, uint8_t speed) {
    uint32_t port = pin >> 4, b = pin & 15u;
    RCC->AHB1ENR |= 1u << port;                 /* GPIOxEN bits are 0..8 */
    (void)RCC->AHB1ENR;
    GPIO_TypeDef *g = gpio_port(pin);
    if (mode == GPIO_AF) {
        volatile uint32_t *afr = &g->AFR[b >> 3];
        *afr = (*afr & ~(0xFu << ((b & 7u) * 4u))) | ((uint32_t)(af & 15u) << ((b & 7u) * 4u));
    }
    g->OSPEEDR = (g->OSPEEDR & ~(3u << (b * 2u))) | ((uint32_t)(speed & 3u) << (b * 2u));
    g->OTYPER  = (g->OTYPER & ~(1u << b)) | ((open_drain ? 1u : 0u) << b);
    g->PUPDR   = (g->PUPDR & ~(3u << (b * 2u))) | ((uint32_t)pull << (b * 2u));
    g->MODER   = (g->MODER & ~(3u << (b * 2u))) | ((uint32_t)mode << (b * 2u));
}

bool board_clock_init(void) {
    /* FPU on (also done in Reset_Handler, harmless twice) */
    SCB->CPACR |= (3u << 20) | (3u << 22);

    /* Back to HSI with both PLLs off: the DFU bootloader leaves the main PLL
     * running (it needs 48 MHz for USB) and a running PLL cannot be re-set. */
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY)) { }
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) { }
    RCC->CR &= ~(RCC_CR_PLLON | RCC_CR_PLLI2SON);
    while (RCC->CR & (RCC_CR_PLLRDY | RCC_CR_PLLI2SRDY)) { }

    RCC->CR |= RCC_CR_HSEON;
    uint32_t t = 0;
    while (!(RCC->CR & RCC_CR_HSERDY)) { if (++t > 2000000u) return false; }

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;
    PWR->CR |= PWR_CR_VOS;                       /* voltage scale 1 for 168 MHz */

    FLASH->ACR = FLASH_ACR_LATENCY_5WS | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;

    /* main PLL: 8 MHz / 8 = 1 MHz; ×336 = 336 MHz VCO; /2 = 168 MHz; /7 = 48 MHz */
    RCC->PLLCFGR = (8u << RCC_PLLCFGR_PLLM_Pos) | (336u << RCC_PLLCFGR_PLLN_Pos) |
                   (0u << RCC_PLLCFGR_PLLP_Pos) | RCC_PLLCFGR_PLLSRC_HSE |
                   (7u << RCC_PLLCFGR_PLLQ_Pos);
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) { }

    /* AHB 168, APB1 42 (÷4), APB2 84 (÷2) */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2)) |
                RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) { }

    /* PLLI2S (same 1 MHz input): ×258 = 258 MHz, /3 = 86 MHz I2S clock.
     * RM0090 table 127: with MCK on, I2SDIV = 3, ODD = 1 → 47.991 kHz. */
    RCC->PLLI2SCFGR = (258u << RCC_PLLI2SCFGR_PLLI2SN_Pos) | (3u << RCC_PLLI2SCFGR_PLLI2SR_Pos);
    RCC->CR |= RCC_CR_PLLI2SON;
    while (!(RCC->CR & RCC_CR_PLLI2SRDY)) { }

    SystemCoreClock = BOARD_SYSCLK_HZ;
    return true;
}

void SysTick_Handler(void) {
    s_ms++;
    if (board_tick_hook) board_tick_hook();
}

uint32_t board_millis(void) { return s_ms; }

void board_delay_ms(uint32_t ms) {
    uint32_t t0 = s_ms;
    while ((uint32_t)(s_ms - t0) < ms) __WFI();
}

void board_delay_us(uint32_t us) {
    uint32_t t0 = DWT->CYCCNT, n = us * (SystemCoreClock / 1000000u);
    while ((uint32_t)(DWT->CYCCNT - t0) < n) { }
}

void board_init(void) {
    bool hse = board_clock_init();
    SysTick_Config(SystemCoreClock / 1000u);
    NVIC_SetPriority(SysTick_IRQn, 15);           /* lowest: never delays audio */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    gpio_config(BOARD_LED_PIN, GPIO_OUT, GPIO_NOPULL, 0, false, 0);
    gpio_config(BOARD_KEY_PIN, GPIO_IN, GPIO_PULLUP, 0, false, 0);
    if (!hse) {                                   /* no crystal: signal it forever */
        for (;;) { gpio_toggle(BOARD_LED_PIN); board_delay_ms(60); }
    }
}
