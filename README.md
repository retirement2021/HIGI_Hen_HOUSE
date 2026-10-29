# HIGI Hen House --- ESP32 Automatic Coop Door

**Firmware:** V2.18\
**Author:** HIGI-Styx\
**Platform:** ESP32-S3 N16R8\
**Development environment:** Arduino IDE\
**Repository:** https://github.com/retirement2021/HIGI_Hen_HOUSE

An automated chicken-coop controller for scheduled guillotine door movement and
lighting, with temperature and humidity monitoring, a rotary-encoder
OLED interface, event history, and door safety/recovery logic.

> **Safety notice:** This project operates a physical door and motor. A
> software timeout or obstruction routine cannot guarantee that an
> animal will never be trapped or injured. Test the mechanism with the
> motor disconnected or unloaded first, verify both limit switches and
> the emergency/manual recovery procedure, and supervise initial
> operation. Use suitable motor-driver, power-supply, wiring, and
> enclosure ratings.

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

## Hardware

The sketch comments identify the following hardware:

-   ESP32-S3 N16R8 board, configured in Arduino IDE as **ESP32S3 Dev
    Module**.
-   1.3-inch SH1106 128×64 I²C OLED.
-   DS3231 RTC module.
-   SHT31 inside-coop temperature/humidity sensor, configured at I²C
    address `0x45`.
-   Optional SHT30 outdoor temperature/humidity probe, configured at I²C
    address `0x44`.
-   KY-040 rotary encoder.
-   Reversible DC motor and suitable driver, such as an L298N or
    TB6612FNG-compatible arrangement.
-   Two door limit switches.
-   Relay module for the coop light.
-   Active buzzer, status LED, fault LED, two red door-area LEDs, and a
    blue cold indicator LED.

### Pin assignments

  Function                      ESP32-S3 GPIO
  --------------------------- ----------------  
-   Motor OPEN                               41
-   Motor CLOSE                              42
-   Upper/open limit switch                  15
-   Lower/closed limit switch                16
-   Coop light relay                         17
-   Buzzer                                    3
-   Green status LED                          6
-   Red fault LED                             7
-   Night/door LED 1                         11
-   Night/door LED 2                         12
-   Blue cold indicator LED                  13
-   Rotary encoder CLK                        4
-   Rotary encoder DT                         5
-   Rotary encoder switch                    18
-   I²C SDA                                   8
-   I²C SCL                                   9

The limit switches are configured with `INPUT_PULLUP` and the sketch
expects the switch input to go LOW when activated. Check the wiring and
switch logic against the actual mechanism before powering the motor.

The sketch comments specify 3.3 V for the I²C devices and encoder.
Confirm the electrical requirements of every module and the motor driver
before connecting hardware. Do not power the motor directly from an
ESP32 GPIO.

## Software and libraries

Install the ESP32 board support package in Arduino IDE and select
**ESP32S3 Dev Module**.

The sketch includes these libraries:

-   `RTClib` (`RTClib.h`)
-   `U8g2` (`U8g2lib.h`)
-   `Sensirion I2C SHT3x` (`SensirionI2cSht3x.h`)
-   `AiEsp32RotaryEncoder` (`AiEsp32RotaryEncoder.h`)

The remaining headers (`WiFi.h`, `time.h`, `math.h`, `Wire.h`,
`Preferences.h`, and `esp_task_wdt.h`) are supplied by the ESP32/Arduino
toolchain.

Library API compatibility can depend on installed versions. If
compilation fails, check the library names and versions before changing
the sketch.

## Wi-Fi credentials

The sketch includes a separate file:

``` cpp
#include "secrets.h"
```

Add the `secrets.h` in the sketch folder with the credential definitions
expected by the sketch:

``` cpp
#pragma once

#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
```


The controller retries Wi-Fi at the configured interval and has a
maximum retry count. Time synchronisation relies on network access when
available; the RTC is used as a fallback source at startup if NTP has
not established a valid system time.

## Configuration

Most user-adjustable settings are grouped near the top of
`Hen_House_V2_18.ino` under **USER SETTINGS** and **USER MAGIC
NUMBERS**. Read the comments beside each setting before changing it.

### Door and light schedules

Winter/GMT and summer/BST have separate settings for:

-   Fixed morning opening time, or sunrise-based opening.
-   Minutes before/after sunrise for opening when sunrise mode is
    selected.
-   Minutes before/after sunset for door closing.
-   Coop-light on/off offsets relative to sunset.

The current sketch has fixed-time opening enabled for both seasons.
Change `useFixedOpenTimeGMT` and `useFixedOpenTime` to `false` to use
the corresponding sunrise-based opening option.

The time-zone rule is intended to apply UK GMT/BST automatically. The
V2.17 comments state that the next seasonal clock-change field
validation is planned for March 2027; verify actual door and light times
across the transition rather than assuming the change has been
field-validated.

### Cold-weather opening delay

-   `enableOutsideSHT` enables/disables the outdoor sensor feature.
-   `outsideTempTriggerC` sets the temperature threshold.
-   `doorOpenDelayIfColdMinutes` sets the delay applied when the outdoor
    temperature is below the threshold.
