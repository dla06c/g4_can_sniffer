#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>

const int NEOPIXEL_PIN = 13;
const uint16_t NEOPIXEL_COUNT = 200;
const unsigned long LIGHTING_SAVE_DEBOUNCE_MS = 500;

enum LightingMode {
  LIGHT_STATIC,
  LIGHT_PATTERN
};

enum LightingPattern {
  PATTERN_ENGINE_PLASMA,
  PATTERN_BREATHING,
  PATTERN_RAINBOW,
  PATTERN_OFF
};

struct LightingConfig {
  LightingMode mode = LIGHT_STATIC;
  LightingPattern pattern = PATTERN_ENGINE_PLASMA;

  uint8_t staticR = 0;
  uint8_t staticG = 80;
  uint8_t staticB = 255;
  uint8_t staticW = 0;

  float maxBrightness = 1.0;
  bool enabled = true;

  // Minutes to wait after the last ECU packet before turning lights off.
  // 0 disables the auto-off timer.
  uint32_t autoOffMinutes = 0;
};

struct RgbwColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t w;
};

extern Adafruit_NeoPixel pixels;
extern Preferences lightingPreferences;
extern LightingConfig lighting;
extern bool lightingPrefsReady;
extern bool lightingSettingsDirty;
extern unsigned long lightingSettingsLastChangeMs;
extern RgbwColor currentLightingOutput;

uint8_t addClamp255(uint8_t a, uint8_t b);
float mapFloatClamped(float x, float inMin, float inMax, float outMin, float outMax);
uint8_t scaleChannel(uint8_t value, float scale);
RgbwColor scaleColor(RgbwColor c, float scale);
RgbwColor lerpColor(RgbwColor a, RgbwColor b, float t);
RgbwColor plasmaPalette(float t);
RgbwColor rainbowPalette(float t);
RgbwColor enginePlasmaColor();
RgbwColor breathingColor();
RgbwColor rainbowColor();

void setupLightingPwm();
void setRgbw(RgbwColor c);
bool lightingAutoOffExpired();
void updateLighting();
void loadLightingSettings();
void saveLightingSettings();
void markLightingSettingsDirty();
const char* lightingModeName();
const char* lightingPatternName();
void handleSetLighting();
void handleLightingState();
