# 🐔 Automated Hen House Controller

### ESP32-S3 based automatic chicken-coop door, lighting and environmental monitoring system

An automated hen-house control system built around an **ESP32-S3**. The system controls a **guillotine-style chicken door powered by a geared motor**, providing automatic opening and closing while incorporating multiple safety features to protect the hens and the mechanism.

The controller also provides environmental monitoring, automatic lighting, an OLED user interface, event logging, Wi-Fi time synchronisation and long-term temperature/humidity recording.

---

## ✨ Features

* 🐔 **Automatic guillotine-style door**
* ⚙️ Geared motor drive with OPEN/CLOSE control
* 🌅 Sunrise/sunset based scheduling
* 🕐 Separate GMT and BST schedules
* 🌡️ Inside and outside temperature/humidity monitoring
* ❄️ Cold-weather morning opening delay
* 💡 Automatic evening lighting
* 🔴 Progressive night/bedtime warning LEDs
* 🛡️ Door obstruction detection and recovery
* 🚪 Limit-switch monitoring and motor timeout protection
* 🔒 Fault detection and lockout system
* 🔔 Audible fault alarm
* 📊 90-day environmental history
* 📝 200-entry event log
* 🕰️ DS3231 RTC with NTP time synchronisation
* 📶 Wi-Fi monitoring and automatic reconnection
* 🖥️ 128×64 SH1106 OLED display
* 🎛️ Rotary encoder user interface
* 💾 Persistent settings and data storage
* 🐕 Watchdog protection for unattended operation

---

# 🏠 Door System

The hen house uses a **vertical guillotine-style door** driven by a **geared motor**.

The motor is controlled in both directions to raise and lower the door, with dedicated limit switches confirming the fully open and fully closed positions.

The door controller uses a state-machine architecture rather than simply running the motor for a predetermined time.

### Door safety features

* Separate OPEN and CLOSED limit switches
* Motor direction interlock
* 50 ms direction-change safety delay
* 15-second motor timeout
* Door position detection at startup
* Automatic homing if the door position is unknown
* Limit-switch conflict detection
* Closing obstruction detection
* Automatic reopening after an obstruction
* Three-minute chicken-release period
* Maximum obstruction retries
* Fault and lockout states
* Manual fault reset

If the door encounters an obstruction while closing, the motor stops and the door automatically reopens to allow the Chicken to clear the area before attempting to close again.

---

# 🌅 Automatic Door Operation

Door operation can be configured around **sunrise and sunset**, or fixed times can be used.

Separate settings are available for:

* **GMT / winter**
* **BST / summer**

The controller automatically detects UK daylight-saving time and selects the appropriate schedule.

The fixed opening times can be disabled to allow the door to operate relative to sunrise instead.

---

# ❄️ Cold Weather Protection

An external waterproof **SHT30 temperature/humidity sensor** monitors the outside temperature.

If the outside temperature is below the configured threshold when the morning door opening is due, the system can delay opening.

### Current settings

**Cold threshold:** -3°C
**Delay:** 35 minutes

If the temperature rises above the threshold during the delay, the door can open immediately.

A manual override is also available to allow the door to open regardless of the outside temperature.

A dedicated blue LED indicates when the outside temperature is below the configured cold threshold.

---

# 🌡️ Environmental Monitoring

Two Sensirion SHT3x-family sensors are used.

| Sensor | Location         | I²C Address |
| ------ | ---------------- | ----------: |
| SHT31  | Inside hen house |      `0x45` |
| SHT30  | Outside          |      `0x44` |

The sensors monitor:

* Temperature
* Relative humidity
* Daily minimum temperature
* Daily maximum temperature
* Daily minimum humidity
* Daily maximum humidity
* Time of recorded extremes

Approximately **90 days of environmental history** is retained for both inside and outside measurements.

Data is stored using the ESP32 `Preferences` system so that it survives power loss.

---

# 💡 Lighting & Night System

The controller operates the hen-house light automatically around sunset.

The evening warning system uses two LEDs to provide a visual indication as closing time approaches.

The warning sequence progressively increases the flashing rate as the door-closing time gets closer.

Once the door is confirmed fully closed, the LEDs enter **night mode** and alternate throughout the night.

The coop light can also be operated manually using the rotary encoder.

---

# 🖥️ User Interface

A **128×64 SH1106 OLED** provides information and control without requiring a computer or web interface.

The rotary encoder is used for navigation and manual control.

