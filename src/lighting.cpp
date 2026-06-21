#include "lighting.h"
#include "ecu_data.h"
#include <WebServer.h>
#include <math.h>

extern WebServer server;

// Most RGBW NeoPixel / SK6812 parts use GRBW byte order.
// If red/green/blue/white appear wrong, try NEO_RGBW instead of NEO_GRBW.
Adafruit_NeoPixel pixels(
  NEOPIXEL_COUNT,
  NEOPIXEL_PIN,
  NEO_RGBW + NEO_KHZ800
);

// Second addressable strip on D12 for interior lighting.
Adafruit_NeoPixel pixelsInterior(
  NEOPIXEL_COUNT,
  NEOPIXEL_PIN_INTERIOR,
  NEO_RGBW + NEO_KHZ800
);

Preferences lightingPreferences;
LightingConfig lighting;
bool lightingPrefsReady = false;
bool lightingSettingsDirty = false;
unsigned long lightingSettingsLastChangeMs = 0;
RgbwColor currentLightingOutput = {0, 0, 0, 0};

uint8_t addClamp255(uint8_t a, uint8_t b) {
  int value = a + b;
  if (value > 255) return 255;
  return value;
}

float mapFloatClamped(float x, float inMin, float inMax, float outMin, float outMax) {
  if (x <= inMin) return outMin;
  if (x >= inMax) return outMax;

  float t = (x - inMin) / (inMax - inMin);
  return outMin + t * (outMax - outMin);
}

uint8_t scaleChannel(uint8_t value, float scale) {
  if (scale < 0.0) scale = 0.0;
  if (scale > 1.0) scale = 1.0;

  int out = round(value * scale);
  if (out < 0) out = 0;
  if (out > 255) out = 255;

  return (uint8_t)out;
}

RgbwColor scaleColor(RgbwColor c, float scale) {
  RgbwColor out;
  out.r = scaleChannel(c.r, scale);
  out.g = scaleChannel(c.g, scale);
  out.b = scaleChannel(c.b, scale);
  out.w = scaleChannel(c.w, scale);
  return out;
}

RgbwColor lerpColor(RgbwColor a, RgbwColor b, float t) {
  if (t < 0.0) t = 0.0;
  if (t > 1.0) t = 1.0;

  RgbwColor out;
  out.r = a.r + (b.r - a.r) * t;
  out.g = a.g + (b.g - a.g) * t;
  out.b = a.b + (b.b - a.b) * t;
  out.w = a.w + (b.w - a.w) * t;
  return out;
}

RgbwColor plasmaPalette(float t) {
  if (t < 0.0) t = 0.0;
  if (t > 1.0) t = 1.0;

  RgbwColor c0 = { 20,   0, 120,  0 };  // deep violet-blue
  RgbwColor c1 = { 80,   0, 255,  0 };  // electric violet
  RgbwColor c2 = { 220,  0, 180,  0 };  // magenta
  RgbwColor c3 = { 255, 80,   0,  0 };  // orange
  RgbwColor c4 = { 255, 240, 200, 40 };  // yellow-white

  if (t < 0.25) return lerpColor(c0, c1, t / 0.25);
  if (t < 0.50) return lerpColor(c1, c2, (t - 0.25) / 0.25);
  if (t < 0.75) return lerpColor(c2, c3, (t - 0.50) / 0.25);
  return lerpColor(c3, c4, (t - 0.75) / 0.25);
}

RgbwColor rainbowPalette(float t) {
  if (t < 0.0) t = 0.0;
  if (t > 1.0) t = 1.0;

  RgbwColor red     = {255,   0,   0, 0};
  RgbwColor orange  = {255,  80,   0, 0};
  RgbwColor yellow  = {255, 220,   0, 0};
  RgbwColor green   = {  0, 255,   0, 0};
  RgbwColor cyan    = {  0, 180, 255, 0};
  RgbwColor blue    = {  0,   0, 255, 0};
  RgbwColor violet  = {160,   0, 255, 0};

  if (t < 1.0 / 6.0) return lerpColor(red, orange, t * 6.0);
  if (t < 2.0 / 6.0) return lerpColor(orange, yellow, (t - 1.0 / 6.0) * 6.0);
  if (t < 3.0 / 6.0) return lerpColor(yellow, green, (t - 2.0 / 6.0) * 6.0);
  if (t < 4.0 / 6.0) return lerpColor(green, cyan, (t - 3.0 / 6.0) * 6.0);
  if (t < 5.0 / 6.0) return lerpColor(cyan, blue, (t - 4.0 / 6.0) * 6.0);
  return lerpColor(blue, violet, (t - 5.0 / 6.0) * 6.0);
}

RgbwColor enginePlasmaColor() {
  float rpmBrightness = mapFloatClamped(ecu.rpm,
    lighting.plasmaRpmMin, lighting.plasmaRpmMax, 0.60, 1.00);

  float loadFactor = mapFloatClamped(ecu.map,
    lighting.plasmaMapMin, lighting.plasmaMapMax, 0.0, 1.0);

  RgbwColor c = plasmaPalette(loadFactor);

  float finalBrightness = rpmBrightness * lighting.maxBrightness;
  return scaleColor(c, finalBrightness);
}

