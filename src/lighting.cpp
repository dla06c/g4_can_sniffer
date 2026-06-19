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
  // <1000 rpm = 50% brightness, 4800+ rpm = 100% brightness.
  float rpmBrightness = mapFloatClamped(ecu.rpm, 1100.0, 5000.0, 0.50, 1.00);

  // MGP is probably better than absolute MAP for this 15-60 range.
  float loadFactor = mapFloatClamped(ecu.mgp, -65.0, 90.0, 0.0, 1.0);

  RgbwColor c = plasmaPalette(loadFactor);

  float finalBrightness = rpmBrightness * lighting.maxBrightness;
  return scaleColor(c, finalBrightness);
}

RgbwColor breathingColor() {
  float phase = (sin(millis() / 700.0) + 1.0) / 2.0;
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
  float t = fmod(millis() / 5000.0, 1.0);
  return scaleColor(rainbowPalette(t), lighting.maxBrightness);
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
  if (lighting.pattern == PATTERN_BREATHING) return "breathing";
  if (lighting.pattern == PATTERN_RAINBOW) return "rainbow";
  if (lighting.pattern == PATTERN_OFF) return "off";
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
  json.reserve(900);
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
  json += "}";

  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");

  server.send(200, "application/json", json);
}
