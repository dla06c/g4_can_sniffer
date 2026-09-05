#include "web_server.h"
#include "ecu_data.h"
#include "can_bus.h"
#include "lighting.h"
#include "dashboard.h"
#include "dashboard_assets.h"
#include <WebSocketsServer.h>

WebServer server(80);
WiFiUDP udp;
WebSocketsServer telemetrySocket(81);

static constexpr unsigned long TELEMETRY_PUSH_INTERVAL_MS = 50;
static uint8_t telemetryClientCount = 0;
static unsigned long lastTelemetryPushMs = 0;

size_t buildTelemetryJson(char* json, size_t capacity) {
  const unsigned long now = millis();
  const unsigned long age = ecu.last_update_ms == 0 ? 999999UL : now - ecu.last_update_ms;
  const long lastFrameAge = lastCanFrameMs > 0 ? (long)(now - lastCanFrameMs) : -1L;
  const long lastDecodedAge = lastCanDecodedMs > 0 ? (long)(now - lastCanDecodedMs) : -1L;

  const uint8_t previewR = addClamp255(currentLightingOutput.r, currentLightingOutput.w);
  const uint8_t previewG = addClamp255(currentLightingOutput.g, currentLightingOutput.w);
  const uint8_t previewB = addClamp255(currentLightingOutput.b, currentLightingOutput.w);

  const int written = snprintf(
    json,
    capacity,
    "{\"rpm\":%.0f,\"ect\":%.1f,\"iat\":%.1f,\"mgp\":%.1f,\"map\":%.1f,\"tps\":%.1f,"
    "\"gp_speed_1\":%.1f,\"speed\":%.1f,"
    "\"ignition_angle\":%.1f,\"injection_actual_pw\":%.2f,\"injection_effective_pw\":%.2f,"
    "\"lambda1\":%.3f,\"lambda_target\":%.3f,\"lambda_error\":%.3f,\"lambda_status\":%.0f,\"lambda_temp\":%.1f,"
    "\"oil_temp\":%.1f,\"battery_v\":%.2f,\"fuel_pressure\":%.1f,\"oil_pressure\":%.1f,"
    "\"boost_target\":%.1f,\"boost_error\":%.1f,\"boost_p\":%.2f,\"boost_i\":%.2f,\"boost_d\":%.2f,\"boost_duty\":%.1f,"
    "\"trig1_err\":%.0f,\"internal_3v3\":%.2f,\"internal_12v\":%.2f,"
    "\"aps_main\":%.1f,\"throttle_target\":%.1f,\"vvt_in_target\":%.1f,\"vvt_in_pos\":%.1f,\"gear\":%u,"
    "\"led_r\":%u,\"led_g\":%u,\"led_b\":%u,"
    "\"age_ms\":%lu,\"can_frames\":%lu,\"can_decoded_frames\":%lu,"
    "\"can_last_frame_age_ms\":%ld,\"can_last_decoded_age_ms\":%ld}",
    ecu.rpm, ecu.ect, ecu.iat, ecu.mgp, ecu.map, ecu.tps,
    ecu.gp_speed_1, ecu.gp_speed_1,
    ecu.ignition_angle, ecu.injection_actual_pw, ecu.injection_effective_pw,
    ecu.lambda1, ecu.lambda_target, ecu.lambda_error, ecu.lambda_status, ecu.lambda_temp,
    ecu.oil_temp, ecu.battery_v, ecu.fuel_pressure, ecu.oil_pressure,
    ecu.boost_target, ecu.boost_error, ecu.boost_p, ecu.boost_i, ecu.boost_d, ecu.boost_duty,
    ecu.trig1_err, ecu.internal_3v3, ecu.internal_12v,
    ecu.aps_main, ecu.throttle_target, ecu.vvt_in_target, ecu.vvt_in_pos, (unsigned int)ecu.gear,
    (unsigned int)previewR, (unsigned int)previewG, (unsigned int)previewB,
    age, canFrameCount, canDecodedFrameCount, lastFrameAge, lastDecodedAge
  );

  if (written <= 0) return 0;
  if ((size_t)written >= capacity) return capacity - 1;
  return (size_t)written;
}