RgbwColor breathingColor() {
  float halfPeriod = 700.0f / lighting.breathingSpeed;
  float phase = (sin(millis() / halfPeriod) + 1.0) / 2.0;
  float brightness = phase * lighting.maxBrightness;

  RgbwColor c = {
    lighting.staticR,
    lighting.staticG,
    lighting.staticB,
    lighting.staticW
  };

  return scaleColor(c, brightness);
}

RgbwColor rainbowColor() {
  float cyclePeriod = 5000.0f / lighting.rainbowSpeed;
  float t = fmod(millis() / cyclePeriod, 1.0);
  return scaleColor(rainbowPalette(t), lighting.maxBrightness);
}

// Helper: write per-pixel colors to a strip respecting enabled zones.
static void applyPixelColors(
    Adafruit_NeoPixel& strip,
    LightingZone zones[4],
    uint8_t r[], uint8_t g[], uint8_t b[])
{
  strip.clear();
  for (int z = 0; z < 4; z++) {
    if (!zones[z].enabled) continue;
    uint16_t s = zones[z].start;
    uint16_t e = zones[z].end;
    if (e >= NEOPIXEL_COUNT) e = NEOPIXEL_COUNT - 1;
    for (uint16_t i = s; i <= e; i++) {
      strip.setPixelColor(i, strip.Color(r[i], g[i], b[i], 0));
    }
  }
  strip.show();
}

void updateColorChase() {
  RgbwColor colors[4] = {
    {lighting.chaseC1R, lighting.chaseC1G, lighting.chaseC1B, 0},
    {lighting.chaseC2R, lighting.chaseC2G, lighting.chaseC2B, 0},
    {lighting.chaseC3R, lighting.chaseC3G, lighting.chaseC3B, 0},
    {lighting.chaseC4R, lighting.chaseC4G, lighting.chaseC4B, 0},
  };

  uint16_t widths[4] = {
    max((uint16_t)1, lighting.chaseW1),
    max((uint16_t)1, lighting.chaseW2),
    max((uint16_t)1, lighting.chaseW3),
    max((uint16_t)1, lighting.chaseW4),
  };
  uint16_t totalWidth = widths[0] + widths[1] + widths[2] + widths[3];

  // Default: 40 pixels per second at speed 1.0.
  float pps = 40.0f * lighting.chaseSpeed;
  int offset = (int)fmod((millis() / 1000.0f) * pps, (float)totalWidth);

  uint8_t pr[NEOPIXEL_COUNT], pg[NEOPIXEL_COUNT], pb[NEOPIXEL_COUNT];
  for (uint16_t i = 0; i < NEOPIXEL_COUNT; i++) {
    int pos = (i + offset) % totalWidth;
    // Determine which color band this pixel falls in.
    int idx = 3;
    int acc = 0;
    for (int c = 0; c < 4; c++) {
      acc += widths[c];
      if (pos < acc) { idx = c; break; }
    }
    RgbwColor c = scaleColor(colors[idx], lighting.maxBrightness);
    pr[i] = c.r;
    pg[i] = c.g;
    pb[i] = c.b;
  }

  applyPixelColors(pixels, lighting.exteriorZones, pr, pg, pb);
  applyPixelColors(pixelsInterior, lighting.interiorZones, pr, pg, pb);

  // Use first chase colour as representative output for the preview swatch.
  currentLightingOutput = scaleColor(colors[0], lighting.maxBrightness);
}

void updateLightning() {
  static uint8_t fade[NEOPIXEL_COUNT] = {0};
  static uint8_t fadeColorIdx[NEOPIXEL_COUNT] = {0};
  static unsigned long nextStrikeMs = 0;

  unsigned long now = millis();

  // Trigger a new lightning strike when the interval elapses.
  if (now >= nextStrikeMs) {
    int count = random(1, max(2, NEOPIXEL_COUNT / 20) + 1);
    for (int f = 0; f < count; f++) {
      int px = random(NEOPIXEL_COUNT);
      fade[px] = 255;
      fadeColorIdx[px] = (uint8_t)random(3);  // randomly pick color 0, 1, or 2
    }
    float intervalMs = 1000.0f / lighting.lightningFrequency;
    nextStrikeMs = now + (unsigned long)intervalMs;
  }

  // Fade all pixels toward zero.
  for (int i = 0; i < NEOPIXEL_COUNT; i++) {
    if (fade[i] > 20) fade[i] -= 20;
    else              fade[i] = 0;
  }

  RgbwColor colorChoices[3] = {
    {lighting.lightningR,   lighting.lightningG,   lighting.lightningB,   0},
    {lighting.lightningC2R, lighting.lightningC2G, lighting.lightningC2B, 0},
    {lighting.lightningC3R, lighting.lightningC3G, lighting.lightningC3B, 0},
  };

  uint8_t pr[NEOPIXEL_COUNT], pg[NEOPIXEL_COUNT], pb[NEOPIXEL_COUNT];
  for (uint16_t i = 0; i < NEOPIXEL_COUNT; i++) {
    RgbwColor base = colorChoices[fadeColorIdx[i]];
    float brightness = (fade[i] / 255.0f) * lighting.maxBrightness;
    RgbwColor c = scaleColor(base, brightness);
    pr[i] = c.r;
    pg[i] = c.g;
    pb[i] = c.b;
  }

  applyPixelColors(pixels, lighting.exteriorZones, pr, pg, pb);
  applyPixelColors(pixelsInterior, lighting.interiorZones, pr, pg, pb);

  // Use the first flash colour scaled to max brightness for the preview swatch.
  currentLightingOutput = scaleColor(colorChoices[0], lighting.maxBrightness);
}

