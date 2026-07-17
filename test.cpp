//LinkDash#include <WiFi.h>#include <WebServer.h>#include <WiFiUdp.h>#include <Preferences.h>#include <math.h>#include <Adafruit_NeoPixel.h>#include <driver/twai.h>

const char* AP_SSID = "LinkDash";const char* AP_PASS = "linkdash123";  // minimum 8 characters

const uint16_t UDP_PORT = 4210;

// Addressable RGBW LED configuration.// For bench testing with one NeoPixel RGBW Mini Button, leave count as 1.// For a strip later, change NEOPIXEL_COUNT to the number of RGBW pixels.const int NEOPIXEL_PIN = 13;const uint16_t NEOPIXEL_COUNT = 10;

// Most RGBW NeoPixel / SK6812 parts use GRBW byte order.// If red/green/blue/white appear wrong, try NEO_RGBW instead of NEO_GRBW.Adafruit_NeoPixel pixels(NEOPIXEL_COUNT,NEOPIXEL_PIN,NEO_GRBW + NEO_KHZ800);

WebServer server(80);WiFiUDP udp;Preferences lightingPreferences;

// ---------------------------------------------------------------------------// CAN / TWAI configuration for Link ECU CAN 1 User Stream 1//// Hardware://   ESP32 GPIO5 -> SN65HVD230 TXD / CTX//   ESP32 GPIO4 -> SN65HVD230 RXD / CRX//   ESP32 3V3   -> SN65HVD230 VCC//   ESP32 GND   -> SN65HVD230 GND//   CANH/CANL   -> Link ECU CANH/CANL//// ECU setup expected://   CAN 1, User Defined, 1 Mbit/s//   Channel 1: Transmit User Stream 1, ID 0x3E8 / 1000 dec, Standard, 50 Hz//   Stream 1 / Frame 1://     Bits 0-15   Engine Speed, Unsigned 16, MS First, multiplier 1, divider 1//     Bits 16-31  MAP,          Unsigned 16, MS First, multiplier 1, divider 1//     Bits 32-47  MGP,          Signed 16,   MS First, multiplier 1, divider 1//     Bits 48-63  Batt Voltage, Unsigned 16, MS First, raw/100 = volts// ---------------------------------------------------------------------------const gpio_num_t CAN_TX_PIN = GPIO_NUM_5;const gpio_num_t CAN_RX_PIN = GPIO_NUM_4;static const uint32_t LINK_ECU_CAN_ID = 0x3E8U;

bool canStarted = false;unsigned long canFrameCount = 0;unsigned long canDecodedFrameCount = 0;unsigned long lastCanFrameMs = 0;unsigned long lastCanDecodedMs = 0;unsigned long lastCanSerialPrintMs = 0;

struct EcuData {float rpm = 0;float ect = 0;float iat = 0;float mgp = 0;float map = 0;float tps = 0;

float ignition_angle = 0;float injection_actual_pw = 0;float injection_effective_pw = 0;

float lambda1 = 0;float lambda_target = 0;float lambda_error = 0;float lambda_status = 0;float lambda_temp = 0;

float oil_temp = 0;float battery_v = 0;float fuel_pressure = 0;float oil_pressure = 0;

float boost_target = 0;float boost_error = 0;float boost_p = 0;float boost_i = 0;float boost_d = 0;float boost_duty = 0;

float trig1_err = 0;float internal_3v3 = 0;float internal_12v = 0;

float aps_main = 0;float throttle_target = 0;float vvt_in_target = 0;float vvt_in_pos = 0;

unsigned long last_update_ms = 0;};

EcuData ecu;

enum LightingMode {LIGHT_STATIC,LIGHT_PATTERN};

enum LightingPattern {PATTERN_ENGINE_PLASMA,PATTERN_BREATHING,PATTERN_RAINBOW,PATTERN_OFF};

struct LightingConfig {LightingMode mode = LIGHT_STATIC;LightingPattern pattern = PATTERN_ENGINE_PLASMA;

uint8_t staticR = 0;uint8_t staticG = 80;uint8_t staticB = 255;uint8_t staticW = 0;

float maxBrightness = 1.0;bool enabled = true;};

LightingConfig lighting;bool lightingPrefsReady = false;bool lightingSettingsDirty = false;unsigned long lightingSettingsLastChangeMs = 0;const unsigned long LIGHTING_SAVE_DEBOUNCE_MS = 500;

struct RgbwColor {uint8_t r;uint8_t g;uint8_t b;uint8_t w;};

RgbwColor currentLightingOutput = {0, 0, 0, 0};

float mapFloatClamped(float x, float inMin, float inMax, float outMin, float outMax) {if (x <= inMin) return outMin;if (x >= inMax) return outMax;

float t = (x - inMin) / (inMax - inMin);return outMin + t * (outMax - outMin);}

uint8_t scaleChannel(uint8_t value, float scale) {if (scale < 0.0) scale = 0.0;if (scale > 1.0) scale = 1.0;

int out = round(value * scale);if (out < 0) out = 0;if (out > 255) out = 255;

return (uint8_t)out;}

RgbwColor scaleColor(RgbwColor c, float scale) {RgbwColor out;out.r = scaleChannel(c.r, scale);out.g = scaleChannel(c.g, scale);out.b = scaleChannel(c.b, scale);out.w = scaleChannel(c.w, scale);return out;}

