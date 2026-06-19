#include "can_bus.h"
#include "ecu_data.h"
#include <WebServer.h>

extern WebServer server;

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

  String json;
  json.reserve(300);
  json += "{";
  json += "\"started\":" + String(canStarted ? "true" : "false") + ",";
  json += "\"frames\":" + String(canFrameCount) + ",";
  json += "\"decoded_frames\":" + String(canDecodedFrameCount) + ",";
  json += "\"last_frame_age_ms\":" + String(lastCanFrameMs > 0 ? (long)(now - lastCanFrameMs) : -1) + ",";
  json += "\"last_decoded_age_ms\":" + String(lastCanDecodedMs > 0 ? (long)(now - lastCanDecodedMs) : -1) + ",";
  json += "\"can_state\":\"" + String(hasStatus ? twaiStateName(twaiStatus.state) : "NOT_STARTED") + "\",";
  json += "\"tx_err\":" + String(hasStatus ? twaiStatus.tx_error_counter : 0) + ",";
  json += "\"rx_err\":" + String(hasStatus ? twaiStatus.rx_error_counter : 0) + ",";
  json += "\"rx_missed\":" + String(hasStatus ? twaiStatus.rx_missed_count : 0) + ",";
  json += "\"bus_error\":" + String(hasStatus ? twaiStatus.bus_error_count : 0);
  json += "}";

  server.send(200, "application/json", json);
}
