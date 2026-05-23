//LinkDash
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <math.h>
#include <Adafruit_NeoPixel.h>
#include <driver/twai.h>

const char* AP_SSID = "LinkDash";
const char* AP_PASS = "linkdash123";  // minimum 8 characters

const uint16_t UDP_PORT = 4210;

// Addressable RGBW LED configuration.
// For bench testing with one NeoPixel RGBW Mini Button, leave count as 1.
// For a strip later, change NEOPIXEL_COUNT to the number of RGBW pixels.
const int NEOPIXEL_PIN = 13;
const uint16_t NEOPIXEL_COUNT = 10;

// Most RGBW NeoPixel / SK6812 parts use GRBW byte order.
// If red/green/blue/white appear wrong, try NEO_RGBW instead of NEO_GRBW.
Adafruit_NeoPixel pixels(
  NEOPIXEL_COUNT,
  NEOPIXEL_PIN,
  NEO_GRBW + NEO_KHZ800
);

WebServer server(80);
WiFiUDP udp;

// ---------------------------------------------------------------------------
// CAN / TWAI configuration for Link ECU CAN 1 User Stream 1
//
// Hardware:
//   ESP32 GPIO5 -> SN65HVD230 TXD / CTX
//   ESP32 GPIO4 -> SN65HVD230 RXD / CRX
//   ESP32 3V3   -> SN65HVD230 VCC
//   ESP32 GND   -> SN65HVD230 GND
//   CANH/CANL   -> Link ECU CANH/CANL
//
// ECU setup expected:
//   CAN 1, User Defined, 1 Mbit/s
//   Channel 1: Transmit User Stream 1, ID 0x3E8 / 1000 dec, Standard, 50 Hz
//   Stream 1 / Frame 1:
//     Bits 0-15   Engine Speed, Unsigned 16, MS First, multiplier 1, divider 1
//     Bits 16-31  MAP,          Unsigned 16, MS First, multiplier 1, divider 1
//     Bits 32-47  MGP,          Signed 16,   MS First, multiplier 1, divider 1
//     Bits 48-63  Batt Voltage, Unsigned 16, MS First, raw/100 = volts
// ---------------------------------------------------------------------------
const gpio_num_t CAN_TX_PIN = GPIO_NUM_5;
const gpio_num_t CAN_RX_PIN = GPIO_NUM_4;
static const uint32_t LINK_ECU_CAN_ID = 0x3E8U;

bool canStarted = false;
unsigned long canFrameCount = 0;
unsigned long canDecodedFrameCount = 0;
unsigned long lastCanFrameMs = 0;
unsigned long lastCanDecodedMs = 0;
unsigned long lastCanSerialPrintMs = 0;


struct EcuData {
  float rpm = 0;
  float ect = 0;
  float iat = 0;
  float mgp = 0;
  float map = 0;
  float tps = 0;

  float ignition_angle = 0;
  float injection_actual_pw = 0;
  float injection_effective_pw = 0;

  float lambda1 = 0;
  float lambda_target = 0;
  float lambda_error = 0;
  float lambda_status = 0;
  float lambda_temp = 0;

  float oil_temp = 0;
  float battery_v = 0;
  float fuel_pressure = 0;
  float oil_pressure = 0;

  float boost_target = 0;
  float boost_error = 0;
  float boost_p = 0;
  float boost_i = 0;
  float boost_d = 0;
  float boost_duty = 0;

  float trig1_err = 0;
  float internal_3v3 = 0;
  float internal_12v = 0;

  float aps_main = 0;
  float throttle_target = 0;
  float vvt_in_target = 0;
  float vvt_in_pos = 0;

  unsigned long last_update_ms = 0;
};

EcuData ecu;

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
};

LightingConfig lighting;

struct RgbwColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t w;
};

RgbwColor currentLightingOutput = {0, 0, 0, 0};

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
  float loadFactor = mapFloatClamped(ecu.map, 15.0, 90.0, 0.0, 1.0);

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
}

void setRgbw(RgbwColor c) {
  currentLightingOutput = c;

  // Addressable RGBW output.
  // For now every pixel receives the same colour. Later this can be extended
  // to zones, gradients, chases, warning flashes, etc.
  uint32_t packed = pixels.Color(c.r, c.g, c.b, c.w);

  for (uint16_t i = 0; i < NEOPIXEL_COUNT; i++) {
    pixels.setPixelColor(i, packed);
  }

  pixels.show();
}

