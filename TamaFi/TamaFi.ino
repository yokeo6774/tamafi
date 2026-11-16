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
      buffer[i * 2] = sample;
      buffer[i * 2 + 1] = sample;
    }
    codecI2S.write((uint8_t *)buffer, n * 2 * sizeof(int16_t));
    samplesLeft -= n;
  }

  if (freq != 0) {
    memset(buffer, 0, sizeof(buffer));
    codecI2S.write((uint8_t *)buffer, sizeof(buffer));
  }
}
#endif

// --------- Basic helpers ---------
bool buttonPressed(int pin, bool &lastState) {
  if (pin < 0) return false;
  bool s = digitalRead(pin);
  bool p = (lastState == HIGH && s == LOW);
  lastState = s;
  return p;
}

// -------------------- NeoPixel Core --------------------
void ledsOff() {
  if (LED_COUNT <= 0 || LED_PIN < 0) return;
  for (int i = 0; i < LED_COUNT; i++)
    leds.setPixelColor(i, 0);
  leds.show();                       // ALWAYS show after writing pixels
}

// Brightness must only change .setBrightness()
// Never rewrite pixels unless needed
void applyLedBrightness() {
  if (LED_COUNT <= 0 || LED_PIN < 0) return;
  uint8_t bri =
      (ledBrightnessIndex == 0) ? 20 :
      (ledBrightnessIndex == 1) ? 90 :
                                  180;

  leds.setBrightness(bri);

  if (!neoPixelsEnabled) {
    ledsOff();
    return;
  }

  leds.show();     // update brightness instantly
}

// -------------------- Color Helpers --------------------
static inline uint32_t C(uint8_t r, uint8_t g, uint8_t b) {
  return leds.Color(r, g, b);
}

// -------------------- Visual Effects --------------------
void ledsHappy() {
  if (LED_COUNT <= 0 || LED_PIN < 0) return;
  if (!neoPixelsEnabled) return;
  uint32_t col = C(120, 40, 200);    // soft violet
  for (int i = 0; i < LED_COUNT; i++) leds.setPixelColor(i, col);
  leds.show();
}

void ledsSad() {
  if (LED_COUNT <= 0 || LED_PIN < 0) return;
  if (!neoPixelsEnabled) return;
  uint32_t col = C(200, 0, 0);       // dim red (non-blinding)
  for (int i = 0; i < LED_COUNT; i++) leds.setPixelColor(i, col);
  leds.show();
}

void ledsWifi() {
  if (LED_COUNT <= 0 || LED_PIN < 0) return;
  if (!neoPixelsEnabled) return;
  uint32_t col = C(0, 90, 255);      // softer blue
  for (int i = 0; i < LED_COUNT; i++) leds.setPixelColor(i, col);
  leds.show();
}

void ledsRest() {
  if (LED_COUNT <= 0 || LED_PIN < 0) return;
  if (!neoPixelsEnabled) return;
  uint32_t col = C(0, 25, 90);       // calm deep-blue
  for (int i = 0; i < LED_COUNT; i++) leds.setPixelColor(i, col);
  leds.show();
}


// ---------- Buzzer core ----------
void stopBuzzerIfNeeded() {
  if (buzzerEndTime == 0) return;
  if (millis() > buzzerEndTime) {
    buzzerToneCompat(0);
    buzzerEndTime = 0;
  }
}

void buzzerPlay(int freq, int durMs) {
  if (!soundEnabled) return;
  buzzerToneCompat(freq);
  buzzerEndTime = millis() + durMs;
}

// ---------- Ultra-Retro sequencer ----------

struct RetroSound {
  const int *freqs;
  const int *times;
  int length;
};

int sndIndex = -1;
int sndStep  = 0;
unsigned long sndNext = 0;

const int CLICK_FREQS[] = { 2100, 1600, 900 };
const int CLICK_TIMES[] = {  20,   20,   20 };
RetroSound SND_CLICK = { CLICK_FREQS, CLICK_TIMES, 3 };

