// Higi-Hen-House V1.1. WiFi enabled Auto Door and coop light controller.
// ESP32 S3 board.
// OLED 1.3" display and a DS3132 RTC modual.
// Do add a "reset" button for the ESP32 board on the final build
// WiFi credentials and lat & long coordinance stored in seperate tab named "Secrets.h" Add your priate data there   

//======================================================
// LIBRARYS
//======================================================

#include <WiFi.h>
#include <time.h>
#include <math.h>
#include <Wire.h>
#include <RTClib.h>
#include <U8g2lib.h>
#include "secrets.h"

// ======================================================
// TIME VALUES
// ======================================================

const unsigned long SECOND = 1000UL;
const unsigned long MINUTE = 60000UL;
const unsigned long HOUR = 3600000UL;

// ======================================================
// ********USER SETTINGS*********
// ======================================================

// Door opens AFTER sunrise
int sunriseOpenOffset = 10;
// Door closes AFTER sunset
int sunsetCloseOffset = 20;

// Coop light comes ON before sunset
int lightOnOffset = 5;
// Coop light goes OFF after sunset. Example,int sunsetCloseOffset=20 + 5mins =25mins
int lightOffMinutes = 25;

// Manual light auto OFF timer.
unsigned long manualLightTimeout = 20 * MINUTE;

// Display goes to sleep after no activity
unsigned long oledTimeout = 5 * MINUTE;

// Display number of events stored.
#define MAX_EVENTS 10        


// ======================================================
// GPIO
// ======================================================

#define MOTOR_OPEN_PIN 41   // Auto Motor OPEN. Via a L298N controller
#define MOTOR_CLOSE_PIN 42  // Auto Motor CLOSE. Via a L298N controller

#define LIMIT_OPEN_PIN 15   // Door Limit Switch OPEN, Normall open contacts
#define LIMIT_CLOSE_PIN 16  // Door Limit Switch CLOSE, Normally open contacts

#define LIGHT_PIN 17         // Auto Coop Light. Via a 3.3v relay board
#define SWITCH_LIGHT_PIN 13  // Manual Coop Light, momentary switch

#define SWITCH_OPEN_PIN 4   // Manual Door OPEN, momentary switch
#define SWITCH_CLOSE_PIN 5  // Manual Door CLOSE, momentary switch

#define EVENT_SCROLL_PIN 18  // Display WAKE (short press), Event scroll (long press)

// INFO
// Pin 8= SDA  
// Pin 9= SCK/SCL  These are for I2C OLED 1.3" display and RTC DS3231, use same pins for both.

// ======================================================
// TIMEOUTS
// ======================================================

#define MOTOR_TIMEOUT (20 * SECOND)        // adjust for how long the motor runs
#define SAFETY_TIMEOUT (45 * SECOND)       // adjust for overall fail safe
#define RETRY_DELAY (5 * MINUTE)           //
#define WIFI_RETRY_INTERVAL (30 * SECOND)  // if WiFi fails retry every __ until connected

// ======================================================
// FUNCTION DECLARATIONS
// ======================================================

bool getTime(struct tm& timeinfo);
void syncRtcFromNtp();

// ======================================================
// OLED
// ======================================================

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

RTC_DS3231 rtc;

bool rtcAvailable = false;

// ======================================================
// MATH
// ======================================================

#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105

// ======================================================
// SUN
// ======================================================

int actualSunrise = 480;
int actualSunset = 1200;

int openTime = 490;
int closeTime = 1220;

int lastDay = -1;

// ======================================================
// DISPLAY DATA
// ======================================================

struct EventLog {

  char msg[64];
  char time[16];
};

EventLog eventHistory[MAX_EVENTS];

int newestEventIndex = -1;
int viewedEventOffset = 0;

char latestEvent[64] = "System Start";
char latestEventTime[16] = "--:--";

char statusLine[32] = "";

unsigned long lastOledActivity = 0;

bool oledSleeping = false;
bool displayDirty = true;

bool viewingHistory = false;

unsigned long historyViewStart = 0;

const unsigned long historyTimeout = 10 * SECOND;

// ======================================================
// LIGHT
// ======================================================

bool lightOn = false;

bool lightCycleComplete = false;

bool manualLightOverride = false;