### Display pages

1. **Main Status**
2. **Today's Schedule**
3. **Inside Environment**
4. **Outside Environment**
5. **Manual Controls**
6. **Event History**
7. **Wi-Fi Status**
8. **System Time**

The display normally refreshes only when required to reduce unnecessary I²C activity. The system-time page updates once per second.

The OLED also enters power-save mode after a period of inactivity.

---

# 📝 Event Logging

The controller maintains a circular **200-entry event history**.

Events include:

* Automatic door opening
* Automatic door closing
* Manual door operation
* Light operation
* Cold-weather delays
* Obstruction detection
* Door timeouts
* Homing operations
* Faults
* Wi-Fi events
* Sensor faults
* System resets

This provides a useful history when diagnosing the system or checking its operation.

---

# 🕰️ Timekeeping

The system uses two time sources:

### Primary

**NTP via Wi-Fi**

### Backup

**DS3231 RTC**

When Wi-Fi is available, the ESP32 synchronises its clock using NTP and updates the DS3231.

If Wi-Fi is unavailable, the RTC provides a backup time source so that automatic door operation can continue.

UK GMT/BST daylight-saving rules are handled automatically.

---

# 📶 Wi-Fi

Wi-Fi is primarily used for:

* NTP time synchronisation
* Connection monitoring
* Automatic reconnection
* Signal-strength monitoring
* IP-address display

The controller is designed so that normal hen-house automation does not depend on a permanent Wi-Fi connection.

---

# 🔌 Hardware

### Controller

**ESP32-S3 WROOM-1 / N16R8**

### Display

**1.3" 128×64 SH1106 OLED**

### Sensors

* Sensirion SHT31 – inside
* Sensirion SHT30 – outside (waterproof)

### RTC

**DS3231**

### Door

* Guillotine-style vertical door
* Geared DC motor
* Motor driver
* OPEN limit switch
* CLOSED limit switch

### User control

* KY-040 rotary encoder

---

# 📌 ESP32-S3 Pinout

| Function             |   GPIO |
| -------------------- | -----: |
| I²C SDA              |  **8** |
| I²C SCL              |  **9** |
| Rotary Encoder CLK   |  **4** |
| Rotary Encoder DT    |  **5** |
| Encoder Button       | **18** |
| Door Motor OPEN      | **41** |
| Door Motor CLOSE     | **42** |
| Door OPEN Limit      | **15** |
| Door CLOSED Limit    | **16** |
| Coop Light           | **17** |
| Green Status LED     |  **6** |
| Red Fault LED        |  **7** |
| Night LED 1          | **11** |
| Night LED 2          | **12** |
| Cold Temperature LED | **13** |

All I²C devices share the same bus at **100 kHz**.

---

# 🛡️ Safety Philosophy

Because the system controls a physical door containing live animals, safety is a major part of the design.

The controller does not rely on a single timing mechanism to determine door position.

Instead it combines:

**Limit switches + motor timeout + state machine + obstruction detection + recovery + fault lockout**

The system also performs a door-position check during startup and will attempt to home the door if its position cannot be established.

---

# 💾 Firmware

**Current Version: V2.02**

The project has evolved through several revisions, including the transition from a DHT22 sensor to the SHT3x family and the addition of the external temperature sensor and cold-weather door protection.

### V2.02 highlights

* SHT31 inside sensor
* SHT30 outside sensor
* Dual I²C sensor support
* Outside environmental monitoring
* Cold-weather opening delay
* Temperature-based delay cancellation
* Persistent environmental history
* Improved event logging
* Environmental log maintenance functions

---

# 🔧 Development Notes

The OLED, DS3231 and both environmental sensors share the same I²C bus.

Reliable physical wiring is important for I²C operation. In testing, intermittent display problems were ultimately traced to unreliable **fly-lead connections between the ESP32-S3 and OLED**. Replacing the leads and soldering the connections resolved the problem.

This is worth considering when reproducing the hardware, particularly where longer or flexible wiring is used.

---

# ⚠️ Disclaimer

This is a personal DIY automation project.

A motorised animal door should be thoroughly tested under real operating conditions before being left unattended. Particular attention should be given to the mechanical door design, limit switches, obstruction detection, motor drive system and emergency/fault conditions.

---

## 📷 Project

**Hen House Automation — ESP32-S3**

*Automatic door • Environmental monitoring • Lighting • Safety • Data logging*
