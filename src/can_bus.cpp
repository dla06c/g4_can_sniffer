#include "can_bus.h"
#include "ecu_data.h"
#include <WebServer.h>

extern WebServer server;

// ---------------------------------------------------------------------------
// CAN / TWAI configuration for Link ECU CAN 1 user-defined transmit channels
//
// Hardware:
//   ESP32 GPIO5 -> SN65HVD230 TXD / CTX
//   ESP32 GPIO4 -> SN65HVD230 RXD / CRX
//   ESP32 3V3   -> SN65HVD230 VCC
//   ESP32 GND   -> SN65HVD230 GND
//   CANH/CANL   -> Link ECU CANH/CANL
//
// ECU setup expected:
//   CAN 1, User Defined, 1 Mbit/s, Standard identifiers
//
// IMPORTANT: These defaults assume the three Link ECU channels use sequential
// IDs 1000, 1001 and 1002 decimal. If PCLink uses different IDs, change only
// these three constants.
// ---------------------------------------------------------------------------
const gpio_num_t CAN_TX_PIN = GPIO_NUM_5;
const gpio_num_t CAN_RX_PIN = GPIO_NUM_4;

static constexpr uint32_t LINK_ECU_CAN_ID_CHANNEL_1 = 0x3E8U;  // 1000 dec
static constexpr uint32_t LINK_ECU_CAN_ID_CHANNEL_2 = 0x3E9U;  // 1001 dec
static constexpr uint32_t LINK_ECU_CAN_ID_CHANNEL_3 = 0x3EAU;  // 1002 dec

static constexpr uint8_t MAX_CAN_FRAMES_PER_LOOP = 12;
static constexpr unsigned long CAN_DIAGNOSTIC_PRINT_INTERVAL_MS = 2000;

bool canStarted = false;
unsigned long canFrameCount = 0;
unsigned long canDecodedFrameCount = 0;
unsigned long lastCanFrameMs = 0;
unsigned long lastCanDecodedMs = 0;
unsigned long lastCanSerialPrintMs = 0;

static unsigned long channel1FrameCount = 0;
static unsigned long channel2FrameCount = 0;
static unsigned long channel3FrameCount = 0;
static unsigned long channel1LastMs = 0;
static unsigned long channel2LastMs = 0;
static unsigned long channel3LastMs = 0;

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

static bool isConfiguredLinkId(uint32_t identifier) {
  return identifier == LINK_ECU_CAN_ID_CHANNEL_1 ||
         identifier == LINK_ECU_CAN_ID_CHANNEL_2 ||
         identifier == LINK_ECU_CAN_ID_CHANNEL_3;
}

// Channel 1 / ID 0x3E8
//   bytes 0-1 Engine Speed, unsigned, raw / 1
//   bytes 2-3 MAP,          unsigned, raw / 1 kPa
//   bytes 4-5 MGP,          signed,   raw / 1 kPa
//   bytes 6-7 Batt Voltage, unsigned, raw / 100 V
static void decodeChannel1(const twai_message_t& msg, unsigned long now) {
  ecu.rpm = (float)readU16BE(msg.data, 0);
  ecu.map = (float)readU16BE(msg.data, 2);
  ecu.mgp = (float)readS16BE(msg.data, 4);
  ecu.battery_v = readU16BE(msg.data, 6) / 100.0f;

  channel1FrameCount++;
  channel1LastMs = now;
}

// Channel 2 / ID 0x3E9
//   bytes 0-1 DI 1 Freq - GP Speed 1, unsigned, raw / 10
//   byte  2   Gear,       unsigned, raw / 1
//   byte  3   TPS,        unsigned, raw / 2 percent
//   bytes 4-5 ECT,        signed,   raw / 10 deg C
//   bytes 6-7 IAT,        signed,   raw / 10 deg C
static void decodeChannel2(const twai_message_t& msg, unsigned long now) {
  ecu.gp_speed_1 = readU16BE(msg.data, 0) / 10.0f;
  ecu.gear = constrain((int)msg.data[2], 0, 5);
  ecu.tps = constrain(msg.data[3] / 2.0f, 0.0f, 100.0f);
  ecu.ect = readS16BE(msg.data, 4) / 10.0f;
  ecu.iat = readS16BE(msg.data, 6) / 10.0f;

  channel2FrameCount++;
  channel2LastMs = now;
}

// Channel 3 / ID 0x3EA
//   bytes 0-1 Oil Pressure,  unsigned, raw / 10 kPa
//   bytes 2-3 Fuel Pressure, unsigned, raw / 10 kPa
//   bytes 4-5 Lambda 1,      unsigned, raw / 1000 lambda
//   bytes 6-7 Lambda Target, unsigned, raw / 1000 lambda
static void decodeChannel3(const twai_message_t& msg, unsigned long now) {
  ecu.oil_pressure = readU16BE(msg.data, 0) / 10.0f;
  ecu.fuel_pressure = readU16BE(msg.data, 2) / 10.0f;
  ecu.lambda1 = readU16BE(msg.data, 4) / 1000.0f;
  ecu.lambda_target = readU16BE(msg.data, 6) / 1000.0f;
  ecu.lambda_error = ecu.lambda1 - ecu.lambda_target;

  channel3FrameCount++;
  channel3LastMs = now;
}

