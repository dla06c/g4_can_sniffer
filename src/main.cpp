// ============================================================================
// ESP32 Wi-Fi CAN Sniffer – LINK ECU G4 MonsoonX
//
// Hardware:
//   SN65HVD230 CAN transceiver
//     ESP32 GPIO 5 (TX/CTX) -> SN65HVD230 TXD
//     ESP32 GPIO 4 (RX/CRX) -> SN65HVD230 RXD
//     ESP32 3V3              -> SN65HVD230 VCC
//     ESP32 GND              -> SN65HVD230 GND
//     SN65HVD230 CANH        -> ECU CAN High
//     SN65HVD230 CANL        -> ECU CAN Low
//
// ECU CAN channel:
//   CAN 1, User Defined, 1 Mbit/s
//   Channel 1: Transmit User Stream 1, ID 0x3E8 (1000 dec), Standard, 50 Hz
//   Stream 1 frame layout (big-endian / MS First):
//     Bytes 0-1  Engine Speed (unsigned, RPM)
//     Bytes 2-3  MAP          (unsigned, kPa absolute)
//     Bytes 4-5  MGP          (signed,   kPa gauge)
//     Bytes 6-7  Batt Voltage (unsigned, raw/100 = V)
//
// Access the web UI at http://192.168.6.1 after connecting to the
// "CANSniffer" Wi-Fi access point (password: cansniff123).
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <driver/twai.h>

// ---------------------------------------------------------------------------
// Wi-Fi access point configuration
// ---------------------------------------------------------------------------
const char* AP_SSID   = "CANSniffer";
const char* AP_PASS   = "cansniff123";   // minimum 8 characters

IPAddress AP_IP    (192, 168, 6, 1);
IPAddress AP_GW    (192, 168, 6, 1);
IPAddress AP_SUBNET(255, 255, 255, 0);

WebServer server(80);

// ---------------------------------------------------------------------------
// CAN / TWAI configuration
//
// NOTE: Default mode is NORMAL (ACK enabled).
//
// When the ESP32 is the only node other than the ECU on the bus (which is
// the typical sniffer scenario), listen-only mode causes the ECU to receive
// an ACK error on every frame it sends.  That drives up the ECU's transmit
// error counter and can cause bus-off, making it appear as if no frames are
// arriving.  Normal/ACK mode solves this while still capturing every frame.
//
// You can switch to listen-only via the web UI if you are tapping into a
// larger, already-active bus where other nodes provide ACKs.
// ---------------------------------------------------------------------------
const gpio_num_t CAN_TX_PIN = GPIO_NUM_5;
const gpio_num_t CAN_RX_PIN = GPIO_NUM_4;

// LINK ECU Stream 1 CAN ID
static const uint32_t LINK_ECU_CAN_ID = 0x3E8U;

enum CanBitrate {
    CAN_RATE_1M,
    CAN_RATE_500K,
    CAN_RATE_250K,
    CAN_RATE_125K
};

CanBitrate canBitrate  = CAN_RATE_1M;
bool       canListenOnly = false;   // false = ACK enabled (recommended for 2-node bus)
bool       canStarted    = false;

// ---------------------------------------------------------------------------
// LINK ECU decoded values
// ---------------------------------------------------------------------------
struct LinkEcuData {
    bool     valid         = false;
    uint32_t lastUpdateMs  = 0;
    uint16_t engineRpm     = 0;   // RPM
    uint16_t mapKpa        = 0;   // kPa absolute
    int16_t  mgpKpa        = 0;   // kPa gauge (signed)
    float    battV         = 0.0f; // Volts
};

LinkEcuData ecuData;

// ---------------------------------------------------------------------------
// Rolling RAM log.  350 x 88 ~= 31 KB.
// Reduce LOG_CAPACITY if your build runs low on RAM.
// ---------------------------------------------------------------------------
const uint16_t LOG_CAPACITY = 350;
const uint8_t  LOG_LINE_LEN = 88;
char     logLines[LOG_CAPACITY][LOG_LINE_LEN];
uint16_t logWriteIndex  = 0;
uint16_t logCount       = 0;
uint32_t droppedLogLines = 0;

unsigned long canFrameCount     = 0;
unsigned long lastCanFrameMs    = 0;
unsigned long lastStatusMs      = 0;
unsigned long lastSerialPrintMs = 0;