void setupLightingPwm() {
  // Function name kept so the rest of your existing setup() does not need to change.
  // This now initialises addressable RGBW LEDs instead of ESP32 LEDC PWM channels.
  pixels.begin();
  pixels.setBrightness(255);  // Brightness is already handled by scaleColor().
  pixels.clear();
  pixels.show();

  pixelsInterior.begin();
  pixelsInterior.setBrightness(255);
  pixelsInterior.clear();
  pixelsInterior.show();
}

void setRgbw(RgbwColor c) {
  currentLightingOutput = c;

  uint32_t packed = pixels.Color(c.r, c.g, c.b, c.w);

  // Exterior strip (D13): apply enabled zones; all other pixels are off.
  pixels.clear();
  for (int z = 0; z < 4; z++) {
    if (!lighting.exteriorZones[z].enabled) continue;
    uint16_t s = lighting.exteriorZones[z].start;
    uint16_t e = lighting.exteriorZones[z].end;
    if (e >= NEOPIXEL_COUNT) e = NEOPIXEL_COUNT - 1;
    for (uint16_t i = s; i <= e; i++) {
      pixels.setPixelColor(i, packed);
    }
  }
  pixels.show();

  // Interior strip (D12): apply enabled zones; all other pixels are off.
  pixelsInterior.clear();
  for (int z = 0; z < 4; z++) {
    if (!lighting.interiorZones[z].enabled) continue;
    uint16_t s = lighting.interiorZones[z].start;
    uint16_t e = lighting.interiorZones[z].end;
    if (e >= NEOPIXEL_COUNT) e = NEOPIXEL_COUNT - 1;
    for (uint16_t i = s; i <= e; i++) {
      pixelsInterior.setPixelColor(i, packed);
    }
  }
  pixelsInterior.show();
}

bool lightingAutoOffExpired() {
  if (lighting.autoOffMinutes == 0) {
    return false;
  }

  // If no valid ECU packet has ever been decoded, treat the timer as expired.
  if (ecu.last_update_ms == 0) {
    return true;
  }

  unsigned long now = millis();
  unsigned long ageMs = now - ecu.last_update_ms;
  unsigned long timeoutMs = lighting.autoOffMinutes * 60000UL;

  return ageMs >= timeoutMs;
}

void updateLighting() {
  if (!lighting.enabled) {
    setRgbw({0, 0, 0, 0});
    return;
  }

  if (lightingAutoOffExpired()) {
    setRgbw({0, 0, 0, 0});
    return;
  }

  if (lighting.mode == LIGHT_STATIC) {
    RgbwColor c = {
      lighting.staticR,
      lighting.staticG,
      lighting.staticB,
      lighting.staticW
    };

    setRgbw(scaleColor(c, lighting.maxBrightness));
    return;
  }

  if (lighting.pattern == PATTERN_ENGINE_PLASMA) {
    setRgbw(enginePlasmaColor());
    return;
  }

  if (lighting.pattern == PATTERN_BREATHING) {
    setRgbw(breathingColor());
    return;
  }

  if (lighting.pattern == PATTERN_RAINBOW) {
    setRgbw(rainbowColor());
    return;
  }

  if (lighting.pattern == PATTERN_COLOR_CHASE) {
    updateColorChase();
    return;
  }

  if (lighting.pattern == PATTERN_LIGHTNING) {
    updateLightning();
    return;
  }

  if (lighting.pattern == PATTERN_OFF) {
    setRgbw({0, 0, 0, 0});
    return;
  }
}

