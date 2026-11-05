[README_Version3.md](https://github.com/user-attachments/files/34425629/README_Version3.md)
# HIGI Hen House — V3.6

An ESP32-based automatic coop controller for managing the door, lighting and environmental monitoring. It is designed for unattended operation, with manual controls and safety checks for the door mechanism.

The Version 3 series is bolted onto the v2.22 sketch which is a robust build in it's self. With just the code required for integration with the other tabs. 
The only additional code added to the V2.22 was the upgrade from a coop light relay module to the YX-mos mosfet switch to enable the coop light dim to off instead of an abrupt light off.

## What it does

- **Automatic door control:** Opens and closes to a configurable schedule based on sunrise/sunset or fixed times. UK GMT/BST changes are handled automatically.
- **Cold-weather opening delay:** If the outside temperature is below the configured threshold, the scheduled morning opening can be delayed. The delay can be overridden in the settings.
- **Manual bedtime close from the dashboard:** During the bedtime warning window, a glowing gold pill appears on the dashboard. This was added for evenings when cold, wind or rain makes it better for the chickens to close the coop early. Tap the pill to request a close; the ESP32 rechecks whether closing is allowed before moving the door. The page shows “Sending” and “Closing” feedback while the request is handled.
- **Password-protected OTA updates:** Firmware can be uploaded over the LAN using ArduinoOTA, without connecting a USB cable. OTA is disabled while the door is moving.
- **LAN dashboard:** scaled to fit onto a phone, View temperatures, humidity, daily minimums and maximums, trends, door and lighting status, schedule, cold-delay status and Wi-Fi signal. 
- **Coop Log:** View recent system messages in the dashboard. The log is held in RAM and is cleared by a restart.
- **Coop lighting:** Scheduled lighting supports configurable PWM dimming and a gradual sunset fade. Manual light control is also available.
- **Door safety:** Limit switches, movement timeouts, obstruction recovery, retry limits and fault lockout help protect the door mechanism.
- **History and monitoring:** Temperature and humidity readings and daily history are maintained, with history saved in ESP32 non-volatile storage.

## Features

-   Automatic door opening and closing based on calculated
    sunrise/sunset times or configurable fixed morning opening times.
-   UK GMT/BST-aware local scheduling.
-   Separate winter (GMT) and summer (BST) schedule settings.
-   Optional cold-weather delay before morning opening, based on the
    outdoor sensor.
-   Automatic coop-light control, with a manual light override.
-   Motor run-time protection, limit-switch handling, obstruction
    reversal, retry handling, and fault lockout.
-   Two red door-area LEDs for bedtime warnings and overnight
    indication, plus a blue cold-weather indicator.
-   Inside and outside temperature/humidity readings using SHT3x
    sensors.
-   OLED pages for status, schedule, manual controls, environment
    history, events, cold/daytime settings, and Wi-Fi information.
-   Rotary encoder for page navigation and controls.
-   DS3231 real-time clock, Wi-Fi/NTP time synchronisation, and fallback
    time seeding from the RTC.
-   Non-volatile storage (ESP32 Preferences/NVS) for environment history
    and selected settings.
-   Status/fault LEDs, buzzer alerts, Wi-Fi retry handling, and watchdog
    servicing.


## Hardware and software

This firmware is configured for an **ESP32-S3 N16R8**, using **ESP32-S3 Dev Module** in the Arduino IDE. It uses an inside SHT31 sensor, an optional outside SHT30 sensor, a DS3231 RTC, an SH1106 OLED display, a rotary encoder and a motor driver.

The sketch uses the ESP32 Arduino core 3.x LEDC API for the coop-light PWM output. Install the libraries required by the sketch before compiling.

## Files

Keep all sketch tabs and headers together in the Arduino sketch folder:

- `Hen_House_V3_6.ino` — main controller
- `Dashboard.ino` — LAN dashboard and its API
- `OTA.ino` — password-protected OTA support
- `HumTrend.h` — trend history data structure
- `RemoteLog.h` — dashboard log buffer
- `secrets.h` — credential template; replace the placeholders with your own Wi-Fi credentials and OTA password and lat/long coordinents before compiling.

## Network and safety notes

The dashboard uses plain HTTP and has **no login**. Anyone who can access the device on your local network may view the dashboard and may be able to request a door close while the board allows it. Use it only on a trusted LAN; do not expose it directly to the internet. you will find the IP address on the "system WiFi" page of the control box. just type the IP address into your web browser.

The browser is not the safety authority: the ESP32 checks the close conditions again before acting. The physical installation still needs suitable limit switches, correctly wired motor control, and safe mechanical travel.