void handleRoot() {
  server.sendHeader("Cache-Control", "no-cache");
  server.sendHeader("Pragma", "no-cache");
  server.send_P(200, "text/html", DASHBOARD_HTML, DASHBOARD_HTML_LENGTH);
}

void handleDashboardBackground() {
  server.sendHeader("Cache-Control", "public, max-age=31536000, immutable");
  server.send_P(200, "image/svg+xml", DASHBOARD_BG_SVG, DASHBOARD_BG_SVG_LENGTH);
}

void handleLinebeamFont() {
  server.sendHeader("Cache-Control", "public, max-age=31536000, immutable");
  server.send_P(
    200,
    "font/woff2",
    reinterpret_cast<const char*>(LINEBEAM_WOFF2),
    LINEBEAM_WOFF2_LENGTH
  );
}

void handleHealth() {
  char status[192];
  snprintf(
    status,
    sizeof(status),
    "OK\nhtml_bytes=%u\nbackground_bytes=%u\nfont_bytes=%u\nfree_heap=%u\nwebsocket_clients=%u\n",
    (unsigned int)DASHBOARD_HTML_LENGTH,
    (unsigned int)DASHBOARD_BG_SVG_LENGTH,
    (unsigned int)LINEBEAM_WOFF2_LENGTH,
    (unsigned int)ESP.getFreeHeap(),
    (unsigned int)telemetryClientCount
  );
  server.send(200, "text/plain", status);
}

void handleData() {
  char json[1792];
  const size_t length = buildTelemetryJson(json, sizeof(json));
  if (length == 0) {
    server.send(500, "application/json", "{\"error\":\"telemetry_encode_failed\"}");
    return;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void telemetrySocketEvent(uint8_t clientNumber, WStype_t type, uint8_t* payload, size_t length) {
  (void)payload;
  (void)length;

  if (type == WStype_CONNECTED) {
    if (telemetryClientCount < 255) telemetryClientCount++;
    char json[1792];
    const size_t jsonLength = buildTelemetryJson(json, sizeof(json));
    if (jsonLength > 0) {
      telemetrySocket.sendTXT(clientNumber, reinterpret_cast<uint8_t*>(json), jsonLength);
    }
  } else if (type == WStype_DISCONNECTED) {
    if (telemetryClientCount > 0) telemetryClientCount--;
  }
}

void setupTelemetryWebSocket() {
  telemetrySocket.begin();
  telemetrySocket.onEvent(telemetrySocketEvent);
  telemetrySocket.enableHeartbeat(15000, 3000, 2);
}

void serviceTelemetryWebSocket() {
  telemetrySocket.loop();
  if (telemetryClientCount == 0) return;

  const unsigned long now = millis();
  if (now - lastTelemetryPushMs < TELEMETRY_PUSH_INTERVAL_MS) return;
  lastTelemetryPushMs = now;

  char json[1792];
  const size_t length = buildTelemetryJson(json, sizeof(json));
  if (length > 0) {
    telemetrySocket.broadcastTXT(reinterpret_cast<uint8_t*>(json), length);
  }
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
  if (count > 31) ecu.gear = constrain((int)values[31], 0, 5);

  ecu.last_update_ms = millis();
}

void readUdpPackets() {
  int packetSize = udp.parsePacket();
  if (!packetSize) return;

  char buffer[512];
  int len = udp.read(buffer, sizeof(buffer) - 1);
  if (len <= 0) return;
  buffer[len] = '\0';

  float values[32];
  int count = 0;

  char* token = strtok(buffer, ",");

  while (token != NULL && count < 32) {
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


void setupRoutes() {
  server.on("/", handleRoot);
  server.on("/dashboard-bg.svg", handleDashboardBackground);
  server.on("/linebeam.woff2", handleLinebeamFont);
  server.on("/health", handleHealth);
  server.on("/data", handleData);
  server.on("/canStatus", handleCanStatus);
  server.on("/setLighting", handleSetLighting);
  server.on("/lightingState", handleLightingState);
  setupTelemetryWebSocket();
}