struct IdStat {
    uint32_t id    = 0;
    bool     ext   = false;
    uint32_t count = 0;
    bool     used  = false;
};

const int MAX_TRACKED_IDS = 80;
IdStat idStats[MAX_TRACKED_IDS];

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
const char* bitrateName() {
    switch (canBitrate) {
        case CAN_RATE_1M:   return "1M";
        case CAN_RATE_500K: return "500K";
        case CAN_RATE_250K: return "250K";
        case CAN_RATE_125K: return "125K";
        default:            return "unknown";
    }
}

twai_timing_config_t getTimingConfig() {
    switch (canBitrate) {
        case CAN_RATE_1M:   { twai_timing_config_t t = TWAI_TIMING_CONFIG_1MBITS();   return t; }
        case CAN_RATE_500K: { twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS(); return t; }
        case CAN_RATE_250K: { twai_timing_config_t t = TWAI_TIMING_CONFIG_250KBITS(); return t; }
        case CAN_RATE_125K: { twai_timing_config_t t = TWAI_TIMING_CONFIG_125KBITS(); return t; }
        default:            { twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS(); return t; }
    }
}

const char* twaiStateName(twai_state_t state) {
    switch (state) {
        case TWAI_STATE_STOPPED:    return "STOPPED";
        case TWAI_STATE_RUNNING:    return "RUNNING";
        case TWAI_STATE_BUS_OFF:    return "BUS_OFF";
        case TWAI_STATE_RECOVERING: return "RECOVERING";
        default:                    return "UNKNOWN";
    }
}

void clearIdStats() {
    for (int i = 0; i < MAX_TRACKED_IDS; i++) idStats[i] = IdStat();
}

void clearLog() {
    logWriteIndex  = 0;
    logCount       = 0;
    droppedLogLines = 0;
    canFrameCount  = 0;
    lastCanFrameMs = 0;
    ecuData        = LinkEcuData();
    clearIdStats();
}

void trackId(const twai_message_t& msg) {
    for (int i = 0; i < MAX_TRACKED_IDS; i++) {
        if (idStats[i].used && idStats[i].id == msg.identifier && idStats[i].ext == (bool)msg.extd) {
            idStats[i].count++;
            return;
        }
    }
    for (int i = 0; i < MAX_TRACKED_IDS; i++) {
        if (!idStats[i].used) {
            idStats[i].used  = true;
            idStats[i].id    = msg.identifier;
            idStats[i].ext   = msg.extd;
            idStats[i].count = 1;
            return;
        }
    }
}

void addLogLine(const char* line) {
    strncpy(logLines[logWriteIndex], line, LOG_LINE_LEN - 1);
    logLines[logWriteIndex][LOG_LINE_LEN - 1] = '\0';
    logWriteIndex = (logWriteIndex + 1) % LOG_CAPACITY;
    if (logCount < LOG_CAPACITY) logCount++;
    else droppedLogLines++;
}

uint16_t oldestLogIndex() {
    if (logCount < LOG_CAPACITY) return 0;
    return logWriteIndex;
}

void formatFrameLine(const twai_message_t& msg, char* out, size_t outSize) {
    int pos = snprintf(out, outSize, "%lu,0x%lX,%s,%s,%u",
        millis(),
        (unsigned long)msg.identifier,
        msg.extd ? "EXT" : "STD",
        msg.rtr  ? "RTR" : "DATA",
        msg.data_length_code);

    for (int i = 0; i < 8; i++) {
        if (pos < 0 || (size_t)pos >= outSize - 1) break;
        if (i < msg.data_length_code && !msg.rtr)
            pos += snprintf(out + pos, outSize - (size_t)pos, ",%02X", msg.data[i]);
        else
            pos += snprintf(out + pos, outSize - (size_t)pos, ",");
    }
    out[outSize - 1] = '\0';
}

