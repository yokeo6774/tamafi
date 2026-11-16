#include <Arduino.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>
#include <Wire.h>
#include <math.h>

#include "hardware_profile.h"
#include "display_backend.h"

#if TAMAFI_SOUND_BACKEND == TAMAFI_SOUND_CODEC
#include <ESP_I2S.h>
#endif

#include "ui.h"
#include "ui_anim.h"

// Graphics
#include "StoneGolem.h"
#include "egg_hatch.h"
#include "effect.h"
#include "background.h"

// --- Hatching state ---
bool hasHatchedOnce = false;
bool hatchTriggered = false;

// --------- Hardware pins ---------
#define BTN_UP    TAMAFI_BTN_UP
#define BTN_OK    TAMAFI_BTN_OK
#define BTN_DOWN  TAMAFI_BTN_DOWN

#define BTN_RIGHT1 TAMAFI_BTN_RIGHT1
#define BTN_RIGHT2 TAMAFI_BTN_RIGHT2
#define BTN_RIGHT3 TAMAFI_BTN_RIGHT3

#define LED_PIN    TAMAFI_LED_PIN
#define LED_COUNT  TAMAFI_LED_COUNT

#define BUZZER_PIN    TAMAFI_BUZZER_PIN
#define BUZZER_CH     5

#define TFT_BRIGHTNESS_PIN TAMAFI_TFT_BRIGHTNESS_PIN
#define TFT_BRIGHTNESS_CH  0

// TFT sizes
#define TFT_W TAMAFI_UI_WIDTH
#define TFT_H TAMAFI_UI_HEIGHT
#define PET_W 115
#define PET_H 110
#define EFFECT_W 100
#define EFFECT_H 95

// --------- Global objects ---------
TamaFiDisplay tft;
TamaFiSprite fb(&tft);
TamaFiSprite petSprite(&tft);
TamaFiSprite effectSprite(&tft);

Adafruit_NeoPixel leds(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
Preferences prefs;

#if TAMAFI_SOUND_BACKEND == TAMAFI_SOUND_CODEC
I2SClass codecI2S;
static bool codecReady = false;
static uint32_t codecPhase = 0;
static const uint32_t CODEC_SAMPLE_RATE = 16000;
static const uint8_t ES8311_ADDR = 0x18;
#endif

// --------- Shared game state (matching ui.h externs) ---------
Screen    currentScreen = SCREEN_BOOT;
Activity  currentActivity = ACT_NONE;
RestPhase restPhase = REST_NONE;

Pet       pet;
WifiStats wifiStats;

Mood      currentMood = MOOD_CALM;
Stage     petStage    = STAGE_BABY;

bool      hungerEffectActive = false;
int       hungerEffectFrame  = 0;

bool      wifiScanInProgress = false;
unsigned long lastWifiScanTime = 0;
unsigned long lastSaveTime     = 0;

bool      soundEnabled     = true;
bool      neoPixelsEnabled = true;
uint8_t   tftBrightnessIndex = 1;
uint8_t   ledBrightnessIndex = 1;
bool      autoSleep          = true;
uint16_t  autoSaveMs         = 30000;

uint8_t   traitCuriosity = 70;
uint8_t   traitActivity  = 60;
uint8_t   traitStress    = 40;

unsigned long lastDecisionTime      = 0;
uint32_t      currentDecisionInterval = 10000;

int       restFrameIndex = 0;
int       petPosX = 120;
int       petPosY = 90;

// --------- Internal logic timers ---------
unsigned long hungerTimer    = 0;
unsigned long happinessTimer = 0;
unsigned long healthTimer    = 0;
unsigned long ageTimer       = 0;
unsigned long lastLogicTick  = 0;

// Rest
unsigned long lastRestAnimTime = 0;
unsigned long restPhaseStart   = 0;
unsigned long restDurationMs   = 0;
bool          restStatsApplied = false;

// Hunger overlay
unsigned long lastHungerFrameTime = 0;

// Death
unsigned long lastDeadFrameTime = 0;

// Buttons (edge)
bool lastUp   = HIGH;
bool lastOk   = HIGH;
bool lastDown = HIGH;

bool lastR1 = HIGH;
bool lastR2 = HIGH;
bool lastR3 = HIGH;

// Menus
int mainMenuIndex     = 0;
int controlsIndex     = 0;
int settingsMenuIndex = 0;

// Buzzer
unsigned long buzzerEndTime = 0;

// Wifi decision randomness
const uint32_t DECISION_INTERVAL_MIN = 8000;
const uint32_t DECISION_INTERVAL_MAX = 15000;

// ------- Forward declarations -------
void sndClick();
void sndGoodFeed();
void sndBadFeed();
void sndDiscover();
void sndRestStart();
void sndRestEnd();
void sndStop();

// --------- PWM compatibility helpers ---------
static void pwmAttachPinCompat(int pin, int channel, uint32_t freq, uint8_t resolution) {
  if (pin < 0) return;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttachChannel(pin, freq, resolution, channel);
#else
  ledcSetup(channel, freq, resolution);
  ledcAttachPin(pin, channel);
#endif
}

static void pwmWriteCompat(int pin, int channel, uint32_t duty) {
  if (pin < 0) return;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pin, duty);
#else
  ledcWrite(channel, duty);
#endif
}

