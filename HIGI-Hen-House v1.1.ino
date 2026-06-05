//======================================================
// LIBRARYS
//======================================================

#include <WiFi.h>
#include <time.h>
#include <math.h>
#include <Wire.h>
#include <RTClib.h>
#include <U8g2lib.h>
#include <AiEsp32RotaryEncoder.h>
#include <esp_task_wdt.h>
#include "esp_wifi.h"
#include "secrets.h"

// ======================================================
// TIME VALUES
// ======================================================
// Converts milliseconds into readable; seconds, minutes and hours.

const unsigned long SECOND = 1000UL;
const unsigned long MINUTE = 60000UL;
const unsigned long HOUR = 3600000UL;

// ======================================================
// ********USER ADJUSTABLE SETTINGS*********
// ======================================================
//user adjustable block

//------
//  DOOR
//------
// Door opens AFTER sunrise
int sunriseOpenOffset = 10;

// Door closes AFTER sunset
int sunsetCloseOffset = 30;

//-------
//  LIGHT
//-------
// Coop light comes ON before sunset.
int lightOnOffset = 5;

// Coop light goes OFF after sunset. Example, Door closes AFTER sunset = 30; + 5 minutes =35minutes
int lightOffMinutes = 35;

// Manual light auto OFF timer.
unsigned long manualLightTimeout = 10 * MINUTE;

//-------
//  DISPLAY
//-------
// Display goes to sleep after no activity
unsigned long oledTimeout = 3 * MINUTE;

//-------
//  SYSTEM
//-------
// Displayed firmware version
const char systemVersion[] = "V1.4 ";

//-------
//  BUZZER
//-------
// Fault buzzer repeat interval
unsigned long buzzerRepeatInterval = 15 * SECOND;

//-------
//  EVENTS
//-------
// Number of events stored.
#define MAX_EVENTS 50

// ======================================================
// *******USER ADJUSTABLE TIMEOUTS********
// =====================================================

#define MOTOR_TIMEOUT (15 * SECOND)  // adjust for how long the door motor runs between limit switches. Plus 2-3 seconds.

#define SAFETY_TIMEOUT (2 * MINUTE)  // adjust for overall fail safe

#define WIFI_RETRY_INTERVAL (45 * SECOND)  // if WiFi fails retry every __ until connected

// ======================================================
// GPIO
// ======================================================

#define MOTOR_OPEN_PIN 41   // Auto Motor OPEN. Via a L298N or TB6612FNG controller
#define MOTOR_CLOSE_PIN 42  // Auto Motor CLOSE. Via a L298N or TB6612FNG controller

#define LIMIT_OPEN_PIN 15   // Door Limit Switch OPEN, Normally open contacts. Connect to GND
#define LIMIT_CLOSE_PIN 16  // Door Limit Switch CLOSE, Normally open contacts. Connect to GND

#define LIGHT_PIN 17  // Auto Coop Light. To control a logic level relay board

#define BUZZER_PIN 3  // Active Buzzer for faults and time outs only. Connect to GND

#define STATUS_LED_PIN 6  // GREEN LED to GND. System on and working. 2k+ resistor.
#define FAULT_LED_PIN 7   // RED LED To GND. Mimicks the buzzer for faults and time outs.1k resistor.

// Rotary Encoder
#define ENCODER_CLK_PIN 4  // KY-040
#define ENCODER_DT_PIN 5   // KY-040
#define ENCODER_SW_PIN 18  // KY-040

// =======================================================
// ADDITIONAL GPI0 PIN INFO
// =======================================================
// INFO: I2C. 1.3" OLED display and RTC DS3231. Use the same connection pins for both.
// Pin 8 = SDA
// Pin 9 = SCK/SCL
// VCC      3.3v
// GND

//INFO: Rotary Switch. KY-040
// Pin 4  = CLK
// Pin 5  = DT
// Pin 18 = SW
// VCC    3.3v
// GND

// INFO: ESP32 board RESET. Momentary push button. Recommeded in the final build.
// Pin RST
// GND

// ======================================================
// ENUMS
// ======================================================

enum MotorCommand {
  MOTOR_STOP,
  MOTOR_OPEN,
  MOTOR_CLOSE
};

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
  SAFETY_OBSTRUCTION,
  SAFETY_WAITING
};

enum FaultState {

  FAULT_NONE,
  FAULT_ACTIVE
};

enum AutoState {
  AUTO_IDLE,            // system idle / no recent manual action
  AUTO_ALLOWED,         // full automation allowed
  AUTO_LOCKED_MANUAL,   // user has taken control override
  AUTO_SAFETY_RECOVERY  // recovering after obstruction/fault
};

AutoState autoState = AUTO_IDLE;

// ======================================================
// DISPLAY PAGES
// ======================================================
// Change the order of the display pages, leave main page where it is.

enum DisplayPage {

  PAGE_MAIN,
  PAGE_MANUAL,
  PAGE_EVENTS,
  PAGE_SYSTEM_WIFI,
  PAGE_SYSTEM_TIME
};

DisplayPage currentPage = PAGE_MAIN;

// ======================================================
// FUNCTION DECLARATIONS
// ======================================================

bool getTime(struct tm& timeinfo);
void syncRtcFromNtp();

void refreshDisplayIfNeeded();

void IRAM_ATTR readEncoderISR();

// ======================================================
// ROTARY ENCODER
// ======================================================

AiEsp32RotaryEncoder rotaryEncoder(
  ENCODER_CLK_PIN,
  ENCODER_DT_PIN,
  ENCODER_SW_PIN,
  -1,
  4);

void IRAM_ATTR readEncoderISR() {

  rotaryEncoder.readEncoder_ISR();
}

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
int storedEventCount = 0;

int displayedEventNumber = 1;