void loadLightingSettings() {
  if (!lightingPrefsReady) return;

  lighting.enabled = lightingPreferences.getBool("enabled", lighting.enabled);

  uint8_t storedMode = lightingPreferences.getUChar("mode", (uint8_t)lighting.mode);
  lighting.mode = storedMode == LIGHT_PATTERN ? LIGHT_PATTERN : LIGHT_STATIC;

  uint8_t storedPattern = lightingPreferences.getUChar("pattern", (uint8_t)lighting.pattern);
  if (storedPattern <= PATTERN_OFF) {
    lighting.pattern = (LightingPattern)storedPattern;
  }

  lighting.staticR = lightingPreferences.getUChar("static_r", lighting.staticR);
  lighting.staticG = lightingPreferences.getUChar("static_g", lighting.staticG);
  lighting.staticB = lightingPreferences.getUChar("static_b", lighting.staticB);
  lighting.staticW = lightingPreferences.getUChar("static_w", lighting.staticW);
  lighting.maxBrightness = constrain(
    lightingPreferences.getFloat("bright", lighting.maxBrightness),
    0.0f,
    1.0f
  );

  lighting.autoOffMinutes = lightingPreferences.getUInt(
    "auto_off_min",
    lighting.autoOffMinutes
  );

  lighting.breathingSpeed = constrain(
    lightingPreferences.getFloat("breath_spd", lighting.breathingSpeed), 0.1f, 10.0f);
  lighting.rainbowSpeed = constrain(
    lightingPreferences.getFloat("rainbow_spd", lighting.rainbowSpeed), 0.1f, 10.0f);

  lighting.plasmaRpmMin = lightingPreferences.getFloat("pl_rpm_min", lighting.plasmaRpmMin);
  lighting.plasmaRpmMax = lightingPreferences.getFloat("pl_rpm_max", lighting.plasmaRpmMax);
  lighting.plasmaMapMin = lightingPreferences.getFloat("pl_map_min", lighting.plasmaMapMin);
  lighting.plasmaMapMax = lightingPreferences.getFloat("pl_map_max", lighting.plasmaMapMax);

  lighting.chaseC1R = lightingPreferences.getUChar("cc1r", lighting.chaseC1R);
  lighting.chaseC1G = lightingPreferences.getUChar("cc1g", lighting.chaseC1G);
  lighting.chaseC1B = lightingPreferences.getUChar("cc1b", lighting.chaseC1B);
  lighting.chaseC2R = lightingPreferences.getUChar("cc2r", lighting.chaseC2R);
  lighting.chaseC2G = lightingPreferences.getUChar("cc2g", lighting.chaseC2G);
  lighting.chaseC2B = lightingPreferences.getUChar("cc2b", lighting.chaseC2B);
  lighting.chaseC3R = lightingPreferences.getUChar("cc3r", lighting.chaseC3R);
  lighting.chaseC3G = lightingPreferences.getUChar("cc3g", lighting.chaseC3G);
  lighting.chaseC3B = lightingPreferences.getUChar("cc3b", lighting.chaseC3B);
  lighting.chaseC4R = lightingPreferences.getUChar("cc4r", lighting.chaseC4R);
  lighting.chaseC4G = lightingPreferences.getUChar("cc4g", lighting.chaseC4G);
  lighting.chaseC4B = lightingPreferences.getUChar("cc4b", lighting.chaseC4B);
  lighting.chaseW1 = lightingPreferences.getUShort("cc1w", lighting.chaseW1);
  lighting.chaseW2 = lightingPreferences.getUShort("cc2w", lighting.chaseW2);
  lighting.chaseW3 = lightingPreferences.getUShort("cc3w", lighting.chaseW3);
  lighting.chaseW4 = lightingPreferences.getUShort("cc4w", lighting.chaseW4);
  lighting.chaseSpeed = constrain(
    lightingPreferences.getFloat("chase_speed", lighting.chaseSpeed), 0.1f, 10.0f);

  lighting.lightningR = lightingPreferences.getUChar("lt_r", lighting.lightningR);
  lighting.lightningG = lightingPreferences.getUChar("lt_g", lighting.lightningG);
  lighting.lightningB = lightingPreferences.getUChar("lt_b", lighting.lightningB);
  lighting.lightningC2R = lightingPreferences.getUChar("lt_c2r", lighting.lightningC2R);
  lighting.lightningC2G = lightingPreferences.getUChar("lt_c2g", lighting.lightningC2G);
  lighting.lightningC2B = lightingPreferences.getUChar("lt_c2b", lighting.lightningC2B);
  lighting.lightningC3R = lightingPreferences.getUChar("lt_c3r", lighting.lightningC3R);
  lighting.lightningC3G = lightingPreferences.getUChar("lt_c3g", lighting.lightningC3G);
  lighting.lightningC3B = lightingPreferences.getUChar("lt_c3b", lighting.lightningC3B);
  lighting.lightningFrequency = constrain(
    lightingPreferences.getFloat("lt_freq", lighting.lightningFrequency), 0.1f, 20.0f);

  // Load zone config.
  const char* extStartKeys[4] = { "ez1s", "ez2s", "ez3s", "ez4s" };
  const char* extEndKeys[4]   = { "ez1e", "ez2e", "ez3e", "ez4e" };
  const char* extEnKeys[4]    = { "ez1en", "ez2en", "ez3en", "ez4en" };
  const char* intStartKeys[4] = { "iz1s", "iz2s", "iz3s", "iz4s" };
  const char* intEndKeys[4]   = { "iz1e", "iz2e", "iz3e", "iz4e" };
  const char* intEnKeys[4]    = { "iz1en", "iz2en", "iz3en", "iz4en" };

  for (int z = 0; z < 4; z++) {
    lighting.exteriorZones[z].start   = lightingPreferences.getUShort(extStartKeys[z], 0);
    lighting.exteriorZones[z].end     = lightingPreferences.getUShort(extEndKeys[z], 0);
    lighting.exteriorZones[z].enabled = lightingPreferences.getBool(extEnKeys[z], false);
    lighting.interiorZones[z].start   = lightingPreferences.getUShort(intStartKeys[z], 0);
    lighting.interiorZones[z].end     = lightingPreferences.getUShort(intEndKeys[z], 0);
    lighting.interiorZones[z].enabled = lightingPreferences.getBool(intEnKeys[z], false);
  }
}