unsigned long manualLightOffTime = 0;

bool lastLightButtonState = HIGH;

// ======================================================
// WIFI
// ======================================================

unsigned long lastWiFiAttempt = 0;

int wifiRetryCount = 0;

bool wifiConnected = false;

// ======================================================
// TIMERS
// ======================================================

unsigned long motorStartTime = 0;
unsigned long lastCheck = 0;

unsigned long safetyStartTime = 0;
unsigned long safetyOpenReachedTime = 0;

// ======================================================
// DEBOUNCE
// ======================================================

const unsigned long debounceMs = 50;

bool lastOpenReading = HIGH;
bool lastCloseReading = HIGH;

bool openStableState = HIGH;
bool closeStableState = HIGH;

unsigned long openDebounceTime = 0;
unsigned long closeDebounceTime = 0;

bool lastScrollButtonState = HIGH;

unsigned long lastScrollPress = 0;

// ======================================================
// ENUMS
// ======================================================

enum DoorState {

  DOOR_STOPPED,
  DOOR_OPENING,
  DOOR_CLOSING,
  DOOR_OPEN,
  DOOR_CLOSED,
  DOOR_UNKNOWN
};

enum SafetyState {

  SAFETY_IDLE,
  SAFETY_OPENING,
  SAFETY_WAITING,
  SAFETY_RECLOSING,
  SAFETY_COMPLETE
};

enum FaultState {

  FAULT_NONE,
  FAULT_ACTIVE
};

// ======================================================
// STATES
// ======================================================

DoorState doorState = DOOR_STOPPED;

SafetyState safetyState = SAFETY_IDLE;

FaultState faultState = FAULT_NONE;

// ======================================================
// FAULT*
// ======================================================

char faultReason[64] = "";

unsigned long lastFaultRetry = 0;

// ======================================================
// LIMIT SWITCH FAILURE / RETRY PROTECTION
// ======================================================

const int MAX_LIMIT_RETRIES = 3;

int openRetryCount = 0;
int closeRetryCount = 0;

bool systemLockout = false;



// ======================================================
// OVERRIDES
// ======================================================

bool manualOverride = false;

bool manualMoveActive = false;

bool dailyResetDone = false;

unsigned long manualOverrideUntil = 0;

// ======================================================
// UTILITIES
// ======================================================

bool getTime(struct tm& timeinfo) {

  if (rtcAvailable) {

    DateTime now = rtc.now();

    timeinfo.tm_year = now.year() - 1900;
    timeinfo.tm_mon = now.month() - 1;
    timeinfo.tm_mday = now.day();

    timeinfo.tm_hour = now.hour();
    timeinfo.tm_min = now.minute();
    timeinfo.tm_sec = now.second();

    timeinfo.tm_wday = now.dayOfTheWeek();

    return true;
  }

  return getLocalTime(&timeinfo);
}

void updateEventTime() {

  struct tm timeinfo;

  if (getTime(timeinfo)) {

    snprintf(
      latestEventTime,
      sizeof(latestEventTime),
      "%02d:%02d",
      timeinfo.tm_hour,
      timeinfo.tm_min);
  }

  else {

    snprintf(
      latestEventTime,
      sizeof(latestEventTime),
      "--:--");
  }
}

void setEvent(const char* msg) {

  // WAKE OLED IF ASLEEP
  if (oledSleeping) {

    u8g2.setPowerSave(0);

    oledSleeping = false;
  }

  strncpy(
    latestEvent,
    msg,
    sizeof(latestEvent) - 1);

  latestEvent[sizeof(latestEvent) - 1] = '\0';

  updateEventTime();

  newestEventIndex =
    (newestEventIndex + 1) % MAX_EVENTS;

  strncpy(
    eventHistory[newestEventIndex].msg,
    latestEvent,
    sizeof(eventHistory[newestEventIndex].msg) - 1);

  eventHistory[newestEventIndex]
    .msg[sizeof(eventHistory[newestEventIndex].msg) - 1] = '\0';

  strncpy(
    eventHistory[newestEventIndex].time,
    latestEventTime,
    sizeof(eventHistory[newestEventIndex].time) - 1);

  eventHistory[newestEventIndex]
    .time[sizeof(eventHistory[newestEventIndex].time) - 1] = '\0';

  viewedEventOffset = 0;

  lastOledActivity = millis();

  displayDirty = true;

  refreshDisplayIfNeeded();

  Serial.println(msg);
}

