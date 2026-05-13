
# Higi Hen House Controller V1.1

WiFi-enabled automatic chicken coop door and lighting controller for ESP32-S3.

Designed for reliable poultry coop automation using sunrise/sunset calculations, RTC backup timing, OLED status display, and safety fault protection.

---

## Features

- Automatic coop door control
- Sunrise / sunset calculations
- WiFi + NTP time synchronization
- DS3231 RTC backup clock
- OLED live status display
- Manual override controls
- Coop lighting automation
- Motor timeout protection
- Fault detection and lockout protection
- Event history logging
- Automatic startup homing
- ESP32-S3 compatible

---

## Hardware Required

- ESP32-S3 development board
- DS3231 RTC module
- 1.3" SH1106 OLED display
- L298N motor driver
- 12V door motor
- Relay module for coop lighting
- Limit switches
- Push buttons

---

## Pin Assignments

| Function | GPIO |
|---|---|
| Motor Open | 41 |
| Motor Close | 42 |
| Open Limit Switch | 15 |
| Close Limit Switch | 16 |
| Coop Light Relay | 17 |
| Manual Light Switch | 13 |
| Manual Open Switch | 4 |
| Manual Close Switch | 5 |
| Event Scroll Button | 18 |

I2C:
- SDA = GPIO 8
- SCL = GPIO 9

---

## Setup

### 1. Install Arduino IDE

Install:
- ESP32 board support
- Required libraries

### 2. Required Libraries

Install using Arduino Library Manager:

- RTClib
- U8g2
- WiFi (ESP32 core)

---

## Configure Secrets

Create a file named:

```cpp
Secrets.h
```

Add:

```cpp
#pragma once

#define WIFI_SSID "yourWiFi"
#define WIFI_PASSWORD "yourPassword"

double latitude = 50.0000;
double longitude = -3.0000;
```

---

## Uploading

1. Select:
   - Board: ESP32S3 Dev Module
2. Open:
   - `HigiHenHouse.ino`
3. Upload firmware

---

## Safety Features

- Motor runtime timeout
- Safety reopen cycle
- Fault detection
- Automatic lockout after repeated failures
- Manual override timeout
- Startup homing recovery

---

## Recommended Final Build Additions

- Physical reset button
- Fuse protection
- Waterproof enclosure
- Emergency manual door release
- Battery backup

---



Higi Hen House Project
