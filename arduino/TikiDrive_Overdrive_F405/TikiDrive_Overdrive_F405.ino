/*
 *  InfiniFX TikiDrive - Overdrive port for WeAct STM32F405RGT6 Core Board
 *  ----------------------------------------------------------------------
 *  DSP: IFX_Overdrive by Philip Salmony @ phils-lab.net (unchanged, see
 *       IFX_Overdrive.c/.h in this folder).
 *
 *  Build (Arduino IDE, STM32duino core "STM32 boards groups"):
 *    Board:             "Generic STM32F4 series"
 *    Board part number: "Generic F405RGTx"
 *    U(S)ART support:   enabled (optional, for Serial debug)
 *    Upload method:     "STM32CubeProgrammer (DFU)" or ST-Link (SWD)
 *
 *  Audio path (48 kHz, 24-bit, I2S2 full duplex master):
 *    Guitar -> TL071 buffer (9 V, 4.5 V bias) -> PCM1808 LIN -> I2S2_ext RX
 *    -> IFX_Overdrive -> I2S2 TX -> UDA1334A -> amp
 *
 *  Fixed I2S2 pins (WeAct F405 core board):
 *    PB12  LRCK / WS   -> PCM1808 LRC  and UDA1334A WSEL
 *    PB13  BCK  / CK   -> PCM1808 BCK  and UDA1334A BCLK
 *    PB15  SD (TX out) -> UDA1334A DIN
 *    PB14  SD (RX in)  <- PCM1808 OUT
 *    PC6   MCK (256fs) -> PCM1808 SCK   (UDA1334A needs no MCLK, PLL from BCLK)
 *
 *  Controls (all configurable below):
 *    PA0  pot 1  DRIVE  (pre-gain 10..110)
 *    PA1  pot 2  BIAS   (asymmetry / clipping character)
 *    PA2  pot 3  TONE   (output low-pass 500 Hz..16.5 kHz)
 *    PA3  pot 4  LEVEL  (output volume)
 *    PC13 momentary footswitch to GND (toggles effect on/off, also the
 *         onboard WeAct KEY button)
 *    PA8  status LED (on = effect engaged), through ~1k resistor to GND
 */

#include "IFX_Overdrive.h"

/* ------------------------------------------------------------------ */
/* User configuration                                                  */
/* ------------------------------------------------------------------ */
#define PIN_POT_DRIVE     PA0
#define PIN_POT_BIAS      PA1
#define PIN_POT_TONE      PA2
#define PIN_POT_LEVEL     PA3
#define PIN_FOOTSWITCH    PC13   /* momentary, to GND, internal pull-up */
#define PIN_STATUS_LED    PA8    /* high = effect on                    */

#define SAMPLING_FREQUENCY_HZ  47991.0f  /* actual rate from PLLI2S N=258 R=3 */

/* Same parameter ranges as the original TikiDrive firmware (main.c) */
#define OD_GAIN_MIN        10.0f
#define OD_GAIN_SCALE     100.0f
#define OD_HPF_CUTOFF_HZ  150.0f   /* fixed input HPF (recommended setting) */
#define OD_LPF_CUTOFF_MIN 500.0f
#define OD_LPF_CUTOFF_SCALE 16000.0f
#define OD_LPF_DAMP         1.0f

/* Control smoothing / update threshold (as in the original firmware) */
#define CONTROL_ALPHA             0.5f
#define CONTROL_CHANGE_THRESHOLD  0.01f
#define CONTROL_PERIOD_MS         5
#define DEBOUNCE_MS               30

/* Frames (stereo samples) per processing block; total DMA buffer is 2x */
#define BLOCK_FRAMES  64

/* ------------------------------------------------------------------ */
/* Audio buffers and state                                             */
/* ------------------------------------------------------------------ */
/* 24-bit-in-32-bit frames: each frame = 2 channels x 2 half-words     */
#define HALFWORDS_PER_FRAME 4
static uint16_t rxBuf[2 * BLOCK_FRAMES * HALFWORDS_PER_FRAME];
static uint16_t txBuf[2 * BLOCK_FRAMES * HALFWORDS_PER_FRAME];

#define INT32_TO_FLOAT  (4.656612873e-10f)  /* 1 / 2^31 */
#define FLOAT_TO_INT32  (2147483000.0f)

static IFX_Overdrive od;

static volatile float outputVolume = 1.0f;
static volatile uint8_t fxEnabled  = 0;
static float fxMix = 0.0f;                    /* bypass crossfade, ISR only */
#define FX_MIX_STEP (1.0f / (0.010f * SAMPLING_FREQUENCY_HZ))  /* ~10 ms ramp */

static I2S_HandleTypeDef  hi2s2;
static DMA_HandleTypeDef  hdmaI2sRx;
static DMA_HandleTypeDef  hdmaI2sTx;

