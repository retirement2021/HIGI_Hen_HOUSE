#pragma once

#include <Arduino.h>

// =============================================================================
// Hardware configuration
// Change these to match your wiring. They are compile-time constants.
// =============================================================================

// Output that switches the camera's 12 V supply.
// IMPORTANT: use a high-side switch (relay or P-channel MOSFET) for the camera.
// Do NOT use a low-side N-channel MOSFET if the camera also has an Ethernet
// cable connected, because the camera ground would be switched.
#define PIN_CAM_SWITCH 4

// Output that drives the IR illuminator. A low-side MOSFET module (YX-MOS,
// AOD4184, etc.) is fine here as long as the lamp is a floating 2-wire load.
#define PIN_LAMP_SWITCH 5

// Push button to GND. GPIO0 is the BOOT button on most ESP32-S3 boards.
// Short press = toggle night view, long press = return to automatic.
#define PIN_BUTTON 0

// DS3231 I2C pins. These are the default ESP32-S3 I2C pins.
#define PIN_I2C_SDA 8
#define PIN_I2C_SCL 9

// Set to false if your relay board / MOSFET board is active-low.
#define CAM_ACTIVE_HIGH true
#define LAMP_ACTIVE_HIGH true

// =============================================================================
// Defaults (stored in NVS after the first save, so the web page wins)
// =============================================================================

// 0,0 means "not configured". The automatic schedule stays disabled until you
// enter a real latitude/longitude on the web page. Manual control always works.
#define DEFAULT_LATITUDE 0.0f
#define DEFAULT_LONGITUDE 0.0f

// Minutes relative to sunset.
#define DEFAULT_CAM_BEFORE 15   // camera on this many minutes before sunset
#define DEFAULT_CAM_AFTER 10    // camera off this many minutes after sunset
#define DEFAULT_LAMP_BEFORE 10  // IR lamp on this many minutes before sunset
#define DEFAULT_LAMP_AFTER 10   // IR lamp off this many minutes after sunset

// How long a manual "night view" lasts before returning to automatic.
#define DEFAULT_MANUAL_MINUTES 30

#define DEFAULT_HOSTNAME "coopcam"

// Fallback access point if no WiFi is configured or the connection fails.
#define AP_SSID "CoopCam-Setup"
#define AP_PASSWORD "coopcam123"