-   `forceOpenDespiteCold` can override the cold delay and is persisted
    in NVS.

### Motor and safety timings

Important settings include:

-   `MOTOR_TIMEOUT`: normal motor run-time limit.
-   `MOTOR_TIMEOUT_RECOVERY`: extended timing used during obstruction
    recovery.
These two values must be tuned to the installed mechanism. Measure actual
travel time, use a small safety margin, and keep the timeout as short as
the mechanism reliably permits. Do not increase a timeout simply to mask
a mechanical or limit-switch fault.
    
-   `SAFETY_WAIT_AFTER_REOPEN_MS`: how long the door remains open after
    an obstruction reversal.
-   `MAX_OBSTRUCTION_RETRIES`: number of obstruction retries.
-   `MAX_LIMIT_RETRIES`: limit-switch retry protection.
-   `SAFETY_TIMEOUT`: overall safety timeout.


### Sensors and environment history

-   Inside SHT31 address: `0x45`. LIN-Address link
-   Outside SHT30 address: `0x44`.
-   Inside sensor polling interval: configured by
    `SHT_READ_INTERVAL_MS`.
-   Outside sensor polling interval: configured by
    `OUTSIDE_SHT_READ_INTERVAL_MS`.
-   Inside and outside history retention are configured separately by
    `ENVIRONMENT_LOG_DAYS` and `ENVIRONMENT_LOG_DAYS_OUTSIDE`.
-   Consecutive sensor failure thresholds are configurable.

The sketch stores selected environment data and settings in ESP32
Preferences/NVS. NVS survives a normal reboot and sketch upload.

## OLED and rotary encoder

A short encoder-button press changes display page. Rotate the encoder on
the Events page to browse events and on the Environment page to move
between current readings, today's values, and saved history.

On the Manual page, a press-and-hold in the light-control range toggles
the coop light.
Holding the encoder button for **5 seconds** clears the
current fault/lockout state. Treat fault clearing as a recovery action,
not proof that the underlying problem has been fixed.

The display automatically sleeps after the configured inactivity
timeout. A button press wakes it.

## First setup and commissioning

1.  Inspect the pin assignments and wiring against your actual board and
    modules.
2.  Install the required Arduino libraries and select **ESP32S3 Dev
    Module**.
3.  Create the local `secrets.h` file with Wi-Fi credentials, lat and long coordinates ; do not
    publish it.
4.  Compile and upload the sketch.
5.  Confirm the OLED, RTC, Wi-Fi/time, and both temperature sensors (if
    installed) initialise as expected in the Serial Monitor.
6.  Confirm the RTC date/time is valid and the displayed local time is
    correct.
7.  With motor power disconnected, verify that both limit switches
    change state correctly and that the door state is reported as
    expected.
8.  Test the motor direction and limit-switch stopping behaviour with
    the mechanism safely supported and supervised.
9.  Test obstruction recovery and fault/lockout handling without putting
    an animal at risk. (block of wood)
10. Verify the scheduled door and light times, manual controls,
    and recovery after power loss.
11. Test cold-delay behaviour. 
12. Supervise operation in the coop and inspect the event log for
    unexpected resets, sensor faults, timeouts, or retries.

Do not rely on the controller for unattended animal safety until the
complete installed system has been tested.

## Troubleshooting

-   **Incorrect schedule:** Check local time, Wi-Fi/NTP status, RTC
    validity, GMT/BST settings, and the selected fixed-time versus
    sunrise option.
-   **Door runs the wrong way:** Check the motor-driver wiring and
    `MOTOR_OPEN_PIN` / `MOTOR_CLOSE_PIN` assignments before further
    powered testing.
-   **Door does not stop at an end:** Check the relevant limit switch,
    wiring, pull-up logic, and physical switch position. Do not rely on
    the software timeout as a substitute for working limit switches.
-   **Cold delay does not behave as expected:** Check the outdoor sensor
    address, `enableOutsideSHT`, displayed outdoor temperature,
    threshold, and event log.
-   **Sensor faults:** Check the shared I²C wiring, supply voltage,
    sensor address, and library installation.
-   **Time is missing after boot:** Check Wi-Fi/NTP access and the
    DS3231 battery, connection, and reported validity.
-   **Compilation errors:** Check the selected ESP32 board target and
    the installed library APIs; avoid making unrelated code changes
    while diagnosing a build error.

## Version notes

-   **V2.18:** Adds a page-change debounce/minimum press filter and
    changes fault clearing so the reset occurs while the encoder button
    is held for five seconds.
-   **V2.17:** Adds DST transition validation logic and a GMT/BST flag
    on the schedule page.
-   Earlier version history is recorded in the comments at the top of
    the sketch.

## Repository contents

The main firmware file is `Hen_House_V2_18.ino`. The repository link in
the sketch points to the wider project, which is intended to include
wiring diagrams, photographs, 3D-printing files, and a parts list.

If you publish this project, include a safe `secrets.example.h` template
and exclude real credentials from version control.
