/*
 * RP2350 Multi-Effect Guitar Unit
 *
 * Hardware:
 * - RP2350 (Raspberry Pi Pico 2)
 * - PCM1808 ADC (I2S audio input)
 * - PCM5102 DAC (I2S audio output)
 * - 0.93" OLED Display (I2C)
 * - Rotary Encoder with Push Button
 *
 * Effects Chain:
 * - Noise Gate
 * - Overdrive
 * - Chorus
 * - Delay
 *
 * Uses Arduino Audio Tools Framework
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <AudioTools.h>
#include <AudioLibs/I2SCodecStream.h>

#include "src/effects/IFX_NoiseGate.h"
#include "src/effects/IFX_Overdrive.h"
#include "src/effects/IFX_Chorus.h"
#include "src/effects/IFX_Delay.h"
#include "src/ui/MenuSystem.h"
#include "src/ui/RotaryEncoder.h"

// Display configuration (0.93" OLED - 128x64)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Rotary encoder pins
#define ENCODER_CLK 6
#define ENCODER_DT 7
#define ENCODER_SW 8

// I2C pins for OLED
#define I2C_SDA 4
#define I2C_SCL 5

// I2S Audio configuration
#define SAMPLE_RATE 48000
#define CHANNELS 2
#define BITS_PER_SAMPLE 16

// Audio buffer size
#define BUFFER_SIZE 256

// Global objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RotaryEncoderUI encoder(ENCODER_CLK, ENCODER_DT, ENCODER_SW);
MenuSystem menu(&display, &encoder);

// Audio Tools objects
I2SCodecStream i2s;
StreamCopy copier;

// Effect instances
IFX_NoiseGate noiseGate;
IFX_Overdrive overdrive;
IFX_Chorus chorus;
IFX_Delay delayFX;

// Effect chain state
struct EffectState {
  bool enabled;
  const char* name;
};

EffectState effects[4] = {
  {true, "NoiseGate"},
  {false, "Overdrive"},
  {false, "Chorus"},
  {false, "Delay"}
};

// Audio buffers
float audioBufferL[BUFFER_SIZE];
float audioBufferR[BUFFER_SIZE];
int16_t inputBuffer[BUFFER_SIZE * 2];
int16_t outputBuffer[BUFFER_SIZE * 2];

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("RP2350 Multi-Effect Unit Starting...");

  // Initialize I2C for display
  Wire.setSDA(I2C_SDA);
  Wire.setSCL(I2C_SCL);
  Wire.begin();

  // Initialize OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("RP2350 Multi-FX");
  display.println("Initializing...");
  display.display();

  // Initialize rotary encoder
  encoder.begin();

  // Initialize I2S audio
  auto config = i2s.defaultConfig(RXTX_MODE);
  config.sample_rate = SAMPLE_RATE;
  config.channels = CHANNELS;
  config.bits_per_sample = BITS_PER_SAMPLE;

  // PCM1808/PCM5102 I2S pins for RP2350
  config.pin_bck = 20;    // Bit Clock
  config.pin_ws = 21;     // Word Select (LRCLK)
  config.pin_data = 22;   // Data Out to DAC
  config.pin_data_rx = 26; // Data In from ADC

  i2s.begin(config);

  // Initialize effects
  IFX_NoiseGate_Init(&noiseGate, SAMPLE_RATE);
  IFX_NoiseGate_SetThreshold(&noiseGate, -40.0f); // -40dB threshold
  IFX_NoiseGate_SetAttackTime(&noiseGate, 1.0f);  // 1ms attack
  IFX_NoiseGate_SetReleaseTime(&noiseGate, 100.0f); // 100ms release

  IFX_Overdrive_Init(&overdrive, SAMPLE_RATE);
  IFX_Overdrive_SetInputHPF(&overdrive, 150.0f);
  IFX_Overdrive_SetPreGain(&overdrive, 175);
  IFX_Overdrive_SetInputLPF(&overdrive, 5000.0f);

  IFX_Chorus_Init(&chorus, SAMPLE_RATE);
  IFX_Chorus_SetRateA(&chorus, 1.5f);
  IFX_Chorus_SetDepthA(&chorus, 0.5f);
  IFX_Chorus_SetMix(&chorus, 0.5f);

  IFX_Delay_Init(&delayFX, SAMPLE_RATE);
  IFX_Delay_SetDelayTime(&delayFX, 250.0f); // 250ms delay
  IFX_Delay_SetFeedback(&delayFX, 0.4f);
  IFX_Delay_SetMix(&delayFX, 0.3f);

  // Initialize menu system
  menu.begin();
  menu.addEffect("NoiseGate", &effects[0].enabled);
  menu.addEffect("Overdrive", &effects[1].enabled);
  menu.addEffect("Chorus", &effects[2].enabled);
  menu.addEffect("Delay", &effects[3].enabled);

  // Add parameters for each effect
  menu.addParameter("NG:Threshold", -60.0f, 0.0f, -40.0f);
  menu.addParameter("NG:Release", 10.0f, 500.0f, 100.0f);

  menu.addParameter("OD:PreGain", 0.0f, 255.0f, 175.0f);
  menu.addParameter("OD:Tone", 1000.0f, 8000.0f, 5000.0f);

  menu.addParameter("CH:Rate", 0.1f, 5.0f, 1.5f);
  menu.addParameter("CH:Depth", 0.0f, 1.0f, 0.5f);
  menu.addParameter("CH:Mix", 0.0f, 1.0f, 0.5f);

  menu.addParameter("DL:Time", 10.0f, 1000.0f, 250.0f);
  menu.addParameter("DL:Feedback", 0.0f, 0.95f, 0.4f);
  menu.addParameter("DL:Mix", 0.0f, 1.0f, 0.3f);

  Serial.println("Initialization complete!");

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Ready!");
  display.display();
  delay(500);
}

void loop() {
  // Update UI
  menu.update();

  // Update effect parameters from menu
  updateEffectParameters();

  // Process audio
  processAudio();

  // Update display periodically (not every loop to save CPU)
  static uint32_t lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate > 50) { // 20Hz update rate
    menu.draw();
    lastDisplayUpdate = millis();
  }
}

void updateEffectParameters() {
  // Update Noise Gate parameters
  IFX_NoiseGate_SetThreshold(&noiseGate, menu.getParameterValue("NG:Threshold"));
  IFX_NoiseGate_SetReleaseTime(&noiseGate, menu.getParameterValue("NG:Release"));

  // Update Overdrive parameters
  IFX_Overdrive_SetPreGain(&overdrive, menu.getParameterValue("OD:PreGain"));
  IFX_Overdrive_SetInputLPF(&overdrive, menu.getParameterValue("OD:Tone"));

  // Update Chorus parameters
  IFX_Chorus_SetRateA(&chorus, menu.getParameterValue("CH:Rate"));
  IFX_Chorus_SetDepthA(&chorus, menu.getParameterValue("CH:Depth"));
  IFX_Chorus_SetMix(&chorus, menu.getParameterValue("CH:Mix"));

  // Update Delay parameters
  IFX_Delay_SetDelayTime(&delayFX, menu.getParameterValue("DL:Time"));
  IFX_Delay_SetFeedback(&delayFX, menu.getParameterValue("DL:Feedback"));
  IFX_Delay_SetMix(&delayFX, menu.getParameterValue("DL:Mix"));
}

void processAudio() {
  // Read audio input from I2S
  size_t bytesRead = i2s.readBytes((uint8_t*)inputBuffer, BUFFER_SIZE * 2 * sizeof(int16_t));

  if (bytesRead == 0) {
    return; // No data available
  }

  size_t samplesRead = bytesRead / (sizeof(int16_t) * 2);

  // Deinterleave and convert to float
  for (size_t i = 0; i < samplesRead; i++) {
    audioBufferL[i] = (float)inputBuffer[i * 2] / 32768.0f;
    audioBufferR[i] = (float)inputBuffer[i * 2 + 1] / 32768.0f;
  }

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

    // Chorus (stereo effect - only left channel here)
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

    // Chorus (stereo effect - right channel)
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