// ======================================================
// LIMIT SWITCHES
// ======================================================

bool openLimitHit() {

  return digitalRead(LIMIT_OPEN_PIN) == LOW;  // Normally open contact
}

bool closeLimitHit() {

  return digitalRead(LIMIT_CLOSE_PIN) == LOW;  // Normally open contact
}

// ======================================================
// DISPLAY
// ======================================================

void buildStatusLine() {

  switch (doorState) {

    case DOOR_OPEN:
      snprintf(statusLine, sizeof(statusLine), "Door OPEN");
      break;

    case DOOR_CLOSED:
      snprintf(statusLine, sizeof(statusLine), "Door CLOSED");
      break;

    case DOOR_OPENING:
      snprintf(statusLine, sizeof(statusLine), "Door OPENING");
      break;

    case DOOR_CLOSING:
      snprintf(statusLine, sizeof(statusLine), "Door CLOSING");
      break;

    default:
      snprintf(statusLine, sizeof(statusLine), "Door STOPPED");
      break;
  }
}

void drawMainDisplay() {

  if (oledSleeping) return;

  buildStatusLine();

  char sunBuf[32];

  snprintf(
    sunBuf,
    sizeof(sunBuf),
    "SR %02d:%02d SS %02d:%02d",
    actualSunrise / 60,
    actualSunrise % 60,
    actualSunset / 60,
    actualSunset % 60);

  u8g2.clearBuffer();

  char histBuf[8];

  snprintf(
    histBuf,
    sizeof(histBuf),
    "%d/%d",
    viewedEventOffset + 1,
    MAX_EVENTS);

  // Smaller font for history counter
  u8g2.setFont(u8g2_font_5x7_tf);

  u8g2.drawStr(108, 8, histBuf);

  // Restore normal font
  u8g2.setFont(u8g2_font_6x13_tf);

  u8g2.setFont(u8g2_font_6x13_tf);

  int timeWidth =
    u8g2.getStrWidth(latestEventTime);

  u8g2.drawStr(
    (128 - timeWidth) / 2,
    11,
    latestEventTime);

  u8g2.setFont(u8g2_font_helvB10_tf);

  int eventWidth =
    u8g2.getStrWidth(latestEvent);

  u8g2.drawStr(
    (128 - eventWidth) / 2,
    28,
    latestEvent);

  u8g2.setFont(u8g2_font_6x13_tf);

  int statusWidth =
    u8g2.getStrWidth(statusLine);

  u8g2.drawStr(
    (128 - statusWidth) / 2,
    46,
    statusLine);

  int sunWidth =
    u8g2.getStrWidth(sunBuf);

  u8g2.drawStr(
    (128 - sunWidth) / 2,
    62,
    sunBuf);

  u8g2.sendBuffer();

  displayDirty = false;
}

void refreshDisplayIfNeeded() {

  static unsigned long lastRefresh = 0;

  if (oledSleeping) return;

  if (!displayDirty && millis() - lastRefresh < 60000UL) {

    return;
  }

  lastRefresh = millis();

  drawMainDisplay();
}

void oledPrint(const char* text) {

  setEvent(text);

  refreshDisplayIfNeeded();
}

// ======================================================
// MOTOR SAFETY
// ======================================================

void safeRelayTransition() {

  digitalWrite(MOTOR_OPEN_PIN, LOW);
  digitalWrite(MOTOR_CLOSE_PIN, LOW);

  delay(50);
}

void stopMotor() {

  digitalWrite(MOTOR_OPEN_PIN, LOW);
  digitalWrite(MOTOR_CLOSE_PIN, LOW);

  if (doorState == DOOR_OPENING || doorState == DOOR_CLOSING) {

    doorState = DOOR_STOPPED;
  }

  manualMoveActive = false;

  displayDirty = true;
}

// ======================================================
// FAULTS
// ======================================================

void clearFaultState() {

  stopMotor();

  faultState = FAULT_NONE;

  memset(faultReason, 0, sizeof(faultReason));

  safetyState = SAFETY_IDLE;

  motorStartTime = 0;

  safetyStartTime = 0;

  oledPrint("Fault Cleared");
}