void saveLightingSettings() {
  if (!lightingPrefsReady) return;

  lightingPreferences.putBool("enabled", lighting.enabled);
  lightingPreferences.putUChar("mode", (uint8_t)lighting.mode);
  lightingPreferences.putUChar("pattern", (uint8_t)lighting.pattern);
  lightingPreferences.putUChar("static_r", lighting.staticR);
  lightingPreferences.putUChar("static_g", lighting.staticG);
  lightingPreferences.putUChar("static_b", lighting.staticB);
  lightingPreferences.putUChar("static_w", lighting.staticW);
  lightingPreferences.putFloat("bright", lighting.maxBrightness);
  lightingPreferences.putUInt("auto_off_min", lighting.autoOffMinutes);

  lightingPreferences.putFloat("breath_spd",  lighting.breathingSpeed);
  lightingPreferences.putFloat("rainbow_spd", lighting.rainbowSpeed);
  lightingPreferences.putFloat("pl_rpm_min",  lighting.plasmaRpmMin);
  lightingPreferences.putFloat("pl_rpm_max",  lighting.plasmaRpmMax);
  lightingPreferences.putFloat("pl_map_min",  lighting.plasmaMapMin);
  lightingPreferences.putFloat("pl_map_max",  lighting.plasmaMapMax);

  lightingPreferences.putUChar("cc1r", lighting.chaseC1R);
  lightingPreferences.putUChar("cc1g", lighting.chaseC1G);
  lightingPreferences.putUChar("cc1b", lighting.chaseC1B);
  lightingPreferences.putUChar("cc2r", lighting.chaseC2R);
  lightingPreferences.putUChar("cc2g", lighting.chaseC2G);
  lightingPreferences.putUChar("cc2b", lighting.chaseC2B);
  lightingPreferences.putUChar("cc3r", lighting.chaseC3R);
  lightingPreferences.putUChar("cc3g", lighting.chaseC3G);
  lightingPreferences.putUChar("cc3b", lighting.chaseC3B);
  lightingPreferences.putUChar("cc4r", lighting.chaseC4R);
  lightingPreferences.putUChar("cc4g", lighting.chaseC4G);
  lightingPreferences.putUChar("cc4b", lighting.chaseC4B);
  lightingPreferences.putUShort("cc1w", lighting.chaseW1);
  lightingPreferences.putUShort("cc2w", lighting.chaseW2);
  lightingPreferences.putUShort("cc3w", lighting.chaseW3);
  lightingPreferences.putUShort("cc4w", lighting.chaseW4);
  lightingPreferences.putFloat("chase_speed", lighting.chaseSpeed);

  lightingPreferences.putUChar("lt_r",    lighting.lightningR);
  lightingPreferences.putUChar("lt_g",    lighting.lightningG);
  lightingPreferences.putUChar("lt_b",    lighting.lightningB);
  lightingPreferences.putUChar("lt_c2r",  lighting.lightningC2R);
  lightingPreferences.putUChar("lt_c2g",  lighting.lightningC2G);
  lightingPreferences.putUChar("lt_c2b",  lighting.lightningC2B);
  lightingPreferences.putUChar("lt_c3r",  lighting.lightningC3R);
  lightingPreferences.putUChar("lt_c3g",  lighting.lightningC3G);
  lightingPreferences.putUChar("lt_c3b",  lighting.lightningC3B);
  lightingPreferences.putFloat("lt_freq", lighting.lightningFrequency);

  const char* extStartKeys[4] = { "ez1s", "ez2s", "ez3s", "ez4s" };
  const char* extEndKeys[4]   = { "ez1e", "ez2e", "ez3e", "ez4e" };
  const char* extEnKeys[4]    = { "ez1en", "ez2en", "ez3en", "ez4en" };
  const char* intStartKeys[4] = { "iz1s", "iz2s", "iz3s", "iz4s" };
  const char* intEndKeys[4]   = { "iz1e", "iz2e", "iz3e", "iz4e" };
  const char* intEnKeys[4]    = { "iz1en", "iz2en", "iz3en", "iz4en" };

  for (int z = 0; z < 4; z++) {
    lightingPreferences.putUShort(extStartKeys[z], lighting.exteriorZones[z].start);
    lightingPreferences.putUShort(extEndKeys[z],   lighting.exteriorZones[z].end);
    lightingPreferences.putBool(extEnKeys[z],      lighting.exteriorZones[z].enabled);
    lightingPreferences.putUShort(intStartKeys[z], lighting.interiorZones[z].start);
    lightingPreferences.putUShort(intEndKeys[z],   lighting.interiorZones[z].end);
    lightingPreferences.putBool(intEnKeys[z],      lighting.interiorZones[z].enabled);
  }
}

void markLightingSettingsDirty() {
  lightingSettingsDirty = true;
  lightingSettingsLastChangeMs = millis();
}

// Parse "start-end" range string (e.g. "35-120") into start/end integers.
// If the format is invalid the values are left unchanged.
void parseZoneRange(const String& s, uint16_t& start, uint16_t& end) {
  int dash = s.indexOf('-');
  // dash <= 0 rejects both "not found" (-1) and a leading dash like "-49" (pos 0).
  // It does NOT reject "0-49" where the digit 0 is at pos 0 and '-' is at pos 1.
  if (dash <= 0) return;
  int a = s.substring(0, dash).toInt();
  int b = s.substring(dash + 1).toInt();
  if (a < 0 || b < 0 || a > b) return;
  start = (uint16_t)a;
  end   = (uint16_t)b;
}

