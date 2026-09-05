# Link ECU CAN channels 1-3

This build decodes three standard 8-byte Link ECU user-defined CAN channels.

## CAN identifiers

The firmware assumes the following sequential identifiers:

| Channel | Decimal ID | Hex ID |
|---|---:|---:|
| 1 | 1000 | `0x3E8` |
| 2 | 1001 | `0x3E9` |
| 3 | 1002 | `0x3EA` |

If the IDs configured in PCLink are different, edit these constants near the top of `src/can_bus.cpp`:

```cpp
static constexpr uint32_t LINK_ECU_CAN_ID_CHANNEL_1 = 0x3E8U;
static constexpr uint32_t LINK_ECU_CAN_ID_CHANNEL_2 = 0x3E9U;
static constexpr uint32_t LINK_ECU_CAN_ID_CHANNEL_3 = 0x3EAU;
```

## Decoded parameters

### Channel 1

| Bytes | Parameter | Decode |
|---|---|---|
| 0-1 | Engine Speed | unsigned big-endian, raw |
| 2-3 | MAP | unsigned big-endian, raw kPa |
| 4-5 | MGP | signed big-endian, raw kPa |
| 6-7 | Battery Voltage | unsigned big-endian, raw / 100 V |

### Channel 2

| Bytes | Parameter | Decode |
|---|---|---|
| 0-1 | GP Speed 1 | unsigned big-endian, raw / 10 |
| 2 | Gear | unsigned byte |
| 3 | TPS | unsigned byte, raw / 2 percent |
| 4-5 | ECT | signed big-endian, raw / 10 °C |
| 6-7 | IAT | signed big-endian, raw / 10 °C |

### Channel 3

| Bytes | Parameter | Decode |
|---|---|---|
| 0-1 | Oil Pressure | unsigned big-endian, raw / 10 kPa |
| 2-3 | Fuel Pressure | unsigned big-endian, raw / 10 kPa |
| 4-5 | Lambda 1 | unsigned big-endian, raw / 1000 |
| 6-7 | Lambda Target | unsigned big-endian, raw / 1000 |

## Dashboard mapping

The drive page uses:

- Engine Speed → RPM display
- Gear → gear indicator
- MGP → boost panel, converted from kPa to psi in the browser
- Oil Pressure → oil panel, converted from kPa to psi in the browser
- Battery Voltage → voltage panel
- ECT → water-temperature panel
- IAT → air-temperature panel

The gear/debug tab also displays GP Speed 1, TPS, MAP, MGP, oil pressure, fuel pressure, Lambda 1, Lambda Target and battery voltage.

The `/canStatus` route now reports independent frame counts and ages for all three channels.
