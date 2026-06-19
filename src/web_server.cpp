#include "web_server.h"
#include "ecu_data.h"
#include "can_bus.h"
#include "lighting.h"
#include "dashboard.h"

WebServer server(80);
WiFiUDP udp;

void handleRoot() {
  server.send(200, "text/html", dashboardHtml());
}

void handleData() {
  unsigned long now = millis();
  unsigned long age = ecu.last_update_ms == 0 ? 999999 : now - ecu.last_update_ms;

  String json;
  json.reserve(900); // Helps prevent minor memory fragmentation delay on ESP32

  json += "{";
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

  // Feed lighting directly through fast data path so we don't have to poll separately
  uint8_t previewR = addClamp255(currentLightingOutput.r, currentLightingOutput.w);
  uint8_t previewG = addClamp255(currentLightingOutput.g, currentLightingOutput.w);
  uint8_t previewB = addClamp255(currentLightingOutput.b, currentLightingOutput.w);

  json += "\"led_r\":" + String(previewR) + ",";
  json += "\"led_g\":" + String(previewG) + ",";
  json += "\"led_b\":" + String(previewB) + ",";

  json += "\"age_ms\":" + String(age) + ",";
  json += "\"can_frames\":" + String(canFrameCount) + ",";
  json += "\"can_decoded_frames\":" + String(canDecodedFrameCount) + ",";
  json += "\"can_last_frame_age_ms\":" + String(lastCanFrameMs > 0 ? (long)(now - lastCanFrameMs) : -1) + ",";
  json += "\"can_last_decoded_age_ms\":" + String(lastCanDecodedMs > 0 ? (long)(now - lastCanDecodedMs) : -1);
  json += "}";

  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");

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

void setupRoutes() {
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/canStatus", handleCanStatus);
  server.on("/setLighting", handleSetLighting);
  server.on("/lightingState", handleLightingState);
}