char latestEvent[64] = "System Start";
char latestEventTime[16] = "--:--";
char displayedEvent[64] = "System Start";
char displayedEventTime[16] = "--:--";

char statusLine[32] = "";

unsigned long lastOledActivity = 0;

bool oledSleeping = false;
bool displayDirty = true;

bool viewingHistory = false;

unsigned long historyViewStart = 0;

int lastEncoderValue = 0;
int manualEncoderAccum = 0;

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

float lastOpenTravelTime = 0;
float lastCloseTravelTime = 0;

unsigned long safetyStartTime = 0;
unsigned long safetyOpenReachedTime = 0;

unsigned long lastBuzzerAlert = 0;

bool buzzerState = false;
int buzzerStep = 0;
unsigned long buzzerStepTime = 0;

bool faultLedState = false;
unsigned long lastFaultLedToggle = 0;

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
// STATES
// ======================================================

DoorState doorState = DOOR_STOPPED;

SafetyState safetyState = SAFETY_IDLE;

FaultState faultState = FAULT_NONE;

// ======================================================
// FAULT*
// ======================================================

char faultReason[64] = "";
char lastLoggedFault[64] = "";

// ======================================================
// LIMIT SWITCH FAILURE / RETRY PROTECTION
// ======================================================

const int MAX_LIMIT_RETRIES = 3;

// CHICKEN / OBSTRUCTION RETRIES
const int MAX_OBSTRUCTION_RETRIES = 5;

int openRetryCount = 0;
int closeRetryCount = 0;

int obstructionRetries = 0;

bool systemLockout = false;


// ======================================================
// OVERRIDES
// ======================================================

bool manualMoveActive = false;

bool dailyResetDone = false;

// Track WHY manual override exists
bool manualOpenedDoor = false;
bool manualClosedDoor = false;

int manualOpenDay = -1;
int manualCloseDay = -1;

AutoState computeAutoState(int nowMinutes) {
  // Faults and lockouts always win
  if (systemLockout || faultState == FAULT_ACTIVE) {
    return AUTO_IDLE;
  }

  // Safety recovery owns the door
  if (safetyState != SAFETY_IDLE) {
    return AUTO_SAFETY_RECOVERY;
  }

  // User manually closed the door
  if (manualClosedDoor) {

    struct tm timeinfo;

    if (getTime(timeinfo)) {

      if (timeinfo.tm_yday != manualCloseDay && nowMinutes >= openTime && nowMinutes < closeTime) {

        manualClosedDoor = false;

      } else {

        return AUTO_LOCKED_MANUAL;
      }
    } else {

      return AUTO_LOCKED_MANUAL;
    }
  }

  // User manually opened the door
  if (manualOpenedDoor) {

    struct tm timeinfo;

    if (getTime(timeinfo)) {

      if (timeinfo.tm_yday != manualOpenDay && nowMinutes >= closeTime) {

        manualOpenedDoor = false;

      } else {

        return AUTO_LOCKED_MANUAL;
      }
    } else {

      return AUTO_LOCKED_MANUAL;
    }
  }

  // Daytime operating window
  if (nowMinutes >= openTime && nowMinutes < closeTime) {
    return AUTO_ALLOWED;
  }

  return AUTO_IDLE;
}

// ======================================================
// UTILITIES
// ======================================================

