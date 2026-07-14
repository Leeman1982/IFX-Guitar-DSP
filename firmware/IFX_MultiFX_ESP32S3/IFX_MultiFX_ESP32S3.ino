/*
*
*   InfiniFX Multi-FX - ESP32-S3 port
*
*   Guitar multi-effects unit based on the InfiniFX DSP modules.
*
*   Signal chain:
*     Guitar -> (external buffer/preamp) -> PCM1808 ->
*       Noise Gate -> TS Boost -> Overdrive -> Chorus -> Delay
*     -> PCM5102 -> Amp
*
*   Hardware (see config.h for the pin map):
*     - ESP32-S3-WROOM-1 module (board: "ESP32S3 Dev Module")
*     - PCM1808 I2S ADC, PCM5102 I2S DAC
*     - 1.3" SH1106 128x64 OLED on I2C
*     - 5 momentary footswitches (one per effect, to GND)
*         short press = toggle effect on/off
*         long press  = select effect for editing
*     - 4 potentiometers, mapped to the selected effect's parameters
*
*   Required Arduino libraries / cores:
*     - esp32 board package by Espressif >= 3.0 (I2S "std" driver)
*     - U8g2 by olikraus (OLED)
*
*/

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#include "config.h"
#include "AudioIO.h"

#include "IFX_NoiseGate.h"
#include "IFX_TubeScreamer.h"
#include "IFX_Overdrive.h"
#include "IFX_Chorus.h"
#include "IFX_Delay.h"

/* ------------------------------------------------------------------ */
/* Effects                                                            */
/* ------------------------------------------------------------------ */

enum FxId : uint8_t {
  FX_GATE = 0,
  FX_BOOST,
  FX_OVERDRIVE,
  FX_CHORUS,
  FX_DELAY,
  FX_COUNT
};

static IFX_NoiseGate    gate;
static IFX_TubeScreamer boost;
static IFX_Overdrive    od;
static IFX_Chorus       chorus;
static IFX_Delay        dly;

/* Shared between UI (core 0) and audio task (core 1). Single 32-bit
 * reads/writes are atomic on the ESP32-S3. */
static volatile bool  fxEnabled[FX_COUNT] = { false, false, false, false, false };
static volatile float odLevel     = 0.5f;
static volatile float chorusLevel = 1.0f;
static volatile float delayLevel  = 1.0f;

static float delayMaxMs = 400.0f; /* raised at startup if PSRAM is found */

/* ------------------------------------------------------------------ */
/* UI state                                                           */
/* ------------------------------------------------------------------ */

static const uint8_t fsPin[FX_COUNT]  = { PIN_FS_GATE, PIN_FS_BOOST, PIN_FS_OVERDRIVE, PIN_FS_CHORUS, PIN_FS_DELAY };
static const uint8_t potPin[4]        = { PIN_POT_1, PIN_POT_2, PIN_POT_3, PIN_POT_4 };

static const char *fxShortName[FX_COUNT] = { "GT", "BST", "OD", "CHO", "DLY" };
static const char *fxLongName[FX_COUNT]  = { "Noise Gate", "TS Boost", "Overdrive", "Chorus", "Delay" };

static const char *paramName[FX_COUNT][4] = {
  { "Thrs", "Attk", "Rels", "Hold" },   /* Gate      */
  { "Driv", "Tone", "Levl", "Tght" },   /* TS Boost  */
  { "Gain", "Voic", "Tone", "Levl" },   /* Overdrive */
  { "Rate", "Dpth", "Mix",  "Levl" },   /* Chorus    */
  { "Time", "Fdbk", "Mix",  "Levl" },   /* Delay     */
};

/* All parameters stored normalised 0..1, mapped in applyParam() */
static float paramVal[FX_COUNT][4] = {
  { 0.35f, 0.10f, 0.20f, 0.20f },
  { 0.50f, 0.60f, 0.50f, 0.30f },
  { 0.40f, 0.50f, 0.50f, 0.50f },
  { 0.25f, 0.50f, 0.50f, 0.70f },
  { 0.30f, 0.40f, 0.35f, 0.70f },
};

/* Gate attack/release are set through one call, so remember both */
static float gateAttackMs  = 2.0f;
static float gateReleaseMs = 20.0f;

static uint8_t editFx = FX_OVERDRIVE;  /* effect the pots are editing */

