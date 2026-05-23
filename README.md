# g4_can_sniffer

ESP32 Wi-Fi CAN bus sniffer for the **LINK ECU G4 MonsoonX** (and any G4/G4+ ECU
with a User Defined CAN stream).  Decodes the ECU's live data and hosts a web UI
accessible from any phone or tablet connected to its Wi-Fi hotspot.

---

## Hardware

| ESP32 GPIO | SN65HVD230 pin | Notes |
|-----------|----------------|-------|
| GPIO 5    | TXD (CTX)      | CAN TX – drives transceiver |
| GPIO 4    | RXD (CRX)      | CAN RX – receives from transceiver |
| 3V3       | VCC            | 3.3 V supply |
| GND       | GND            | Ground |
| –         | CANH           | Connect to ECU CAN High |
| –         | CANL           | Connect to ECU CAN Low |

> **Important:** Do **not** add external termination resistors if the ECU
> already terminates the bus (most LINK ECUs do internally).  Adding extra
> 120 Ω resistors creates a 60 Ω impedance which will degrade signal quality.

---

## ECU CAN configuration (LINK PCLink)

```
CAN Module 1
  Mode    : User Defined
  Bitrate : 1 Mbit/s

Channel 1
  Mode         : Transmit User Stream 1
  ID           : 1000 decimal (0x3E8)
  Format       : Standard
  Transmit Rate: 50 Hz

Stream 1 – Frame 1
  Parameter     Start  Width  Order    Type      Multiplier  Divider  Offset
  Engine Speed    0     16    MS First  Unsigned     1          1       0
  MAP            16     16    MS First  Unsigned     1          1       0
  MGP            32     16    MS First  Signed       1          1       0
  Batt Voltage   48     16    MS First  Unsigned   100          1       0
```

---

## Building & flashing

Install [PlatformIO](https://platformio.org/) (CLI or VS Code extension), then:

```bash
# Build
pio run

# Build and flash over USB
pio run -t upload

# Open serial monitor (115200 baud)
pio device monitor
```

---

## Wi-Fi & web UI

After flashing, the ESP32 creates a Wi-Fi access point:

| Setting  | Value         |
|----------|---------------|
| SSID     | `CANSniffer`  |
| Password | `cansniff123` |
| URL      | http://192.168.6.1 |

### Web UI features

* **LINK ECU panel** – live decoded Engine Speed, MAP, MGP, and Battery Voltage
* **CAN bus stats** – frame count, last frame age, TWAI state, TX/RX error counters
* **Mode selector** – switch between *Normal / ACK* and *Listen-only* without reflashing
* **Bitrate selector** – 1M / 500K / 250K / 125K without reflashing
* **Rolling terminal** – last 120 raw CAN frames with timestamps
* **ID summary** – all seen CAN IDs, format, and frame counts
* **CSV download** – complete captured log as a `.csv` file

### Why Normal/ACK mode is the default

When the ESP32 is the only node other than the ECU on the bus, running in
*listen-only* mode means no node ever sends an ACK bit.  The ECU's transmit
error counter increments on every frame until it goes *error-passive* or
*bus-off*, making it appear as if no frames arrive.  **Normal (ACK-enabled)
mode** solves this while still capturing every frame.

Switch to *listen-only* mode from the web UI if you are tapping into a larger
active bus where other nodes already provide ACKs.

---

## Serial output

```
ESP32 Wi-Fi CAN Sniffer – LINK ECU G4 MonsoonX
Wi-Fi AP SSID : CANSniffer
Wi-Fi AP IP   : 192.168.6.1
Open browser  : http://192.168.6.1
TWAI/CAN sniffer started
  TX GPIO : 5
  RX GPIO : 4
  Mode    : normal (ACK enabled)
  Bitrate : 1M

STATUS frames=50 log=50 dropped=0 bitrate=1M mode=normal last_age_ms=4
  ECU rpm=2350 MAP=98 kPa MGP=-2 kPa Batt=14.20V
```

---

## REST API

| Endpoint      | Method | Description |
|---------------|--------|-------------|
| `/`           | GET    | Web UI (HTML) |
| `/status`     | GET    | CAN bus statistics (JSON) |
| `/decoded`    | GET    | Decoded LINK ECU values (JSON) |
| `/terminal`   | GET    | Recent raw CAN frames (plain text, `?lines=N`) |
| `/ids`        | GET    | Seen CAN IDs summary (plain text) |
| `/download`   | GET    | Full captured log (CSV attachment) |
| `/clear`      | GET    | Clear log and reset counters |
| `/config`     | GET    | Set `?bitrate=1M\|500K\|250K\|125K` and/or `?mode=normal\|listen` |

### `/decoded` response example

```json
{
  "valid": true,
  "last_age_ms": 12,
  "engine_rpm": 2350,
  "map_kpa": 98,
  "mgp_kpa": -2,
  "batt_v": 14.20
}
```