// ---------------------------------------------------------------------------
// Decode a LINK ECU Stream 1 frame (ID 0x3E8, DLC 8)
// Layout (all big-endian / MS First):
//   [0:1] Engine Speed – unsigned, 1 RPM/bit
//   [2:3] MAP          – unsigned, 1 kPa/bit  (absolute)
//   [4:5] MGP          – signed,   1 kPa/bit  (gauge)
//   [6:7] Batt Voltage – unsigned, 0.01 V/bit (divide raw by 100)
// ---------------------------------------------------------------------------
void decodeLinkEcuFrame(const twai_message_t& msg) {
    if (msg.identifier != LINK_ECU_CAN_ID) return;
    if (msg.extd || msg.rtr)               return;
    if (msg.data_length_code < 8)          return;

    ecuData.engineRpm    = ((uint16_t)msg.data[0] << 8) | msg.data[1];
    ecuData.mapKpa       = ((uint16_t)msg.data[2] << 8) | msg.data[3];
    ecuData.mgpKpa       = (int16_t)(((uint16_t)msg.data[4] << 8) | msg.data[5]);
    uint16_t battRaw     = ((uint16_t)msg.data[6] << 8) | msg.data[7];
    ecuData.battV        = battRaw / 100.0f;
    ecuData.valid        = true;
    ecuData.lastUpdateMs = millis();
}

// ---------------------------------------------------------------------------
// CAN driver start / stop
// ---------------------------------------------------------------------------
bool stopCan() {
    if (!canStarted) return true;
    twai_stop();
    twai_driver_uninstall();
    canStarted = false;
    Serial.println("TWAI/CAN stopped");
    return true;
}

bool startCan() {
    if (canStarted) stopCan();

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        CAN_TX_PIN,
        CAN_RX_PIN,
        canListenOnly ? TWAI_MODE_LISTEN_ONLY : TWAI_MODE_NORMAL
    );
    g_config.rx_queue_len = 64;
    g_config.tx_queue_len = 4;

    twai_timing_config_t t_config = getTimingConfig();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t result = twai_driver_install(&g_config, &t_config, &f_config);
    if (result != ESP_OK) {
        Serial.printf("TWAI driver install failed: 0x%x\n", result);
        return false;
    }

    result = twai_start();
    if (result != ESP_OK) {
        Serial.printf("TWAI start failed: 0x%x\n", result);
        twai_driver_uninstall();
        return false;
    }

    canStarted = true;
    Serial.println();
    Serial.println("TWAI/CAN sniffer started");
    Serial.printf("  TX GPIO : %d\n", (int)CAN_TX_PIN);
    Serial.printf("  RX GPIO : %d\n", (int)CAN_RX_PIN);
    Serial.printf("  Mode    : %s\n", canListenOnly ? "listen-only (no ACK)" : "normal (ACK enabled)");
    Serial.printf("  Bitrate : %s\n", bitrateName());
    Serial.println();
    return true;
}

// ---------------------------------------------------------------------------
// Read all pending CAN frames from the TWAI RX queue
// ---------------------------------------------------------------------------
void readCanFrames() {
    if (!canStarted) return;

    twai_message_t msg;
    while (twai_receive(&msg, 0) == ESP_OK) {
        canFrameCount++;
        lastCanFrameMs = millis();

        trackId(msg);
        decodeLinkEcuFrame(msg);

        char line[LOG_LINE_LEN];
        formatFrameLine(msg, line, sizeof(line));
        addLogLine(line);

        unsigned long now = millis();
        if (now - lastSerialPrintMs >= 50) {
            lastSerialPrintMs = now;
            Serial.println(line);
        }
    }
}

