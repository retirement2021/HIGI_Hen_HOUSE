# CoopCam V1.06

ESP32-S3 firmware for a chicken-coop camera, IR lamp and optional lens heater.

## What it does

- Switches the camera's 12 V supply off daily at **08:45–15:30** and **00:02–05:30**.
- Drives an IR lamp around sunset by PWM, with a 4-second fade and a 10-minute manual override.
- Reads a DS3231 RTC and an SHT4x temperature/humidity sensor on a shared I2C bus.
- Calculates sunrise/sunset offline from the latitude/longitude in `secrets.h`.
- Optionally drives a 12 V / 7 W lens heater:
  - manual tile: one 5-minute burst;
  - automatic: one 5-minute burst on any camera power-up, only when ambient is at or below 8 °C and `temp − dew point ≤ 2 °C`;
  - hard cut-out at 15 °C; invalid sensor data forces the heater off.
- Serves a small web dashboard and a timestamped remote log at `/api/log`.

The dashboard's **View Camera** tile opens the camera's own page at
`http://192.168.1.98/index.html`. This version does not proxy the camera, so a
new camera tab requires the camera's normal login.

## Hardware map

| GPIO | Function |
| --- | --- |
| 4 | Camera relay (active LOW) |
| 5 | IR lamp PWM (YX-MOS) |
| 6 | Lens heater switch (YX-MOS) |
| 8 | I2C SDA — DS3231 + SHT4x |
| 9 | I2C SCL — DS3231 + SHT4x |
| 10 | Manual IR button to GND |
| 48 | Onboard WS2812 status LED |

All grounds are common. The camera, IR lamp and heater use a 12 V supply; a
buck converter provides 5 V to the ESP32, relay and YX-MOS modules.

## Configuration

Edit the `USER SETTINGS` block near the top of `CoopCam_V1_06.ino` for schedules,
IR timing, heater options and pin changes.

Put WiFi credentials and location in `secrets.h`:

```cpp
#define WIFI_SSID "your-ssid"
#define WIFI_PASS "your-password"
constexpr float LATITUDE  = 50.8559f;
constexpr float LONGITUDE = -3.9462f;

OTA password higicoopcam
change password in the coopcam.ino on line 108
```

## Build

Arduino IDE board settings:

- Board: **ESP32S3 Dev Module**
- Flash: **16 MB**
- Partition scheme: **16 MB with OTA** (two app slots)
- PSRAM: **Disabled**
- USB CDC On Boot: **Enabled**

Required libraries:

- **RTClib** by Adafruit
- **Sensirion I2C SHT4x** by Sensirion

Upload over USB or via ArduinoOTA.

## Web endpoints

| Path | Purpose |
| --- | --- |
| `/` | Dashboard |
| `/status.json` | Current status as JSON |
| `/api/log` | Timestamped remote log |
| `/cam/on`, `/cam/off` | Manual camera power |
| `/ir/up`, `/ir/down` | IR brightness |
| `/lamp/on`, `/lamp/off` | IR lamp override |
| `/heater/toggle` | Manual heater burst |
| `/override/clear` | Cancel manual override |
| `/sim` | Simulator/test mode |
| `/reboot` | Restart the ESP32 |

