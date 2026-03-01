/*
 * RP2350 Multi-Effect Guitar Unit - Professional LCD Version
 *
 * Hardware:
 * - RP2350 (Raspberry Pi Pico 2)
 * - PCM1808 ADC (I2S audio input)
 * - PCM5102 DAC (I2S audio output)
 * - 128x64 LCD Module (ST7567S COG, I2C)
 * - Rotary Encoder with Push Button (KY-040 or similar)
 *
 * Features:
 * - Professional multi-page UI with graphics
 * - Real-time VU meters
 * - Visual effect chain display
 * - Parameter bar graphs and sliders
 * - CPU usage monitoring
 * - Smooth animations
 *
 * Effects Chain:
 * - Noise Gate
 * - Overdrive
 * - Chorus
 * - Delay
 *
 * Uses Arduino Audio Tools Framework and U8g2 Graphics Library
 */

#include <Wire.h>
#include <U8g2lib.h>
#include <AudioTools.h>
#include <AudioLibs/I2SCodecStream.h>

#include "src/effects/IFX_NoiseGate.h"
#include "src/effects/IFX_Overdrive.h"
#include "src/effects/IFX_Chorus.h"
#include "src/effects/IFX_Delay.h"
#include "src/ui/ProfessionalUI.h"
#include "src/ui/RotaryEncoder.h"
#include "src/ui/VUMeter.h"

// Display configuration (ST7567S LCD - 128x64)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define LCD_I2C_ADDRESS 0x3F  // Common address for ST7567S, try 0x3C if not working

// Pin configuration
#define I2C_SDA 4
#define I2C_SCL 5
#define ENCODER_CLK 6       // Rotary encoder A
#define ENCODER_DT 7        // Rotary encoder B
#define ENCODER_SW 8        // Rotary encoder push button

// I2S Audio configuration
#define SAMPLE_RATE 48000
#define CHANNELS 2
#define BITS_PER_SAMPLE 16

// Audio buffer size
#define BUFFER_SIZE 256

// Global objects - U8g2 for ST7567S display
U8G2_ST7567_ENH_DG128064_F_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

RotaryEncoderUI encoder(ENCODER_CLK, ENCODER_DT, ENCODER_SW);
ProfessionalUI ui(&display, &encoder);
VUMeter vuMeter;

// Audio Tools objects
I2SCodecStream i2s;

// Effect instances
IFX_NoiseGate noiseGate;
IFX_Overdrive overdrive;
IFX_Chorus chorus;
IFX_Delay delayFX;

// Effect chain state
struct EffectState {
  bool enabled;
  const char* name;
  const char* shortName;
};

EffectState effects[4] = {
  {true, "Noise Gate", "GATE"},
  {false, "Overdrive", "DRIVE"},
  {false, "Chorus", "CHOR"},
  {false, "Delay", "DLAY"}
};

// Audio buffers
float audioBufferL[BUFFER_SIZE];
float audioBufferR[BUFFER_SIZE];
int16_t inputBuffer[BUFFER_SIZE * 2];
int16_t outputBuffer[BUFFER_SIZE * 2];

// Performance monitoring
uint32_t audioProcessTime = 0;
uint32_t maxProcessTime = 0;
float cpuLoad = 0.0f;