RgbwColor lerpColor(RgbwColor a, RgbwColor b, float t) {if (t < 0.0) t = 0.0;if (t > 1.0) t = 1.0;

RgbwColor out;out.r = a.r + (b.r - a.r) * t;out.g = a.g + (b.g - a.g) * t;out.b = a.b + (b.b - a.b) * t;out.w = a.w + (b.w - a.w) * t;return out;}

RgbwColor plasmaPalette(float t) {if (t < 0.0) t = 0.0;if (t > 1.0) t = 1.0;

RgbwColor c0 = { 20,   0, 120,  0 };  // deep violet-blueRgbwColor c1 = { 80,   0, 255,  0 };  // electric violetRgbwColor c2 = { 220,  0, 180,  0 };  // magentaRgbwColor c3 = { 255, 80,   0,  0 };  // orangeRgbwColor c4 = { 255, 240, 200, 40 };  // yellow-white

if (t < 0.25) return lerpColor(c0, c1, t / 0.25);if (t < 0.50) return lerpColor(c1, c2, (t - 0.25) / 0.25);if (t < 0.75) return lerpColor(c2, c3, (t - 0.50) / 0.25);return lerpColor(c3, c4, (t - 0.75) / 0.25);}

RgbwColor rainbowPalette(float t) {if (t < 0.0) t = 0.0;if (t > 1.0) t = 1.0;

RgbwColor red     = {255,   0,   0, 0};RgbwColor orange  = {255,  80,   0, 0};RgbwColor yellow  = {255, 220,   0, 0};RgbwColor green   = {  0, 255,   0, 0};RgbwColor cyan    = {  0, 180, 255, 0};RgbwColor blue    = {  0,   0, 255, 0};RgbwColor violet  = {160,   0, 255, 0};

if (t < 1.0 / 6.0) return lerpColor(red, orange, t * 6.0);if (t < 2.0 / 6.0) return lerpColor(orange, yellow, (t - 1.0 / 6.0) * 6.0);if (t < 3.0 / 6.0) return lerpColor(yellow, green, (t - 2.0 / 6.0) * 6.0);if (t < 4.0 / 6.0) return lerpColor(green, cyan, (t - 3.0 / 6.0) * 6.0);if (t < 5.0 / 6.0) return lerpColor(cyan, blue, (t - 4.0 / 6.0) * 6.0);return lerpColor(blue, violet, (t - 5.0 / 6.0) * 6.0);}

RgbwColor enginePlasmaColor() {// <1000 rpm = 50% brightness, 4800+ rpm = 100% brightness.float rpmBrightness = mapFloatClamped(ecu.rpm, 1100.0, 5000.0, 0.50, 1.00);

// MGP is probably better than absolute MAP for this 15-60 range.float loadFactor = mapFloatClamped(ecu.map, 15.0, 90.0, 0.0, 1.0);

RgbwColor c = plasmaPalette(loadFactor);

float finalBrightness = rpmBrightness * lighting.maxBrightness;return scaleColor(c, finalBrightness);}

RgbwColor breathingColor() {float phase = (sin(millis() / 700.0) + 1.0) / 2.0;float brightness = phase * lighting.maxBrightness;

RgbwColor c = {lighting.staticR,lighting.staticG,lighting.staticB,lighting.staticW};

return scaleColor(c, brightness);}

RgbwColor rainbowColor() {float t = fmod(millis() / 5000.0, 1.0);return scaleColor(rainbowPalette(t), lighting.maxBrightness);}

