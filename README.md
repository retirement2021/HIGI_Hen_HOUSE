# 🐔 Hen House Automation (ESP32-S3)

A reliable, fully automated chicken coop controller built around the **ESP32-S3**, designed for year-round unattended operation.

All configurable settings put together for easy customisation in *User adjustable settings*

This project automates the daily management of a hen house by controlling the coop door and lighting based on sunrise and sunset calculations, while continuously monitoring environmental conditions and protecting the system with multiple safety features.

The firmware has been designed with robustness in mind, allowing it to continue operating even if Wi-Fi or internet access is unavailable.


---

## Features

### 🚪 Automatic Coop Door

* Opens automatically at sunrise.
* Closes automatically at sunset.
* Configurable door open/close offsets.
* Uses calculated sunrise and sunset times.
* Seasonal adjustment for BST/GMT.
* Safe motor control using limit switches.
* Startup recovery after power loss.
* Automatic fault detection and lockout protection.

### 💡 Automatic Lighting

* Interior coop lighting controlled independently.
* Configurable on/off offsets from sunset.
* Manual override available.
* Automatic return to scheduled operation.

###   Manual Operations

* Door Open/Close
* Light On/Off

### 🌡️ Environmental Monitoring

* Temperature monitoring.
* Humidity monitoring.
* Live readings displayed on the OLED screen.
* Persistent 30 day min & max temperature and humidity storage with time and date stamps

### ⏰ Accurate Timekeeping

* NTP time synchronisation over Wi-Fi.
* Real-Time Clock (RTC) backup.
* Continues operating if internet is unavailable.
* Automatic time recovery after reconnecting.

### 📺 OLED Status Display

Displays:

* Current time
* Temperature
* Humidity
* Sunrise and sunset times
* Events history with time stamps
* Environment logging
* Door status
* Lighting status
* Wi-Fi information
* System diagnostics

### 🔔 Alerts & Indicators

* Status LEDs
* Audible buzzer notifications
* Fault indication
* Coop door Bedtime warning indication
* Coop door Night time indication

### 🛡️ Reliability Features

* Watchdog monitoring
* Motor timeout protection
* Door obstruction detection
* Limit switch validation
* Automatic recovery from power failures
* Designed for continuous 24/7 operation

---

# Hardware

The project is designed around the following hardware:

* ESP32-S3 (N16R8)
* OLED display 1.3" (U8g2)
* DHT22 temperature/humidity sensor
* DS3231 RTC module
* DC motor GB37-520
* Motor driver L298N
* Open and close limit reed switch
* Status LEDs
* Piezo buzzer
* Rotary encoder. KY–40
* Logic level relay module (Coop light)
* Coop door LEDs

---

# Software Libraries

Typical libraries used include:

* WiFi
* Preferences
* RTClib
* U8g2
* DHT Sensor Library
* NTPClient (or ESP32 time functions)

---

# Operation

Once powered, the controller:

1. Initialises all hardware.
2. Connects to Wi-Fi (if available).
3. Synchronises the clock using NTP.
4. Falls back to the RTC if Wi-Fi is unavailable.
5. Calculates today's sunrise and sunset.
6. Recovers the correct door position if required.
7. Enters fully automatic operation.

Throughout the day the controller:

* Opens the coop door at sunrise.
* Controls interior lighting according to schedule.
* Continuously monitors temperature and humidity.
* Updates the OLED display.
* Monitors motor safety.
* Detects faults and reports errors.
* Stores min/max temp & humidity in non-volatile memory.
* Stores event history in volitile memory
* Close the coop door at sunset.

---

# Safety

The firmware includes several layers of protection to help prevent equipment damage and improve reliability:

* Motor timeout protection
* Limit switch validation
* Startup safety checks
* Fault lockout
* Manual override timeout
* RTC backup
* Automatic recovery after power interruption
* Watchdog monitoring

---

# Project Goals

This project was created to provide a dependable chicken coop automation system that requires minimal daily intervention while remaining safe and reliable.

The focus throughout development has been on:

* Reliability
* Fault tolerance
* Accurate scheduling
* Ease of use
* Low maintenance
* Long-term autonomous operation

---


# License

This project is released under the MIT License.

Feel free to use, modify and improve the code. Contributions, suggestions and pull requests are always welcome.

---

## Author

Designed and developed as a robust ESP32-S3 automation system for reliable, real-world chicken coop management.

*"Because the hens deserve a lie-in too."* 🐔