bool decodeLinkEcuFrame(const twai_message_t& msg) {
  if (!isConfiguredLinkId(msg.identifier)) return false;
  if (msg.extd || msg.rtr) return false;
  if (msg.data_length_code < 8) return false;

  const unsigned long now = millis();

  switch (msg.identifier) {
    case LINK_ECU_CAN_ID_CHANNEL_1:
      decodeChannel1(msg, now);
      break;
    case LINK_ECU_CAN_ID_CHANNEL_2:
      decodeChannel2(msg, now);
      break;
    case LINK_ECU_CAN_ID_CHANNEL_3:
      decodeChannel3(msg, now);
      break;
    default:
      return false;
  }

  ecu.last_update_ms = now;
  lastCanDecodedMs = now;
  canDecodedFrameCount++;
  return true;
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

  // Accept the four-ID block 0x3E8..0x3EB in hardware, then reject 0x3EB in
  // software. Masking the two identifier LSBs lets one TWAI single filter cover
  // all three configured sequential IDs without accepting the rest of the bus.
  const uint32_t comparedIdentifierBits = 0x7FFU & ~0x3U;
  twai_filter_config_t f_config = {};
  f_config.acceptance_code =
    (LINK_ECU_CAN_ID_CHANNEL_1 & comparedIdentifierBits) << 21;
  f_config.acceptance_mask = ~(comparedIdentifierBits << 21);
  f_config.single_filter = true;

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
  Serial.println("Expected Link channel IDs:");
  Serial.println("  Channel 1: 0x3E8 / 1000");
  Serial.println("  Channel 2: 0x3E9 / 1001");
  Serial.println("  Channel 3: 0x3EA / 1002");
  Serial.println();

  return true;
}

void readCanFrames() {
  if (!canStarted) return;

  twai_message_t msg;
  uint8_t processed = 0;

  // Bound the work done in one loop so HTTP/WebSocket service cannot be
  // starved if the receive queue backs up.
  while (processed < MAX_CAN_FRAMES_PER_LOOP && twai_receive(&msg, 0) == ESP_OK) {
    processed++;
    canFrameCount++;
    lastCanFrameMs = millis();

    const bool decoded = decodeLinkEcuFrame(msg);

    const unsigned long now = millis();
    if (decoded && now - lastCanSerialPrintMs >= CAN_DIAGNOSTIC_PRINT_INTERVAL_MS) {
      lastCanSerialPrintMs = now;

      Serial.print("CAN summary | RPM=");
      Serial.print(ecu.rpm, 0);
      Serial.print(" Speed=");
      Serial.print(ecu.gp_speed_1, 1);
      Serial.print(" Gear=");
      Serial.print(ecu.gear);
      Serial.print(" TPS=");
      Serial.print(ecu.tps, 1);
      Serial.print(" MAP=");
      Serial.print(ecu.map, 1);
      Serial.print(" MGP=");
      Serial.print(ecu.mgp, 1);
      Serial.print(" Batt=");
      Serial.print(ecu.battery_v, 2);
      Serial.print(" ECT=");
      Serial.print(ecu.ect, 1);
      Serial.print(" IAT=");
      Serial.print(ecu.iat, 1);
      Serial.print(" Oil=");
      Serial.print(ecu.oil_pressure, 1);
      Serial.print(" Fuel=");
      Serial.print(ecu.fuel_pressure, 1);
      Serial.print(" L1=");
      Serial.print(ecu.lambda1, 3);
      Serial.print(" LT=");
      Serial.println(ecu.lambda_target, 3);
    }
  }
}

void handleCanStatus() {
  const unsigned long now = millis();

  twai_status_info_t twaiStatus;
  const bool hasStatus = canStarted && twai_get_status_info(&twaiStatus) == ESP_OK;

  String json;
  json.reserve(640);
  json += "{";
  json += "\"started\":" + String(canStarted ? "true" : "false") + ",";
  json += "\"frames\":" + String(canFrameCount) + ",";
  json += "\"decoded_frames\":" + String(canDecodedFrameCount) + ",";
  json += "\"channel_1_id\":" + String(LINK_ECU_CAN_ID_CHANNEL_1) + ",";
  json += "\"channel_2_id\":" + String(LINK_ECU_CAN_ID_CHANNEL_2) + ",";
  json += "\"channel_3_id\":" + String(LINK_ECU_CAN_ID_CHANNEL_3) + ",";
  json += "\"channel_1_frames\":" + String(channel1FrameCount) + ",";
  json += "\"channel_2_frames\":" + String(channel2FrameCount) + ",";
  json += "\"channel_3_frames\":" + String(channel3FrameCount) + ",";
  json += "\"channel_1_age_ms\":" + String(channel1LastMs > 0 ? (long)(now - channel1LastMs) : -1L) + ",";
  json += "\"channel_2_age_ms\":" + String(channel2LastMs > 0 ? (long)(now - channel2LastMs) : -1L) + ",";
  json += "\"channel_3_age_ms\":" + String(channel3LastMs > 0 ? (long)(now - channel3LastMs) : -1L) + ",";
  json += "\"last_frame_age_ms\":" + String(lastCanFrameMs > 0 ? (long)(now - lastCanFrameMs) : -1L) + ",";
  json += "\"last_decoded_age_ms\":" + String(lastCanDecodedMs > 0 ? (long)(now - lastCanDecodedMs) : -1L) + ",";
  json += "\"can_state\":\"" + String(hasStatus ? twaiStateName(twaiStatus.state) : "NOT_STARTED") + "\",";
  json += "\"tx_err\":" + String(hasStatus ? twaiStatus.tx_error_counter : 0) + ",";
  json += "\"rx_err\":" + String(hasStatus ? twaiStatus.rx_error_counter : 0) + ",";
  json += "\"rx_missed\":" + String(hasStatus ? twaiStatus.rx_missed_count : 0) + ",";
  json += "\"bus_error\":" + String(hasStatus ? twaiStatus.bus_error_count : 0);
  json += "}";

  server.send(200, "application/json", json);
}