void setupLightingPwm() {// Function name kept so the rest of your existing setup() does not need to change.// This now initialises addressable RGBW LEDs instead of ESP32 LEDC PWM channels.pixels.begin();pixels.setBrightness(255);  // Brightness is already handled by scaleColor().pixels.clear();pixels.show();}

void setRgbw(RgbwColor c) {currentLightingOutput = c;

// Addressable RGBW output.// For now every pixel receives the same colour. Later this can be extended// to zones, gradients, chases, warning flashes, etc.uint32_t packed = pixels.Color(c.r, c.g, c.b, c.w);

for (uint16_t i = 0; i < NEOPIXEL_COUNT; i++) {pixels.setPixelColor(i, packed);}

pixels.show();}

void updateLighting() {if (!lighting.enabled) {setRgbw({0, 0, 0, 0});return;}

if (lighting.mode == LIGHT_STATIC) {RgbwColor c = {lighting.staticR,lighting.staticG,lighting.staticB,lighting.staticW};

setRgbw(scaleColor(c, lighting.maxBrightness));
return;

}

if (lighting.pattern == PATTERN_ENGINE_PLASMA) {setRgbw(enginePlasmaColor());return;}

if (lighting.pattern == PATTERN_BREATHING) {setRgbw(breathingColor());return;}

if (lighting.pattern == PATTERN_RAINBOW) {setRgbw(rainbowColor());return;}

if (lighting.pattern == PATTERN_OFF) {setRgbw({0, 0, 0, 0});return;}}

void loadLightingSettings() {if (!lightingPrefsReady) return;

lighting.enabled = lightingPreferences.getBool("enabled", lighting.enabled);

uint8_t storedMode = lightingPreferences.getUChar("mode", (uint8_t)lighting.mode);lighting.mode = storedMode == LIGHT_PATTERN ? LIGHT_PATTERN : LIGHT_STATIC;

uint8_t storedPattern = lightingPreferences.getUChar("pattern", (uint8_t)lighting.pattern);if (storedPattern <= PATTERN_OFF) {lighting.pattern = (LightingPattern)storedPattern;}

lighting.staticR = lightingPreferences.getUChar("static_r", lighting.staticR);lighting.staticG = lightingPreferences.getUChar("static_g", lighting.staticG);lighting.staticB = lightingPreferences.getUChar("static_b", lighting.staticB);lighting.staticW = lightingPreferences.getUChar("static_w", lighting.staticW);lighting.maxBrightness = constrain(lightingPreferences.getFloat("bright", lighting.maxBrightness),0.0f,1.0f);}

void saveLightingSettings() {if (!lightingPrefsReady) return;

lightingPreferences.putBool("enabled", lighting.enabled);lightingPreferences.putUChar("mode", (uint8_t)lighting.mode);lightingPreferences.putUChar("pattern", (uint8_t)lighting.pattern);lightingPreferences.putUChar("static_r", lighting.staticR);lightingPreferences.putUChar("static_g", lighting.staticG);lightingPreferences.putUChar("static_b", lighting.staticB);lightingPreferences.putUChar("static_w", lighting.staticW);lightingPreferences.putFloat("bright", lighting.maxBrightness);}

void markLightingSettingsDirty() {lightingSettingsDirty = true;lightingSettingsLastChangeMs = millis();}

String dashboardHtml() {return R"rawliteral(

    <circle class="outer-ring" cx="260" cy="260" r="252" />
    <circle class="inner-dial" cx="260" cy="260" r="205" style="fill:url(#dialGradientBoost)" />
    <circle class="black-mask" cx="260" cy="260" r="116" />
    <circle class="inner-ring" cx="260" cy="260" r="119" />
    <circle class="inner-ring" cx="260" cy="260" r="109" />

    <g id="boost-blue-arc"></g>
    <g id="boost-red-arc"></g>
    <g id="boost-ticks"></g>
    <g id="boost-labels"></g>
    <g id="iat-temp-arc"></g>
    <g id="iat-temp-ticks"></g>
    <g id="iat-temp-labels"></g>

    <line id="boostNeedle" class="needle-blue" x1="202.3" y1="149.0" x2="166.0" y2="79.0" />
    <line id="iatNeedle" class="needle-red temp-sub-needle" x1="260" y1="385" x2="260" y2="455" />

    <text class="center-value" id="cardash_mgp" x="260" y="252">0</text>
    <text class="unit" x="260" y="289">KPA</text>
    <text class="subtitle" x="260" y="330">MGP / BOOST</text>

    <text class="small-label temp-readout-label" x="260" y="410">IAT</text>
    <text class="small-label temp-readout-value" id="cardash_iat" x="260" y="435">0</text>
    <text class="small-label temp-readout-unit" x="260" y="458">°C</text>
  </svg>

  <svg id="rpmGauge" class="gauge" viewBox="0 0 520 520" role="img" aria-label="RPM gauge">
    <defs>
      <radialGradient id="dialGradientRpm" cx="50%" cy="50%" r="55%">
        <stop offset="0%" stop-color="#000" />
        <stop offset="55%" stop-color="rgb(var(--dial-rgb))" stop-opacity="0.22" />
        <stop offset="100%" stop-color="rgb(var(--dial-rgb))" stop-opacity="0.48" />
      </radialGradient>
      <radialGradient id="redGlow" cx="35%" cy="30%" r="72%">
        <stop offset="0%" stop-color="#ff2a2a" />
        <stop offset="55%" stop-color="#d00000" />
        <stop offset="100%" stop-color="#700000" />
      </radialGradient>
    </defs>

    <circle class="outer-ring" cx="260" cy="260" r="252" />
    <circle class="inner-dial" cx="260" cy="260" r="205" style="fill:url(#dialGradientRpm)" />
    <circle class="black-mask" cx="260" cy="260" r="116" />
    <circle class="inner-ring" cx="260" cy="260" r="119" />
    <circle class="inner-ring" cx="260" cy="260" r="109" />

    <g id="rpm-left-arc"></g>
    <g id="rpm-top-track"></g>
    <g id="rpm-red-arc"></g>
    <g id="rpm-ticks"></g>
    <g id="rpm-labels"></g>
    <g id="ect-temp-arc"></g>
    <g id="ect-temp-ticks"></g>
    <g id="ect-temp-labels"></g>

    <line id="rpmNeedle" class="needle-blue" x1="202.3" y1="149.0" x2="166.0" y2="79.0" />
    <line id="ectNeedle" class="needle-red temp-sub-needle" x1="260" y1="385" x2="260" y2="455" />

    <text class="center-value" id="cardash_rpm" x="260" y="252">0</text>
    <text class="unit" x="260" y="289">RPM</text>
    <text class="subtitle" x="260" y="116">RPM x 1000</text>

    <circle class="warning-light" id="rpmWarningLight" cx="212" cy="419" r="19" />

    <text class="small-label temp-readout-label" x="260" y="410">ECT</text>
    <text class="small-label temp-readout-value" id="cardash_ect" x="260" y="435">0</text>
    <text class="small-label temp-readout-unit" x="260" y="458">°C</text>
  </svg>
</main>

<div class="dash-aux">
  <div class="dash-pill" id="cardash_3v3_panel">
    <span>3.3V INTERNAL</span>
    <strong id="cardash_3v3">0.00</strong>
  </div>
  <div class="dash-pill" id="cardash_12v_panel">
    <span>12V INTERNAL</span>
    <strong id="cardash_12v">0.0</strong>
  </div>
  <div class="dash-pill battery-pill" id="cardash_batt_panel">
    <span>BATTERY</span>
    <div class="battery-pill-main">
      <strong id="cardash_batt_aux">0.0</strong>
      <div class="battery-level" aria-label="Battery level indicator">
        <div class="battery-level-fill" id="batteryLevelFill" style="width:0%"></div>
      </div>
    </div>
  </div>
</div>

<div class="grid">
  <div class="card wide" id="card_rpm">
    <div class="label">Engine Speed</div>
    <div class="value" id="rpm">0</div>
    <div class="unit">rpm</div>
  </div>

  <div class="card" id="card_mgp">
    <div class="label">MGP / Boost</div>
    <div class="value" id="mgp">0</div>
    <div class="unit">kPa gauge</div>
  </div>

  <div class="card" id="card_map">
    <div class="label">MAP</div>
    <div class="value" id="map">0</div>
    <div class="unit">kPa absolute</div>
  </div>

  <div class="card" id="card_lambda">
    <div class="label">Lambda</div>
    <div class="value" id="lambda1">0.00</div>
    <div class="unit">actual</div>
  </div>

  <div class="card" id="card_lambda_target">
    <div class="label">Lambda Target</div>
    <div class="value" id="lambda_target">0.00</div>
    <div class="unit">target</div>
  </div>

  <div class="card" id="card_ect">
    <div class="label">Coolant</div>
    <div class="value" id="ect">0</div>
    <div class="unit">°C</div>
  </div>

  <div class="card" id="card_oil_pressure">
    <div class="label">Oil Pressure</div>
    <div class="value" id="oil_pressure">0</div>
    <div class="unit">kPa</div>
  </div>

  <div class="card" id="card_battery">
    <div class="label">Battery</div>
    <div class="value" id="battery_v">0.0</div>
    <div class="unit">V</div>
  </div>

  <div class="card" id="card_tps">
    <div class="label">TPS Main</div>
    <div class="value" id="tps">0</div>
    <div class="unit">%</div>
  </div>
</div>

<div class="grid">
  <div class="card" id="card_health_ect">
    <div class="label">Coolant</div>
    <div class="value" id="health_ect">0</div>
    <div class="unit">°C</div>
  </div>

  <div class="card" id="card_iat">
    <div class="label">Intake Air Temp</div>
    <div class="value" id="iat">0</div>
    <div class="unit">°C</div>
  </div>

  <div class="card" id="card_oil_temp">
    <div class="label">Oil Temp</div>
    <div class="value" id="oil_temp">0</div>
    <div class="unit">°C</div>
  </div>

  <div class="card" id="card_health_oil_pressure">
    <div class="label">Oil Pressure</div>
    <div class="value" id="health_oil_pressure">0</div>
    <div class="unit">kPa</div>
  </div>

  <div class="card" id="card_fuel_pressure">
    <div class="label">Fuel Pressure</div>
    <div class="value" id="fuel_pressure">0</div>
    <div class="unit">kPa</div>
  </div>

  <div class="card" id="card_health_battery">
    <div class="label">Battery</div>
    <div class="value" id="health_battery_v">0.0</div>
    <div class="unit">V</div>
  </div>

  <div class="card" id="card_3v3">
    <div class="label">3.3V Internal</div>
    <div class="value" id="internal_3v3">0.00</div>
    <div class="unit">V</div>
  </div>

  <div class="card" id="card_12v">
    <div class="label">12V Internal</div>
    <div class="value" id="internal_12v">0.0</div>
    <div class="unit">V</div>
  </div>

  <div class="card" id="card_trig">
    <div class="label">Trig1 Err Counter</div>
    <div class="value" id="trig1_err">0</div>
    <div class="unit">count</div>
  </div>

  <div class="card" id="card_lambda_status">
    <div class="label">Lambda Status</div>
    <div class="value" id="lambda_status">0</div>
    <div class="unit">status code</div>
  </div>

  <div class="card" id="card_lambda_temp">
    <div class="label">Lambda Temp</div>
    <div class="value" id="lambda_temp">0</div>
    <div class="unit">°C</div>
  </div>
</div>

<div class="grid">
  <div class="card">
    <div class="label">Ignition Angle</div>
    <div class="value" id="ignition_angle">0.0</div>
    <div class="unit">deg</div>
  </div>

  <div class="card">
    <div class="label">Injection Actual PW</div>
    <div class="value" id="injection_actual_pw">0.0</div>
    <div class="unit">ms</div>
  </div>

  <div class="card">
    <div class="label">Injection Effective PW</div>
    <div class="value" id="injection_effective_pw">0.0</div>
    <div class="unit">ms</div>
  </div>

  <div class="card" id="card_lambda_error">
    <div class="label">Lambda Error</div>
    <div class="value" id="lambda_error">0.00</div>
    <div class="unit">λ</div>
  </div>

  <div class="card">
    <div class="label">Boost Target</div>
    <div class="value" id="boost_target">0</div>
    <div class="unit">kPa</div>
  </div>

  <div class="card" id="card_boost_error">
    <div class="label">Boost Target Error</div>
    <div class="value" id="boost_error">0</div>
    <div class="unit">kPa</div>
  </div>

  <div class="card">
    <div class="label">Boost P</div>
    <div class="smallvalue" id="boost_p">0.0</div>
    <div class="unit">proportional</div>
  </div>

  <div class="card">
    <div class="label">Boost I</div>
    <div class="smallvalue" id="boost_i">0.0</div>
    <div class="unit">integral</div>
  </div>

  <div class="card">
    <div class="label">Boost D</div>
    <div class="smallvalue" id="boost_d">0.0</div>
    <div class="unit">derivative</div>
  </div>

  <div class="card">
    <div class="label">Boost Duty</div>
    <div class="value" id="boost_duty">0</div>
    <div class="unit">%</div>
  </div>

  <div class="card">
    <div class="label">APS Main</div>
    <div class="value" id="aps_main">0</div>
    <div class="unit">%</div>
  </div>

  <div class="card">
    <div class="label">E-Throttle Target</div>
    <div class="value" id="throttle_target">0</div>
    <div class="unit">%</div>
  </div>

  <div class="card">
    <div class="label">VVT Inlet Target</div>
    <div class="value" id="vvt_in_target">0</div>
    <div class="unit">deg</div>
  </div>

  <div class="card">
    <div class="label">VVT Inlet Position</div>
    <div class="value" id="vvt_in_pos">0</div>
    <div class="unit">deg</div>
  </div>
</div>

<div class="grid">
  <div class="card wide">
    <div class="label">Lighting Enabled</div>
    <div class="lighting-row">
      <button onclick="setLightingEnabled(true)">On</button>
      <button onclick="setLightingEnabled(false)">Off</button>
    </div>
  </div>

  <div class="card wide">
    <div class="label">Mode</div>
    <select id="lighting_mode" onchange="applyLighting()">
      <option value="static">Static Colour</option>
      <option value="pattern">Pattern / Theme</option>
    </select>
  </div>

  <div class="card wide">
    <div class="label">Static Colour</div>
    <input type="color" id="static_color" value="#0050ff" onchange="applyLighting()">
  </div>

  <div class="card wide">
    <div class="label">Pattern / Theme</div>
    <select id="lighting_pattern" onchange="applyLighting()">
      <option value="engine_plasma">Engine Plasma</option>
      <option value="breathing">Breathing</option>
      <option value="rainbow">Rainbow</option>
      <option value="off">Off</option>
    </select>
  </div>

  <div class="card wide">
    <div class="label">Live Lighting Output</div>
    <div id="lighting_preview"
        style="height:80px;border-radius:14px;border:1px solid #555;background:#000;margin-bottom:10px;">
    </div>
    <div class="unit" id="lighting_preview_text">RGBW: 0, 0, 0, 0</div>
    <div class="unit" id="lighting_preview_mode">Mode: --</div>
  </div>

  <div class="card wide">
    <div class="label">Max Brightness</div>
    <input type="range" id="lighting_brightness" min="0" max="100" value="100" oninput="applyLighting()">
    <div class="unit"><span id="brightness_label">100</span>%</div>
  </div>

  <div class="card wide">
    <div class="label">Engine Plasma Mapping</div>
    <div class="unit">Brightness: 1000 rpm = 50%, 4800 rpm = 100%</div>
    <div class="unit">Colour: MGP &lt;15 = cold, MGP &gt;60 = hot</div>
  </div>
</div>

// ---------------------------------------------------------------------------// CAN helpers// ---------------------------------------------------------------------------const char* twaiStateName(twai_state_t state) {switch (state) {case TWAI_STATE_STOPPED:    return "STOPPED";case TWAI_STATE_RUNNING:    return "RUNNING";case TWAI_STATE_BUS_OFF:    return "BUS_OFF";case TWAI_STATE_RECOVERING: return "RECOVERING";default:                    return "UNKNOWN";}}

uint16_t readU16BE(const uint8_t* data, int index) {return ((uint16_t)data[index] << 8) | data[index + 1];}

int16_t readS16BE(const uint8_t* data, int index) {return (int16_t)readU16BE(data, index);}

void decodeLinkEcuFrame(const twai_message_t& msg) {if (msg.identifier != LINK_ECU_CAN_ID) return;if (msg.extd || msg.rtr) return;if (msg.data_length_code < 8) return;

uint16_t rpmRaw  = readU16BE(msg.data, 0);uint16_t mapRaw  = readU16BE(msg.data, 2);int16_t  mgpRaw  = readS16BE(msg.data, 4);uint16_t battRaw = readU16BE(msg.data, 6);

ecu.rpm = rpmRaw;ecu.map = mapRaw;ecu.mgp = mgpRaw;ecu.battery_v = battRaw / 100.0f;

// These dashboard values are not in the first CAN frame yet.// They intentionally remain at their default/previous values until later frames are added.

ecu.last_update_ms = millis();lastCanDecodedMs = ecu.last_update_ms;canDecodedFrameCount++;}

bool startCan() {if (canStarted) {twai_stop();twai_driver_uninstall();canStarted = false;}

// Normal mode is intentional. On a two-node test bus, the ECU needs another// active CAN node to ACK its transmitted frames.twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN,CAN_RX_PIN,TWAI_MODE_NORMAL);

g_config.rx_queue_len = 64;g_config.tx_queue_len = 4;

twai_timing_config_t t_config = TWAI_TIMING_CONFIG_1MBITS();twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

esp_err_t result = twai_driver_install(&g_config, &t_config, &f_config);if (result != ESP_OK) {Serial.print("TWAI driver install failed: 0x");Serial.println(result, HEX);return false;}

result = twai_start();if (result != ESP_OK) {Serial.print("TWAI start failed: 0x");Serial.println(result, HEX);twai_driver_uninstall();return false;}

canStarted = true;

Serial.println();Serial.println("TWAI/CAN started for LinkDash");Serial.print("CAN TX GPIO: ");Serial.println((int)CAN_TX_PIN);Serial.print("CAN RX GPIO: ");Serial.println((int)CAN_RX_PIN);Serial.println("CAN bitrate: 1 Mbit/s");Serial.println("CAN mode: normal / ACK enabled");Serial.println("Expected Link Stream ID: 0x3E8");Serial.println();

return true;}

void readCanFrames() {if (!canStarted) return;

twai_message_t msg;

while (twai_receive(&msg, 0) == ESP_OK) {canFrameCount++;lastCanFrameMs = millis();

decodeLinkEcuFrame(msg);

unsigned long now = millis();
if (msg.identifier == LINK_ECU_CAN_ID && now - lastCanSerialPrintMs >= 250) {
  lastCanSerialPrintMs = now;

  Serial.print("CAN 0x");
  Serial.print(msg.identifier, HEX);
  Serial.print(" DLC=");
  Serial.print(msg.data_length_code);
  Serial.print(" DATA:");

  for (int i = 0; i < msg.data_length_code; i++) {
    Serial.print(" ");
    if (msg.data[i] < 16) Serial.print("0");
    Serial.print(msg.data[i], HEX);
  }

  Serial.print(" | RPM=");
  Serial.print(ecu.rpm, 0);
  Serial.print(" MAP=");
  Serial.print(ecu.map, 1);
  Serial.print(" MGP=");
  Serial.print(ecu.mgp, 1);
  Serial.print(" Batt=");
  Serial.println(ecu.battery_v, 2);
}

}}

void handleCanStatus() {unsigned long now = millis();

twai_status_info_t twaiStatus;bool hasStatus = canStarted && twai_get_status_info(&twaiStatus) == ESP_OK;

long lastFrameAge = lastCanFrameMs > 0 ? (long)(now - lastCanFrameMs) : -1;long lastDecodedAge = lastCanDecodedMs > 0 ? (long)(now - lastCanDecodedMs) : -1;

String json = "{";json += ""started":" + String(canStarted ? "true" : "false") + ",";json += ""frames":" + String(canFrameCount) + ",";json += ""decoded_frames":" + String(canDecodedFrameCount) + ",";json += ""last_frame_age_ms":" + String(lastFrameAge) + ",";json += ""last_decoded_age_ms":" + String(lastDecodedAge) + ",";json += ""can_state":"" + String(hasStatus ? twaiStateName(twaiStatus.state) : "NOT_STARTED") + "",";json += ""tx_err":" + String(hasStatus ? twaiStatus.tx_error_counter : 0) + ",";json += ""rx_err":" + String(hasStatus ? twaiStatus.rx_error_counter : 0) + ",";json += ""rx_missed":" + String(hasStatus ? twaiStatus.rx_missed_count : 0) + ",";json += ""bus_error":" + String(hasStatus ? twaiStatus.bus_error_count : 0);json += "}";

server.send(200, "application/json", json);}

void handleRoot() {server.send(200, "text/html", dashboardHtml());}

void handleSetLighting() {if (server.hasArg("enabled")) {lighting.enabled = server.arg("enabled").toInt() == 1;}

if (server.hasArg("mode")) {String mode = server.arg("mode");

if (mode == "static") {
  lighting.mode = LIGHT_STATIC;
} else if (mode == "pattern") {
  lighting.mode = LIGHT_PATTERN;
}

}

if (server.hasArg("pattern")) {String pattern = server.arg("pattern");

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

if (server.hasArg("r")) lighting.staticR = constrain(server.arg("r").toInt(), 0, 255);if (server.hasArg("g")) lighting.staticG = constrain(server.arg("g").toInt(), 0, 255);if (server.hasArg("b")) lighting.staticB = constrain(server.arg("b").toInt(), 0, 255);if (server.hasArg("w")) lighting.staticW = constrain(server.arg("w").toInt(), 0, 255);

if (server.hasArg("brightness")) {lighting.maxBrightness = constrain(server.arg("brightness").toFloat(), 0.0, 1.0);}

updateLighting();markLightingSettingsDirty();

Serial.print("Lighting update | enabled=");Serial.print(lighting.enabled);Serial.print(" mode=");Serial.print(lighting.mode);Serial.print(" pattern=");Serial.print(lighting.pattern);Serial.print(" RGBW=");Serial.print(lighting.staticR);Serial.print(",");Serial.print(lighting.staticG);Serial.print(",");Serial.print(lighting.staticB);Serial.print(",");Serial.print(lighting.staticW);Serial.print(" brightness=");Serial.println(lighting.maxBrightness);

server.send(200, "application/json", "{"ok"}");}

void handleData() {unsigned long now = millis();unsigned long age = ecu.last_update_ms == 0 ? 999999 : now - ecu.last_update_ms;

String json = "{";json += ""rpm":" + String(ecu.rpm, 0) + ",";json += ""ect":" + String(ecu.ect, 1) + ",";json += ""iat":" + String(ecu.iat, 1) + ",";json += ""mgp":" + String(ecu.mgp, 1) + ",";json += ""map":" + String(ecu.map, 1) + ",";json += ""tps":" + String(ecu.tps, 1) + ",";

json += ""ignition_angle":" + String(ecu.ignition_angle, 1) + ",";json += ""injection_actual_pw":" + String(ecu.injection_actual_pw, 2) + ",";json += ""injection_effective_pw":" + String(ecu.injection_effective_pw, 2) + ",";

json += ""lambda1":" + String(ecu.lambda1, 3) + ",";json += ""lambda_target":" + String(ecu.lambda_target, 3) + ",";json += ""lambda_error":" + String(ecu.lambda_error, 3) + ",";json += ""lambda_status":" + String(ecu.lambda_status, 0) + ",";json += ""lambda_temp":" + String(ecu.lambda_temp, 1) + ",";

json += ""oil_temp":" + String(ecu.oil_temp, 1) + ",";json += ""battery_v":" + String(ecu.battery_v, 2) + ",";json += ""fuel_pressure":" + String(ecu.fuel_pressure, 1) + ",";json += ""oil_pressure":" + String(ecu.oil_pressure, 1) + ",";

json += ""boost_target":" + String(ecu.boost_target, 1) + ",";json += ""boost_error":" + String(ecu.boost_error, 1) + ",";json += ""boost_p":" + String(ecu.boost_p, 2) + ",";json += ""boost_i":" + String(ecu.boost_i, 2) + ",";json += ""boost_d":" + String(ecu.boost_d, 2) + ",";json += ""boost_duty":" + String(ecu.boost_duty, 1) + ",";

json += ""trig1_err":" + String(ecu.trig1_err, 0) + ",";json += ""internal_3v3":" + String(ecu.internal_3v3, 2) + ",";json += ""internal_12v":" + String(ecu.internal_12v, 2) + ",";

json += ""aps_main":" + String(ecu.aps_main, 1) + ",";json += ""throttle_target":" + String(ecu.throttle_target, 1) + ",";json += ""vvt_in_target":" + String(ecu.vvt_in_target, 1) + ",";json += ""vvt_in_pos":" + String(ecu.vvt_in_pos, 1) + ",";

json += ""age_ms":" + String(age) + ",";json += ""can_frames":" + String(canFrameCount) + ",";json += ""can_decoded_frames":" + String(canDecodedFrameCount) + ",";json += ""can_last_frame_age_ms":" + String(lastCanFrameMs > 0 ? (long)(now - lastCanFrameMs) : -1) + ",";json += ""can_last_decoded_age_ms":" + String(lastCanDecodedMs > 0 ? (long)(now - lastCanDecodedMs) : -1);json += "}";

server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");server.sendHeader("Pragma", "no-cache");server.sendHeader("Expires", "0");



server.send(200, "application/json", json);}

void updateFromValues(float values[], int count) {if (count > 0) ecu.rpm = values[0];if (count > 1) ecu.ect = values[1];if (count > 2) ecu.iat = values[2];if (count > 3) ecu.mgp = values[3];if (count > 4) ecu.map = values[4];if (count > 5) ecu.tps = values[5];

if (count > 6) ecu.ignition_angle = values[6];if (count > 7) ecu.injection_actual_pw = values[7];if (count > 8) ecu.injection_effective_pw = values[8];

if (count > 9) ecu.lambda1 = values[9];if (count > 10) ecu.lambda_target = values[10];if (count > 11) ecu.lambda_error = values[11];if (count > 12) ecu.lambda_status = values[12];if (count > 13) ecu.lambda_temp = values[13];

if (count > 14) ecu.oil_temp = values[14];if (count > 15) ecu.battery_v = values[15];if (count > 16) ecu.fuel_pressure = values[16];if (count > 17) ecu.oil_pressure = values[17];

if (count > 18) ecu.boost_target = values[18];if (count > 19) ecu.boost_error = values[19];if (count > 20) ecu.boost_p = values[20];if (count > 21) ecu.boost_i = values[21];if (count > 22) ecu.boost_d = values[22];if (count > 23) ecu.boost_duty = values[23];

if (count > 24) ecu.trig1_err = values[24];if (count > 25) ecu.internal_3v3 = values[25];if (count > 26) ecu.internal_12v = values[26];

if (count > 27) ecu.aps_main = values[27];if (count > 28) ecu.throttle_target = values[28];if (count > 29) ecu.vvt_in_target = values[29];if (count > 30) ecu.vvt_in_pos = values[30];

ecu.last_update_ms = millis();}

void readUdpPackets() {int packetSize = udp.parsePacket();if (!packetSize) return;

char buffer[512];int len = udp.read(buffer, sizeof(buffer) - 1);if (len <= 0) return;buffer[len] = '\0';

float values[31];int count = 0;

char* token = strtok(buffer, ",");

while (token != NULL && count < 31) {values[count] = atof(token);count++;token = strtok(NULL, ",");}

if (count >= 10) {updateFromValues(values, count);

Serial.print("RX fields: ");
Serial.print(count);
Serial.print(" | RPM: ");
Serial.print(ecu.rpm);
Serial.print(" | MAP: ");
Serial.print(ecu.map);
Serial.print(" | Lambda: ");
Serial.println(ecu.lambda1);

} else {Serial.print("Ignored short UDP packet: ");Serial.println(buffer);}}

uint8_t addClamp255(uint8_t a, uint8_t b) {int value = a + b;if (value > 255) return 255;return value;}

const char* lightingModeName() {if (lighting.mode == LIGHT_STATIC) return "static";if (lighting.mode == LIGHT_PATTERN) return "pattern";return "unknown";}

const char* lightingPatternName() {if (lighting.pattern == PATTERN_ENGINE_PLASMA) return "engine_plasma";if (lighting.pattern == PATTERN_BREATHING) return "breathing";if (lighting.pattern == PATTERN_RAINBOW) return "rainbow";if (lighting.pattern == PATTERN_OFF) return "off";return "unknown";}

void handleLightingState() {// Browser preview cannot truly show RGBW, so approximate W by adding it// into RGB for the preview patch. Raw RGBW values are also returned.uint8_t previewR = addClamp255(currentLightingOutput.r, currentLightingOutput.w);uint8_t previewG = addClamp255(currentLightingOutput.g, currentLightingOutput.w);uint8_t previewB = addClamp255(currentLightingOutput.b, currentLightingOutput.w);

String json = "{";json += ""enabled":" + String(lighting.enabled ? "true" : "false") + ",";json += ""mode":"" + String(lightingModeName()) + "",";json += ""pattern":"" + String(lightingPatternName()) + "",";json += ""max_brightness":" + String(lighting.maxBrightness, 3) + ",";json += ""static_r":" + String(lighting.staticR) + ",";json += ""static_g":" + String(lighting.staticG) + ",";json += ""static_b":" + String(lighting.staticB) + ",";json += ""static_w":" + String(lighting.staticW) + ",";

json += ""r":" + String(currentLightingOutput.r) + ",";json += ""g":" + String(currentLightingOutput.g) + ",";json += ""b":" + String(currentLightingOutput.b) + ",";json += ""w":" + String(currentLightingOutput.w) + ",";

json += ""preview_r":" + String(previewR) + ",";json += ""preview_g":" + String(previewG) + ",";json += ""preview_b":" + String(previewB) + ",";

json += ""rpm":" + String(ecu.rpm, 0) + ",";json += ""mgp":" + String(ecu.mgp, 1);json += "}";

server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");server.sendHeader("Pragma", "no-cache");server.sendHeader("Expires", "0");

server.send(200, "application/json", json);}



void setup() {Serial.begin(115200);delay(500);

lightingPrefsReady = lightingPreferences.begin("lighting", false);if (lightingPrefsReady) {loadLightingSettings();} else {Serial.println("Failed to open lighting preferences");}

setupLightingPwm();updateLighting();

startCan();

Serial.print("Addressable RGBW pixels: ");Serial.println(NEOPIXEL_COUNT);Serial.print("NeoPixel data pin: GPIO ");Serial.println(NEOPIXEL_PIN);

WiFi.mode(WIFI_AP);

IPAddress localIp(192, 168, 4, 1);IPAddress gateway(192, 168, 4, 1);IPAddress subnet(255, 255, 255, 0);

WiFi.softAPConfig(localIp, gateway, subnet);WiFi.softAP(AP_SSID, AP_PASS);

Serial.println();Serial.println("ESP32 dashboard AP started");Serial.print("SSID: ");Serial.println(AP_SSID);Serial.print("Dashboard: http://");Serial.println(WiFi.softAPIP());

udp.begin(UDP_PORT);

server.on("/", handleRoot);server.on("/data", handleData);server.on("/canStatus", handleCanStatus);server.on("/setLighting", handleSetLighting);server.on("/lightingState", handleLightingState);server.begin();

Serial.println("Live ECU source: CAN 1 / User Stream 1 / ID 0x3E8");Serial.print("UDP replay fallback still available on port ");Serial.println(UDP_PORT);}

void loop() {readCanFrames();

// UDP replay remains in the file as a fallback, but is disabled here so it// cannot overwrite live CAN values while testing in the car.// readUdpPackets();

server.handleClient();

static unsigned long lastLightingUpdateMs = 0;unsigned long now = millis();

if (now - lastLightingUpdateMs >= 30) {lastLightingUpdateMs = now;updateLighting();}

if (lightingSettingsDirty && now - lightingSettingsLastChangeMs >= LIGHTING_SAVE_DEBOUNCE_MS) {saveLightingSettings();lightingSettingsDirty = false;}}