void updateLighting() {
  if (!lighting.enabled) {
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

String dashboardHtml() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>Link ECU Dashboard</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">

  <style>
    :root {
      --bg: #0f1115;
      --panel: #1c2028;
      --panel2: #252b36;
      --text: #f2f4f8;
      --muted: #9da7b4;
      --warn: #6b3e00;
      --danger: #6b1515;
      --ok: #163f2a;
      --accent: #4da3ff;
      --cardash-voltage-label-height: 30px;
      --gauge-glow-blue: rgba(77, 163, 255, 0.5);
      --gauge-glow-cyan: rgba(40, 215, 255, 0.55);
      --gauge-glow-orange: rgba(255, 157, 77, 0.5);
    }

    body {
      margin: 0;
      font-family: Arial, Helvetica, sans-serif;
      background: var(--bg);
      color: var(--text);
    }

    header {
      padding: 12px 16px;
      background: #171a21;
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
      border-bottom: 1px solid #303642;
    }

    .title {
      font-size: 20px;
      font-weight: bold;
    }

    .status {
      font-size: 13px;
      color: var(--muted);
      text-align: right;
    }

    nav {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(90px, 1fr));
      gap: 8px;
      padding: 10px;
      background: #14171d;
      position: sticky;
      top: 0;
      z-index: 5;
    }

    button, select, input[type="color"], input[type="range"] {
      width: 100%;
      box-sizing: border-box;
    }

    button {
      border: 0;
      border-radius: 10px;
      padding: 12px 8px;
      background: var(--panel);
      color: var(--text);
      font-size: 15px;
      font-weight: bold;
    }

    button.active {
      background: var(--accent);
      color: #07111d;
    }

    select {
      border: 1px solid #303642;
      border-radius: 10px;
      padding: 12px;
      background: #111722;
      color: var(--text);
      font-size: 16px;
    }

    input[type="color"] {
      height: 52px;
      border: 0;
      border-radius: 10px;
      background: var(--panel2);
      padding: 4px;
    }

    input[type="range"] {
      margin-top: 12px;
    }

    .page {
      display: none;
      padding: 12px;
    }

    .page.active {
      display: block;
    }

    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(145px, 1fr));
      gap: 12px;
    }

    .card {
      background: var(--panel);
      border-radius: 14px;
      padding: 14px;
      text-align: center;
      border: 1px solid #303642;
      min-height: 92px;
    }

    .wide {
      grid-column: span 2;
    }

    .label {
      font-size: 13px;
      color: var(--muted);
      margin-bottom: 8px;
      white-space: nowrap;
    }

    .value {
      font-size: 34px;
      font-weight: bold;
      line-height: 1.05;
    }

    .smallvalue {
      font-size: 24px;
      font-weight: bold;
      line-height: 1.1;
    }

    .unit {
      font-size: 12px;
      color: var(--muted);
      margin-top: 5px;
    }

    .ok {
      background: var(--ok);
    }

    .warn {
      background: var(--warn);
    }

    .danger {
      background: var(--danger);
    }

    .summary {
      background: var(--panel2);
      border-radius: 14px;
      padding: 14px;
      margin-bottom: 12px;
      border: 1px solid #303642;
      font-size: 15px;
      color: var(--muted);
    }

    .alert {
      background: var(--danger);
      color: white;
      padding: 14px;
      margin: 12px;
      border-radius: 14px;
      text-align: center;
      font-weight: bold;
      font-size: 20px;
      display: none;
    }

    .lighting-row {
      display: flex;
      gap: 10px;
    }

    .cardash-wrap {
      display: grid;
      grid-template-columns: 1.3fr 1fr;
      gap: 12px;
    }

    .cardash-hero {
      background: radial-gradient(circle at top, #2b3545, #111722);
      border: 1px solid #303642;
      border-radius: 18px;
      padding: 18px;
      text-align: center;
    }

    .cardash-label {
      font-size: 13px;
      color: var(--muted);
      margin-bottom: 6px;
    }

    .cardash-rpm {
      font-size: 58px;
      font-weight: bold;
      line-height: 1;
    }

    .cardash-speed {
      font-size: 46px;
      font-weight: bold;
      line-height: 1;
    }

    .cardash-sub {
      color: var(--muted);
      font-size: 13px;
      margin-top: 6px;
    }

    .cardash-bar-bg {
      height: 18px;
      border-radius: 999px;
      background: #0b0d11;
      overflow: hidden;
      border: 1px solid #303642;
      margin-top: 12px;
    }

    .cardash-bar-fill {
      height: 100%;
      width: 0%;
      background: linear-gradient(90deg, #4da3ff, #a855f7, #f97316);
      border-radius: 999px;
    }

    .cardash-mini-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(130px, 1fr));
      gap: 12px;
      margin-top: 12px;
    }

    .cardash-mini {
      background: var(--panel);
      border: 1px solid #303642;
      border-radius: 14px;
      padding: 12px;
      text-align: center;
    }

    .cardash-mini .big {
      font-size: 28px;
      font-weight: bold;
    }

    .cardash-mini.warn {
      background: var(--warn);
    }

    .cardash-mini.danger {
      background: var(--danger);
    }

    @media (max-width: 720px) {
      .cardash-wrap {
        grid-template-columns: 1fr;
      }

      .cardash-rpm {
        font-size: 44px;
      }

      .cardash-speed {
        font-size: 38px;
      }
    }

    

    .cluster-wrap {
      padding: 10px;
    }

    .cluster-main {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 16px;
      align-items: stretch;
    }

    .cluster-footer {
      margin-top: 16px;
    }

    .cluster-temps {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 16px;
    }

    .cluster-volts {
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      gap: 12px;
    }

    .gauge-card {
      position: relative;
      overflow: hidden;
      background:
        radial-gradient(circle at 50% 16%, rgba(139, 205, 255, 0.22), transparent 34%),
        linear-gradient(180deg, #364251 0%, #161d28 10%, #0b0f15 100%);
      border: 1px solid #465364;
      border-radius: 26px;
      padding: 18px 16px 14px;
      text-align: center;
      box-shadow:
        inset 0 1px 0 rgba(255, 255, 255, 0.08),
        inset 0 -18px 30px rgba(0, 0, 0, 0.35),
        0 18px 28px rgba(0, 0, 0, 0.22);
    }

    .gauge-card::after {
      content: '';
      position: absolute;
      inset: 0;
      background: linear-gradient(180deg, rgba(255,255,255,0.08), transparent 26%, rgba(0,0,0,0.16));
      pointer-events: none;
    }

    .gauge-card::before {
      content: '';
      position: absolute;
      inset: 10px;
      border-radius: 22px;
      border: 1px solid rgba(124, 144, 165, 0.14);
      box-shadow: inset 0 0 35px rgba(0, 0, 0, 0.38);
      pointer-events: none;
    }

    .gauge-card.secondary {
      padding-top: 14px;
    }

    .gauge-title {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      min-width: 108px;
      padding: 6px 12px;
      border-radius: 999px;
      border: 1px solid rgba(111, 129, 149, 0.45);
      background: linear-gradient(180deg, rgba(29, 39, 52, 0.95), rgba(10, 14, 20, 0.95));
      box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.06);
      color: #ced6df;
      font-size: 11px;
      font-weight: bold;
      letter-spacing: 0.18em;
      margin-bottom: 8px;
      position: relative;
      z-index: 1;
    }

    .gauge-svg {
      width: 100%;
      max-width: 360px;
      height: auto;
      margin-top: 2px;
      position: relative;
      z-index: 1;
    }

    .gauge-card.secondary .gauge-svg {
      max-width: 250px;
    }

    .gauge-value {
      font-size: 40px;
      font-weight: bold;
      line-height: 1;
      position: relative;
      z-index: 1;
      letter-spacing: 0.08em;
      text-shadow: 0 0 14px rgba(255, 255, 255, 0.12);
    }

    .gauge-card.secondary .gauge-value {
      font-size: 30px;
    }

    .gauge-unit {
      color: #97a6b7;
      font-size: 11px;
      margin-top: 4px;
      position: relative;
      z-index: 1;
      text-transform: uppercase;
      letter-spacing: 0.2em;
    }

    .gauge-readout {
      width: min(150px, 72%);
      margin: -14px auto 0;
      padding: 10px 12px 11px;
      border-radius: 16px;
      border: 1px solid #45556a;
      background:
        linear-gradient(180deg, rgba(33, 46, 61, 0.98), rgba(8, 12, 18, 0.98)),
        linear-gradient(90deg, rgba(77, 163, 255, 0.18), transparent 55%);
      box-shadow:
        inset 0 1px 0 rgba(255, 255, 255, 0.06),
        inset 0 0 26px rgba(0, 0, 0, 0.4),
        0 10px 18px rgba(0, 0, 0, 0.22);
      position: relative;
      z-index: 1;
    }

    .gauge-card.secondary .gauge-readout {
      width: min(128px, 76%);
      margin-top: -10px;
      padding-top: 9px;
      padding-bottom: 9px;
    }

    .gauge-caption {
      color: #677789;
      font-size: 10px;
      letter-spacing: 0.22em;
      text-transform: uppercase;
      margin-top: 9px;
      position: relative;
      z-index: 1;
    }

    .gauge-bezel-outer {
      fill: #45505d;
      stroke: #718095;
      stroke-width: 2;
    }

    .gauge-bezel-inner {
      fill: #1c242f;
      stroke: #0d1117;
      stroke-width: 6;
    }

    .gauge-face {
      fill: #0c121a;
      stroke: #2e3948;
      stroke-width: 2;
    }

    .gauge-face-ring {
      fill: none;
      stroke: rgba(159, 181, 204, 0.08);
      stroke-width: 1.5;
    }

    .gauge-glare {
      fill: rgba(159, 209, 255, 0.06);
    }

    .gauge-scale {
      fill: #93a1b1;
      font-size: 10px;
      font-weight: bold;
      letter-spacing: 0.05em;
    }

    .gauge-scale-alert {
      fill: #ffb38c;
    }

    .gauge-center-label {
      fill: #6c7b8b;
      font-size: 9px;
      font-weight: bold;
      letter-spacing: 0.18em;
    }

    .gauge-status-line {
      stroke: rgba(115, 134, 156, 0.32);
      stroke-width: 1;
    }

    .gauge-tick {
      stroke: #8f9eb0;
      stroke-width: 2.2;
    }

    .gauge-tick.minor {
      stroke-width: 1.3;
      opacity: 0.55;
    }

    .gauge-arc-bg {
      fill: none;
      stroke: #28323e;
      stroke-width: 10;
      stroke-linecap: round;
    }

    .gauge-arc-active {
      fill: none;
      stroke: #4da3ff;
      stroke-width: 10;
      stroke-linecap: round;
      filter: drop-shadow(0 0 7px var(--gauge-glow-blue));
    }

    .gauge-zone {
      fill: none;
      stroke-linecap: round;
      opacity: 0.92;
    }

    .gauge-zone-warn {
      stroke: #ffb14d;
      stroke-width: 8;
    }

    .gauge-zone-danger {
      stroke: #ff5a5a;
      stroke-width: 8;
    }

    .boost-arc {
      stroke: #28d7ff;
      filter: drop-shadow(0 0 8px var(--gauge-glow-cyan));
    }

    .temp-arc {
      stroke: #ff9d4d;
      filter: drop-shadow(0 0 8px var(--gauge-glow-orange));
    }

    .needle-pack {
      transform-origin: 100px 100px;
      transition: transform 80ms linear;
    }

    .gauge-needle-shadow {
      stroke: rgba(0, 0, 0, 0.42);
      stroke-width: 7;
      stroke-linecap: round;
    }

    .gauge-needle {
      stroke: #f2f4f8;
      stroke-width: 4.4;
      stroke-linecap: round;
    }

    .gauge-needle-tail {
      stroke: rgba(242, 244, 248, 0.4);
      stroke-width: 3;
      stroke-linecap: round;
    }

    .boost-needle {
      stroke: #8ef2ff;
    }

    .temp-needle {
      stroke: #ffd7a8;
      stroke-width: 3.5;
    }

    .gauge-hub {
      fill: #f2f4f8;
    }

    .gauge-hub-ring {
      fill: none;
      stroke: rgba(242, 244, 248, 0.4);
      stroke-width: 2.5;
    }

    .gauge-redline {
      fill: none;
      stroke: #ff4d4d;
      stroke-width: 7;
      stroke-linecap: round;
    }

    .secondary-gauge-base {
      fill: none;
      stroke: rgba(115, 134, 156, 0.45);
      stroke-width: 2.5;
      stroke-linecap: round;
    }

    .secondary-gauge-tick {
      stroke: rgba(143, 158, 176, 0.7);
      stroke-width: 1.6;
      stroke-linecap: round;
    }

    .secondary-needle-pack {
      transform-origin: 100px 160px;
      transition: transform 80ms linear;
    }

    .secondary-needle-shadow {
      stroke: rgba(0, 0, 0, 0.38);
      stroke-width: 3.5;
      stroke-linecap: round;
    }

    .secondary-needle {
      stroke: #ffd7a8;
      stroke-width: 2.2;
      stroke-linecap: round;
    }

    .secondary-hub {
      fill: #f2f4f8;
    }

    .secondary-label {
      fill: #6c7b8b;
      font-size: 7px;
      font-weight: bold;
      letter-spacing: 0.2em;
    }

    .secondary-value {
      fill: #cfd8e2;
      font-size: 9px;
      font-weight: bold;
      letter-spacing: 0.08em;
    }

    .gauge-card.warn .secondary-value {
      fill: #ffd07f;
    }

    .gauge-card.danger .secondary-value {
      fill: #ff8f8f;
    }

    .voltage-card {
      background: linear-gradient(180deg, #1b2230, #10151d);
      border: 1px solid #303642;
      border-radius: 18px;
      padding: 14px 12px;
      text-align: center;
    }

    .voltage-card.warn {
      background: linear-gradient(180deg, #5f4311, #31210a);
    }

    .voltage-card.danger {
      background: linear-gradient(180deg, #5d1b1b, #2d0d0d);
    }

    .voltage-label {
      color: var(--muted);
      font-size: 11px;
      font-weight: bold;
      letter-spacing: 0.08em;
      margin-top: 8px;
      min-height: var(--cardash-voltage-label-height);
    }

    .voltage-value {
      font-size: 28px;
      font-weight: bold;
      line-height: 1.1;
      margin-top: 6px;
    }

    .voltage-icon {
      width: 56px;
      height: 56px;
      margin: 0 auto;
      border-radius: 16px;
      display: flex;
      align-items: center;
      justify-content: center;
      background: rgba(7, 11, 17, 0.9);
      border: 1px solid #303642;
      color: #8bd5ff;
      box-shadow: inset 0 0 18px rgba(0, 0, 0, 0.45);
    }

    .voltage-icon svg {
      width: 30px;
      height: 30px;
      fill: none;
      stroke: currentColor;
      stroke-width: 1.8;
      stroke-linecap: round;
      stroke-linejoin: round;
    }

    @media (max-width: 720px) {
      .cluster-main,
      .cluster-temps {
        grid-template-columns: 1fr;
      }

      .cluster-volts {
        grid-template-columns: repeat(3, minmax(0, 1fr));
      }

      .gauge-value {
        font-size: 34px;
      }
    }

    @media (max-width: 520px) {
      .cluster-volts {
        grid-template-columns: 1fr;
      }
    }


    .lighting-row button {
      flex: 1;
    }

    @media (max-width: 480px) {
      .value {
        font-size: 28px;
      }

      .wide {
        grid-column: span 1;
      }

      header {
        display: block;
      }

      .status {
        text-align: left;
        margin-top: 4px;
      }
        
      .fullscreen_btn {
         width: auto;
         padding: 8px 12px;
         font-size: 13px;
      }       
    }
  </style>
</head>

<body>
  <header>
    <div class="title">Link ECU Dashboard</div>
    <div style="display:flex;gap:8px;align-items:center;">
      <button id="fullscreen_btn" onclick="goFullscreen()">Fullscreen</button>
      <div class="status" id="status">Waiting for data...</div>
    </div>
  </header>

  <div class="alert" id="main_alert"></div>

  <nav>
    <button id="btn_cardash" class="active" onclick="showPage('cardash')">CarDash</button>
    <button id="btn_driving" onclick="showPage('driving')">Driving</button>
    <button id="btn_health" onclick="showPage('health')">Health</button>
    <button id="btn_debug" onclick="showPage('debug')">Debug</button>
    <button id="btn_lighting" onclick="showPage('lighting')">Lighting</button>
  </nav>

  <section id="page_cardash" class="page active">
    <div class="cluster-wrap">

      <div class="cluster-main">
        <div class="gauge-card" id="cardash_rpm_panel">
          <!-- <div class="gauge-title">TACHOMETER</div> -->

          <svg class="gauge-svg" viewBox="0 0 200 200" role="img" aria-label="Tachometer gauge showing engine speed with a warning zone and redline">
            <circle class="gauge-bezel-outer" cx="100" cy="100" r="88" />
            <circle class="gauge-bezel-inner" cx="100" cy="100" r="82" />
            <circle class="gauge-face" cx="100" cy="100" r="74" />
            <circle class="gauge-face-ring" cx="100" cy="100" r="68" />
            <ellipse class="gauge-glare" cx="100" cy="62" rx="46" ry="18" />

            <path class="gauge-arc-bg" d="M 35 145 A 75 75 0 1 1 165 145" />
            <!--  <path class="gauge-zone gauge-zone-warn" d="M 138 42 A 75 75 0 0 1 151 56"> -->
              <title>Warning zone approaching engine redline</title>
            </path>
            <path class="gauge-redline" d="M 143 45 A 75 75 0 0 1 165 145">
              <title>Danger zone at engine redline</title>
            </path>
            <path class="gauge-arc-active" id="tach_arc" d="M 35 145 A 75 75 0 1 1 165 145" />

            <line class="gauge-tick minor" x1="43" y1="116" x2="49" y2="113" />
            <line class="gauge-tick minor" x1="61" y1="56" x2="65" y2="62" />
            <line class="gauge-tick minor" x1="84" y1="31" x2="86" y2="38" />
            <line class="gauge-tick" x1="35" y1="145" x2="43" y2="137" />
            <line class="gauge-tick" x1="55" y1="70" x2="64" y2="77" />
            <line class="gauge-tick" x1="100" y1="25" x2="100" y2="37" />
            <line class="gauge-tick" x1="145" y1="70" x2="136" y2="77" />
            <line class="gauge-tick" x1="165" y1="145" x2="157" y2="137" />
            <line class="gauge-tick minor" x1="116" y1="31" x2="114" y2="38" />
            <line class="gauge-tick minor" x1="139" y1="56" x2="135" y2="62" />
            <line class="gauge-tick minor" x1="157" y1="116" x2="151" y2="113" />

            <text class="gauge-scale" x="31" y="162">0</text>
            <text class="gauge-scale" x="47" y="68">2</text>
            <text class="gauge-scale" x="96" y="22">4</text>
            <text class="gauge-scale" x="143" y="68">6</text>
            <text class="gauge-scale gauge-scale-alert" x="164" y="162">8</text>

            <line class="gauge-status-line" x1="66" y1="127" x2="134" y2="127" />
            <text class="gauge-center-label" x="100" y="119" text-anchor="middle">RPM x 1000</text>

            <path class="secondary-gauge-base" d="M 68 160 A 32 32 0 0 1 132 160" />
            <line class="secondary-gauge-tick" x1="68" y1="160" x2="73" y2="157" />
            <line class="secondary-gauge-tick" x1="100" y1="128" x2="100" y2="134" />
            <line class="secondary-gauge-tick" x1="132" y1="160" x2="127" y2="157" />
            <text class="secondary-label" x="100" y="150" text-anchor="middle">COOLANT</text>
            <g id="ect_secondary_needle" class="secondary-needle-pack">
              <line class="secondary-needle-shadow" x1="100.8" y1="160.8" x2="100.8" y2="136.8" />
              <line class="secondary-needle" x1="100" y1="160" x2="100" y2="136" />
              <line class="secondary-needle" x1="100" y1="161" x2="100" y2="169" />
            </g>
            <circle class="secondary-hub" cx="100" cy="160" r="2.6" />
            <text class="secondary-value" id="cardash_ect_sub" x="100" y="178" text-anchor="middle">0°C</text>

            <g id="tach_needle" class="needle-pack">
              <line class="gauge-needle-shadow" x1="101" y1="102" x2="101" y2="45" />
              <line class="gauge-needle" x1="100" y1="100" x2="100" y2="42" />
              <line class="gauge-needle-tail" x1="100" y1="103" x2="100" y2="122" />
            </g>
            <circle class="gauge-hub-ring" cx="100" cy="100" r="12" />
            <circle class="gauge-hub" cx="100" cy="100" r="7" />
          </svg>

          <div class="gauge-readout">
            <div class="gauge-value" id="cardash_rpm">0</div>
            <div class="gauge-unit">rpm</div>
          </div>
          <div class="gauge-caption">engine speed</div>
        </div>

        <div class="gauge-card" id="cardash_mgp_panel">
          <!-- <div class="gauge-title">BOOST / MGP</div> -->

          <svg class="gauge-svg" viewBox="0 0 200 200" role="img" aria-label="Boost gauge showing manifold gauge pressure with warning and danger zones">
            <circle class="gauge-bezel-outer" cx="100" cy="100" r="88" />
            <circle class="gauge-bezel-inner" cx="100" cy="100" r="82" />
            <circle class="gauge-face" cx="100" cy="100" r="74" />
            <circle class="gauge-face-ring" cx="100" cy="100" r="68" />
            <ellipse class="gauge-glare" cx="100" cy="62" rx="46" ry="18" />

            <path class="gauge-arc-bg" d="M 35 145 A 75 75 0 1 1 165 145" />
            <!--  <path class="gauge-zone gauge-zone-warn" d="M 132 38 A 75 75 0 0 1 149 54"> -->
              <title>Warning zone for elevated boost pressure</title>
            </path>
            <path class="gauge-zone gauge-zone-danger" d="M 149 54 A 75 75 0 0 1 165 145">
              <title>Danger zone for excessive boost pressure</title>
            </path>
            <path class="gauge-arc-active boost-arc" id="boost_arc" d="M 35 145 A 75 75 0 1 1 165 145" />

            <line class="gauge-tick minor" x1="43" y1="116" x2="49" y2="113" />
            <line class="gauge-tick minor" x1="61" y1="56" x2="65" y2="62" />
            <line class="gauge-tick minor" x1="84" y1="31" x2="86" y2="38" />
            <line class="gauge-tick" x1="35" y1="145" x2="43" y2="137" />
            <line class="gauge-tick" x1="55" y1="70" x2="64" y2="77" />
            <line class="gauge-tick" x1="100" y1="25" x2="100" y2="37" />
            <line class="gauge-tick" x1="145" y1="70" x2="136" y2="77" />
            <line class="gauge-tick" x1="165" y1="145" x2="157" y2="137" />
            <line class="gauge-tick minor" x1="116" y1="31" x2="114" y2="38" />
            <line class="gauge-tick minor" x1="139" y1="56" x2="135" y2="62" />
            <line class="gauge-tick minor" x1="157" y1="116" x2="151" y2="113" />

            <text class="gauge-scale" x="20" y="162">-100</text>
            <text class="gauge-scale" x="48" y="68">0</text>
            <text class="gauge-scale" x="88" y="22">100</text>
            <text class="gauge-scale" x="130" y="68">200</text>
            <text class="gauge-scale" x="152" y="162">250</text>

            <line class="gauge-status-line" x1="62" y1="127" x2="138" y2="127" />
            <text class="gauge-center-label" x="100" y="119" text-anchor="middle">BOOST PRESSURE</text>

            <path class="secondary-gauge-base" d="M 68 160 A 32 32 0 0 1 132 160" />
            <line class="secondary-gauge-tick" x1="68" y1="160" x2="73" y2="157" />
            <line class="secondary-gauge-tick" x1="100" y1="128" x2="100" y2="134" />
            <line class="secondary-gauge-tick" x1="132" y1="160" x2="127" y2="157" />
            <text class="secondary-label" x="100" y="150" text-anchor="middle">INTAKE</text>
            <g id="iat_secondary_needle" class="secondary-needle-pack">
              <line class="secondary-needle-shadow" x1="100.8" y1="160.8" x2="100.8" y2="136.8" />
              <line class="secondary-needle" x1="100" y1="160" x2="100" y2="136" />
              <line class="secondary-needle" x1="100" y1="161" x2="100" y2="169" />
            </g>
            <circle class="secondary-hub" cx="100" cy="160" r="2.6" />
            <text class="secondary-value" id="cardash_iat_sub" x="100" y="178" text-anchor="middle">0°C</text>

            <g id="boost_needle" class="needle-pack">
              <line class="gauge-needle-shadow" x1="101" y1="102" x2="101" y2="45" />
              <line class="gauge-needle boost-needle" x1="100" y1="100" x2="100" y2="42" />
              <line class="gauge-needle-tail" x1="100" y1="103" x2="100" y2="122" />
            </g>
            <circle class="gauge-hub-ring" cx="100" cy="100" r="12" />
            <circle class="gauge-hub" cx="100" cy="100" r="7" />
          </svg>

          <div class="gauge-readout">
            <div class="gauge-value" id="cardash_mgp">0</div>
            <div class="gauge-unit">kPa gauge</div>
          </div>
          <div class="gauge-caption">turbo load</div>
        </div>
      </div>

      <div class="cluster-footer">
        <div class="cluster-volts">
          <div class="voltage-card" id="cardash_3v3_panel">
            <div class="voltage-icon">
              <svg viewBox="0 0 24 24" aria-hidden="true">
                <rect x="6" y="6" width="12" height="12" rx="2" />
                <path d="M9 3v3M12 3v3M15 3v3M9 18v3M12 18v3M15 18v3M3 9h3M3 12h3M3 15h3M18 9h3M18 12h3M18 15h3" />
              </svg>
            </div>
            <div class="voltage-label">3.3V INTERNAL</div>
            <div class="voltage-value" id="cardash_3v3">0.00</div>
            <div class="gauge-unit">V</div>
          </div>

          <div class="voltage-card" id="cardash_12v_panel">
            <div class="voltage-icon">
              <svg viewBox="0 0 24 24" aria-hidden="true">
                <path d="M13 2 6 13h5l-1 9 8-12h-5l1-8Z" />
              </svg>
            </div>
            <div class="voltage-label">12V INTERNAL</div>
            <div class="voltage-value" id="cardash_12v">0.0</div>
            <div class="gauge-unit">V</div>
          </div>

          <div class="voltage-card" id="cardash_batt_panel">
            <div class="voltage-icon">
              <svg viewBox="0 0 24 24" aria-hidden="true">
                <rect x="3" y="7" width="16" height="10" rx="2" />
                <path d="M19 10h2v4h-2M8 10v4M6 12h4" />
              </svg>
            </div>
            <div class="voltage-label">BATTERY</div>
            <div class="voltage-value" id="cardash_batt">0.0</div>
            <div class="gauge-unit">V</div>
          </div>
        </div>
      </div>
    </div>
  </section>

  <section id="page_driving" class="page">
    <div class="summary" id="driving_summary">Main live driving values.</div>

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
  </section>

  <section id="page_health" class="page">
    <div class="summary">Engine health, sensor sanity, voltage rails, and trigger/lambda status.</div>

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
  </section>

  <section id="page_debug" class="page">
    <div class="summary">Tuning and control-loop debug values.</div>

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
  </section>

  <section id="page_lighting" class="page">
    <div class="summary">Cabin RGBW lighting controls.</div>

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
  </section>

<script>
function hexToRgb(hex) {
  const clean = hex.replace('#', '');
  return {
    r: parseInt(clean.substring(0, 2), 16),
    g: parseInt(clean.substring(2, 4), 16),
    b: parseInt(clean.substring(4, 6), 16)
  };
}

async function goFullscreen() {
  const el = document.documentElement;

  try {
    if (el.requestFullscreen) {
      await el.requestFullscreen();
    } else if (el.webkitRequestFullscreen) {
      await el.webkitRequestFullscreen();
    }
  } catch (err) {
    console.log('Fullscreen request failed', err);
  }
}

async function applyLighting() {
  const mode = document.getElementById('lighting_mode').value;
  const pattern = document.getElementById('lighting_pattern').value;
  const color = hexToRgb(document.getElementById('static_color').value);
  const brightnessPct = Number(document.getElementById('lighting_brightness').value);
  const brightness = brightnessPct / 100.0;

  document.getElementById('brightness_label').textContent = brightnessPct;

  const url =
    '/setLighting?' +
    'enabled=1' +
    '&mode=' + encodeURIComponent(mode) +
    '&pattern=' + encodeURIComponent(pattern) +
    '&r=' + color.r +
    '&g=' + color.g +
    '&b=' + color.b +
    '&w=0' +
    '&brightness=' + brightness;

  await fetch(url);
}

async function setLightingEnabled(enabled) {
  await fetch('/setLighting?enabled=' + (enabled ? '1' : '0'));
}

function showPage(name) {
  const pages = ['cardash', 'driving', 'health', 'debug', 'lighting'];

  for (const p of pages) {
    document.getElementById('page_' + p).classList.remove('active');
    document.getElementById('btn_' + p).classList.remove('active');
  }

  document.getElementById('page_' + name).classList.add('active');
  document.getElementById('btn_' + name).classList.add('active');
}

function setText(id, value, decimals = 0) {
  const el = document.getElementById(id);
  if (!el) return;

  const n = Number(value);
  if (!Number.isFinite(n)) {
    el.textContent = '--';
    return;
  }

  el.textContent = n.toFixed(decimals);
}

function setCardState(id, state) {
  const el = document.getElementById(id);
  if (!el) return;

  el.classList.remove('ok', 'warn', 'danger');

  if (state) {
    el.classList.add(state);
  }
}

function updateWarnings(d) {
  const stale = d.age_ms > 1500;

  const oilPressureDanger = d.rpm > 1500 && d.oil_pressure < 150;
  const ectWarn = d.ect >= 100 && d.ect < 110;
  const ectDanger = d.ect >= 110;
  const battWarn = d.battery_v > 0 && d.battery_v < 12.2;
  const lambdaDanger = d.map > 120 && d.lambda1 > d.lambda_target + 0.08;
  const fuelPressureDanger = d.rpm > 1500 && d.fuel_pressure > 0 && d.fuel_pressure < 250;
  const trigDanger = d.trig1_err > 0;

  setCardState('card_ect', ectDanger ? 'danger' : ectWarn ? 'warn' : null);
  setCardState('card_health_ect', ectDanger ? 'danger' : ectWarn ? 'warn' : null);

  setCardState('card_oil_pressure', oilPressureDanger ? 'danger' : null);
  setCardState('card_health_oil_pressure', oilPressureDanger ? 'danger' : null);

  setCardState('card_battery', battWarn ? 'warn' : null);
  setCardState('card_health_battery', battWarn ? 'warn' : null);

  setCardState('card_lambda', lambdaDanger ? 'danger' : null);
  setCardState('card_fuel_pressure', fuelPressureDanger ? 'danger' : null);
  setCardState('card_trig', trigDanger ? 'danger' : null);

  setCardState('card_lambda_error', Math.abs(d.lambda_error) > 0.08 ? 'warn' : null);
  setCardState('card_boost_error', Math.abs(d.boost_error) > 20 ? 'warn' : null);

  const alertBox = document.getElementById('main_alert');

  let alerts = [];

  if (stale) alerts.push('DATA STALE');
  if (oilPressureDanger) alerts.push('LOW OIL PRESSURE');
  if (ectDanger) alerts.push('HIGH COOLANT TEMP');
  if (lambdaDanger) alerts.push('LEAN ON BOOST');
  if (fuelPressureDanger) alerts.push('LOW FUEL PRESSURE');
  if (trigDanger) alerts.push('TRIGGER ERROR');

  if (alerts.length > 0) {
    alertBox.textContent = alerts.join(' | ');
    alertBox.style.display = 'block';
  } else {
    alertBox.style.display = 'none';
  }
}

async function refreshLightingState() {
  try {
    const res = await fetch('/lightingState');
    const s = await res.json();

    const preview = document.getElementById('lighting_preview');
    const text = document.getElementById('lighting_preview_text');
    const mode = document.getElementById('lighting_preview_mode');

    if (!preview || !text || !mode) return;

    preview.style.backgroundColor =
      'rgb(' + s.preview_r + ',' + s.preview_g + ',' + s.preview_b + ')';

    text.textContent =
      'RGBW output: ' + s.r + ', ' + s.g + ', ' + s.b + ', ' + s.w +
      ' | Preview RGB: ' + s.preview_r + ', ' + s.preview_g + ', ' + s.preview_b;

    mode.textContent =
      'Enabled: ' + s.enabled +
      ' | Mode: ' + s.mode +
      ' | Pattern: ' + s.pattern +
      ' | RPM: ' + Math.round(s.rpm) +
      ' | MGP: ' + Number(s.mgp).toFixed(1);
  } catch (err) {
    const text = document.getElementById('lighting_preview_text');
    if (text) text.textContent = 'Lighting preview fetch error';
  }
}

function clamp(value, min, max) {
  return Math.min(Math.max(value, min), max);
}

function valueToNeedleAngle(value, min, max) {
  const pct = clamp((value - min) / (max - min), 0, 1);

  // Gauge sweep: -130° left to +130° right.
  return -130 + pct * 260;
}

function setGaugeArc(id, value, min, max) {
  const arc = document.getElementById(id);
  if (!arc) return;

  const pct = clamp((value - min) / (max - min), 0, 1);
  const length = arc.getTotalLength();
  arc.style.strokeDasharray = length;
  arc.style.strokeDashoffset = length * (1 - pct);
}

function setGaugeNeedle(id, value, min, max) {
  const needle = document.getElementById(id);
  if (!needle) return;

  const angle = valueToNeedleAngle(value, min, max);
  needle.style.transform = 'rotate(' + angle + 'deg)';
}

function setMiniPanelState(id, state) {
  const el = document.getElementById(id);
  if (!el) return;

  el.classList.remove('warn', 'danger');
  if (state) el.classList.add(state);
}

function updateCarDash(d) {
  const rpm = Number(d.rpm) || 0;
  const mgp = Number(d.mgp) || 0;
  const ect = Number(d.ect) || 0;
  const iat = Number(d.iat) || 0;
  const battery = Number(d.battery_v) || 0;
  const internal3v3 = Number(d.internal_3v3) || 0;
  const internal12v = Number(d.internal_12v) || 0;

  setText('cardash_rpm', rpm, 0);
  setGaugeNeedle('tach_needle', rpm, 0, 8000);
  setGaugeArc('tach_arc', rpm, 0, 8000);

  setText('cardash_mgp', mgp, 0);
  setGaugeNeedle('boost_needle', mgp, -100, 250);
  setGaugeArc('boost_arc', mgp, -100, 250);

  setText('cardash_ect_sub', ect, 0, '°C');
  setGaugeNeedle('ect_secondary_needle', ect, 0, 140);

  setText('cardash_iat_sub', iat, 0, '°C');
  setGaugeNeedle('iat_secondary_needle', iat, 0, 140);

  setText('cardash_3v3', internal3v3, 2);
  setText('cardash_12v', internal12v, 1);
  setText('cardash_batt', battery, 1);

  const ectWarn = ect >= 100 && ect < 110;
  const ectDanger = ect >= 110;
  const iatWarn = iat >= 60 && iat < 80;
  const iatDanger = iat >= 80;
  const batteryWarnThreshold = 12.2;
  const batteryDangerThreshold = 11.5;
  const internal3v3WarnLow = 3.15;
  const internal3v3WarnHigh = 3.45;
  const internal3v3DangerLow = 3.0;
  const internal3v3DangerHigh = 3.6;
  const internal12vWarnLow = 11.5;
  const internal12vWarnHigh = 13.5;
  const internal12vDangerLow = 10.5;
  const internal12vDangerHigh = 14.5;

  const battWarn = battery > 0 && battery < batteryWarnThreshold;
  const battDanger = battery > 0 && battery < batteryDangerThreshold;
  const rail3v3Warn =
    internal3v3 > 0 && (internal3v3 < internal3v3WarnLow || internal3v3 > internal3v3WarnHigh);
  const rail3v3Danger =
    internal3v3 > 0 && (internal3v3 < internal3v3DangerLow || internal3v3 > internal3v3DangerHigh);
  const rail12vWarn =
    internal12v > 0 && (internal12v < internal12vWarnLow || internal12v > internal12vWarnHigh);
  const rail12vDanger =
    internal12v > 0 && (internal12v < internal12vDangerLow || internal12v > internal12vDangerHigh);

  setMiniPanelState('cardash_rpm_panel', ectDanger ? 'danger' : ectWarn ? 'warn' : null);
  setMiniPanelState('cardash_mgp_panel', iatDanger ? 'danger' : iatWarn ? 'warn' : null);
  setMiniPanelState('cardash_batt_panel', battDanger ? 'danger' : battWarn ? 'warn' : null);
  setMiniPanelState('cardash_3v3_panel', rail3v3Danger ? 'danger' : rail3v3Warn ? 'warn' : null);
  setMiniPanelState('cardash_12v_panel', rail12vDanger ? 'danger' : rail12vWarn ? 'warn' : null);
}


async function refreshData() {
  try {
    const res = await fetch('/data');
    const d = await res.json();

    updateCarDash(d);

    setText('rpm', d.rpm, 0);
    setText('mgp', d.mgp, 0);
    setText('map', d.map, 0);
    setText('lambda1', d.lambda1, 2);
    setText('lambda_target', d.lambda_target, 2);
    setText('ect', d.ect, 0);
    setText('oil_pressure', d.oil_pressure, 0);
    setText('battery_v', d.battery_v, 1);
    setText('tps', d.tps, 0);

    setText('health_ect', d.ect, 0);
    setText('iat', d.iat, 0);
    setText('oil_temp', d.oil_temp, 0);
    setText('health_oil_pressure', d.oil_pressure, 0);
    setText('fuel_pressure', d.fuel_pressure, 0);
    setText('health_battery_v', d.battery_v, 1);
    setText('internal_3v3', d.internal_3v3, 2);
    setText('internal_12v', d.internal_12v, 1);
    setText('trig1_err', d.trig1_err, 0);
    setText('lambda_status', d.lambda_status, 0);
    setText('lambda_temp', d.lambda_temp, 0);

    setText('ignition_angle', d.ignition_angle, 1);
    setText('injection_actual_pw', d.injection_actual_pw, 1);
    setText('injection_effective_pw', d.injection_effective_pw, 1);
    setText('lambda_error', d.lambda_error, 2);
    setText('boost_target', d.boost_target, 0);
    setText('boost_error', d.boost_error, 0);
    setText('boost_p', d.boost_p, 1);
    setText('boost_i', d.boost_i, 1);
    setText('boost_d', d.boost_d, 1);
    setText('boost_duty', d.boost_duty, 0);
    setText('aps_main', d.aps_main, 0);
    setText('throttle_target', d.throttle_target, 0);
    setText('vvt_in_target', d.vvt_in_target, 0);
    setText('vvt_in_pos', d.vvt_in_pos, 0);

    const status = document.getElementById('status');

    if (d.age_ms > 1500) {
      status.textContent = 'Data stale — last packet ' + d.age_ms + ' ms ago';
      status.style.color = '#ff7777';
    } else {
      status.textContent = 'Live — last packet ' + d.age_ms + ' ms ago';
      status.style.color = '#9da7b4';
    }

    const summary = document.getElementById('driving_summary');
    summary.textContent =
      'RPM ' + Math.round(d.rpm) +
      ' | MAP ' + Math.round(d.map) + ' kPa' +
      ' | Lambda ' + Number(d.lambda1).toFixed(2) +
      ' / target ' + Number(d.lambda_target).toFixed(2) +
      ' | Oil ' + Math.round(d.oil_pressure) + ' kPa';

    updateWarnings(d);

  } catch (err) {
    const status = document.getElementById('status');
    status.textContent = 'Dashboard fetch error';
    status.style.color = '#ff7777';
  }
}

setInterval(refreshData, 100);
refreshData();

setInterval(refreshLightingState, 100);
refreshLightingState();
</script>

</body>
</html>
)rawliteral";
}


// ---------------------------------------------------------------------------
// CAN helpers
// ---------------------------------------------------------------------------
const char* twaiStateName(twai_state_t state) {
  switch (state) {
    case TWAI_STATE_STOPPED:    return "STOPPED";
    case TWAI_STATE_RUNNING:    return "RUNNING";
    case TWAI_STATE_BUS_OFF:    return "BUS_OFF";
    case TWAI_STATE_RECOVERING: return "RECOVERING";
    default:                    return "UNKNOWN";
  }
}

uint16_t readU16BE(const uint8_t* data, int index) {
  return ((uint16_t)data[index] << 8) | data[index + 1];
}

int16_t readS16BE(const uint8_t* data, int index) {
  return (int16_t)readU16BE(data, index);
}

void decodeLinkEcuFrame(const twai_message_t& msg) {
  if (msg.identifier != LINK_ECU_CAN_ID) return;
  if (msg.extd || msg.rtr) return;
  if (msg.data_length_code < 8) return;

  uint16_t rpmRaw  = readU16BE(msg.data, 0);
  uint16_t mapRaw  = readU16BE(msg.data, 2);
  int16_t  mgpRaw  = readS16BE(msg.data, 4);
  uint16_t battRaw = readU16BE(msg.data, 6);

  ecu.rpm = rpmRaw;
  ecu.map = mapRaw;
  ecu.mgp = mgpRaw;
  ecu.battery_v = battRaw / 100.0f;

  // These dashboard values are not in the first CAN frame yet.
  // They intentionally remain at their default/previous values until later frames are added.

  ecu.last_update_ms = millis();
  lastCanDecodedMs = ecu.last_update_ms;
  canDecodedFrameCount++;
}

bool startCan() {
  if (canStarted) {
    twai_stop();
    twai_driver_uninstall();
    canStarted = false;
  }

  // Normal mode is intentional. On a two-node test bus, the ECU needs another
  // active CAN node to ACK its transmitted frames.
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
    CAN_TX_PIN,
    CAN_RX_PIN,
    TWAI_MODE_NORMAL
  );

  g_config.rx_queue_len = 64;
  g_config.tx_queue_len = 4;

  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_1MBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  esp_err_t result = twai_driver_install(&g_config, &t_config, &f_config);
  if (result != ESP_OK) {
    Serial.print("TWAI driver install failed: 0x");
    Serial.println(result, HEX);
    return false;
  }

  result = twai_start();
  if (result != ESP_OK) {
    Serial.print("TWAI start failed: 0x");
    Serial.println(result, HEX);
    twai_driver_uninstall();
    return false;
  }

  canStarted = true;

  Serial.println();
  Serial.println("TWAI/CAN started for LinkDash");
  Serial.print("CAN TX GPIO: ");
  Serial.println((int)CAN_TX_PIN);
  Serial.print("CAN RX GPIO: ");
  Serial.println((int)CAN_RX_PIN);
  Serial.println("CAN bitrate: 1 Mbit/s");
  Serial.println("CAN mode: normal / ACK enabled");
  Serial.println("Expected Link Stream ID: 0x3E8");
  Serial.println();

  return true;
}