/* ------------------------------------------------------------------ */
/* Clock configuration: 8 MHz HSE crystal -> 168 MHz SYSCLK            */
/* (overrides the weak HSI-based config of the generic variant)        */
/* ------------------------------------------------------------------ */
extern "C" void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM       = 8;             /* 8 MHz / 8 = 1 MHz  */
  RCC_OscInitStruct.PLL.PLLN       = 336;
  RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2; /* 168 MHz SYSCLK     */
  RCC_OscInitStruct.PLL.PLLQ       = 7;             /* 48 MHz (USB)       */
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
    Error_Handler();
  }
}

/* ------------------------------------------------------------------ */
/* I2S2 full-duplex MSP: GPIO + DMA                                    */
/* ------------------------------------------------------------------ */
extern "C" void HAL_I2S_MspInit(I2S_HandleTypeDef *hi2s)
{
  GPIO_InitTypeDef GPIO_InitStruct = {};

  if (hi2s->Instance != SPI2) {
    return;
  }

  __HAL_RCC_SPI2_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* PB12 = WS, PB13 = CK, PB15 = SD (TX) */
  GPIO_InitStruct.Pin       = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_15;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* PC6 = MCK (256fs for the PCM1808) */
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* PB14 = I2S2ext_SD (RX from PCM1808), note AF6 */
  GPIO_InitStruct.Pin       = GPIO_PIN_14;
  GPIO_InitStruct.Alternate = GPIO_AF6_I2S2ext;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* RX: I2S2_ext -> memory, DMA1 Stream3 Channel3 */
  hdmaI2sRx.Instance                 = DMA1_Stream3;
  hdmaI2sRx.Init.Channel             = DMA_CHANNEL_3;
  hdmaI2sRx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
  hdmaI2sRx.Init.PeriphInc           = DMA_PINC_DISABLE;
  hdmaI2sRx.Init.MemInc              = DMA_MINC_ENABLE;
  hdmaI2sRx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  hdmaI2sRx.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
  hdmaI2sRx.Init.Mode                = DMA_CIRCULAR;
  hdmaI2sRx.Init.Priority            = DMA_PRIORITY_HIGH;
  hdmaI2sRx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
  if (HAL_DMA_Init(&hdmaI2sRx) != HAL_OK) {
    Error_Handler();
  }
  __HAL_LINKDMA(hi2s, hdmarx, hdmaI2sRx);

  /* TX: memory -> I2S2, DMA1 Stream4 Channel0 */
  hdmaI2sTx.Instance                 = DMA1_Stream4;
  hdmaI2sTx.Init.Channel             = DMA_CHANNEL_0;
  hdmaI2sTx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
  hdmaI2sTx.Init.PeriphInc           = DMA_PINC_DISABLE;
  hdmaI2sTx.Init.MemInc              = DMA_MINC_ENABLE;
  hdmaI2sTx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  hdmaI2sTx.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
  hdmaI2sTx.Init.Mode                = DMA_CIRCULAR;
  hdmaI2sTx.Init.Priority            = DMA_PRIORITY_HIGH;
  hdmaI2sTx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
  if (HAL_DMA_Init(&hdmaI2sTx) != HAL_OK) {
    Error_Handler();
  }
  __HAL_LINKDMA(hi2s, hdmatx, hdmaI2sTx);

  /* Half/complete callbacks are driven from the RX stream interrupt */
  HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
}

extern "C" void DMA1_Stream3_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdmaI2sRx);
}

/* ------------------------------------------------------------------ */
/* Audio processing                                                    */
/* ------------------------------------------------------------------ */
static void processBlock(uint16_t *rx, uint16_t *tx)
{
  const float mixTarget = fxEnabled ? 1.0f : 0.0f;
  const float volume    = outputVolume;

  for (uint32_t n = 0; n < BLOCK_FRAMES; n++) {
    /* Left channel in, 24-bit left-justified in 32 bits (MSB half first) */
    int32_t sampleIn = (int32_t)(((uint32_t)rx[0] << 16) | rx[1]);
    float inp = (float)sampleIn * INT32_TO_FLOAT;

    /* Always run the effect so filter states stay warm and CPU load is
       constant; crossfade wet/dry over ~10 ms to avoid bypass clicks */
    float wet = IFX_Overdrive_Update(&od, inp);

    if (fxMix < mixTarget) {
      fxMix += FX_MIX_STEP;
      if (fxMix > 1.0f) fxMix = 1.0f;
    } else if (fxMix > mixTarget) {
      fxMix -= FX_MIX_STEP;
      if (fxMix < 0.0f) fxMix = 0.0f;
    }

    float out = volume * (fxMix * wet + (1.0f - fxMix) * inp);
    if (out > 1.0f) {
      out = 1.0f;
    } else if (out < -1.0f) {
      out = -1.0f;
    }

    int32_t sampleOut = (int32_t)(out * FLOAT_TO_INT32);
    tx[0] = (uint16_t)((uint32_t)sampleOut >> 16);
    tx[1] = (uint16_t)((uint32_t)sampleOut & 0xFFFFu);
    tx[2] = tx[0];  /* same signal on the right channel */
    tx[3] = tx[1];

    rx += HALFWORDS_PER_FRAME;
    tx += HALFWORDS_PER_FRAME;
  }
}

