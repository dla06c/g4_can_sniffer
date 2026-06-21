#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>

const int NEOPIXEL_PIN = 13;           // D13 – exterior lighting
const int NEOPIXEL_PIN_INTERIOR = 12;  // D12 – interior lighting
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
  PATTERN_COLOR_CHASE,
  PATTERN_LIGHTNING,
  PATTERN_OFF
};

// A named LED index range on a strip with an on/off flag.
struct LightingZone {
  uint16_t start = 0;
  uint16_t end = 0;
  bool enabled = false;
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

  // Zone configuration – 4 zones per strip.
  // Exterior (D13): Outside Zones 1-4.
  // Interior (D12): Interior Zones 1-4.
  LightingZone exteriorZones[4];
  LightingZone interiorZones[4];

  // Breathing pattern – speed multiplier (1.0 = default 700 ms half-period).
  float breathingSpeed = 1.0;

  // Rainbow pattern – speed multiplier (1.0 = 5 s full cycle).
  float rainbowSpeed = 1.0;

  // Engine Plasma – configurable RPM and MAP mapping ranges.
  float plasmaRpmMin = 1100.0;
  float plasmaRpmMax = 4200.0;
  float plasmaMapMin = 30.0;
  float plasmaMapMax = 70.0;

  // Color Chase pattern – four chase colors (RGB), per-color pixel widths, and speed multiplier.
  uint8_t chaseC1R = 255; uint8_t chaseC1G =   0; uint8_t chaseC1B =   0;
  uint8_t chaseC2R =   0; uint8_t chaseC2G = 255; uint8_t chaseC2B =   0;
  uint8_t chaseC3R =   0; uint8_t chaseC3G =   0; uint8_t chaseC3B = 255;
  uint8_t chaseC4R = 255; uint8_t chaseC4G = 128; uint8_t chaseC4B =   0;
  uint16_t chaseW1 = 50;
  uint16_t chaseW2 = 50;
  uint16_t chaseW3 = 50;
  uint16_t chaseW4 = 50;
  float   chaseSpeed = 1.0;  // 0.1–10.0×

  // Lightning pattern – up to 3 flash colors (RGB) randomly chosen per strike, and flash frequency (Hz).
  uint8_t lightningR = 200;
  uint8_t lightningG = 200;
  uint8_t lightningB = 255;
  uint8_t lightningC2R = 200; uint8_t lightningC2G = 200; uint8_t lightningC2B = 255;
  uint8_t lightningC3R = 200; uint8_t lightningC3G = 200; uint8_t lightningC3B = 255;
  float   lightningFrequency = 2.0;  // 0.1–20.0 Hz
};

struct RgbwColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t w;
};

extern Adafruit_NeoPixel pixels;
extern Adafruit_NeoPixel pixelsInterior;
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
void updateColorChase();
void updateLightning();

void setupLightingPwm();
void setRgbw(RgbwColor c);
bool lightingAutoOffExpired();
void updateLighting();
void loadLightingSettings();
void saveLightingSettings();
void markLightingSettingsDirty();
const char* lightingModeName();
const char* lightingPatternName();
void parseZoneRange(const String& s, uint16_t& start, uint16_t& end);
void handleSetLighting();
void handleLightingState();