// ======================================================
// FAULT HANDLING
// =====================================================


void triggerLockout(const char* reason);

void triggerFault(const char* reason) {

  stopMotor();

  faultState = FAULT_ACTIVE;

  strncpy(faultReason, reason, sizeof(faultReason) - 1);
  faultReason[sizeof(faultReason) - 1] = '\0';

  // OPEN failure tracking
  if (strstr(reason, "OPEN") != nullptr) {

    openRetryCount++;

    if (openRetryCount >= MAX_LIMIT_RETRIES) {
      triggerLockout("OPEN LIMIT FAIL");
      return;
    }
  }

  // CLOSE failure tracking
  if (strstr(reason, "CLOSE") != nullptr) {

    closeRetryCount++;

    if (closeRetryCount >= MAX_LIMIT_RETRIES) {
      triggerLockout("CLOSE LIMIT FAIL");
      return;
    }
  }

  oledPrint("FAULT");
  oledPrint(reason);
}

void triggerLockout(const char* reason) {

  stopMotor();

  systemLockout = true;
  faultState = FAULT_ACTIVE;

  strncpy(faultReason, reason, sizeof(faultReason) - 1);
  faultReason[sizeof(faultReason) - 1] = '\0';

  oledPrint("SYSTEM LOCKOUT");
  oledPrint(reason);

  Serial.println("!!! LOCKOUT TRIGGERED !!!");
}

// ======================================================
// MANUAL OVERRIDE
// ======================================================

void activateManualOverride() {

  manualOverride = true;

  manualOverrideUntil = millis() + (1 * HOUR);
}

bool automationAllowed() {

  if (!manualOverride) {
    return true;
  }

  if (millis() > manualOverrideUntil) {

    manualOverride = false;

    return true;
  }

  return false;
}

// ======================================================
// OPEN
// ======================================================

void startOpening() {

  if (systemLockout) {
    oledPrint("LOCKED OUT");
    return;
  }

  if (faultState == FAULT_ACTIVE) return;

  if (openLimitHit()) {

    stopMotor();

    doorState = DOOR_OPEN;

    openRetryCount = 0;

    oledPrint("Already OPEN");

    return;
  }

  safeRelayTransition();

  digitalWrite(MOTOR_OPEN_PIN, HIGH);

  motorStartTime = millis();

  doorState = DOOR_OPENING;

  oledPrint("Start OPENING");
}

// ======================================================
// CLOSE
// ======================================================

void startClosing() {

  if (systemLockout) {
    oledPrint("LOCKED OUT");
    return;
  }

  if (faultState == FAULT_ACTIVE) return;

  if (closeLimitHit()) {

    stopMotor();

    doorState = DOOR_CLOSED;

    oledPrint("Already CLOSED");

    return;
  }

  safeRelayTransition();

  digitalWrite(MOTOR_CLOSE_PIN, HIGH);

  motorStartTime = millis();

  doorState = DOOR_CLOSING;

  oledPrint("Start CLOSING");
}

// ======================================================
// WIFI
// ======================================================

void connectWiFi() {

  bool currentState = (WiFi.status() == WL_CONNECTED);

  // ==========================================
  // WIFI JUST CONNECTED
  // ==========================================
  if (currentState && !wifiConnected) {

  wifiConnected = true;

  wifiRetryCount = 0;

  oledPrint("WiFi Connected");

  Serial.println("WiFi Connected");

  Serial.println(WiFi.localIP());

  syncRtcFromNtp();
}
  // ==========================================
  // WIFI JUST DISCONNECTED
  // ==========================================
  if (!currentState && wifiConnected) {

    wifiConnected = false;

    oledPrint("WiFi Lost");

    Serial.println("WiFi Lost");
  }

  // Already connected
  if (currentState) {
    return;
  }

  // Retry timer
  if (millis() - lastWiFiAttempt < WIFI_RETRY_INTERVAL) {
    return;
  }

  lastWiFiAttempt = millis();

  wifiRetryCount++;

  oledPrint("WiFi reconnect");

  if (wifiRetryCount > 5) {

    WiFi.disconnect(true);

    delay(500);
  }

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD);
}
// ======================================================
// TIME
// ======================================================

