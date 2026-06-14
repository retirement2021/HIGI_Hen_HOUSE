
# HIGI Hen House V1.5 Firmware Summary

This is an advanced Arduino sketch for an **ESP32-S3 based automatic chicken coop controller**. Here's what it does:

## Core Functionality
- **Automated door control**: Opens at sunrise, closes at sunset with adjustable offsets
- **Lighting automation**: Turns coop lights on/off based on sunset with separate timing
- **Temperature/humidity monitoring**: DHT22 sensor with 90-day persistent logging
- **Manual override**: Rotary encoder for complete manual control
- **Safety features**: Obstruction detection, motor timeouts, limit switch conflict detection

## Key Features

**Display & UI**
- 1.3" OLED display with 6 pages (Main, Manual, Events, Environment, WiFi, System Time)
- Rotary encoder navigation (KY-040)
- Event logging (100 events stored)
- Scrollable event history

**Automation & Timing**
- Dual seasonal timing: separate parameters for BST (summer) and GMT (winter)
- Sunrise/sunset calculation based on GPS coordinates
- WiFi NTP time sync with RTC (DS3231) fallback
- Daily midnight reset

**Safety & Fault Handling**
- Motor timeout protection (default 15 seconds)
- Safety obstruction cycle: if door blocked, opens and retries
- Limit switch conflict detection
- System lockout after repeated failures
- Fault logging and visual/audio alerts (buzzer + LED)

**Hardware Control**
- Motor control via L298N/TB6612FNG driver
- Relay-controlled coop light
- Status LEDs (green for normal, red for fault)
- Night mode LEDs with different flash patterns (sunset warning vs. secured door)
- Active buzzer for alerts

**Connectivity**
- WiFi with auto-reconnect
- IP address and signal strength display
- Graceful fallback to RTC when WiFi unavailable

## Notable Implementation Details
- Adjustable seasonal offsets for door and light timing
- Debounced limit switch reading (50ms)
- Manual override tracks which action was taken to prevent conflicting automation
- Comprehensive event timestamps and history
- Watchdog timer (35-second timeout)
- Persistent storage using Preferences for environment logs