void readCanFrames() {
  if (!canStarted) return;

  twai_message_t msg;

  while (twai_receive(&msg, 0) == ESP_OK) {
    canFrameCount++;
    lastCanFrameMs = millis();

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
  }
}

void handleCanStatus() {
  unsigned long now = millis();

  twai_status_info_t twaiStatus;
  bool hasStatus = canStarted && twai_get_status_info(&twaiStatus) == ESP_OK;

  long lastFrameAge = lastCanFrameMs > 0 ? (long)(now - lastCanFrameMs) : -1;
  long lastDecodedAge = lastCanDecodedMs > 0 ? (long)(now - lastCanDecodedMs) : -1;

  String json = "{";
  json += "\"started\":" + String(canStarted ? "true" : "false") + ",";
  json += "\"frames\":" + String(canFrameCount) + ",";
  json += "\"decoded_frames\":" + String(canDecodedFrameCount) + ",";
  json += "\"last_frame_age_ms\":" + String(lastFrameAge) + ",";
  json += "\"last_decoded_age_ms\":" + String(lastDecodedAge) + ",";
  json += "\"can_state\":\"" + String(hasStatus ? twaiStateName(twaiStatus.state) : "NOT_STARTED") + "\",";
  json += "\"tx_err\":" + String(hasStatus ? twaiStatus.tx_error_counter : 0) + ",";
  json += "\"rx_err\":" + String(hasStatus ? twaiStatus.rx_error_counter : 0) + ",";
  json += "\"rx_missed\":" + String(hasStatus ? twaiStatus.rx_missed_count : 0) + ",";
  json += "\"bus_error\":" + String(hasStatus ? twaiStatus.bus_error_count : 0);
  json += "}";

  server.send(200, "application/json", json);
}