void setupTime() {

  configTzTime(
    "GMT0BST,M3.5.0/1,M10.5.0/2",
    "pool.ntp.org");
}

void waitForValidTime() {

  struct tm timeinfo;

  unsigned long start = millis();

  while (!getTime(timeinfo)) {

    delay(500);

    if (millis() - start > 10000UL) {

      oledPrint("NTP Timeout");

      return;
    }
  }

  oledPrint("Time synced");

  if (!rtcAvailable) return;

  if (getLocalTime(&timeinfo)) {

    rtc.adjust(DateTime(
      timeinfo.tm_year + 1900,
      timeinfo.tm_mon + 1,
      timeinfo.tm_mday,
      timeinfo.tm_hour,
      timeinfo.tm_min,
      timeinfo.tm_sec));

    Serial.println("RTC synced from NTP");
  }
}

void syncRtcFromNtp() {

  if (!rtcAvailable) return;

  struct tm timeinfo;

  if (getLocalTime(&timeinfo)) {

    rtc.adjust(DateTime(
      timeinfo.tm_year + 1900,
      timeinfo.tm_mon + 1,
      timeinfo.tm_mday,
      timeinfo.tm_hour,
      timeinfo.tm_min,
      timeinfo.tm_sec));

    Serial.println("RTC synced from NTP");
  }
}

// ======================================================
// SUN CALCULATIONS
// ======================================================

int calculateSunTime(
  int year,
  int month,
  int day,
  double latitude,
  double longitude,
  bool sunriseCalc) {

  const double zenith = 90.833;

  int N1 = floor(275 * month / 9);
  int N2 = floor((month + 9) / 12);
  int N3 = (1 + floor((year - 4 * floor(year / 4) + 2) / 3));

  int N = N1 - (N2 * N3) + day - 30;

  double lngHour = longitude / 15.0;

  double t = sunriseCalc
               ? N + ((6 - lngHour) / 24)
               : N + ((18 - lngHour) / 24);

  double M = (0.9856 * t) - 3.289;

  double L =
    M + (1.916 * sin(DEG_TO_RAD * M)) + (0.020 * sin(2 * DEG_TO_RAD * M)) + 282.634;

  while (L < 0) L += 360;
  while (L >= 360) L -= 360;

  double RA = RAD_TO_DEG * atan(0.91764 * tan(DEG_TO_RAD * L));

  while (RA < 0) RA += 360;
  while (RA >= 360) RA -= 360;

  double Lquadrant = floor(L / 90) * 90;
  double RAquadrant = floor(RA / 90) * 90;

  RA += (Lquadrant - RAquadrant);

  RA /= 15;

  double sinDec = 0.39782 * sin(DEG_TO_RAD * L);
  double cosDec = cos(asin(sinDec));

  double cosH =
    (cos(DEG_TO_RAD * zenith) - (sinDec * sin(DEG_TO_RAD * latitude))) / (cosDec * cos(DEG_TO_RAD * latitude));

  if (cosH > 1 || cosH < -1) {
    return -1;
  }

  double H = sunriseCalc
               ? 360 - RAD_TO_DEG * acos(cosH)
               : RAD_TO_DEG * acos(cosH);

  H /= 15;

  double T = H + RA - (0.06571 * t) - 6.622;

  double UT = T - lngHour;

  while (UT < 0) UT += 24;
  while (UT >= 24) UT -= 24;

  time_t now;
  time(&now);

  struct tm localTimeInfo;

  localtime_r(&now, &localTimeInfo);

  if (localTimeInfo.tm_isdst > 0) {
    UT += 1.0;
  }

  int hour = (int)UT;
  int minute = (UT - hour) * 60;

  return (hour * 60) + minute;
}

// ======================================================
// UPDATE SUN
// ======================================================