const char* lightingModeName() {
  if (lighting.mode == LIGHT_STATIC) return "static";
  if (lighting.mode == LIGHT_PATTERN) return "pattern";
  return "unknown";
}

const char* lightingPatternName() {
  if (lighting.pattern == PATTERN_ENGINE_PLASMA) return "engine_plasma";
  if (lighting.pattern == PATTERN_BREATHING)    return "breathing";
  if (lighting.pattern == PATTERN_RAINBOW)      return "rainbow";
  if (lighting.pattern == PATTERN_COLOR_CHASE)  return "color_chase";
  if (lighting.pattern == PATTERN_LIGHTNING)    return "lightning";
  if (lighting.pattern == PATTERN_OFF)          return "off";
  return "unknown";
}

void handleSetLighting() {
  if (server.hasArg("enabled")) {
    lighting.enabled = server.arg("enabled").toInt() == 1;
  }

  if (server.hasArg("mode")) {
    String mode = server.arg("mode");

    if (mode == "static") {
      lighting.mode = LIGHT_STATIC;
    } else if (mode == "pattern") {
      lighting.mode = LIGHT_PATTERN;
    }
  }

  if (server.hasArg("pattern")) {
    String pattern = server.arg("pattern");

    if (pattern == "engine_plasma") {
      lighting.pattern = PATTERN_ENGINE_PLASMA;
    } else if (pattern == "breathing") {
      lighting.pattern = PATTERN_BREATHING;
    } else if (pattern == "rainbow") {
      lighting.pattern = PATTERN_RAINBOW;
    } else if (pattern == "color_chase") {
      lighting.pattern = PATTERN_COLOR_CHASE;
    } else if (pattern == "lightning") {
      lighting.pattern = PATTERN_LIGHTNING;
    } else if (pattern == "off") {
      lighting.pattern = PATTERN_OFF;
    }
  }

  if (server.hasArg("r")) lighting.staticR = constrain(server.arg("r").toInt(), 0, 255);
  if (server.hasArg("g")) lighting.staticG = constrain(server.arg("g").toInt(), 0, 255);
  if (server.hasArg("b")) lighting.staticB = constrain(server.arg("b").toInt(), 0, 255);
  if (server.hasArg("w")) lighting.staticW = constrain(server.arg("w").toInt(), 0, 255);

  if (server.hasArg("brightness")) {
    lighting.maxBrightness = constrain(server.arg("brightness").toFloat(), 0.0, 1.0);
  }

  if (server.hasArg("auto_off_minutes")) {
    int minutes = server.arg("auto_off_minutes").toInt();
    lighting.autoOffMinutes = (uint32_t)constrain(minutes, 0, 1440);
  }

  // Zone parameters: ext_z1_range, ext_z1_en, ..., int_z1_range, int_z1_en, ...
  const char* extRangeArgs[4] = { "ext_z1_range", "ext_z2_range", "ext_z3_range", "ext_z4_range" };
  const char* extEnArgs[4]    = { "ext_z1_en",    "ext_z2_en",    "ext_z3_en",    "ext_z4_en"    };
  const char* intRangeArgs[4] = { "int_z1_range", "int_z2_range", "int_z3_range", "int_z4_range" };
  const char* intEnArgs[4]    = { "int_z1_en",    "int_z2_en",    "int_z3_en",    "int_z4_en"    };

  for (int z = 0; z < 4; z++) {
    if (server.hasArg(extRangeArgs[z])) {
      parseZoneRange(server.arg(extRangeArgs[z]),
                     lighting.exteriorZones[z].start,
                     lighting.exteriorZones[z].end);
    }
    if (server.hasArg(extEnArgs[z])) {
      lighting.exteriorZones[z].enabled = server.arg(extEnArgs[z]).toInt() == 1;
    }
    if (server.hasArg(intRangeArgs[z])) {
      parseZoneRange(server.arg(intRangeArgs[z]),
                     lighting.interiorZones[z].start,
                     lighting.interiorZones[z].end);
    }
    if (server.hasArg(intEnArgs[z])) {
      lighting.interiorZones[z].enabled = server.arg(intEnArgs[z]).toInt() == 1;
    }
  }

  // Per-pattern parameters.
  if (server.hasArg("breathing_speed")) {
    lighting.breathingSpeed = constrain(server.arg("breathing_speed").toFloat(), 0.1, 10.0);
  }
  if (server.hasArg("rainbow_speed")) {
    lighting.rainbowSpeed = constrain(server.arg("rainbow_speed").toFloat(), 0.1, 10.0);
  }
  if (server.hasArg("plasma_rpm_min")) {
    lighting.plasmaRpmMin = constrain(server.arg("plasma_rpm_min").toFloat(), 0.0, 9000.0);
  }
  if (server.hasArg("plasma_rpm_max")) {
    lighting.plasmaRpmMax = constrain(server.arg("plasma_rpm_max").toFloat(), 0.0, 9000.0);
  }
  if (server.hasArg("plasma_map_min")) {
    lighting.plasmaMapMin = constrain(server.arg("plasma_map_min").toFloat(), 0.0, 300.0);
  }
  if (server.hasArg("plasma_map_max")) {
    lighting.plasmaMapMax = constrain(server.arg("plasma_map_max").toFloat(), 0.0, 300.0);
  }
  if (server.hasArg("chase_c1_r")) lighting.chaseC1R = constrain(server.arg("chase_c1_r").toInt(), 0, 255);
  if (server.hasArg("chase_c1_g")) lighting.chaseC1G = constrain(server.arg("chase_c1_g").toInt(), 0, 255);
  if (server.hasArg("chase_c1_b")) lighting.chaseC1B = constrain(server.arg("chase_c1_b").toInt(), 0, 255);
  if (server.hasArg("chase_c2_r")) lighting.chaseC2R = constrain(server.arg("chase_c2_r").toInt(), 0, 255);
  if (server.hasArg("chase_c2_g")) lighting.chaseC2G = constrain(server.arg("chase_c2_g").toInt(), 0, 255);
  if (server.hasArg("chase_c2_b")) lighting.chaseC2B = constrain(server.arg("chase_c2_b").toInt(), 0, 255);
  if (server.hasArg("chase_c3_r")) lighting.chaseC3R = constrain(server.arg("chase_c3_r").toInt(), 0, 255);
  if (server.hasArg("chase_c3_g")) lighting.chaseC3G = constrain(server.arg("chase_c3_g").toInt(), 0, 255);
  if (server.hasArg("chase_c3_b")) lighting.chaseC3B = constrain(server.arg("chase_c3_b").toInt(), 0, 255);
  if (server.hasArg("chase_c4_r")) lighting.chaseC4R = constrain(server.arg("chase_c4_r").toInt(), 0, 255);
  if (server.hasArg("chase_c4_g")) lighting.chaseC4G = constrain(server.arg("chase_c4_g").toInt(), 0, 255);
  if (server.hasArg("chase_c4_b")) lighting.chaseC4B = constrain(server.arg("chase_c4_b").toInt(), 0, 255);
  if (server.hasArg("chase_w1")) lighting.chaseW1 = (uint16_t)constrain(server.arg("chase_w1").toInt(), 1, 500);
  if (server.hasArg("chase_w2")) lighting.chaseW2 = (uint16_t)constrain(server.arg("chase_w2").toInt(), 1, 500);
  if (server.hasArg("chase_w3")) lighting.chaseW3 = (uint16_t)constrain(server.arg("chase_w3").toInt(), 1, 500);
  if (server.hasArg("chase_w4")) lighting.chaseW4 = (uint16_t)constrain(server.arg("chase_w4").toInt(), 1, 500);
  if (server.hasArg("chase_speed")) {
    lighting.chaseSpeed = constrain(server.arg("chase_speed").toFloat(), 0.1, 10.0);
  }
  if (server.hasArg("lightning_r")) lighting.lightningR = constrain(server.arg("lightning_r").toInt(), 0, 255);
  if (server.hasArg("lightning_g")) lighting.lightningG = constrain(server.arg("lightning_g").toInt(), 0, 255);
  if (server.hasArg("lightning_b")) lighting.lightningB = constrain(server.arg("lightning_b").toInt(), 0, 255);
  if (server.hasArg("lightning_c2_r")) lighting.lightningC2R = constrain(server.arg("lightning_c2_r").toInt(), 0, 255);
  if (server.hasArg("lightning_c2_g")) lighting.lightningC2G = constrain(server.arg("lightning_c2_g").toInt(), 0, 255);
  if (server.hasArg("lightning_c2_b")) lighting.lightningC2B = constrain(server.arg("lightning_c2_b").toInt(), 0, 255);
  if (server.hasArg("lightning_c3_r")) lighting.lightningC3R = constrain(server.arg("lightning_c3_r").toInt(), 0, 255);
  if (server.hasArg("lightning_c3_g")) lighting.lightningC3G = constrain(server.arg("lightning_c3_g").toInt(), 0, 255);
  if (server.hasArg("lightning_c3_b")) lighting.lightningC3B = constrain(server.arg("lightning_c3_b").toInt(), 0, 255);
  if (server.hasArg("lightning_freq")) {
    lighting.lightningFrequency = constrain(server.arg("lightning_freq").toFloat(), 0.1, 20.0);
  }

  updateLighting();
  markLightingSettingsDirty();

  Serial.print("Lighting update | enabled=");
  Serial.print(lighting.enabled);
  Serial.print(" mode=");
  Serial.print(lighting.mode);
  Serial.print(" pattern=");
  Serial.print(lighting.pattern);
  Serial.print(" RGBW=");
  Serial.print(lighting.staticR);
  Serial.print(",");
  Serial.print(lighting.staticG);
  Serial.print(",");
  Serial.print(lighting.staticB);
  Serial.print(",");
  Serial.print(lighting.staticW);
  Serial.print(" brightness=");
  Serial.println(lighting.maxBrightness);

  server.send(200, "application/json", "{\"ok\":true}");
}