// VU meter levels
float peakLevelL = 0.0f;
float peakLevelR = 0.0f;
float rmsLevelL = 0.0f;
float rmsLevelR = 0.0f;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("RP2350 Multi-Effect Unit (Professional LCD) Starting...");

  // Initialize I2C for display
  Wire.setSDA(I2C_SDA);
  Wire.setSCL(I2C_SCL);
  Wire.begin();
  Wire.setClock(400000); // 400kHz I2C for faster updates

  // Initialize U8g2 display (ST7567S)
  display.begin();
  display.setContrast(128); // Adjust contrast (0-255)

  // Show splash screen
  display.clearBuffer();
  display.setFont(u8g2_font_helvB12_tr);
  display.drawStr(8, 20, "RP2350");
  display.setFont(u8g2_font_7x14_tr);
  display.drawStr(10, 38, "Multi-Effect");
  display.drawStr(30, 54, "Guitar DSP");
  display.sendBuffer();
  delay(1500);

  // Initialize rotary encoder
  encoder.begin();

  // Initialize I2S audio
  auto config = i2s.defaultConfig(RXTX_MODE);
  config.sample_rate = SAMPLE_RATE;
  config.channels = CHANNELS;
  config.bits_per_sample = BITS_PER_SAMPLE;

  // PCM1808/PCM5102 I2S pins for RP2350
  config.pin_bck = 20;       // Bit Clock
  config.pin_ws = 21;        // Word Select (LRCLK)
  config.pin_data = 22;      // Data Out to DAC
  config.pin_data_rx = 26;   // Data In from ADC

  i2s.begin(config);

  // Initialize effects
  IFX_NoiseGate_Init(&noiseGate, SAMPLE_RATE);
  IFX_NoiseGate_SetThreshold(&noiseGate, -40.0f);
  IFX_NoiseGate_SetAttackTime(&noiseGate, 1.0f);
  IFX_NoiseGate_SetReleaseTime(&noiseGate, 100.0f);

  IFX_Overdrive_Init(&overdrive, SAMPLE_RATE);
  IFX_Overdrive_SetInputHPF(&overdrive, 150.0f);
  IFX_Overdrive_SetPreGain(&overdrive, 175);
  IFX_Overdrive_SetInputLPF(&overdrive, 5000.0f);

  IFX_Chorus_Init(&chorus, SAMPLE_RATE);
  IFX_Chorus_SetRateA(&chorus, 1.5f);
  IFX_Chorus_SetDepthA(&chorus, 0.5f);
  IFX_Chorus_SetMix(&chorus, 0.5f);

  IFX_Delay_Init(&delayFX, SAMPLE_RATE);
  IFX_Delay_SetDelayTime(&delayFX, 250.0f);
  IFX_Delay_SetFeedback(&delayFX, 0.4f);
  IFX_Delay_SetMix(&delayFX, 0.3f);

  // Initialize VU meter
  vuMeter.begin(SAMPLE_RATE);

  // Initialize Professional UI
  ui.begin();

  // Register effects with UI
  ui.addEffect("Noise Gate", "GATE", &effects[0].enabled);
  ui.addEffect("Overdrive", "DRIVE", &effects[1].enabled);
  ui.addEffect("Chorus", "CHOR", &effects[2].enabled);
  ui.addEffect("Delay", "DLAY", &effects[3].enabled);

  // Add parameters for each effect
  ui.addParameter(0, "Threshold", -60.0f, 0.0f, -40.0f, "dB");
  ui.addParameter(0, "Release", 10.0f, 500.0f, 100.0f, "ms");

  ui.addParameter(1, "PreGain", 0.0f, 255.0f, 175.0f, "");
  ui.addParameter(1, "Tone", 1000.0f, 8000.0f, 5000.0f, "Hz");

  ui.addParameter(2, "Rate", 0.1f, 5.0f, 1.5f, "Hz");
  ui.addParameter(2, "Depth", 0.0f, 1.0f, 0.5f, "%");
  ui.addParameter(2, "Mix", 0.0f, 1.0f, 0.5f, "%");

  ui.addParameter(3, "Time", 10.0f, 1000.0f, 250.0f, "ms");
  ui.addParameter(3, "Feedback", 0.0f, 0.95f, 0.4f, "%");
  ui.addParameter(3, "Mix", 0.0f, 1.0f, 0.3f, "%");

  Serial.println("Initialization complete!");

  // Final ready screen
  display.clearBuffer();
  display.setFont(u8g2_font_9x15_tr);
  display.drawStr(35, 32, "Ready!");
  display.sendBuffer();
  delay(500);
}

void loop() {
  // Update UI
  ui.update();

  // Update effect parameters from UI
  updateEffectParameters();

  // Process audio
  uint32_t startTime = micros();
  processAudio();
  uint32_t endTime = micros();

  // Calculate CPU load
  audioProcessTime = endTime - startTime;
  if (audioProcessTime > maxProcessTime) {
    maxProcessTime = audioProcessTime;
  }

  // Calculate percentage (buffer time at 48kHz, 256 samples = 5333us)
  cpuLoad = (float)audioProcessTime / 5333.0f * 100.0f;

  // Update UI with performance data
  ui.setCPULoad(cpuLoad);
  ui.setVULevels(peakLevelL, peakLevelR, rmsLevelL, rmsLevelR);

  // Update display periodically (not every loop to save CPU)
  static uint32_t lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate > 33) { // 30Hz update rate
    ui.draw();
    lastDisplayUpdate = millis();
  }
}