void updateSunTimes(struct tm timeinfo) {

  if (timeinfo.tm_mday == lastDay) {
    return;
  }

  lastDay = timeinfo.tm_mday;

  int year = timeinfo.tm_year + 1900;
  int month = timeinfo.tm_mon + 1;
  int day = timeinfo.tm_mday;

  actualSunrise = calculateSunTime(
    year,
    month,
    day,
    latitude,
    longitude,
    true);

  actualSunset = calculateSunTime(
    year,
    month,
    day,
    latitude,
    longitude,
    false);

  openTime = actualSunrise + sunriseOpenOffset;
  closeTime = actualSunset + sunsetCloseOffset;

  char buf[64];

  snprintf(
    buf,
    sizeof(buf),
    "SR %02d:%02d SS %02d:%02d",
    actualSunrise / 60,
    actualSunrise % 60,
    actualSunset / 60,
    actualSunset % 60);

  // Do NOT log as an event (prevents showing on line 2)
  displayDirty = true;
}

// ======================================================
// LIGHT CONTROL
// ======================================================

void updateLight(int nowMinutes) {

  if (manualLightOverride) return;

  int onTime = actualSunset - lightOnOffset;
  int offTime = actualSunset + lightOffMinutes;

  // TURN ON
  if (!lightOn && !lightCycleComplete && nowMinutes >= onTime && nowMinutes < offTime) {

    digitalWrite(LIGHT_PIN, HIGH);

    lightOn = true;

    oledPrint("Light ON");
  }

  // TURN OFF
  if (lightOn && nowMinutes >= offTime) {

    digitalWrite(LIGHT_PIN, LOW);

    lightOn = false;

    lightCycleComplete = true;

    oledPrint("Light OFF");
  }

  // RESET NEXT MORNING
  if (nowMinutes < actualSunrise) {

    lightCycleComplete = false;
  }
}
// ======================================================
// SWITCHES
// ======================================================

void handleSwitch() {

  bool openRaw = digitalRead(SWITCH_OPEN_PIN);
  bool closeRaw = digitalRead(SWITCH_CLOSE_PIN);
  bool lightButton = digitalRead(SWITCH_LIGHT_PIN);

  if (openRaw != lastOpenReading) {
    openDebounceTime = millis();
  }

  if ((millis() - openDebounceTime) > debounceMs) {
    openStableState = openRaw;
  }

  lastOpenReading = openRaw;

  if (closeRaw != lastCloseReading) {
    closeDebounceTime = millis();
  }

  if ((millis() - closeDebounceTime) > debounceMs) {
    closeStableState = closeRaw;
  }

  lastCloseReading = closeRaw;

  bool openPressed = (openStableState == LOW);
  bool closePressed = (closeStableState == LOW);

  if (openPressed && closePressed) {

    clearFaultState();

    return;
  }

  if (faultState == FAULT_ACTIVE) return;

  if (openPressed && !manualMoveActive) {

    activateManualOverride();

    manualMoveActive = true;

    oledPrint("Manual OPEN");

    startOpening();
  }

  if (closePressed && !manualMoveActive) {

    activateManualOverride();

    manualMoveActive = true;

    oledPrint("Manual CLOSE");

    startClosing();
  }

  static unsigned long lastLightPress = 0;

  if (lastLightButtonState == HIGH && lightButton == LOW) {

    if (millis() - lastLightPress > 300) {

      lastLightPress = millis();

      manualLightOverride = !manualLightOverride;

      if (manualLightOverride) {

        lightOn = true;

        digitalWrite(LIGHT_PIN, HIGH);

        manualLightOffTime =
          millis() + manualLightTimeout;

        oledPrint("Manual Light ON");
      }

      else {

        lightOn = false;

        digitalWrite(LIGHT_PIN, LOW);

        manualLightOffTime = millis();

        oledPrint("Manual Light OFF");
      }
    }
  }

  lastLightButtonState = lightButton;
}

// ======================================================
// UPDATE DOOR
// ======================================================

void updateDoor() {

  bool openHit = openLimitHit();
  bool closeHit = closeLimitHit();


  switch (doorState) {

    case DOOR_OPENING:

      if (openHit) {

        openRetryCount = 0;

        stopMotor();

        doorState = DOOR_OPEN;

        oledPrint("Fully Open");
      }

      else if (millis() - motorStartTime > MOTOR_TIMEOUT) {

        triggerFault("OPEN timeout");
      }

      break;

    case DOOR_CLOSING:

      if (closeHit) {

        closeRetryCount = 0;

        stopMotor();

        doorState = DOOR_CLOSED;

        oledPrint("Fully Closed");

        if (!manualOverride && safetyState == SAFETY_IDLE) {

          safetyState = SAFETY_OPENING;

          safetyStartTime = millis();

          startOpening();
        }
      }

      else if (millis() - motorStartTime > MOTOR_TIMEOUT) {

        triggerFault("CLOSE timeout");
      }

      break;

    default:
      break;
  }
}