void handleRoot() {
  server.send(200, "text/html", dashboardHtml());
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

  updateLighting();

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

void handleData() {
  unsigned long now = millis();
  unsigned long age = ecu.last_update_ms == 0 ? 999999 : now - ecu.last_update_ms;

  String json = "{";
  json += "\"rpm\":" + String(ecu.rpm, 0) + ",";
  json += "\"ect\":" + String(ecu.ect, 1) + ",";
  json += "\"iat\":" + String(ecu.iat, 1) + ",";
  json += "\"mgp\":" + String(ecu.mgp, 1) + ",";
  json += "\"map\":" + String(ecu.map, 1) + ",";
  json += "\"tps\":" + String(ecu.tps, 1) + ",";

  json += "\"ignition_angle\":" + String(ecu.ignition_angle, 1) + ",";
  json += "\"injection_actual_pw\":" + String(ecu.injection_actual_pw, 2) + ",";
  json += "\"injection_effective_pw\":" + String(ecu.injection_effective_pw, 2) + ",";

  json += "\"lambda1\":" + String(ecu.lambda1, 3) + ",";
  json += "\"lambda_target\":" + String(ecu.lambda_target, 3) + ",";
  json += "\"lambda_error\":" + String(ecu.lambda_error, 3) + ",";
  json += "\"lambda_status\":" + String(ecu.lambda_status, 0) + ",";
  json += "\"lambda_temp\":" + String(ecu.lambda_temp, 1) + ",";

  json += "\"oil_temp\":" + String(ecu.oil_temp, 1) + ",";
  json += "\"battery_v\":" + String(ecu.battery_v, 2) + ",";
  json += "\"fuel_pressure\":" + String(ecu.fuel_pressure, 1) + ",";
  json += "\"oil_pressure\":" + String(ecu.oil_pressure, 1) + ",";

  json += "\"boost_target\":" + String(ecu.boost_target, 1) + ",";
  json += "\"boost_error\":" + String(ecu.boost_error, 1) + ",";
  json += "\"boost_p\":" + String(ecu.boost_p, 2) + ",";
  json += "\"boost_i\":" + String(ecu.boost_i, 2) + ",";
  json += "\"boost_d\":" + String(ecu.boost_d, 2) + ",";
  json += "\"boost_duty\":" + String(ecu.boost_duty, 1) + ",";

  json += "\"trig1_err\":" + String(ecu.trig1_err, 0) + ",";
  json += "\"internal_3v3\":" + String(ecu.internal_3v3, 2) + ",";
  json += "\"internal_12v\":" + String(ecu.internal_12v, 2) + ",";

  json += "\"aps_main\":" + String(ecu.aps_main, 1) + ",";
  json += "\"throttle_target\":" + String(ecu.throttle_target, 1) + ",";
  json += "\"vvt_in_target\":" + String(ecu.vvt_in_target, 1) + ",";
  json += "\"vvt_in_pos\":" + String(ecu.vvt_in_pos, 1) + ",";

  json += "\"age_ms\":" + String(age) + ",";
  json += "\"can_frames\":" + String(canFrameCount) + ",";
  json += "\"can_decoded_frames\":" + String(canDecodedFrameCount) + ",";
  json += "\"can_last_frame_age_ms\":" + String(lastCanFrameMs > 0 ? (long)(now - lastCanFrameMs) : -1) + ",";
  json += "\"can_last_decoded_age_ms\":" + String(lastCanDecodedMs > 0 ? (long)(now - lastCanDecodedMs) : -1);
  json += "}";

  server.send(200, "application/json", json);
}

void updateFromValues(float values[], int count) {
  if (count > 0) ecu.rpm = values[0];
  if (count > 1) ecu.ect = values[1];
  if (count > 2) ecu.iat = values[2];
  if (count > 3) ecu.mgp = values[3];
  if (count > 4) ecu.map = values[4];
  if (count > 5) ecu.tps = values[5];

  if (count > 6) ecu.ignition_angle = values[6];
  if (count > 7) ecu.injection_actual_pw = values[7];
  if (count > 8) ecu.injection_effective_pw = values[8];

  if (count > 9) ecu.lambda1 = values[9];
  if (count > 10) ecu.lambda_target = values[10];
  if (count > 11) ecu.lambda_error = values[11];
  if (count > 12) ecu.lambda_status = values[12];
  if (count > 13) ecu.lambda_temp = values[13];

  if (count > 14) ecu.oil_temp = values[14];
  if (count > 15) ecu.battery_v = values[15];
  if (count > 16) ecu.fuel_pressure = values[16];
  if (count > 17) ecu.oil_pressure = values[17];

  if (count > 18) ecu.boost_target = values[18];
  if (count > 19) ecu.boost_error = values[19];
  if (count > 20) ecu.boost_p = values[20];
  if (count > 21) ecu.boost_i = values[21];
  if (count > 22) ecu.boost_d = values[22];
  if (count > 23) ecu.boost_duty = values[23];

  if (count > 24) ecu.trig1_err = values[24];
  if (count > 25) ecu.internal_3v3 = values[25];
  if (count > 26) ecu.internal_12v = values[26];

  if (count > 27) ecu.aps_main = values[27];
  if (count > 28) ecu.throttle_target = values[28];
  if (count > 29) ecu.vvt_in_target = values[29];
  if (count > 30) ecu.vvt_in_pos = values[30];

  ecu.last_update_ms = millis();
}

void readUdpPackets() {
  int packetSize = udp.parsePacket();
  if (!packetSize) return;

  char buffer[512];
  int len = udp.read(buffer, sizeof(buffer) - 1);
  if (len <= 0) return;
  buffer[len] = '\0';

  float values[31];
  int count = 0;

  char* token = strtok(buffer, ",");

  while (token != NULL && count < 31) {
    values[count] = atof(token);
    count++;
    token = strtok(NULL, ",");
  }

  if (count >= 10) {
    updateFromValues(values, count);

    Serial.print("RX fields: ");
    Serial.print(count);
    Serial.print(" | RPM: ");
    Serial.print(ecu.rpm);
    Serial.print(" | MAP: ");
    Serial.print(ecu.map);
    Serial.print(" | Lambda: ");
    Serial.println(ecu.lambda1);
  } else {
    Serial.print("Ignored short UDP packet: ");
    Serial.println(buffer);
  }
}

uint8_t addClamp255(uint8_t a, uint8_t b) {
  int value = a + b;
  if (value > 255) return 255;
  return value;
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

void handleLightingState() {
  // Browser preview cannot truly show RGBW, so approximate W by adding it
  // into RGB for the preview patch. Raw RGBW values are also returned.
  uint8_t previewR = addClamp255(currentLightingOutput.r, currentLightingOutput.w);
  uint8_t previewG = addClamp255(currentLightingOutput.g, currentLightingOutput.w);
  uint8_t previewB = addClamp255(currentLightingOutput.b, currentLightingOutput.w);

  String json = "{";
  json += "\"enabled\":" + String(lighting.enabled ? "true" : "false") + ",";
  json += "\"mode\":\"" + String(lightingModeName()) + "\",";
  json += "\"pattern\":\"" + String(lightingPatternName()) + "\",";
  json += "\"max_brightness\":" + String(lighting.maxBrightness, 3) + ",";

  json += "\"r\":" + String(currentLightingOutput.r) + ",";
  json += "\"g\":" + String(currentLightingOutput.g) + ",";
  json += "\"b\":" + String(currentLightingOutput.b) + ",";
  json += "\"w\":" + String(currentLightingOutput.w) + ",";

  json += "\"preview_r\":" + String(previewR) + ",";
  json += "\"preview_g\":" + String(previewG) + ",";
  json += "\"preview_b\":" + String(previewB) + ",";

  json += "\"rpm\":" + String(ecu.rpm, 0) + ",";
  json += "\"mgp\":" + String(ecu.mgp, 1);
  json += "}";

  server.send(200, "application/json", json);
}



void setup() {
  Serial.begin(115200);
  delay(500);

  setupLightingPwm();
  updateLighting();

  startCan();

  Serial.print("Addressable RGBW pixels: ");
  Serial.println(NEOPIXEL_COUNT);
  Serial.print("NeoPixel data pin: GPIO ");
  Serial.println(NEOPIXEL_PIN);

  WiFi.mode(WIFI_AP);

  IPAddress localIp(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.softAPConfig(localIp, gateway, subnet);
  WiFi.softAP(AP_SSID, AP_PASS);

  Serial.println();
  Serial.println("ESP32 dashboard AP started");
  Serial.print("SSID: ");
  Serial.println(AP_SSID);
  Serial.print("Dashboard: http://");
  Serial.println(WiFi.softAPIP());

  udp.begin(UDP_PORT);

server.on("/", handleRoot);
server.on("/data", handleData);
server.on("/canStatus", handleCanStatus);
server.on("/setLighting", handleSetLighting);
server.on("/lightingState", handleLightingState);
server.begin();

  Serial.println("Live ECU source: CAN 1 / User Stream 1 / ID 0x3E8");
  Serial.print("UDP replay fallback still available on port ");
  Serial.println(UDP_PORT);
}

void loop() {
  readCanFrames();

  // UDP replay remains in the file as a fallback, but is disabled here so it
  // cannot overwrite live CAN values while testing in the car.
  // readUdpPackets();

  server.handleClient();

  static unsigned long lastLightingUpdateMs = 0;
  unsigned long now = millis();

  if (now - lastLightingUpdateMs >= 30) {
    lastLightingUpdateMs = now;
    updateLighting();
  }
}