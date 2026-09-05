//LinkDash
#include <WiFi.h>
#include "ecu_data.h"
#include "lighting.h"
#include "can_bus.h"
#include "web_server.h"

const char* AP_SSID = "LinkDash";
const char* AP_PASS = "linkdash123";  // minimum 8 characters

const uint16_t UDP_PORT = 4210;

// ---------------------------------------------------------------------------
// Shared application state
// ---------------------------------------------------------------------------
EcuData ecu;

void setup() {
  Serial.begin(115200);
  delay(500);

  lightingPrefsReady = lightingPreferences.begin("lighting", false);
  if (lightingPrefsReady) {
    loadLightingSettings();
  } else {
    Serial.println("Failed to open lighting preferences");
  }

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

  setupRoutes();
  server.begin();

  Serial.println("Live ECU source: CAN channels 1-3 / IDs 0x3E8-0x3EA");
  Serial.print("UDP replay fallback still available on port ");
  Serial.println(UDP_PORT);
}

void loop() {
  readCanFrames();

  // UDP replay remains in the file as a fallback, but is disabled here so it
  // cannot overwrite live CAN values while testing in the car.
  // readUdpPackets();

  server.handleClient();
  serviceTelemetryWebSocket();

  // updateLighting() skips unchanged static output and self-throttles animated
  // patterns, so it is safe to call every loop without repeatedly blocking on show().
  updateLighting();

  unsigned long now = millis();

  if (lightingSettingsDirty && now - lightingSettingsLastChangeMs >= LIGHTING_SAVE_DEBOUNCE_MS) {
    saveLightingSettings();
    lightingSettingsDirty = false;
  }
}