// ======================================================
// SAFETY STATE MACHINE
// ======================================================

void updateSafetyCycle() {

  switch (safetyState) {

    case SAFETY_IDLE:
      break;

    case SAFETY_OPENING:

      if (doorState == DOOR_OPEN) {

        safetyOpenReachedTime = millis();

        safetyState = SAFETY_WAITING;

        oledPrint("Safety wait");
      }

      break;

    case SAFETY_WAITING:

      if (millis() - safetyOpenReachedTime > 2000UL) {

        startClosing();

        safetyState = SAFETY_RECLOSING;
      }

      break;

    case SAFETY_RECLOSING:

      if (doorState == DOOR_CLOSED) {

        safetyState = SAFETY_COMPLETE;

        oledPrint("Safety done");
      }

      break;

    case SAFETY_COMPLETE:

      safetyState = SAFETY_IDLE;

      break;
  }

  if (safetyState != SAFETY_IDLE) {

    if (millis() - safetyStartTime > SAFETY_TIMEOUT) {

      safetyState = SAFETY_IDLE;

      triggerFault("Safety timeout");
    }
  }
}

// ======================================================
// STARTUP HOMING
// ======================================================

void performStartupRecovery() {


  if (!openLimitHit() && !closeLimitHit()) {

    doorState = DOOR_UNKNOWN;

    oledPrint("Door UNKNOWN");

    oledPrint("Homing CLOSE");

    safeRelayTransition();

    digitalWrite(MOTOR_CLOSE_PIN, HIGH);

    unsigned long start = millis();

    while (millis() - start < MOTOR_TIMEOUT) {

      if (closeLimitHit()) {

        digitalWrite(MOTOR_CLOSE_PIN, LOW);

        doorState = DOOR_CLOSED;

        oledPrint("Home OK");

        return;
      }

      delay(5);
    }

    digitalWrite(MOTOR_CLOSE_PIN, LOW);

    triggerFault("HOME FAIL");

    return;
  }

  if (openLimitHit()) {

    doorState = DOOR_OPEN;

    oledPrint("Door OPEN");
  }

  if (closeLimitHit()) {

    doorState = DOOR_CLOSED;

    oledPrint("Door CLOSED");
  }
}

// ======================================================
// WELCOME SCREEN
// ======================================================

void showWelcomeScreen() {

  u8g2.setPowerSave(0);

  oledSleeping = false;

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_helvB12_tr);

  u8g2.drawStr(45, 18, "HIGI");
  u8g2.drawStr(18, 36, "Hen House");

  u8g2.setFont(u8g2_font_6x13_tf);

  u8g2.drawStr(18, 52, "Auto Controller V1.1");

  u8g2.sendBuffer();

  unsigned long start = millis();

  while (millis() - start < 2000) {
    delay(1);
  }
}

// ======================================================
// SETUP
// ======================================================

void setup() {

  Serial.begin(115200);

  Wire.begin();

  if (rtc.begin()) {
    rtcAvailable = true;

    Serial.println("RTC OK");
  } else {
    rtcAvailable = false;
    Serial.println("RTC NOT FOUND");
  }

  u8g2.begin();

  showWelcomeScreen();

  pinMode(MOTOR_OPEN_PIN, OUTPUT);
  pinMode(MOTOR_CLOSE_PIN, OUTPUT);

  pinMode(LIMIT_OPEN_PIN, INPUT_PULLUP);
  pinMode(LIMIT_CLOSE_PIN, INPUT_PULLUP);

  pinMode(SWITCH_OPEN_PIN, INPUT_PULLUP);
  pinMode(SWITCH_CLOSE_PIN, INPUT_PULLUP);
  pinMode(SWITCH_LIGHT_PIN, INPUT_PULLUP);
  pinMode(EVENT_SCROLL_PIN, INPUT_PULLUP);


  pinMode(LIGHT_PIN, OUTPUT);

  stopMotor();

  WiFi.mode(WIFI_STA);

  WiFi.setSleep(false);

  oledPrint("Connecting WiFi");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  setupTime();

  waitForValidTime();

  performStartupRecovery();

  oledPrint("System Ready");
}