void updateEffectParameters() {
  // Get parameters from UI
  float* params;

  // Noise Gate (effect 0)
  params = ui.getEffectParameters(0);
  if (params != nullptr) {
    IFX_NoiseGate_SetThreshold(&noiseGate, params[0]);
    IFX_NoiseGate_SetReleaseTime(&noiseGate, params[1]);
  }

  // Overdrive (effect 1)
  params = ui.getEffectParameters(1);
  if (params != nullptr) {
    IFX_Overdrive_SetPreGain(&overdrive, params[0]);
    IFX_Overdrive_SetInputLPF(&overdrive, params[1]);
  }

  // Chorus (effect 2)
  params = ui.getEffectParameters(2);
  if (params != nullptr) {
    IFX_Chorus_SetRateA(&chorus, params[0]);
    IFX_Chorus_SetDepthA(&chorus, params[1]);
    IFX_Chorus_SetMix(&chorus, params[2]);
  }

  // Delay (effect 3)
  params = ui.getEffectParameters(3);
  if (params != nullptr) {
    IFX_Delay_SetDelayTime(&delayFX, params[0]);
    IFX_Delay_SetFeedback(&delayFX, params[1]);
    IFX_Delay_SetMix(&delayFX, params[2]);
  }
}

void processAudio() {
  // Read audio input from I2S
  size_t bytesRead = i2s.readBytes((uint8_t*)inputBuffer, BUFFER_SIZE * 2 * sizeof(int16_t));

  if (bytesRead == 0) {
    return; // No data available
  }

  size_t samplesRead = bytesRead / (sizeof(int16_t) * 2);

  // Reset peak/RMS accumulators
  peakLevelL = 0.0f;
  peakLevelR = 0.0f;
  float sumSqL = 0.0f;
  float sumSqR = 0.0f;

  // Deinterleave and convert to float
  for (size_t i = 0; i < samplesRead; i++) {
    audioBufferL[i] = (float)inputBuffer[i * 2] / 32768.0f;
    audioBufferR[i] = (float)inputBuffer[i * 2 + 1] / 32768.0f;

    // Track input levels for VU meter
    float absL = abs(audioBufferL[i]);
    float absR = abs(audioBufferR[i]);
    if (absL > peakLevelL) peakLevelL = absL;
    if (absR > peakLevelR) peakLevelR = absR;
    sumSqL += audioBufferL[i] * audioBufferL[i];
    sumSqR += audioBufferR[i] * audioBufferR[i];
  }

  // Calculate RMS levels
  rmsLevelL = sqrt(sumSqL / samplesRead);
  rmsLevelR = sqrt(sumSqR / samplesRead);

  // Process effect chain - LEFT CHANNEL
  for (size_t i = 0; i < samplesRead; i++) {
    float sample = audioBufferL[i];

    // Noise Gate
    if (effects[0].enabled) {
      sample = IFX_NoiseGate_Process(&noiseGate, sample);
    }

    // Overdrive
    if (effects[1].enabled) {
      sample = IFX_Overdrive_Process(&overdrive, sample);
    }

    // Chorus
    if (effects[2].enabled) {
      sample = IFX_Chorus_ProcessA(&chorus, sample);
    }

    // Delay
    if (effects[3].enabled) {
      sample = IFX_Delay_Process(&delayFX, sample);
    }

    audioBufferL[i] = sample;
  }

  // Process effect chain - RIGHT CHANNEL
  for (size_t i = 0; i < samplesRead; i++) {
    float sample = audioBufferR[i];

    // Noise Gate
    if (effects[0].enabled) {
      sample = IFX_NoiseGate_Process(&noiseGate, sample);
    }

    // Overdrive
    if (effects[1].enabled) {
      sample = IFX_Overdrive_Process(&overdrive, sample);
    }

    // Chorus
    if (effects[2].enabled) {
      sample = IFX_Chorus_ProcessB(&chorus, sample);
    }

    // Delay
    if (effects[3].enabled) {
      sample = IFX_Delay_Process(&delayFX, sample);
    }

    audioBufferR[i] = sample;
  }

  // Interleave and convert back to int16
  for (size_t i = 0; i < samplesRead; i++) {
    // Clamp and convert
    float sampleL = audioBufferL[i];
    float sampleR = audioBufferR[i];

    if (sampleL > 1.0f) sampleL = 1.0f;
    if (sampleL < -1.0f) sampleL = -1.0f;
    if (sampleR > 1.0f) sampleR = 1.0f;
    if (sampleR < -1.0f) sampleR = -1.0f;

    outputBuffer[i * 2] = (int16_t)(sampleL * 32767.0f);
    outputBuffer[i * 2 + 1] = (int16_t)(sampleR * 32767.0f);
  }

  // Write to I2S output
  i2s.write((uint8_t*)outputBuffer, samplesRead * 2 * sizeof(int16_t));
}