// ---------------------------------------------------------------------------
// HTML page
// ---------------------------------------------------------------------------
String htmlPage() {
    return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>ESP32 CAN Sniffer – LINK ECU</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    :root {
      --bg: #0f1115; --panel: #1c2028; --panel2: #252b36;
      --text: #f2f4f8; --muted: #9da7b4;
      --accent: #4da3ff; --danger: #ff6b6b; --ok: #41d17d; --warn: #ffc857;
    }
    body { margin:0; font-family:Arial,Helvetica,sans-serif; background:var(--bg); color:var(--text); }
    header { padding:12px 16px; background:#171a21; border-bottom:1px solid #303642;
             display:flex; justify-content:space-between; gap:12px; align-items:center; }
    h1 { font-size:20px; margin:0; }
    h2 { font-size:15px; margin:10px 0 6px; color:var(--muted); }
    .status { color:var(--muted); font-size:13px; text-align:right; }
    main { padding:12px; }
    .grid { display:grid; grid-template-columns:repeat(auto-fit,minmax(140px,1fr)); gap:10px; margin-bottom:12px; }
    .card { background:var(--panel); border:1px solid #303642; border-radius:14px; padding:12px; }
    .ecu-card { background:#0d1f16; border:1px solid #1a4d2e; border-radius:14px; padding:12px; }
    .label { color:var(--muted); font-size:12px; margin-bottom:6px; }
    .value { font-size:24px; font-weight:bold; }
    .unit  { font-size:13px; color:var(--muted); font-weight:normal; margin-left:3px; }
    .stale { color:var(--warn); }
    .controls { display:grid; grid-template-columns:repeat(auto-fit,minmax(130px,1fr));
                gap:8px; margin-bottom:12px; }
    button,select,a.button { border:0; border-radius:10px; padding:12px 8px;
      background:var(--panel2); color:var(--text); font-size:15px;
      font-weight:bold; text-align:center; text-decoration:none; display:block; }
    button.primary { background:var(--accent); color:#07111d; }
    button.danger  { background:#6b1515; }
    .terminal { background:#05070a; color:#d7e2f2; border:1px solid #303642;
      border-radius:14px; padding:12px; font-family:Consolas,Menlo,Monaco,monospace;
      font-size:12px; white-space:pre; overflow:auto; height:46vh; }
    .ids { background:var(--panel); border:1px solid #303642; border-radius:14px; padding:12px;
      font-family:Consolas,Menlo,Monaco,monospace; font-size:12px; white-space:pre;
      overflow:auto; max-height:180px; margin-top:12px; }
    .ok { color:var(--ok); } .warn { color:var(--warn); } .dangerText { color:var(--danger); }
    @media(max-width:600px){ header{display:block;} .status{text-align:left;margin-top:6px;} .terminal{height:42vh;} }
  </style>
</head>
<body>
<header>
  <h1>ESP32 CAN Sniffer – LINK ECU</h1>
  <div class="status" id="top_status">Connecting...</div>
</header>
<main>

  <!-- LINK ECU decoded values -->
  <h2>LINK ECU – Stream 1 (ID 0x3E8)</h2>
  <div class="grid">
    <div class="ecu-card">
      <div class="label">Engine Speed</div>
      <div class="value" id="ecu_rpm">--<span class="unit">RPM</span></div>
    </div>
    <div class="ecu-card">
      <div class="label">MAP (absolute)</div>
      <div class="value" id="ecu_map">--<span class="unit">kPa</span></div>
    </div>
    <div class="ecu-card">
      <div class="label">MGP (gauge)</div>
      <div class="value" id="ecu_mgp">--<span class="unit">kPa</span></div>
    </div>
    <div class="ecu-card">
      <div class="label">Battery Voltage</div>
      <div class="value" id="ecu_batt">--<span class="unit">V</span></div>
    </div>
  </div>

  <!-- CAN bus stats -->
  <h2>CAN Bus</h2>
  <div class="grid">
    <div class="card"><div class="label">Frames</div><div class="value" id="frames">0</div></div>
    <div class="card"><div class="label">Last Frame Age</div><div class="value" id="last_age">--</div></div>
    <div class="card"><div class="label">CAN State</div><div class="value" id="can_state">--</div></div>
    <div class="card"><div class="label">Mode / Bitrate</div><div class="value" id="mode_rate">--</div></div>
    <div class="card"><div class="label">Log Lines</div><div class="value" id="log_lines">0</div></div>
    <div class="card"><div class="label">Dropped</div><div class="value" id="dropped">0</div></div>
  </div>

  <!-- Controls -->
  <div class="controls">
    <select id="bitrate" onchange="applyCanConfig()">
      <option value="1M">1 Mbit/s</option>
      <option value="500K">500 kbit/s</option>
      <option value="250K">250 kbit/s</option>
      <option value="125K">125 kbit/s</option>
    </select>
    <select id="mode" onchange="applyCanConfig()">
      <option value="normal">Normal / ACK</option>
      <option value="listen">Listen-only</option>
    </select>
    <button class="primary" onclick="applyCanConfig()">Apply CAN Config</button>
    <button onclick="pauseToggle()" id="pause_btn">Pause View</button>
    <button class="danger" onclick="clearLog()">Clear Log</button>
    <a class="button" href="/download" download="can_log.csv">Download CSV</a>
  </div>

  <div class="terminal" id="terminal">Waiting for CAN frames...</div>
  <div class="ids" id="ids">ID summary will appear here.</div>
</main>

<script>
let paused = false;
let configApplying = false;
let lastKnownBitrate = '1M';
let lastKnownMode = 'normal';

function pauseToggle() {
  paused = !paused;
  document.getElementById('pause_btn').textContent = paused ? 'Resume View' : 'Pause View';
}

async function applyCanConfig() {
  const bitrateEl = document.getElementById('bitrate');
  const modeEl    = document.getElementById('mode');
  const bitrate   = bitrateEl.value;
  const mode      = modeEl.value;
  configApplying  = true;
  try {
    document.getElementById('top_status').textContent =
      'Applying CAN config: ' + bitrate + ' / ' + mode + '...';
    const res = await fetch(
      '/config?bitrate=' + encodeURIComponent(bitrate) +
      '&mode='    + encodeURIComponent(mode) +
      '&_='       + Date.now(), { cache: 'no-store' });
    const reply = await res.json();
    if (reply.ok) {
      lastKnownBitrate = reply.bitrate;
      lastKnownMode    = reply.mode === 'listen-only' ? 'listen' : 'normal';
      bitrateEl.value  = lastKnownBitrate;
      modeEl.value     = lastKnownMode;
    }
  } catch(err) {
    document.getElementById('top_status').textContent = 'Config apply failed';
  }
  configApplying = false;
  await refreshAll();
}

async function clearLog() {
  if (!confirm('Clear captured CAN log?')) return;
  await fetch('/clear?_=' + Date.now(), { cache: 'no-store' });
  await refreshAll();
}

async function refreshDecoded() {
  try {
    const res = await fetch('/decoded');
    const d   = await res.json();
    const age = d.last_age_ms;
    const stale = age < 0 || age > 2000;
    function set(id, text) {
      const el = document.getElementById(id);
      if (stale) el.className = 'value stale'; else el.className = 'value';
      const unit = el.querySelector('.unit');
      el.textContent = text;
      if (unit) el.appendChild(unit);
    }
    if (d.valid) {
      set('ecu_rpm',  d.engine_rpm);
      set('ecu_map',  d.map_kpa);
      set('ecu_mgp',  d.mgp_kpa);
      set('ecu_batt', d.batt_v.toFixed(2));
    } else {
      ['ecu_rpm','ecu_map','ecu_mgp','ecu_batt'].forEach(id => {
        document.getElementById(id).textContent = '--';
      });
    }
  } catch(err) { /* ignore */ }
}

async function refreshStatus() {
  try {
    const res = await fetch('/status');
    const s   = await res.json();
    document.getElementById('frames').textContent    = s.frames;
    document.getElementById('last_age').textContent  = s.last_age_ms < 0 ? '--' : s.last_age_ms + ' ms';
    document.getElementById('can_state').textContent = s.can_state;
    document.getElementById('mode_rate').textContent = s.mode + ' / ' + s.bitrate;
    document.getElementById('log_lines').textContent = s.log_lines;
    document.getElementById('dropped').textContent   = s.dropped;
    lastKnownBitrate = s.bitrate;
    lastKnownMode    = s.mode === 'listen-only' ? 'listen' : 'normal';
    const activeId = document.activeElement ? document.activeElement.id : '';
    if (!configApplying && activeId !== 'bitrate' && activeId !== 'mode') {
      document.getElementById('bitrate').value = lastKnownBitrate;
      document.getElementById('mode').value    = lastKnownMode;
    }
    const st = document.getElementById('top_status');
    if (s.frames > 0 && s.last_age_ms >= 0 && s.last_age_ms < 1000)
      st.innerHTML = '<span class="ok">Live CAN frames detected</span>';
    else if (s.frames > 0)
      st.innerHTML = '<span class="warn">Frames seen, stream may be stale</span>';
    else
      st.innerHTML = '<span class="dangerText">No CAN frames seen yet</span>';
  } catch(err) { document.getElementById('top_status').textContent = 'Status fetch error'; }
}

async function refreshTerminal() {
  if (paused) return;
  try {
    const res  = await fetch('/terminal?lines=120');
    const text = await res.text();
    const el   = document.getElementById('terminal');
    el.textContent = text || 'No log lines yet.';
    el.scrollTop   = el.scrollHeight;
  } catch(err) { document.getElementById('terminal').textContent = 'Terminal fetch error'; }
}

async function refreshIds() {
  try {
    const res  = await fetch('/ids');
    const text = await res.text();
    document.getElementById('ids').textContent = text || 'No IDs seen yet.';
  } catch(err) { document.getElementById('ids').textContent = 'ID summary fetch error'; }
}

async function refreshAll() {
  await refreshStatus();
  await refreshDecoded();
  await refreshTerminal();
  await refreshIds();
}

setInterval(refreshStatus,  500);
setInterval(refreshDecoded, 500);
setInterval(refreshTerminal, 500);
setInterval(refreshIds,     2000);
refreshAll();
</script>
</body>
</html>
)rawliteral";
}

// ---------------------------------------------------------------------------
// HTTP handlers
// ---------------------------------------------------------------------------
void handleRoot() {
    server.send(200, "text/html", htmlPage());
}

void handleStatus() {
    unsigned long now = millis();
    twai_status_info_t twaiStatus;
    bool hasStatus = canStarted && twai_get_status_info(&twaiStatus) == ESP_OK;
    long lastAge   = lastCanFrameMs > 0 ? (long)(now - lastCanFrameMs) : -1;

    String json = "{";
    json += "\"frames\":"     + String(canFrameCount) + ",";
    json += "\"last_age_ms\":" + String(lastAge) + ",";
    json += "\"can_state\":\"" + String(hasStatus ? twaiStateName(twaiStatus.state) : "NOT_STARTED") + "\",";
    json += "\"tx_err\":"      + String(hasStatus ? twaiStatus.tx_error_counter : 0) + ",";
    json += "\"rx_err\":"      + String(hasStatus ? twaiStatus.rx_error_counter : 0) + ",";
    json += "\"rx_missed\":"   + String(hasStatus ? twaiStatus.rx_missed_count  : 0) + ",";
    json += "\"bus_error\":"   + String(hasStatus ? twaiStatus.bus_error_count  : 0) + ",";
    json += "\"mode\":\""      + String(canListenOnly ? "listen-only" : "normal") + "\",";
    json += "\"bitrate\":\""   + String(bitrateName()) + "\",";
    json += "\"log_lines\":"   + String(logCount) + ",";
    json += "\"dropped\":"     + String(droppedLogLines);
    json += "}";
    server.send(200, "application/json", json);
}

void handleDecoded() {
    unsigned long now = millis();
    long lastAge = ecuData.lastUpdateMs > 0 ? (long)(now - ecuData.lastUpdateMs) : -1;

    // Format batt_v with two decimal places (batt voltage is always non-negative)
    char battBuf[12];
    float bv = ecuData.battV < 0.0f ? 0.0f : ecuData.battV;
    snprintf(battBuf, sizeof(battBuf), "%d.%02d",
        (int)bv,
        (int)((bv - (int)bv) * 100.0f + 0.5f));

    String json = "{";
    json += "\"valid\":"       + String(ecuData.valid ? "true" : "false") + ",";
    json += "\"last_age_ms\":" + String(lastAge) + ",";
    json += "\"engine_rpm\":"  + String(ecuData.engineRpm) + ",";
    json += "\"map_kpa\":"     + String(ecuData.mapKpa) + ",";
    json += "\"mgp_kpa\":"     + String(ecuData.mgpKpa) + ",";
    json += "\"batt_v\":"      + String(battBuf);
    json += "}";
    server.send(200, "application/json", json);
}

void handleTerminal() {
    int requested = 120;
    if (server.hasArg("lines")) requested = server.arg("lines").toInt();
    if (requested < 1)           requested = 1;
    if (requested > LOG_CAPACITY) requested = LOG_CAPACITY;

    uint16_t linesToSend = min((uint16_t)requested, logCount);
    uint16_t start;
    if (logCount < LOG_CAPACITY)
        start = logCount > linesToSend ? logCount - linesToSend : 0;
    else
        start = (logWriteIndex + LOG_CAPACITY - linesToSend) % LOG_CAPACITY;

    String out;
    out.reserve(linesToSend * 80);
    for (uint16_t i = 0; i < linesToSend; i++) {
        uint16_t idx = (start + i) % LOG_CAPACITY;
        out += logLines[idx];
        out += "\n";
    }
    server.send(200, "text/plain", out);
}

void handleIds() {
    String out;
    out.reserve(2048);
    out += "id,format,count\n";
    for (int i = 0; i < MAX_TRACKED_IDS; i++) {
        if (!idStats[i].used) continue;
        out += "0x";
        out += String(idStats[i].id, HEX);
        out += ",";
        out += idStats[i].ext ? "EXT" : "STD";
        out += ",";
        out += String(idStats[i].count);
        out += "\n";
    }
    server.send(200, "text/plain", out);
}

void handleDownload() {
    server.sendHeader("Content-Disposition", "attachment; filename=\"can_log.csv\"");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/csv", "");
    server.sendContent("ms,id,format,type,dlc,b0,b1,b2,b3,b4,b5,b6,b7\n");
    uint16_t start = oldestLogIndex();
    for (uint16_t i = 0; i < logCount; i++) {
        uint16_t idx = (start + i) % LOG_CAPACITY;
        server.sendContent(logLines[idx]);
        server.sendContent("\n");
    }
    server.sendContent("");
}

void handleClear() {
    clearLog();
    server.send(200, "application/json", "{\"ok\":true}");
}

void handleConfig() {
    bool changed = false;

    if (server.hasArg("bitrate")) {
        String b = server.arg("bitrate");
        b.trim(); b.toUpperCase();
        CanBitrate prev = canBitrate;
        if      (b == "1M")   canBitrate = CAN_RATE_1M;
        else if (b == "500K") canBitrate = CAN_RATE_500K;
        else if (b == "250K") canBitrate = CAN_RATE_250K;
        else if (b == "125K") canBitrate = CAN_RATE_125K;
        if (canBitrate != prev) changed = true;
    }

    if (server.hasArg("mode")) {
        String m = server.arg("mode");
        m.trim(); m.toLowerCase();
        bool prev = canListenOnly;
        if      (m == "listen" || m == "listen-only") canListenOnly = true;
        else if (m == "normal" || m == "ack")         canListenOnly = false;
        if (canListenOnly != prev) changed = true;
    }

    bool startOk = true;
    if (changed) {
        clearLog();
        startOk = startCan();
    }

    String json = "{";
    json += "\"ok\":"       + String(startOk ? "true" : "false") + ",";
    json += "\"bitrate\":\"" + String(bitrateName()) + "\",";
    json += "\"mode\":\""    + String(canListenOnly ? "listen-only" : "normal") + "\"";
    json += "}";
    server.send(200, "application/json", json);
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println();
    Serial.println("ESP32 Wi-Fi CAN Sniffer – LINK ECU G4 MonsoonX");

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_GW, AP_SUBNET);
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.printf("Wi-Fi AP SSID : %s\n", AP_SSID);
    Serial.printf("Wi-Fi AP IP   : %s\n", WiFi.softAPIP().toString().c_str());
    Serial.println("Open browser  : http://192.168.6.1");

    startCan();

    server.on("/",        handleRoot);
    server.on("/status",  handleStatus);
    server.on("/decoded", handleDecoded);
    server.on("/terminal",handleTerminal);
    server.on("/ids",     handleIds);
    server.on("/download",handleDownload);
    server.on("/clear",   handleClear);
    server.on("/config",  handleConfig);
    server.begin();
    Serial.println("Web server started");
}

void loop() {
    readCanFrames();
    server.handleClient();

    unsigned long now = millis();
    if (now - lastStatusMs >= 1000) {
        lastStatusMs = now;
        Serial.printf("STATUS frames=%lu log=%u dropped=%u bitrate=%s mode=%s",
            canFrameCount, logCount, droppedLogLines,
            bitrateName(),
            canListenOnly ? "listen-only" : "normal");
        if (lastCanFrameMs == 0) Serial.println(" last_age=none");
        else Serial.printf(" last_age_ms=%lu\n", now - lastCanFrameMs);

        if (ecuData.valid) {
            Serial.printf("  ECU rpm=%u MAP=%u kPa MGP=%d kPa Batt=%.2fV\n",
                ecuData.engineRpm, ecuData.mapKpa,
                ecuData.mgpKpa,    ecuData.battV);
        }
    }
}