const int GOOD_FREQS[] = { 600, 900, 1200, 1500 };
const int GOOD_TIMES[] = { 40,  40,  40,   60   };
RetroSound SND_GOOD = { GOOD_FREQS, GOOD_TIMES, 4 };

const int BAD_FREQS[] = { 900, 700, 500, 300 };
const int BAD_TIMES[] = { 50,  50,  60,  80   };
RetroSound SND_BAD = { BAD_FREQS, BAD_TIMES, 4 };

const int DISC_FREQS[] = { 400, 650, 900, 1200, 1500 };
const int DISC_TIMES[] = { 40,  40,  40,  40,   60    };
RetroSound SND_DISC = { DISC_FREQS, DISC_TIMES, 5 };

const int REST_START_FREQS[] = { 600, 400, 300 };
const int REST_START_TIMES[] = { 60,  70,  90  };
RetroSound SND_REST_START = { REST_START_FREQS, REST_START_TIMES, 3 };

const int REST_END_FREQS[] = { 300, 500, 700 };
const int REST_END_TIMES[] = { 60,  60,  80  };
RetroSound SND_REST_END = { REST_END_FREQS, REST_END_TIMES, 3 };

const int HATCH_FREQS[] = { 500, 800, 1200, 1600, 2000 };
const int HATCH_TIMES[] = {  60,  60,   60,   80,  100 };
RetroSound SND_HATCH = { HATCH_FREQS, HATCH_TIMES, 5 };

void sndUpdate() {
  if (!soundEnabled) {
    buzzerToneCompat(0);
    sndIndex = -1;
    sndStep = 0;
    return;
  }

  if (sndIndex < 0) return;

  unsigned long now = millis();
  if (now >= sndNext) {
    const RetroSound *snd = nullptr;

    switch (sndIndex) {
      case 0: snd = &SND_CLICK;       break;
      case 1: snd = &SND_GOOD;        break;
      case 2: snd = &SND_BAD;         break;
      case 3: snd = &SND_DISC;        break;
      case 4: snd = &SND_REST_START;  break;
      case 5: snd = &SND_REST_END;    break;
      case 6: snd = &SND_HATCH;       break;
      default: sndIndex = -1; sndStep = 0; return;
    }

    if (sndStep >= snd->length) {
      buzzerToneCompat(0);
      sndIndex = -1;
      sndStep = 0;
      return;
    }

#if TAMAFI_SOUND_BACKEND == TAMAFI_SOUND_CODEC
    codecPlayToneBlocking(snd->freqs[sndStep], snd->times[sndStep]);
    sndNext = millis();
#else
    buzzerToneCompat(snd->freqs[sndStep]);
    sndNext = now + snd->times[sndStep];
#endif
    sndStep++;
  }
}

void sndStop() {
  buzzerToneCompat(0);
  buzzerEndTime = 0;
  sndIndex = -1;
  sndStep = 0;
}

// ---- public sound API ----
static void sndStart(int index) {
  if (!soundEnabled) return;
  sndIndex = index;
  sndStep = 0;
  sndNext = 0;
}

void sndClick()       { sndStart(0); }
void sndGoodFeed()    { sndStart(1); ledsHappy(); }
void sndBadFeed()     { sndStart(2); ledsSad(); }
void sndDiscover()    { sndStart(3); ledsWifi(); }
void sndRestStart()   { sndStart(4); }
void sndRestEnd()     { sndStart(5); }
void sndHatch()       { sndStart(6); }

// ---------- TFT brightness ----------
void applyTftBrightness() {
  if (TFT_BRIGHTNESS_PIN < 0) return;
  uint8_t val = (tftBrightnessIndex == 0) ? 60 :
                (tftBrightnessIndex == 1) ? 150 : 255;
  pwmWriteCompat(TFT_BRIGHTNESS_PIN, TFT_BRIGHTNESS_CH, val);
}

void tftBacklightOnEarly() {
  if (TFT_BRIGHTNESS_PIN < 0) return;
  pinMode(TFT_BRIGHTNESS_PIN, OUTPUT);
  digitalWrite(TFT_BRIGHTNESS_PIN, HIGH);
}