static void buzzerToneCompat(uint32_t freq) {
#if TAMAFI_SOUND_BACKEND == TAMAFI_SOUND_BUZZER
  if (BUZZER_PIN < 0) return;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWriteTone(BUZZER_PIN, freq);
#else
  ledcWriteTone(BUZZER_CH, freq);
#endif
#elif TAMAFI_SOUND_BACKEND == TAMAFI_SOUND_CODEC
  (void)freq;
#else
  (void)freq;
#endif
}

#if TAMAFI_SOUND_BACKEND == TAMAFI_SOUND_CODEC
static bool codecWriteReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

static bool codecReadReg(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((int)ES8311_ADDR, 1) != 1) return false;
  value = Wire.read();
  return true;
}

static void codecSetMuted(bool muted) {
  if (!codecReady) return;
  uint8_t reg31 = 0;
  codecReadReg(0x31, reg31);
  if (muted) reg31 |= 0x60;
  else reg31 &= ~0x60;
  codecWriteReg(0x31, reg31);
}

static bool codecBegin() {
  if (codecReady) return true;

  if (TAMAFI_AUDIO_EN_PIN >= 0) {
    pinMode(TAMAFI_AUDIO_EN_PIN, OUTPUT);
    digitalWrite(TAMAFI_AUDIO_EN_PIN, LOW);
    delay(10);
  }

  codecI2S.setPins(
      TAMAFI_I2S_BCLK_PIN,
      TAMAFI_I2S_LRCK_PIN,
      TAMAFI_I2S_DOUT_PIN,
      -1,
      TAMAFI_I2S_MCLK_PIN);
  if (!codecI2S.begin(I2S_MODE_STD, CODEC_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    return false;
  }

  codecWriteReg(0x00, 0x1F);
  delay(20);
  codecWriteReg(0x00, 0x00);
  codecWriteReg(0x00, 0x80);

  codecWriteReg(0x01, 0x3F);
  codecWriteReg(0x02, 0x00);
  codecWriteReg(0x03, 0x10);
  codecWriteReg(0x04, 0x10);
  codecWriteReg(0x05, 0x00);
  codecWriteReg(0x06, 0x03);
  codecWriteReg(0x07, 0x00);
  codecWriteReg(0x08, 0xFF);

  uint8_t reg00 = 0x80;
  codecReadReg(0x00, reg00);
  codecWriteReg(0x00, reg00 & 0xBF);
  codecWriteReg(0x09, 0x0C);
  codecWriteReg(0x0A, 0x0C);

  codecWriteReg(0x0D, 0x01);
  codecWriteReg(0x0E, 0x02);
  codecWriteReg(0x12, 0x00);
  codecWriteReg(0x13, 0x10);
  codecWriteReg(0x1C, 0x6A);
  codecWriteReg(0x37, 0x08);
  codecWriteReg(0x32, 0xDF);

  if (TAMAFI_AUDIO_EN_PIN >= 0) {
    digitalWrite(TAMAFI_AUDIO_EN_PIN, HIGH);
  }

  codecReady = true;
  codecSetMuted(false);
  return true;
}

static void codecPlayToneBlocking(uint32_t freq, uint16_t durationMs) {
  if (!codecBegin()) return;

  const uint16_t amplitude = (freq == 0) ? 0 : 8000;
  uint32_t phaseInc = (freq == 0) ? 0 : (uint32_t)(((uint64_t)freq << 32) / CODEC_SAMPLE_RATE);
  uint32_t samplesLeft = (CODEC_SAMPLE_RATE * (uint32_t)durationMs) / 1000;
  int16_t buffer[64 * 2];

  codecSetMuted(freq == 0);

  while (samplesLeft > 0) {
    uint32_t n = samplesLeft > 64 ? 64 : samplesLeft;
    for (uint32_t i = 0; i < n; i++) {
      int16_t sample = amplitude == 0 ? 0 : ((codecPhase & 0x80000000UL) ? amplitude : -amplitude);
      codecPhase += phaseInc;
      buffer[i *