bool getTime(struct tm& timeinfo) {

  // FIRST CHOICE = NTP / ESP32 SYSTEM CLOCK
  if (WiFi.status() == WL_CONNECTED) {

    if (getLocalTime(&timeinfo, 100)) {
      return true;
    }
  }

  // SECOND CHOICE = RTC FALLBACK
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

  return false;
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

// ======================================================
// WIFI SIGNAL QUALITY
// ======================================================

int getAverageRSSI() {

  long total = 0;

  const int samples = 5;

  for (int i = 0; i < samples; i++) {

    total += WiFi.RSSI();

    delay(10);
  }

  return total / samples;
}
const char* getWifiQuality() {

  if (WiFi.status() != WL_CONNECTED) {
    return "NO WIFI";
  }

  int rssi = getAverageRSSI();

  // change the "text"of the strength of WiFi signal
  if (rssi >= -50) return "BEST";
  if (rssi >= -62) return "GOOD";
  if (rssi >= -75) return "USABLE";
  if (rssi >= -87) return "POOR";

  return "BAD";
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

  if (storedEventCount < MAX_EVENTS) {
    storedEventCount++;
  }

  displayedEventNumber = storedEventCount;

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

bool debouncedRead(
  uint8_t pin,
  bool& stableState,
  bool& lastReading,
  unsigned long& lastDebounceTime) {
  bool reading = digitalRead(pin);

  // RAW INPUT CHANGED
  if (reading != lastReading) {

    lastDebounceTime = millis();

    lastReading = reading;
  }

  // INPUT STABLE LONG ENOUGH?
  if ((millis() - lastDebounceTime) > debounceMs) {

    stableState = reading;
  }

  return stableState;
}

bool openLimitHit() {

  return debouncedRead(
           LIMIT_OPEN_PIN,
           openStableState,
           lastOpenReading,
           openDebounceTime)
         == LOW;
}

bool closeLimitHit() {

  return debouncedRead(
           LIMIT_CLOSE_PIN,
           closeStableState,
           lastCloseReading,
           closeDebounceTime)
         == LOW;
}
// ======================================================
// DISPLAY
// ======================================================

void buildStatusLine() {

  switch (doorState) {

    case DOOR_OPEN:
      snprintf(statusLine, sizeof(statusLine), "Door Open");
      break;

    case DOOR_CLOSED:
      snprintf(statusLine, sizeof(statusLine), "Door Closed");
      break;

    case DOOR_OPENING:
      snprintf(statusLine, sizeof(statusLine), "Door Opening");
      break;

    case DOOR_CLOSING:
      snprintf(statusLine, sizeof(statusLine), "Door Closing");
      break;

    default:
      snprintf(statusLine, sizeof(statusLine), "Door Stopped");
      break;
  }
}

void drawHeader() {
  //============================================
  // TITLE MAIN PAGE
  //============================================
  u8g2.setFont(u8g2_font_helvB10_tf);

  const char* title = "Auto Hen House";

  int titleWidth =
    u8g2.getStrWidth(title);

  u8g2.drawStr(
    (128 - titleWidth) / 2,
    12,
    title);

  // VERSION
  u8g2.setFont(u8g2_font_5x7_tf);

  const char* version = systemVersion;

  int versionWidth =
    u8g2.getStrWidth(version);

  u8g2.drawStr(
    (128 - versionWidth) / 2,
    21,
    version);

  // DIVIDER
  u8g2.drawHLine(0, 24, 128);
}

void drawMainDisplay() {

  if (oledSleeping) return;

  buildStatusLine();

  u8g2.clearBuffer();

  // HEADER
  drawHeader();

  // DOOR STATUS
  u8g2.setFont(u8g2_font_helvB12_tf);

  int statusWidth =
    u8g2.getStrWidth(statusLine);

  u8g2.drawStr(
    (128 - statusWidth) / 2,
    44,
    statusLine);


  // LIGHT STATUS
  u8g2.setFont(u8g2_font_6x13_tf);

  const char* lightText =
    lightOn ? "Light ON" : "Light OFF";

  u8g2.drawStr(8, 63, lightText);

  // ==========================================
  // WIFI TEXT main page
  // ==========================================
  u8g2.setFont(u8g2_font_5x7_tf);

  bool connected = (WiFi.status() == WL_CONNECTED);

  const char* label = "WiFi";
  int labelWidth = u8g2.getStrWidth(label);

  int y = 63;

  if (connected) {

    int x = 128 - labelWidth - 1;
    u8g2.drawStr(x, y, label);

  } else {

    int frame = (millis() / 130) % 3;  // speed of the dot animation

    int dotRadius = 2;
    int spacing = 7;

    int totalWidth = labelWidth + 1 + (3 * spacing);

    int xStart = 128 - totalWidth - 1;

    // label
    u8g2.drawStr(xStart, y, label);

    int baseX = xStart + labelWidth + 5;

    for (int i = 0; i < 3; i++) {

      int x = baseX + i * spacing;

      if (i == frame) {
        u8g2.drawDisc(x, y - 3, dotRadius);  // small solid dot
      } else {
        u8g2.drawCircle(x, y - 3, dotRadius);  // small hollow dot
      }
    }
  }
  u8g2.sendBuffer();
  displayDirty = false;
}

void drawManualPage() {

  if (oledSleeping) return;

  u8g2.clearBuffer();

  // =====================================================
  // TITLE MANUAL CONTROL PAGE
  // =====================================================

  u8g2.setFont(u8g2_font_6x13_tf);

  const char* title = "Manual Control";

  int titleWidth =
    u8g2.getStrWidth(title);

  u8g2.drawStr(
    (128 - titleWidth) / 2,
    12,
    title);

  u8g2.drawHLine(0, 16, 128);

  // =====================================================
  // CONTROL TEXT
  // =====================================================

  u8g2.setFont(u8g2_font_6x12_tf);

  u8g2.drawStr(6, 30, "CCW = OPEN");

  u8g2.drawStr(6, 44, "CW  = CLOSE");

  // =====================================================
  // LIGHT CONTROL
  // =====================================================

  u8g2.drawStr(6, 58, "Hold Btn = Light");

  if (lightOn) {

    u8g2.drawDisc(118, 54, 3);
  }

  // =====================================================
  // DOOR MOVEMENT INDICATOR
  // =====================================================

  if (doorState == DOOR_OPENING) {

    // DOUBLE UP ARROWS

    u8g2.drawLine(104, 30, 110, 24);
    u8g2.drawLine(110, 24, 116, 30);

    u8g2.drawLine(104, 40, 110, 34);
    u8g2.drawLine(110, 34, 116, 40);
  }

  else if (doorState == DOOR_CLOSING) {

    // DOUBLE DOWN ARROWS

    u8g2.drawLine(104, 24, 110, 30);
    u8g2.drawLine(110, 30, 116, 24);

    u8g2.drawLine(104, 34, 110, 40);
    u8g2.drawLine(110, 40, 116, 34);
  }

  u8g2.sendBuffer();
}

void drawEventsPage() {

  if (oledSleeping) return;

  u8g2.clearBuffer();

  // =====================================================
  // TITLE EVENTS PAGE
  // =====================================================

  u8g2.setFont(u8g2_font_6x13_tf);

  const char* title = "Events";

  int titleWidth = u8g2.getStrWidth(title);

  u8g2.drawStr((128 - titleWidth) / 2, 12, title);

  u8g2.drawHLine(0, 16, 128);

  // =====================================================
  // NO EVENTS
  // =====================================================

  if (storedEventCount == 0) {

    u8g2.drawStr(20, 40, "No Events");

    u8g2.sendBuffer();

    return;
  }

  // =====================================================
  // GET CURRENT EVENT
  // =====================================================

  int index = newestEventIndex - viewedEventOffset;

  if (index < 0) {
    index += MAX_EVENTS;
  }

  // =====================================================
  // TIME
  // =====================================================

  u8g2.setFont(u8g2_font_6x10_tf);

  u8g2.drawStr(
    2,
    28,
    eventHistory[index].time);

  // =====================================================
  // EVENT COUNTER
  // =====================================================

  char counterBuf[16];

  snprintf(
    counterBuf,
    sizeof(counterBuf),
    "%d/%d",
    storedEventCount - viewedEventOffset,
    storedEventCount);

  u8g2.drawStr(88, 28, counterBuf);

  // =====================================================
  // EVENT MESSAGE
  // =====================================================

  u8g2.setFont(u8g2_font_helvB10_tf);

  char line1[22] = "";
  char line2[22] = "";

  const char* msg = eventHistory[index].msg;

  int len = strlen(msg);

  if (len <= 20) {
    strncpy(line1, msg, sizeof(line1) - 1);
  } else

  {
    int split = 20;

    while (split > 0 && msg[split] != ' ') {
      split--;
    }

    // No space found, hard split
    if (split == 0) {
      split = 20;
    }

    strncpy(line1, msg, split);
    line1[split] = '\0';

    while (msg[split] == ' ') {
      split++;
    }

    strncpy(line2, msg + split, sizeof(line2) - 1);
  }

  u8g2.drawStr(2, 46, line1);
  u8g2.drawStr(2, 64, line2);

  u8g2.sendBuffer();
}

// ======================================================
// SYSTEM TIME PAGE
// ======================================================

void drawSystemTimePage() {

  if (oledSleeping) return;

  u8g2.clearBuffer();

  struct tm timeinfo;

  char timeBuf[16] = "--:--:--";

  if (getTime(timeinfo)) {

    snprintf(
      timeBuf,
      sizeof(timeBuf),
      "%02d:%02d:%02d",
      timeinfo.tm_hour,
      timeinfo.tm_min,
      timeinfo.tm_sec);
  }

  // =====================================================
  // TITLE SYSTEM TIME
  // =====================================================

  u8g2.setFont(u8g2_font_6x13_tf);

  const char* title = "System Times";

  int titleWidth = u8g2.getStrWidth(title);

  u8g2.drawStr(
    (128 - titleWidth) / 2,
    12,
    title);

  u8g2.drawHLine(0, 16, 128);

  // =====================================================
  // CURRENT TIME
  // =====================================================

  u8g2.setFont(u8g2_font_7x14_tf);

  int timeWidth = u8g2.getStrWidth(timeBuf);

  u8g2.drawStr(
    (128 - timeWidth) / 2,
    36,
    timeBuf);

  // =====================================================
  // SUNRISE / SUNSET
  // =====================================================

  char sunBuf[32];

  snprintf(
    sunBuf,
    sizeof(sunBuf),
    "SR %02d:%02d  SS %02d:%02d",
    actualSunrise / 60,
    actualSunrise % 60,
    actualSunset / 60,
    actualSunset % 60);

  u8g2.drawStr(2, 58, sunBuf);

  u8g2.sendBuffer();

  displayDirty = false;
}
//========================================
//SYSTEM WIFI PAGE
//========================================

void drawSystemWiFiPage() {

  if (oledSleeping) return;

  u8g2.clearBuffer();

  // =====================================================
  // TITLE SYSTEM WIFI
  // =====================================================

  u8g2.setFont(u8g2_font_6x13_tf);

  const char* title = "System WiFi";

  int titleWidth = u8g2.getStrWidth(title);

  u8g2.drawStr(
    (128 - titleWidth) / 2,
    12,
    title);

  u8g2.drawHLine(0, 16, 128);

  // =====================================================
  // LARGER TEXT
  // =====================================================

  u8g2.setFont(u8g2_font_7x14_tf);

  // =====================================================
  // WIFI QUALITY (CENTERED)
  // =====================================================

  char wifiBuf[24];

  snprintf(
    wifiBuf,
    sizeof(wifiBuf),
    "WiFi - %s",
    getWifiQuality());

  int wifiWidth =
    u8g2.getStrWidth(wifiBuf);

  u8g2.drawStr(
    (128 - wifiWidth) / 2,
    34,
    wifiBuf);

  // =====================================================
  // IP ADDRESS (CENTERED)
  // =====================================================

  char ipBuf[32];

  if (WiFi.status() == WL_CONNECTED) {

    snprintf(
      ipBuf,
      sizeof(ipBuf),
      "IP %s",
      WiFi.localIP().toString().c_str());
  } else {

    snprintf(
      ipBuf,
      sizeof(ipBuf),
      "IP No WiFi");
  }

  int ipWidth =
    u8g2.getStrWidth(ipBuf);

  u8g2.drawStr(
    (128 - ipWidth) / 2,
    56,
    ipBuf);

  // =====================================================

  u8g2.sendBuffer();

  displayDirty = false;
}

void refreshDisplayIfNeeded() {

  static unsigned long lastRefresh = 0;

  if (oledSleeping) return;

  unsigned long refreshRate = 1000UL;

  // SYSTEM PAGE UPDATES EVERY SECOND
  if (currentPage == PAGE_SYSTEM_TIME) {

    refreshRate = 1000UL;
  }

  if (!displayDirty && millis() - lastRefresh < refreshRate) {

    return;
  }

  lastRefresh = millis();

  if (currentPage == PAGE_MAIN) {

    drawMainDisplay();
  }

  else if (currentPage == PAGE_MANUAL) {

    drawManualPage();
  }

  else if (currentPage == PAGE_EVENTS) {

    drawEventsPage();
  }

  else if (currentPage == PAGE_SYSTEM_TIME) {

    drawSystemTimePage();
  }

  else if (currentPage == PAGE_SYSTEM_WIFI) {

    drawSystemWiFiPage();
  }
}
void oledPrint(const char* text) {

  // LOG EVENT + DISPLAY IT
  setEvent(text);
}

void oledNotice(const char* text) {

  // DISPLAY ONLY (NO EVENT LOGGING)

  strncpy(
    latestEvent,
    text,
    sizeof(latestEvent) - 1);

  latestEvent[sizeof(latestEvent) - 1] = '\0';

  updateEventTime();

  displayDirty = true;

  refreshDisplayIfNeeded();
}

// ======================================================
// MOTOR SAFETY
// ======================================================

void setMotor(MotorCommand cmd) {

  // HARD SAFETY FIRST
  digitalWrite(MOTOR_OPEN_PIN, LOW);
  digitalWrite(MOTOR_CLOSE_PIN, LOW);

  delay(50);

  if (cmd == MOTOR_OPEN) {
    digitalWrite(MOTOR_OPEN_PIN, HIGH);
  } else if (cmd == MOTOR_CLOSE) {
    digitalWrite(MOTOR_CLOSE_PIN, HIGH);
  }
}

void motorSafetyCheck() {
  bool openState = digitalRead(MOTOR_OPEN_PIN);
  bool closeState = digitalRead(MOTOR_CLOSE_PIN);

  if (openState && closeState) {
    setMotor(MOTOR_STOP);
    triggerFault("MOTOR CONFLICT");
  }
}

void safeRelayTransition() {
  setMotor(MOTOR_STOP);
  delay(50);
}


void stopMotor() {

  setMotor(MOTOR_STOP);

  if (doorState == DOOR_OPENING || doorState == DOOR_CLOSING) {

    doorState = DOOR_STOPPED;
  }

  manualMoveActive = false;
  manualEncoderAccum = 0;

  displayDirty = true;
}

// ======================================================
// BUZZER
// ======================================================

void updateBuzzerPattern() {

  if (faultState != FAULT_ACTIVE && !systemLockout) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerStep = 0;
    buzzerState = false;
    return;
  }

  unsigned long now = millis();

  switch (buzzerStep) {

    case 0:  // start beep cycle
      digitalWrite(BUZZER_PIN, HIGH);
      buzzerState = true;
      buzzerStepTime = now;
      buzzerStep = 1;
      break;

    case 1:  // first ON duration (50ms)
      if (now - buzzerStepTime >= 90) {
        digitalWrite(BUZZER_PIN, LOW);
        buzzerStepTime = now;
        buzzerStep = 2;
      }
      break;

    case 2:  // pause (80ms)
      if (now - buzzerStepTime >= 120) {
        digitalWrite(BUZZER_PIN, HIGH);
        buzzerStepTime = now;
        buzzerStep = 3;
      }
      break;

    case 3:  // second ON duration (50ms)
      if (now - buzzerStepTime >= 90) {
        digitalWrite(BUZZER_PIN, LOW);
        buzzerStepTime = now;
        buzzerStep = 4;
      }
      break;

    case 4:  // wait until repeat interval
      if (now - buzzerStepTime >= buzzerRepeatInterval) {
        buzzerStep = 0;
      }
      break;
  }
}

void updateFaultBuzzer() {

  // ONLY ALERT DURING FAULTS
  if (faultState != FAULT_ACTIVE && !systemLockout) {
    return;
  }

  // WAIT FOR NEXT ALERT INTERVAL
  if (millis() - lastBuzzerAlert >= buzzerRepeatInterval) {

    lastBuzzerAlert = millis();
  }
}

// ======================================================
// STATUS LEDs
// ======================================================

void updateStatusLEDs() {

  // ==========================================
  // NORMAL OPERATION
  // ==========================================

  if (faultState == FAULT_NONE && !systemLockout) {

    digitalWrite(STATUS_LED_PIN, HIGH);  // GREEN ON

    digitalWrite(FAULT_LED_PIN, LOW);  // RED OFF

    return;
  }

  // ==========================================
  // FAULT / LOCKOUT
  // ==========================================

  digitalWrite(STATUS_LED_PIN, LOW);  // GREEN OFF

  unsigned long flashRate = systemLockout ? 150 : 500;

  if (millis() - lastFaultLedToggle >= flashRate) {

    lastFaultLedToggle = millis();

    faultLedState = !faultLedState;

    digitalWrite(FAULT_LED_PIN, faultLedState);
  }
}

// ======================================================
// FAULTS
// ======================================================

void clearFaultState() {

  stopMotor();

  faultState = FAULT_NONE;

  memset(faultReason, 0, sizeof(faultReason));

  memset(lastLoggedFault, 0, sizeof(lastLoggedFault));

  safetyState = SAFETY_IDLE;

  motorStartTime = 0;

  safetyStartTime = 0;

  manualOpenedDoor = false;
  manualClosedDoor = false;

  setEvent("Fault Cleared");
}

// ======================================================
// FAULT HANDLING
// =====================================================


void triggerLockout(const char* reason);

void triggerFault(const char* reason) {

  stopMotor();

  faultState = FAULT_ACTIVE;

  lastBuzzerAlert = millis();

  manualMoveActive = false;
  manualEncoderAccum = 0;

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


  if (strcmp(lastLoggedFault, reason) != 0) {
    setEvent("FAULT");
    setEvent(reason);

    strncpy(
      lastLoggedFault,
      reason,
      sizeof(lastLoggedFault) - 1);

    lastLoggedFault[sizeof(lastLoggedFault) - 1] = '\0';
  }
}

void triggerLockout(const char* reason) {

  stopMotor();

  systemLockout = true;
  faultState = FAULT_ACTIVE;

  lastBuzzerAlert = millis();

  strncpy(faultReason, reason, sizeof(faultReason) - 1);
  faultReason[sizeof(faultReason) - 1] = '\0';

  setEvent("SYSTEM LOCKOUT");
  setEvent(reason);

  Serial.println("!!! LOCKOUT TRIGGERED !!!");
}

// ======================================================
// MANUAL OVERRIDE
// ======================================================

bool automationAllowed() {

  return autoState == AUTO_ALLOWED;
}

// ======================================================
// MOTOR PERMISSION GATE (HARD PRIORITY RULE)
// ======================================================

bool canAutoAct() {

  if (systemLockout) return false;
  if (faultState == FAULT_ACTIVE) return false;

  return autoState == AUTO_ALLOWED;
}

// ======================================================
// OPEN
// ======================================================


void startOpening() {

  // ==================================================
  // HARD PRIORITY GATE
  // ==================================================


  if (systemLockout) {
    oledNotice("LOCKED OUT");
    return;
  }

  if (faultState == FAULT_ACTIVE) return;

  if (openLimitHit()) {

    stopMotor();

    doorState = DOOR_OPEN;

    openRetryCount = 0;

    return;
  }

  safeRelayTransition();

  setMotor(MOTOR_OPEN);

  motorStartTime = millis();

  doorState = DOOR_OPENING;
}

// ======================================================
// CLOSE
// ======================================================

void startClosing() {

  // ==================================================
  // HARD PRIORITY GATE
  // ==================================================

  if (systemLockout) {
    oledNotice("LOCKED OUT");
    return;
  }

  if (faultState == FAULT_ACTIVE) return;

  if (closeLimitHit()) {

    stopMotor();

    doorState = DOOR_CLOSED;

    return;
  }

  safeRelayTransition();

  setMotor(MOTOR_CLOSE);

  motorStartTime = millis();

  doorState = DOOR_CLOSING;
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

    setEvent("WiFi Connected");

    Serial.println("WiFi Connected");

    Serial.println(WiFi.localIP());

    syncRtcFromNtp();
  }

  // ==========================================
  // WIFI JUST DISCONNECTED
  // ==========================================

  if (!currentState && wifiConnected) {

    wifiConnected = false;

    setEvent("WiFi Lost");

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

  // WAIT SPECIFICALLY FOR NTP / ESP32 SYSTEM TIME

  while (!getLocalTime(&timeinfo, 1000)) {

    esp_task_wdt_reset();

    if (millis() - start > 10000UL) {

      setEvent("NTP Timeout");

      return;
    }
  }

  setEvent("Time synced");

  syncRtcFromNtp();
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

    setEvent("Light ON");
  }

  // TURN OFF
  if (lightOn && nowMinutes >= offTime) {

    digitalWrite(LIGHT_PIN, LOW);

    lightOn = false;

    lightCycleComplete = true;

    setEvent("Light OFF");
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

  // =====================================================
  // ROTARY ENCODER ROTATION
  // =====================================================

  if (rotaryEncoder.encoderChanged() && currentPage == PAGE_MANUAL) {

    int value = rotaryEncoder.readEncoder();

    static int lastValue = value;

    int delta = value - lastValue;

    manualEncoderAccum += delta;

    // -----------------------------------------
    // CCW = OPEN
    // Require 3 detents
    // -----------------------------------------

    if (!manualMoveActive && manualEncoderAccum >= 3) {

      manualEncoderAccum = 0;

      if (openLimitHit()) {

        oledNotice("Already OPEN");

        manualEncoderAccum = 0;
      } else {

        manualMoveActive = true;

        // Cancel any manual override and return to AUTO mode
        manualOpenedDoor = false;
        manualClosedDoor = false;

        manualOpenDay = -1;
        manualCloseDay = -1;

        setEvent("Manual OPEN");
        startOpening();
      }
    }

    // -----------------------------------------
    // CW = CLOSE
    // Require 3 detents
    // -----------------------------------------

    if (!manualMoveActive && manualEncoderAccum <= -3) {

      manualEncoderAccum = 0;

      if (closeLimitHit()) {

        oledNotice("Already CLOSED");

        manualEncoderAccum = 0;
      } else {

        manualMoveActive = true;

        manualClosedDoor = true;
        manualOpenedDoor = false;

        struct tm timeinfo;
        if (getTime(timeinfo)) {
          manualCloseDay = timeinfo.tm_yday;
        }

        setEvent("Manual CLOSE");

        startClosing();
      }
    }

    lastValue = value;
  }
}
// ======================================================
// UPDATE DOOR
// ======================================================

void updateDoor() {

  bool openHit = openLimitHit();
  bool closeHit = closeLimitHit();

  // INVALID STATE
  if (openHit && closeHit) {

    triggerFault("LIMIT CONFLICT");

    return;
  }


  switch (doorState) {

    case DOOR_OPENING:

      if (openHit) {

        openRetryCount = 0;

        lastOpenTravelTime =
          (millis() - motorStartTime) / 1000.0f;

        stopMotor();

        doorState = DOOR_OPEN;

        char buf[32];

        snprintf(
          buf,
          sizeof(buf),
          "Door Open %.1fs",
          lastOpenTravelTime);

        setEvent(buf);
      }

      else if (millis() - motorStartTime > MOTOR_TIMEOUT) {

        triggerFault("OPEN timeout");
      }

      break;

    case DOOR_CLOSING:

      // ==========================================
      // DOOR CLOSED SUCCESSFULLY
      // ==========================================
      if (closeHit) {

        closeRetryCount = 0;

        obstructionRetries = 0;

        lastCloseTravelTime =
          (millis() - motorStartTime) / 1000.0f;

        stopMotor();

        doorState = DOOR_CLOSED;

        char buf[32];

        snprintf(
          buf,
          sizeof(buf),
          "Door Close %.1fs",
          lastCloseTravelTime);

        setEvent(buf);

        displayDirty = true;
      }

      // ==========================================
      // CLOSE TIMEOUT
      // POSSIBLE CHICKEN OBSTRUCTION
      // ==========================================
      else if (millis() - motorStartTime > MOTOR_TIMEOUT) {

        stopMotor();

        obstructionRetries++;

        setEvent("Close obstruction");

        // TOO MANY RETRIES
        if (obstructionRetries >= MAX_OBSTRUCTION_RETRIES) {

          safetyState = SAFETY_IDLE;

          triggerFault("Door blocked");
        }

        else {

          // START SAFETY TIMER
          safetyStartTime = millis();

          // REOPEN DOOR

          safetyState = SAFETY_OBSTRUCTION;

          startOpening();
        }
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

    // ==========================================
    // NORMAL STATE
    // ==========================================
    case SAFETY_IDLE:
      break;

    // ==========================================
    // DOOR IS REOPENING
    // ==========================================
    case SAFETY_OBSTRUCTION:

      // WAIT UNTIL FULLY OPEN
      if (doorState == DOOR_OPEN) {

        safetyOpenReachedTime = millis();

        safetyState = SAFETY_WAITING;
      }

      break;

    // ==========================================
    // WAIT BEFORE RETRY CLOSE
    // ==========================================
    case SAFETY_WAITING:

      // WAIT 5 SECONDS
      if (millis() - safetyOpenReachedTime > 5000UL) {

        startClosing();

        safetyState = SAFETY_IDLE;
      }

      break;
  }

  // ==========================================
  // GLOBAL SAFETY TIMEOUT
  // ==========================================
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

    setEvent("Door UNKNOWN");

    setEvent("Homing CLOSE");

    safeRelayTransition();

    setMotor(MOTOR_CLOSE);

    unsigned long start = millis();

    while (millis() - start < MOTOR_TIMEOUT) {

      esp_task_wdt_reset();

      if (closeLimitHit()) {

        stopMotor();

        doorState = DOOR_CLOSED;

        setEvent("Home OK");

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

    setEvent("Door OPEN");
  } else if (closeLimitHit()) {

    doorState = DOOR_CLOSED;

    //setEvent("Door CLOSED");
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

  u8g2.drawStr(18, 52, "Auto Controller");

  u8g2.sendBuffer();

  unsigned long start = millis();

  while (millis() - start < 2000) {

    esp_task_wdt_reset();

    delay(1);
  }
}

// ======================================================
// SETUP
// ======================================================

void setup() {

  Serial.begin(115200);

  // ======================================================
  // WATCHDOG
  // ======================================================

  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 35000,  // 35 second watchdog timeout
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
    .trigger_panic = true
  };

  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);


  esp_reset_reason_t resetReason = esp_reset_reason();

  Wire.begin();

  u8g2.begin();

  showWelcomeScreen();

  if (rtc.begin()) {

    rtcAvailable = true;

    if (rtc.lostPower()) {

      setEvent("RTC Battery Low");
      Serial.println("RTC LOST POWER");
    }

    DateTime now = rtc.now();

    if (now.year() < 2025 || now.year() > 2050) {

      rtcAvailable = false;

      Serial.println("RTC INVALID");
    } else {

      Serial.println("RTC OK");
    }

  } else {

    rtcAvailable = false;

    Serial.println("RTC NOT FOUND");
  }

  if (resetReason == ESP_RST_POWERON) {

    setEvent("Powered Up");
  } else if (resetReason == ESP_RST_TASK_WDT) {

    setEvent("Watchdog Reset");
  } else if (resetReason == ESP_RST_SW) {

    setEvent("Software Reset");
  } else {

    setEvent("System Restart");
  }

  if (!rtcAvailable) {

    setEvent("RTC Invalid");
  }

  pinMode(MOTOR_OPEN_PIN, OUTPUT);
  pinMode(MOTOR_CLOSE_PIN, OUTPUT);

  pinMode(LIMIT_OPEN_PIN, INPUT_PULLUP);
  pinMode(LIMIT_CLOSE_PIN, INPUT_PULLUP);

  // INITIALIZE DEBOUNCE STATES
  lastOpenReading = digitalRead(LIMIT_OPEN_PIN);
  openStableState = lastOpenReading;

  lastCloseReading = digitalRead(LIMIT_CLOSE_PIN);
  closeStableState = lastCloseReading;

  pinMode(LIGHT_PIN, OUTPUT);

  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(FAULT_LED_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);

  digitalWrite(STATUS_LED_PIN, LOW);
  digitalWrite(FAULT_LED_PIN, LOW);

  lastBuzzerAlert = millis();
  // Rotary Encoder Setup
  rotaryEncoder.begin();

  rotaryEncoder.setup(
    readEncoderISR);

  rotaryEncoder.setBoundaries(
    -1000,
    1000,
    false);

  rotaryEncoder.disableAcceleration();

  rotaryEncoder.setEncoderValue(0);

  lastEncoderValue = rotaryEncoder.readEncoder();

  stopMotor();

  WiFi.mode(WIFI_STA);

  // Disable WiFi power saving
  WiFi.setSleep(false);

  // Auto reconnect
  WiFi.setAutoReconnect(true);

  // Maximum transmit power
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  // Lock WiFi bandwidth to 20MHz
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  setupTime();

  waitForValidTime();


  performStartupRecovery();
}

// ======================================================
// EVENT SCROLL HANDLER
// ======================================================

void handleEventScroll() {

  static unsigned long buttonDownTime = 0;

  static bool buttonWasDown = false;

  static bool resetPromptShown = false;

  bool buttonDown =
    rotaryEncoder.isEncoderButtonDown();

  // =====================================================
  // REAL EVENT SCROLLING WITH ROTARY
  // =====================================================

  if (currentPage == PAGE_EVENTS) {

    int encoderValue = rotaryEncoder.readEncoder();

    if (encoderValue != lastEncoderValue) {

      int delta = encoderValue - lastEncoderValue;

      // CLOCKWISE = OLDER EVENTS
      if (delta > 0) {

        viewedEventOffset++;

        if (viewedEventOffset >= storedEventCount) {
          viewedEventOffset = storedEventCount - 1;
        }
      }

      // COUNTERCLOCKWISE = NEWER EVENTS
      if (delta < 0) {

        viewedEventOffset--;

        if (viewedEventOffset < 0) {
          viewedEventOffset = 0;
          manualMoveActive = false;
        }
      }

      displayedEventNumber =
        storedEventCount - viewedEventOffset;

      viewingHistory = true;

      historyViewStart = millis();

      lastOledActivity = millis();

      displayDirty = true;

      drawEventsPage();
    }

    lastEncoderValue = encoderValue;
  }

  // =====================================================
  // BUTTON PRESSED
  // =====================================================

  if (buttonDown && !buttonWasDown) {

    buttonDownTime = millis();

    resetPromptShown = false;
  }

  // =====================================================
  // BUTTON HELD
  // =====================================================

  if (buttonDown) {

    unsigned long holdTime =
      millis() - buttonDownTime;

    // SHOW RESET WARNING
    if (holdTime >= 3000 && !resetPromptShown) {

      oledNotice("Hold for RESET");

      resetPromptShown = true;
    }
  }

  // =====================================================
  // BUTTON RELEASED
  // =====================================================

  if (!buttonDown && buttonWasDown) {

    unsigned long pressTime =
      millis() - buttonDownTime;

    // =====================================================
    // 5+ SEC = RESET
    // =====================================================

    if (pressTime >= 5000) {

      systemLockout = false;

      openRetryCount = 0;

      closeRetryCount = 0;

      clearFaultState();

      setEvent("RESET OK");

      delay(750);

      displayDirty = true;

      drawMainDisplay();

      buttonWasDown = buttonDown;

      return;
    }

    // =====================================================
    // SHORT PRESS = CHANGE PAGE
    // =====================================================

    if (pressTime < 500) {

      // -------------------------------------------------
      // WAKE DISPLAY ONLY
      // -------------------------------------------------
      if (oledSleeping) {

        u8g2.setPowerSave(0);

        oledSleeping = false;

        currentPage = PAGE_MAIN;

        lastOledActivity = millis();

        displayDirty = true;

        refreshDisplayIfNeeded();

        buttonWasDown = buttonDown;

        return;
      }

      // -------------------------------------------------
      // NORMAL PAGE CHANGE
      // -------------------------------------------------
      currentPage =
        (DisplayPage)((currentPage + 1) % 5);

      viewedEventOffset = 0;

      // RESET ENCODER BASELINE
      // PREVENTS SCROLL JUMP
      lastEncoderValue =
        rotaryEncoder.readEncoder();

      viewingHistory = false;

      lastOledActivity = millis();

      displayDirty = true;

      refreshDisplayIfNeeded();
    }

    // =====================================================
    // MANUAL PAGE LIGHT TOGGLE
    // =====================================================

    else if (
      currentPage == PAGE_MANUAL && pressTime >= 700 && pressTime < 2000) {

      manualLightOverride = !manualLightOverride;

      if (manualLightOverride) {

        lightOn = true;

        digitalWrite(LIGHT_PIN, HIGH);

        manualLightOffTime =
          millis() + manualLightTimeout;

        setEvent("Manual Light ON");
      }

      else {

        lightOn = false;

        digitalWrite(LIGHT_PIN, LOW);

        manualLightOffTime = 0;

        setEvent("Manual Light OFF");
      }

      lastOledActivity = millis();

      displayDirty = true;

      refreshDisplayIfNeeded();
    }

    // =====================================================
    // RESET CANCELLED
    // =====================================================

    else if (pressTime >= 3000 && pressTime < 5000) {

      oledNotice("Reset Cancelled");

      delay(750);

      displayDirty = true;

      drawMainDisplay();
    }
  }

  buttonWasDown = buttonDown;
}

// ======================================================
// LOOP
// ======================================================

void loop() {

  esp_task_wdt_reset();

  handleSwitch();

  handleEventScroll();

  updateDoor();

  updateSafetyCycle();

  motorSafetyCheck();

  updateBuzzerPattern();

  updateStatusLEDs();

  connectWiFi();

  refreshDisplayIfNeeded();

  // ==========================================
  // MANUAL LIGHT AUTO OFF
  // ==========================================

  if (manualLightOverride && manualLightOffTime > 0 && millis() >= manualLightOffTime) {

    manualLightOverride = false;

    manualLightOffTime = 0;

    lightOn = false;

    digitalWrite(LIGHT_PIN, LOW);

    setEvent("Light Timeout");
  }

  if (!oledSleeping && millis() - lastOledActivity > oledTimeout) {

    u8g2.clearBuffer();
    u8g2.sendBuffer();

    u8g2.setPowerSave(1);

    oledSleeping = true;
  }

  if (millis() - lastCheck > 1 * MINUTE) {

    lastCheck = millis();

    struct tm timeinfo;

    if (!getTime(timeinfo)) {
      setEvent("Time failed");
      return;
    }

    updateSunTimes(timeinfo);

    int nowMinutes =
      (timeinfo.tm_hour * 60) + timeinfo.tm_min;

    autoState = computeAutoState(nowMinutes);

    static AutoState lastAutoState = AUTO_IDLE;

    if (autoState != lastAutoState) {

      switch (autoState) {

        case AUTO_IDLE:
          setEvent("AUTO_IDLE");
          break;

        case AUTO_ALLOWED:
          setEvent("AUTO_ALLOWED");
          break;

        case AUTO_LOCKED_MANUAL:
          setEvent("AUTO_LOCKED");
          break;

        case AUTO_SAFETY_RECOVERY:
          setEvent("AUTO_SAFETY");
          break;
      }

      lastAutoState = autoState;
    }

    updateLight(nowMinutes);

    // ONLY decision logic (keep INSIDE block for now)
    if (autoState == AUTO_ALLOWED) {

      if (doorState == DOOR_CLOSED && nowMinutes >= openTime && nowMinutes < closeTime) {

        setEvent("AUTO OPEN");
        startOpening();
      }

      if (doorState == DOOR_OPEN && nowMinutes >= closeTime) {

        setEvent("AUTO CLOSE");
        startClosing();
      }
    }
  }
}