// ---------- WiFi scan ----------
void startWifiScan() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  WiFi.scanNetworks(true);
  wifiScanInProgress = true;
}

bool checkWifiScanDone() {
  if (!wifiScanInProgress) return false;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return false;

  wifiScanInProgress = false;
  lastWifiScanTime = millis();

  if (n < 0) {
    wifiStats = WifiStats();
    WiFi.scanDelete();
    return true;
  }

  WifiStats s;
  s.netCount    = n;
  s.strongCount = 0;
  s.hiddenCount = 0;
  s.openCount   = 0;
  s.wpaCount    = 0;
  int totalRSSI = 0;

  for (int i = 0; i < n; i++) {
    int rssi = WiFi.RSSI(i);
    totalRSSI += rssi;
    if (rssi > -60) s.strongCount++;
    if (WiFi.SSID(i).length() == 0) s.hiddenCount++;
    if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) s.openCount++;
    else s.wpaCount++;
  }

  s.avgRSSI = (n > 0) ? (totalRSSI / n) : -100;
  wifiStats = s;

  WiFi.scanDelete();
  return true;
}

// ---------- Persistence ----------
void saveState() {
  prefs.putInt("hunger", pet.hunger);
  prefs.putInt("happy",  pet.happiness);
  prefs.putInt("health", pet.health);
  
  prefs.putULong("ageMin", pet.ageMinutes);
  prefs.putULong("ageHr",  pet.ageHours);
  prefs.putULong("ageDay", pet.ageDays);

  prefs.putUChar("stage",  (uint8_t)petStage);
  prefs.putBool("hatched", hasHatchedOnce);

  prefs.putBool("sound", soundEnabled);
  prefs.putUChar("tftBri", tftBrightnessIndex);
  prefs.putUChar("ledBri", ledBrightnessIndex);
  prefs.putBool("neo", neoPixelsEnabled);

  prefs.putUChar("tCur", traitCuriosity);
  prefs.putUChar("tAct", traitActivity);
  prefs.putUChar("tStr", traitStress);
}

void loadState() {
  int h = prefs.getInt("hunger", -1);
  if (h == -1) {
    pet.hunger     = 70;
    pet.happiness  = 70;
    pet.health     = 70;
    pet.ageMinutes = 0;

    petStage       = STAGE_BABY;
    hasHatchedOnce = false;

    soundEnabled       = true;
    tftBrightnessIndex = 1;
    ledBrightnessIndex = 1;
    neoPixelsEnabled   = true;

    traitCuriosity = random(40, 90);
    traitActivity  = random(30, 90);
    traitStress    = random(20, 80);

    saveState();
    return;
  }

  pet.hunger     = prefs.getInt("hunger", 70);
  pet.happiness  = prefs.getInt("happy",  70);
  pet.health     = prefs.getInt("health", 70);
  
  pet.ageMinutes = prefs.getULong("ageMin", 0);
  pet.ageHours   = prefs.getULong("ageHr", 0);
  pet.ageDays    = prefs.getULong("ageDay", 0);


  petStage       = (Stage)prefs.getUChar("stage", (uint8_t)STAGE_BABY);
  hasHatchedOnce = prefs.getBool("hatched", false);

  soundEnabled       = prefs.getBool("sound", true);
  tftBrightnessIndex = prefs.getUChar("tftBri", 1);
  ledBrightnessIndex = prefs.getUChar("ledBri", 1);
  neoPixelsEnabled   = prefs.getBool("neo", true);

  traitCuriosity = prefs.getUChar("tCur", 70);
  traitActivity  = prefs.getUChar("tAct", 60);
  traitStress    = prefs.getUChar("tStr", 40);
}