void handleLightingState() {
  // Browser preview cannot truly show RGBW, so approximate W by adding it
  // into RGB for the preview patch. Raw RGBW values are also returned.
  uint8_t previewR = addClamp255(currentLightingOutput.r, currentLightingOutput.w);
  uint8_t previewG = addClamp255(currentLightingOutput.g, currentLightingOutput.w);
  uint8_t previewB = addClamp255(currentLightingOutput.b, currentLightingOutput.w);

  unsigned long now = millis();
  unsigned long packetAgeMs = ecu.last_update_ms == 0 ? 999999UL : now - ecu.last_update_ms;

  String json;
  json.reserve(2200);
  json += "{";
  json += "\"enabled\":" + String(lighting.enabled ? "true" : "false") + ",";
  json += "\"mode\":\"" + String(lightingModeName()) + "\",";
  json += "\"pattern\":\"" + String(lightingPatternName()) + "\",";
  json += "\"max_brightness\":" + String(lighting.maxBrightness, 3) + ",";
  json += "\"auto_off_minutes\":" + String(lighting.autoOffMinutes) + ",";
  json += "\"auto_off_expired\":" + String(lightingAutoOffExpired() ? "true" : "false") + ",";
  json += "\"last_packet_age_ms\":" + String(packetAgeMs) + ",";
  json += "\"static_r\":" + String(lighting.staticR) + ",";
  json += "\"static_g\":" + String(lighting.staticG) + ",";
  json += "\"static_b\":" + String(lighting.staticB) + ",";
  json += "\"static_w\":" + String(lighting.staticW) + ",";

  json += "\"r\":" + String(currentLightingOutput.r) + ",";
  json += "\"g\":" + String(currentLightingOutput.g) + ",";
  json += "\"b\":" + String(currentLightingOutput.b) + ",";
  json += "\"w\":" + String(currentLightingOutput.w) + ",";

  json += "\"preview_r\":" + String(previewR) + ",";
  json += "\"preview_g\":" + String(previewG) + ",";
  json += "\"preview_b\":" + String(previewB) + ",";

  json += "\"rpm\":" + String(ecu.rpm, 0) + ",";
  json += "\"mgp\":" + String(ecu.mgp, 1) + ",";

  // Zone configuration arrays.
  auto appendZones = [&](const LightingZone zones[4], const char* key) {
    json += "\"";
    json += key;
    json += "\":[";
    for (int z = 0; z < 4; z++) {
      if (z > 0) json += ",";
      json += "{\"start\":" + String(zones[z].start) +
              ",\"end\":"   + String(zones[z].end)   +
              ",\"enabled\":" + String(zones[z].enabled ? "true" : "false") + "}";
    }
    json += "]";
  };

  appendZones(lighting.exteriorZones, "exterior_zones");
  json += ",";
  appendZones(lighting.interiorZones, "interior_zones");

  // Per-pattern parameters.
  json += ",\"breathing_speed\":"    + String(lighting.breathingSpeed, 2);
  json += ",\"rainbow_speed\":"      + String(lighting.rainbowSpeed, 2);
  json += ",\"plasma_rpm_min\":"     + String(lighting.plasmaRpmMin, 0);
  json += ",\"plasma_rpm_max\":"     + String(lighting.plasmaRpmMax, 0);
  json += ",\"plasma_map_min\":"     + String(lighting.plasmaMapMin, 0);
  json += ",\"plasma_map_max\":"     + String(lighting.plasmaMapMax, 0);
  json += ",\"chase_c1_r\":"  + String(lighting.chaseC1R);
  json += ",\"chase_c1_g\":"  + String(lighting.chaseC1G);
  json += ",\"chase_c1_b\":"  + String(lighting.chaseC1B);
  json += ",\"chase_c2_r\":"  + String(lighting.chaseC2R);
  json += ",\"chase_c2_g\":"  + String(lighting.chaseC2G);
  json += ",\"chase_c2_b\":"  + String(lighting.chaseC2B);
  json += ",\"chase_c3_r\":"  + String(lighting.chaseC3R);
  json += ",\"chase_c3_g\":"  + String(lighting.chaseC3G);
  json += ",\"chase_c3_b\":"  + String(lighting.chaseC3B);
  json += ",\"chase_c4_r\":"  + String(lighting.chaseC4R);
  json += ",\"chase_c4_g\":"  + String(lighting.chaseC4G);
  json += ",\"chase_c4_b\":"  + String(lighting.chaseC4B);
  json += ",\"chase_w1\":"    + String(lighting.chaseW1);
  json += ",\"chase_w2\":"    + String(lighting.chaseW2);
  json += ",\"chase_w3\":"    + String(lighting.chaseW3);
  json += ",\"chase_w4\":"    + String(lighting.chaseW4);
  json += ",\"chase_speed\":"       + String(lighting.chaseSpeed, 2);
  json += ",\"lightning_r\":"    + String(lighting.lightningR);
  json += ",\"lightning_g\":"    + String(lighting.lightningG);
  json += ",\"lightning_b\":"    + String(lighting.lightningB);
  json += ",\"lightning_c2_r\":" + String(lighting.lightningC2R);
  json += ",\"lightning_c2_g\":" + String(lighting.lightningC2G);
  json += ",\"lightning_c2_b\":" + String(lighting.lightningC2B);
  json += ",\"lightning_c3_r\":" + String(lighting.lightningC3R);
  json += ",\"lightning_c3_g\":" + String(lighting.lightningC3G);
  json += ",\"lightning_c3_b\":" + String(lighting.lightningC3B);
  json += ",\"lightning_freq\":" + String(lighting.lightningFrequency, 2);

  json += "}";

  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");

  server.send(200, "application/json", json);
}