// ======================================================
// EVENT SCROLL HANDLER
// ======================================================
void handleEventScroll() {

  static bool lastState = HIGH;
  static unsigned long pressStart = 0;

  bool state = digitalRead(EVENT_SCROLL_PIN);

  // Button pressed
  if (lastState == HIGH && state == LOW) {

    pressStart = millis();
  }

  // Button released
  if (lastState == LOW && state == HIGH) {

    unsigned long pressTime = millis() - pressStart;

    // SHORT PRESS → reset to main (latest event)
    if (pressTime < 500) {

      viewedEventOffset = 0;

      viewingHistory = false;

      if (newestEventIndex >= 0) {

        strncpy(
          latestEvent,
          eventHistory[newestEventIndex].msg,
          sizeof(latestEvent) - 1);

        strncpy(
          latestEventTime,
          eventHistory[newestEventIndex].time,
          sizeof(latestEventTime) - 1);
      }

      u8g2.setPowerSave(0);
      oledSleeping = false;

      lastOledActivity = millis();

      displayDirty = true;

      drawMainDisplay();
    }

    // LONG PRESS → scroll history
    else {

      viewedEventOffset++;

      if (viewedEventOffset >= MAX_EVENTS) {
        viewedEventOffset = 0;
      }

      int index =
        newestEventIndex - viewedEventOffset;

      if (index < 0) {
        index += MAX_EVENTS;
      }

      if (strlen(eventHistory[index].msg) > 0) {

        viewingHistory = true;

        historyViewStart = millis();

        strncpy(
          latestEvent,
          eventHistory[index].msg,
          sizeof(latestEvent) - 1);

        strncpy(
          latestEventTime,
          eventHistory[index].time,
          sizeof(latestEventTime) - 1);

        lastOledActivity = millis();

        displayDirty = true;

        drawMainDisplay();
      }
    }
  }

  lastState = state;
}

// ======================================================
// LOOP
// ======================================================

void loop() {

  handleSwitch();

  handleEventScroll();

  updateDoor();

  updateSafetyCycle();

  connectWiFi();

  refreshDisplayIfNeeded();

  if (!oledSleeping && millis() - lastOledActivity > oledTimeout) {

    u8g2.clearBuffer();
    u8g2.sendBuffer();

    u8g2.setPowerSave(1);

    oledSleeping = true;
  }

  if (!manualLightOverride && manualLightOffTime > 0 && millis() - manualLightOffTime > 3000UL) {

    manualLightOffTime = 0;

    displayDirty = true;
  }


  if (millis() - lastCheck > 1 * MINUTE) {

    lastCheck = millis();

    struct tm timeinfo;

    if (!getTime(timeinfo)) {

      oledPrint("Time failed");

      return;
    }

    updateSunTimes(timeinfo);

    int nowMinutes =
      (timeinfo.tm_hour * 60) + timeinfo.tm_min;

    // ======================================================
    // MANUAL LIGHT AUTO OFF TIMER
    // ======================================================

    if (manualLightOverride && millis() > manualLightOffTime) {

      manualLightOverride = false;

      lightOn = false;

      digitalWrite(LIGHT_PIN, LOW);

      oledPrint("Light Timeout");
    }

    updateLight(nowMinutes);

    if (automationAllowed() && faultState == FAULT_NONE && doorState == DOOR_OPEN && nowMinutes >= closeTime) {

      oledPrint("AUTO CLOSE");

      startClosing();
    }

    if (automationAllowed() && faultState == FAULT_NONE && doorState == DOOR_CLOSED && nowMinutes >= openTime && nowMinutes < closeTime) {

      oledPrint("AUTO OPEN");

      startOpening();
    }

    if (nowMinutes < actualSunrise && !dailyResetDone) {

      manualLightOverride = false;

      dailyResetDone = true;

      oledPrint("Daily reset");
    }

    if (nowMinutes >= actualSunrise) {

      dailyResetDone = false;
    }
  }
}