// ---------- Mood & evolution ----------
void updateMood() {
  if (pet.health < 25 || (wifiStats.netCount == 0 && lastWifiScanTime > 0 &&
                          millis() - lastWifiScanTime > 60000)) {
    currentMood = MOOD_SICK;
    return;
  }

  if (pet.hunger < 25) {
    currentMood = MOOD_HUNGRY;
    return;
  }

  if (pet.happiness > 80 && wifiStats.netCount > 8) {
    currentMood = MOOD_EXCITED;
    return;
  }

  if (pet.happiness > 60 && wifiStats.netCount > 0) {
    currentMood = MOOD_HAPPY;
    return;
  }

  if (wifiStats.netCount == 0 && millis() - lastWifiScanTime > 30000) {
    currentMood = MOOD_BORED;
    return;
  }

  if (wifiStats.hiddenCount > 0 || wifiStats.openCount > 0) {
    currentMood = MOOD_CURIOUS;
    return;
  }

  currentMood = MOOD_CALM;
}

void updateEvolution() {
  unsigned long a = pet.ageMinutes;
  int avg = (pet.hunger + pet.happiness + pet.health) / 3;

  if (a >= 180 && avg > 40 && petStage < STAGE_ELDER) {
    petStage = STAGE_ELDER;
    sndDiscover();
  } else if (a >= 60 && avg > 45 && petStage < STAGE_ADULT) {
    petStage = STAGE_ADULT;
    sndDiscover();
  } else if (a >= 20 && avg > 35 && petStage < STAGE_TEEN) {
    petStage = STAGE_TEEN;
    sndDiscover();
  }
}

// ---------- Rest state machine ----------
void stepRest() {
  if (currentActivity != ACT_REST || restPhase == REST_NONE) return;

  unsigned long now = millis();

  switch (restPhase) {
    case REST_ENTER:
      // Going to sleep: egg_hatch_5 -> 4 -> 3 -> 2 -> 1
      if (now - lastRestAnimTime >= REST_ENTER_DELAY) {
        lastRestAnimTime = now;

        if (restFrameIndex > 0) {
          restFrameIndex--;          // 4,3,2,1,0
        } else {
          // reached egg_hatch_1 (index 0) → deep sleep
          restFrameIndex   = 0;
          restPhase        = REST_DEEP;
          restPhaseStart   = now;
          restStatsApplied = false;
        }
      }
      break;

    case REST_DEEP:
      // Stay on egg_hatch_1 while sleeping
      // (restFrameIndex is kept at 0)
      if (neoPixelsEnabled) {
        float phase = (now - restPhaseStart) / (float)REST_BREATHE_MS;
        int breathe = (int)(sin(phase) * 40.0f + 60.0f);
        breathe = constrain(breathe, 0, 255);
        for (int i = 0; i < LED_COUNT; i++)
          leds.setPixelColor(i, leds.Color(0, 0, breathe));
        leds.show();
      }

      if (!restStatsApplied && now - restPhaseStart > restDurationMs / 2) {
        int hungerDelta    = -3;
        int happinessDelta = +10;
        int healthDelta    = +15;

        pet.hunger    = constrain(pet.hunger + hungerDelta, 0, 100);
        pet.happiness = constrain(pet.happiness + happinessDelta, 0, 100);
        pet.health    = constrain(pet.health + healthDelta, 0, 100);
        restStatsApplied = true;
      }

      if (now - restPhaseStart >= restDurationMs) {
        // time to wake up
        restPhase        = REST_WAKE;
        restPhaseStart   = now;
        lastRestAnimTime = now;
        sndRestEnd();
        ledsOff();
        restFrameIndex = 0;           // start from egg_hatch_1
      }
      break;

    case REST_WAKE:
      // Waking: egg_hatch_1 -> 2 -> 3 -> 4 -> 5
      if (now - lastRestAnimTime >= REST_WAKE_DELAY) {
        lastRestAnimTime = now;

        if (restFrameIndex < 4) {
          restFrameIndex++;           // 0,1,2,3,4
        } else {
          // done, back to idle
          restFrameIndex   = 4;
          restPhase        = REST_NONE;
          currentActivity  = ACT_NONE;
        }
      }
      break;

    default:
      break;
  }
}



// ---------- WiFi-based activities ----------
void resolveHunt() {
  int n = wifiStats.netCount;
  int hungerDelta = 0;
  int happyDelta  = 0;