extern "C" void HAL_I2SEx_TxRxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
  (void)hi2s;
  processBlock(&rxBuf[0], &txBuf[0]);
}

extern "C" void HAL_I2SEx_TxRxCpltCallback(I2S_HandleTypeDef *hi2s)
{
  (void)hi2s;
  processBlock(&rxBuf[BLOCK_FRAMES * HALFWORDS_PER_FRAME],
               &txBuf[BLOCK_FRAMES * HALFWORDS_PER_FRAME]);
}

/* ------------------------------------------------------------------ */
/* Setup                                                               */
/* ------------------------------------------------------------------ */
void setup()
{
  pinMode(PIN_FOOTSWITCH, INPUT_PULLUP);
  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, LOW);
  analogReadResolution(12);

  IFX_Overdrive_Init(&od, SAMPLING_FREQUENCY_HZ, OD_HPF_CUTOFF_HZ,
                     OD_GAIN_MIN, OD_LPF_CUTOFF_MIN, OD_LPF_DAMP);

  /* PLLI2S: 1 MHz * 258 / 3 = 86 MHz I2S clock -> 47.991 kHz with MCLK */
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {};
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2S;
  PeriphClkInit.PLLI2S.PLLI2SN = 258;
  PeriphClkInit.PLLI2S.PLLI2SR = 3;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
    Error_Handler();
  }

  hi2s2.Instance          = SPI2;
  hi2s2.Init.Mode         = I2S_MODE_MASTER_TX;
  hi2s2.Init.Standard     = I2S_STANDARD_PHILIPS;
  hi2s2.Init.DataFormat   = I2S_DATAFORMAT_24B;
  hi2s2.Init.MCLKOutput   = I2S_MCLKOUTPUT_ENABLE;
  hi2s2.Init.AudioFreq    = I2S_AUDIOFREQ_48K;
  hi2s2.Init.CPOL         = I2S_CPOL_LOW;
  hi2s2.Init.ClockSource  = I2S_CLOCK_PLL;
  hi2s2.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_ENABLE;
  if (HAL_I2S_Init(&hi2s2) != HAL_OK) {
    Error_Handler();
  }

  /* Size = number of 32-bit slots in the whole double buffer (L+R) */
  if (HAL_I2SEx_TransmitReceive_DMA(&hi2s2, txBuf, rxBuf,
                                    2 * BLOCK_FRAMES * 2) != HAL_OK) {
    Error_Handler();
  }
}

/* ------------------------------------------------------------------ */
/* Loop: pots + footswitch at control rate                             */
/* ------------------------------------------------------------------ */
void loop()
{
  static float    control[4]     = {-1.0f, -1.0f, -1.0f, -1.0f};
  static uint32_t lastControlMs  = 0;
  static uint8_t  lastSwitchRaw  = HIGH;
  static uint8_t  switchState    = HIGH;
  static uint32_t lastEdgeMs     = 0;

  uint32_t now = millis();

  /* Footswitch: debounce, toggle on press */
  uint8_t raw = digitalRead(PIN_FOOTSWITCH);
  if (raw != lastSwitchRaw) {
    lastEdgeMs = now;
    lastSwitchRaw = raw;
  }
  if ((now - lastEdgeMs) > DEBOUNCE_MS && raw != switchState) {
    switchState = raw;
    if (switchState == LOW) {          /* pressed */
      fxEnabled = !fxEnabled;
      digitalWrite(PIN_STATUS_LED, fxEnabled ? HIGH : LOW);
    }
  }

  /* Pots */
  if ((now - lastControlMs) >= CONTROL_PERIOD_MS) {
    lastControlMs = now;

    static const uint32_t potPin[4] = {PIN_POT_DRIVE, PIN_POT_BIAS,
                                       PIN_POT_TONE, PIN_POT_LEVEL};
    for (uint8_t i = 0; i < 4; i++) {
      float reading = (float)analogRead(potPin[i]) * (1.0f / 4095.0f);

      if (control[i] < 0.0f) {
        control[i] = reading;          /* first pass: take pot as-is */
      } else {
        float smoothed = CONTROL_ALPHA * control[i]
                         + (1.0f - CONTROL_ALPHA) * reading;
        if (fabsf(smoothed - control[i]) < CONTROL_CHANGE_THRESHOLD) {
          continue;
        }
        control[i] = smoothed;
      }

      switch (i) {
        case 0:  /* DRIVE */
          IFX_Overdrive_SetGain(&od, OD_GAIN_MIN + OD_GAIN_SCALE * control[0]);
          break;
        case 1:  /* BIAS: same range as the original firmware's 5th pot */
          od.Q = -0.05f - control[1];
          break;
        case 2:  /* TONE */
          IFX_Overdrive_SetLPF(&od, OD_LPF_CUTOFF_MIN
                               + OD_LPF_CUTOFF_SCALE * control[2], OD_LPF_DAMP);
          break;
        case 3:  /* LEVEL */
          outputVolume = control[3];
          break;
      }
    }
  }
}