/* Footswitch state */
struct Button {
  bool     rawLast;
  uint32_t lastChangeMs;
  bool     pressed;
  uint32_t pressStartMs;
  bool     longFired;
};
static Button button[FX_COUNT];

/* Pot state */
static float potFiltered[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
static bool  potLocked[4]   = { true, true, true, true };
static float potLockRef[4]  = { 0.0f, 0.0f, 0.0f, 0.0f };

static bool displayDirty = true;

static U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

/* ------------------------------------------------------------------ */
/* Parameter mapping                                                  */
/* ------------------------------------------------------------------ */

static void applyParam(uint8_t fx, uint8_t idx, float v) {

  paramVal[fx][idx] = v;

  switch (fx) {

    case FX_GATE:
      switch (idx) {
        case 0: IFX_NoiseGate_SetThreshold(&gate, 0.1f * v * v); break; /* squared for fine control at low settings */
        case 1: gateAttackMs  = 0.5f + 19.5f * v;
                IFX_NoiseGate_SetAttackReleaseTime(&gate, gateAttackMs, gateReleaseMs, SAMPLE_RATE_HZ); break;
        case 2: gateReleaseMs = 1.0f + 99.0f * v;
                IFX_NoiseGate_SetAttackReleaseTime(&gate, gateAttackMs, gateReleaseMs, SAMPLE_RATE_HZ); break;
        case 3: gate.holdTimeS = 0.001f * (1.0f + 99.0f * v); break;
      }
      break;

    case FX_BOOST:
      switch (idx) {
        case 0: IFX_TubeScreamer_SetDrive(&boost, powf(30.0f, v));         break; /* 1 .. 30, exponential  */
        case 1: IFX_TubeScreamer_SetTone(&boost, 500.0f * powf(10.0f, v)); break; /* 500 Hz .. 5 kHz       */
        case 2: IFX_TubeScreamer_SetLevel(&boost, 2.0f * v);               break; /* 0 .. 2                */
        case 3: IFX_TubeScreamer_SetTight(&boost, 80.0f * powf(10.0f, v)); break; /* 80 Hz .. 800 Hz       */
      }
      break;

    case FX_OVERDRIVE:
      /* Same ranges as the original STM32 firmware */
      switch (idx) {
        case 0: IFX_Overdrive_SetGain(&od, 10.0f + 100.0f * v);            break;
        case 1: IFX_Overdrive_SetHPF(&od, 1000.0f - 900.0f * v);           break;
        case 2: IFX_Overdrive_SetLPF(&od, 500.0f + 16000.0f * v, 1.0f);    break;
        case 3: odLevel = v;                                               break;
      }
      break;

    case FX_CHORUS:
      switch (idx) {
        case 0: { float rate = 0.1f + 4.9f * v;
                  IFX_Chorus_SetRate(&chorus, rate, 1.17f * rate); }       break;
        case 1: { float depthMs = 0.5f + 4.5f * v;
                  IFX_Chorus_SetDepth(&chorus, depthMs, 0.8f * depthMs); } break;
        case 2: IFX_Chorus_SetMix(&chorus, v);                             break;
        case 3: chorusLevel = 1.5f * v;                                    break;
      }
      break;

    case FX_DELAY:
      switch (idx) {
        case 0: IFX_Delay_SetLength(&dly, 50.0f + (delayMaxMs - 50.0f) * v, SAMPLE_RATE_HZ); break;
        case 1: IFX_Delay_SetFeedback(&dly, 0.95f * v);                    break;
        case 2: IFX_Delay_SetMix(&dly, v);                                 break;
        case 3: delayLevel = 1.5f * v;                                     break;
      }
      break;
  }

}

static void applyAllParams(void) {
  for (uint8_t fx = 0; fx < FX_COUNT; fx++) {
    for (uint8_t p = 0; p < 4; p++) {
      applyParam(fx, p, paramVal[fx][p]);
    }
  }
}

/* ------------------------------------------------------------------ */
/* Audio task (core 1)                                                */
/* ------------------------------------------------------------------ */

static void audioTask(void *param) {

  static int32_t rxBuf[BLOCK_FRAMES * 2];
  static int32_t txBuf[BLOCK_FRAMES * 2];

  (void) param;

  for (;;) {

    AudioIO_Read(rxBuf, BLOCK_FRAMES);

    for (uint32_t n = 0; n < BLOCK_FRAMES; n++) {

      /* Guitar on the PCM1808 left channel */
      float x = AudioIO_SampleToFloat(rxBuf[2 * n]);

      if (fxEnabled[FX_GATE])      { x = IFX_NoiseGate_Update(&gate, x); }
      if (fxEnabled[FX_BOOST])     { x = IFX_TubeScreamer_Update(&boost, x); }
      if (fxEnabled[FX_OVERDRIVE]) { x = odLevel * IFX_Overdrive_Update(&od, x); }
      if (fxEnabled[FX_CHORUS])    { x = chorusLevel * IFX_Chorus_Update(&chorus, x); }
      if (fxEnabled[FX_DELAY])     { x = delayLevel * IFX_Delay_Update(&dly, x); }

      int32_t s = AudioIO_FloatToSample(x);

      /* Same signal on both DAC channels */
      txBuf[2 * n]     = s;
      txBuf[2 * n + 1] = s;

    }

    AudioIO_Write(txBuf, BLOCK_FRAMES);

  }

}

/* ------------------------------------------------------------------ */
/* Footswitches                                                       */
/* ------------------------------------------------------------------ */

static void selectFxForEdit(uint8_t fx) {

  editFx = fx;

  /* Lock all pots until they are moved, so parameters don't jump */
  for (uint8_t p = 0; p < 4; p++) {
    potLocked[p]  = true;
    potLockRef[p] = potFiltered[p];
  }

  displayDirty = true;

}

static void pollButtons(void) {

  uint32_t now = millis();

  for (uint8_t i = 0; i < FX_COUNT; i++) {

    Button *b = &button[i];
    bool raw = (digitalRead(fsPin[i]) == LOW);

    if (raw != b->rawLast) {
      b->rawLast      = raw;
      b->lastChangeMs = now;
    }

    if ((now - b->lastChangeMs) < BUTTON_DEBOUNCE_MS) {
      continue;
    }

    if (raw && !b->pressed) {
      /* Press started */
      b->pressed      = true;
      b->pressStartMs = now;
      b->longFired    = false;
    }

    if (raw && b->pressed && !b->longFired && (now - b->pressStartMs) >= BUTTON_LONGPRESS_MS) {
      /* Long press: select effect for editing */
      b->longFired = true;
      selectFxForEdit(i);
    }

    if (!raw && b->pressed) {
      /* Released: short press toggles the effect */
      b->pressed = false;
      if (!b->longFired) {
        fxEnabled[i] = !fxEnabled[i];
        displayDirty = true;
      }
    }

  }

}

/* ------------------------------------------------------------------ */
/* Pots                                                               */
/* ------------------------------------------------------------------ */

static void pollPots(void) {

  for (uint8_t p = 0; p < 4; p++) {

    float raw = (float) analogRead(potPin[p]) * (1.0f / 4095.0f);

    potFiltered[p] = POT_FILTER_ALPHA * potFiltered[p] + (1.0f - POT_FILTER_ALPHA) * raw;

    if (potLocked[p]) {
      /* Unlock once the pot has clearly moved */
      if (fabsf(potFiltered[p] - potLockRef[p]) > POT_PICKUP_THRESHOLD) {
        potLocked[p] = false;
      }
    }

    if (!potLocked[p]) {
      if (fabsf(potFiltered[p] - paramVal[editFx][p]) > 0.004f) {
        applyParam(editFx, p, potFiltered[p]);
        displayDirty = true;
      }
    }

  }

}

/* ------------------------------------------------------------------ */
/* Display                                                            */
/* ------------------------------------------------------------------ */

static void drawDisplay(void) {

  u8g2.clearBuffer();

  /* Top row: one cell per effect */
  u8g2.setFont(u8g2_font_5x8_tr);

  for (uint8_t i = 0; i < FX_COUNT; i++) {

    const int x = i * 26;
    const int w = 24;
    const int h = 13;

    if (fxEnabled[i]) {
      u8g2.drawRBox(x, 0, w, h, 2);
      u8g2.setDrawColor(0);
    } else {
      u8g2.drawRFrame(x, 0, w, h, 2);
    }

    int tw = u8g2.getStrWidth(fxShortName[i]);
    u8g2.drawStr(x + (w - tw) / 2, 10, fxShortName[i]);
    u8g2.setDrawColor(1);

    if (i == editFx) {
      u8g2.drawHLine(x + 2, 15, w - 4); /* marker under selected effect */
    }

  }

  /* Selected effect name */
  u8g2.setFont(u8g2_font_7x13B_tr);
  u8g2.drawStr(0, 31, fxLongName[editFx]);

  /* Parameters: 2 x 2 grid of labelled bars */
  u8g2.setFont(u8g2_font_5x8_tr);

  for (uint8_t p = 0; p < 4; p++) {

    const int x = (p % 2) * 64;
    const int y = 38 + (p / 2) * 13;
    const int barX = x + 26;
    const int barW = 34;

    u8g2.drawStr(x, y + 8, paramName[editFx][p]);
    u8g2.drawFrame(barX, y + 1, barW, 8);
    int fill = (int) (paramVal[editFx][p] * (float) (barW - 2));
    if (fill > 0) {
      u8g2.drawBox(barX + 1, y + 2, fill, 6);
    }

    if (potLocked[p]) {
      u8g2.drawPixel(barX + barW + 2, y + 5); /* dot = pot not picked up yet */
    }

  }

  u8g2.sendBuffer();

}

/* ------------------------------------------------------------------ */
/* Setup / loop                                                       */
/* ------------------------------------------------------------------ */

void setup() {

  Serial.begin(115200);

  /* Footswitches */
  for (uint8_t i = 0; i < FX_COUNT; i++) {
    pinMode(fsPin[i], INPUT_PULLUP);
    button[i] = { false, 0, false, 0, false };
  }

  /* Pots */
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  for (uint8_t p = 0; p < 4; p++) {
    potFiltered[p] = (float) analogRead(potPin[p]) * (1.0f / 4095.0f);
    potLockRef[p]  = potFiltered[p];
  }

  /* OLED */
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  u8g2.begin();
  u8g2.setBusClock(400000);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x13B_tr);
  u8g2.drawStr(14, 30, "InfiniFX  S3");
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(22, 44, "Multi-FX booting");
  u8g2.sendBuffer();

  /* Longer delay times when PSRAM is available */
  delayMaxMs = psramFound() ? 1500.0f : 400.0f;

  /* Initialise effects (parameter values applied below) */
  IFX_NoiseGate_Init(&gate, 0.02f, 2.0f, 20.0f, 20.0f, SAMPLE_RATE_HZ);
  IFX_TubeScreamer_Init(&boost, SAMPLE_RATE_HZ, 250.0f, 8.0f, 2000.0f, 1.0f);
  IFX_Overdrive_Init(&od, SAMPLE_RATE_HZ, 1000.0f, 10.0f, 500.0f, 1.0f);
  IFX_Chorus_Init(&chorus, 12.0f, 17.0f, 2.0f, 1.6f, 0.6f, 0.6f, 0.8f, 0.94f, 0.5f, SAMPLE_RATE_HZ);

  bool delayOk = IFX_Delay_Init(&dly, delayMaxMs, 300.0f, 0.35f, 0.4f, SAMPLE_RATE_HZ);

  applyAllParams();

  /* Audio I/O and processing task on core 1 */
  bool audioOk = AudioIO_Init();

  if (!audioOk || !delayOk) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_7x13B_tr);
    u8g2.drawStr(0, 30, audioOk ? "DELAY MEM FAIL" : "I2S INIT FAIL");
    u8g2.sendBuffer();
    Serial.println(audioOk ? "Delay buffer allocation failed" : "I2S init failed");
    for (;;) { delay(1000); }
  }

  xTaskCreatePinnedToCore(audioTask, "audio", 8192, NULL, configMAX_PRIORITIES - 2, NULL, 1);

  Serial.println("InfiniFX ESP32-S3 Multi-FX running");
  Serial.printf("PSRAM: %s, max delay: %.0f ms\n", psramFound() ? "yes" : "no", delayMaxMs);

  displayDirty = true;

}

void loop() {

  static uint32_t lastPotMs  = 0;
  static uint32_t lastDrawMs = 0;

  uint32_t now = millis();

  pollButtons();

  if ((now - lastPotMs) >= 10) {
    lastPotMs = now;
    pollPots();
  }

  if (displayDirty && (now - lastDrawMs) >= 60) {
    lastDrawMs   = now;
    displayDirty = false;
    drawDisplay();
  }

  delay(2);

}
