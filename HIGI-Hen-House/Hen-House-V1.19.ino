// System: ESP32S3 N16R8 board. Arduino IDE: ESP32S3 Dev Module

// The system uses a motor and cord guillotine style door.
// All customisation is placed together in one block split into two parts ***USER SETTING*** & **USER MAGIC NUMBERS***
// All controlled via a rotary encoder. Short press,wake and page change, >=1 second press toggle coop light on/off (manual page only), >=5 seconds fault reset
//Rooster Crow Control: Added an option for fixed time door-opening schedule to help prevent early morning crowing in the summer.
//======================================================
// LIBRARYS
//======================================================
#include <DHT.h>
#include <WiFi.h>
#include <time.h>
#include <math.h>
#include <Wire.h>
#include <RTClib.h>
#include "secrets.h"
#include <U8g2lib.h>
#include "esp_wifi.h"
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <AiEsp32RotaryEncoder.h>

// ======================================================
// TIME VALUES
// ======================================================
// Converts milliseconds into readable; seconds, minutes and hours.

const unsigned long SECOND = 1000UL;
const unsigned long MINUTE = 60000UL;
const unsigned long HOUR = 3600000UL;

// ======================================================
// ********USER SETTINGS*********
// ======================================================

//--------------------------------------
//  SYSTEM
//--------------------------------------
// Displayed firmware version
const char systemVersion[] = "V1.19";

//-------------------------------------
//  DOOR GMT TIME. (winter)
//-------------------------------------
// Positive offset = AFTER sunrise/sunset
// Negative offset = BEFORE sunrise/sunset.

//MORNING. 
// Door opening offset from sunrise - GMT
// Soltice twilight start -37 minutes. Default +30
int sunriseOpenOffsetGMT = +30;

//EVENING
// Door closing offset from sunset - GMT
// Soltice twilight end +37 minutes. Default +11
// **** See light OFF
int sunsetCloseOffsetGMT = +11;

//--------------------------------------
//  LIGHT GMT TIME. (winter)
//--------------------------------------
// Positive offset = AFTER sunrise/sunset
// Negative offset = BEFORE sunrise/sunset.

//LIGHT ON
// Coop light ON offset from sunset - GMT
// Soltice twilight end +37 minutes. Default -20
int lightOnOffsetGMT = -20;

//LIGHT OFF
// Coop light OFF offset from sunset - GMT
// Soltice twilight end +37 minutes. Default +25
// **** Example: Door closing at +11, plus 2 minutes = 13 minutes.(light turns off 2 minutes after door closes)
int lightOffMinutesGMT = +13;


//--------------------------------------
// DOOR OPENING MODE BST
//--------------------------------------
//
// BST (summer): Choose fixed time OR sunrise + offset. 
// false = Sunrise + offset (BST summer only) Hens Only
// true  = Fixed time  (BST summer only) Rooster Crow Control
bool useFixedOpenTime = true;

// Fixed opening time for BST summer (24-hour clock)
int fixedDoorOpenHour = 6;
int fixedDoorOpenMinute = 30;

//====================================
//  DOOR BST TIME (summer)
//------------------------------------
// Positive offset = AFTER sunrise/sunset
// Negative offset = BEFORE sunrise/sunset.

//  MORNING -Only used when door opening mode above is set to false. (BST summer only)
//  Door opening offset from sunrise - BST
//  Soltice twilight start -45 minutes. Default +20
int sunriseOpenOffsetBST = +20;

//EVENING
// Door closing offset from sunset - BST
// Soltice twilight end +45 minutes. Default +20
// **** See coop light OFF
int sunsetCloseOffsetBST = +28;

//------------------------------------
//  LIGHT BST TIME (Summer)
//------------------------------------
// Positive offset = AFTER sunrise/sunset
// Negative offset = BEFORE sunrise/sunset.

//LIGHT ON
// Coop light ON offset from sunset - BST
// Soltice twilight end +45 minutes. Default +5
int lightOnOffsetBST = +5;

//LIGHT OFF
// Coop light OFF offset from sunset - BST
// Soltice twilight end +45 minutes. Default +22
// **** Example: Door closing at +28, plus 2 minutes = +22.(light turns off 2 minutes after door closes)
int lightOffMinutesBST = +30;

//--------------------------------------
// Door LED indications
//--------------------------------------
// Progressive bedtime flash stages (visual chicken homing). keep stage 1 time shorter than Door closing offset.
const unsigned long BEDTIME_STAGE0_DELAY_MINS = 0;  // Stage 0: Minutes after coop light comes ON
const unsigned long BEDTIME_STAGE1_MINS = 9;        // Stage 1: Minutes before door close.
const unsigned long BEDTIME_STAGE2_MINS = 5;        // Stage 2: Minutes before door close
const unsigned long BEDTIME_STAGE3_MINS = 1;        // stage 3: Minutes before door close

const unsigned long STAGE0_FLASH_MS = 500;   // Stage 0: Slow flash (1000ms = 1 second)
const unsigned long STAGE1_FLASH_MS = 400;   // Stage 1: Medium flash
const unsigned long STAGE2_FLASH_MS = 300;   // Stage 2: Faster flash
const unsigned long STAGE3_FLASH_MS = 200;   // Stage 3: Quick flash

const unsigned long NIGHT_FLASH_MS = 1200;  // Alternating flash. When door is fully closed for the night.

//======================================
// Manual coop light auto OFF timer.
//--------------------------------------
unsigned long manualLightTimeout = 10 * MINUTE;

//--------------------------------------
//  EVENTS
//--------------------------------------
// Number of events stored.
#define MAX_EVENTS 100

//--------------------------------------
// ENVIRONMENT LOG ENTRIES
//--------------------------------------
// Number of persistent days stored.
const int ENVIRONMENT_LOG_DAYS = 30;

//======================================================
//*******USER MAGIC NUMBERS*******
//======================================================

//--------------------------------------
//*** MOTOR *** IMPORTANT SETUP ***
//--------------------------------------

// Normal door open/close timing
#define MOTOR_TIMEOUT (15 * SECOND)  // Adjust for how long the door motor runs between the 2 door limit switches (from fully closed to fully open position) \
                                     // Check Event page, eg "DOOR OPEN 13.8s", add 1-2 seconds. Keep this tight.

// Automatic door obstruction timing
#define MOTOR_TIMEOUT_RECOVERY (18 * SECOND)  // Only triggered when a door obstruction is detected (chicken in the door way). Time difference between MOTOR_TIMEOUT and MOTOR_TIMEOUT_RECOVERY. eg additional 3 seconds. \
                                              // Allows for the MOTOR_TIMEOUT time to finish (full 15 seconds) plus a few seconds for the extra cord to unwind and rewind, eliminating a MOTOR_TIMEOUT fault. \
                                              // Once triggered, check Event page, find the sequence, eg "Close Obstruction", "Auto Safety", "Door Open 15.9s", add 2-4 seconds. \                                              
//---------------------------------------

// Door Obstruction Pause
#define SAFETY_WAIT_AFTER_REOPEN_MS (5 * SECOND)  // Holds the door open for a pause of 5 seconds before a retry of the auto door obstruction close. Allows the chicken to vacate the door way.

// Door Obstruction Retries
const int MAX_OBSTRUCTION_RETRIES = 3;  // Number of attemps to fully close the door before a fault condition occures.

// DHT22 Sensor
#define MAX_CONSECUTIVE_DHT_FAILURES 10        // Log fault after 10 failures
#define DHT_FIRST_READ_DELAY_MS (30 * SECOND)  // On power up, delay before first reading (~30 seconds for sensor settling)
#define DHT_READ_INTERVAL_MS (9 * MINUTE)      // Subsequent reading interval every __ minutes (9 minutes=160 reads in 24hrs)

// WiFi
#define WIFI_RETRY_INTERVAL (90 * SECOND)  // If WiFi fails, retry every __ until connected
#define MAX_WIFI_RETRIES 20                // Stop WiFi retry after this many attempts

// Rotary Encoder
#define ENCODER_DETENTS 3  // Encoder detents for door open/close on the manual page

// BUZZER
unsigned long buzzerRepeatInterval = (15 * SECOND);  // Fault buzzer repeat interval

// DISPLAY
unsigned long oledTimeout = (2 * MINUTE);  // Display sleeps after no activity.

// ENUM DISPLAY PAGES
// Change the order of the display pages

enum DisplayPage {

  PAGE_MAIN,         // Info Only:
  PAGE_ENVIRONMENT,  // Scroll info
  PAGE_MANUAL,       // Interactive:
  PAGE_EVENTS,       // Scroll info:
  PAGE_SYSTEM_TIME,  // Info only
  PAGE_SYSTEM_WIFI   // Info only

};

// First display page
DisplayPage currentPage = PAGE_MAIN;

// Display Pages
#define NUM_DISPLAY_PAGES 6  // number of display pages

// Event Scroll Sensitivity
#define EVENT_SCROLL_TIMEOUT_MS (10 * SECOND)  // Exit event view after 10 sec inactivity

// Watchdog
#define WATCHDOG_TIMEOUT_MS (35 * SECOND)  // Watchdog timeout

// Safety Timeout
#define SAFETY_TIMEOUT (2 * MINUTE)  // adjust for overall fail safe

// ======================================================
// ****END OF USER ADJUSTABLE SETTINGS****
// ======================================================

// ======================================================
// GPIO & connections specific to the ESP32 S3 board
// ======================================================

#define MOTOR_OPEN_PIN 41   // Motor OPEN. Via a L298N or TB6612FNG controller
#define MOTOR_CLOSE_PIN 42  // Motor CLOSE. Via a L298N or TB6612FNG controller

#define LIMIT_OPEN_PIN 15   // Door Limit Switch top, Normally open contacts. Connect to GND
#define LIMIT_CLOSE_PIN 16  // Door Limit Switch bottom, Normally open contacts. Connect to GND

#define LIGHT_PIN 17  // Auto Coop Light. To control a logic level relay board module. 3.3v or 5v coil

#define BUZZER_PIN 3  // 3V Active Buzzer for alert fault. Connect to GND

#define STATUS_LED_PIN 6  // 3mm GREEN LED connect to GND. System healthy. 2k+ resistor.
#define FAULT_LED_PIN 7   // 3mm RED LED connect to GND. Fault warning. 1k resistor.

#define NIGHT_LED1_PIN 11  // 5mm RED LED connect to GND. Installed above the Coop door for chicken homing & visual referance. 100-330R resistor.
#define NIGHT_LED2_PIN 12  // 5mm RED LED connect to GND. Installed above the Coop door for chicken homing & visual referance. 100-330R resistor.

// Rotary Encoder
#define ENCODER_CLK_PIN 4  // KY-040
#define ENCODER_DT_PIN 5   // KY-040
#define ENCODER_SW_PIN 18  // KY-040

// Temp & Humidity
#define DHTPIN 13
#define DHTTYPE DHT22
//#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

//#define pin 14  //SPARE, possible DHT22 external frost stat.

// =======================================================
// ADDITIONAL GPI0 PIN INFO
// =======================================================

// INFO: 1.3" OLED display and RTC DS3231 are both I2C protocol, Use the same connection pins for both.
// Pin 8 = SDA
// Pin 9 = SCK/SCL
// VCC      3.3v or 5V
// GND

//INFO: Rotary Encoder. KY-040
// Pin 4  = CLK
// Pin 5  = DT
// Pin 18 = SW
// VCC    3.3v or 5v
// GND

//INFO: Coop Temperature/Humidity. DHT22
// pin 13 = data
// VCC      3.3v or 5v
// GND

//INFO: Logic Level Relay Board. Switch for coop light
// pin 17 = data
// VCC      3.3v or 5v
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

Preferences preferences;

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
// ENVIRONMENT DATA
// ======================================================

struct ClimateDay {

  char dateString[20];

  float maxTemp;
  float minTemp;

  float maxHum;
  float minHum;

  char maxTempTime[16];
  char minTempTime[16];

  char maxHumTime[16];
  char minHumTime[16];
};

ClimateDay climateLog[ENVIRONMENT_LOG_DAYS];

float currentTemp = NAN;
float currentHumidity = NAN;

float todayMaxTemp = -100;
float todayMinTemp = 100;

float todayMaxHum = -100;
float todayMinHum = 100;

char todayMaxTempTime[16] = "";
char todayMinTempTime[16] = "";

char todayMaxHumTime[16] = "";
char todayMinHumTime[16] = "";

int environmentLogIndex = -2;

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

const unsigned long historyTimeout = EVENT_SCROLL_TIMEOUT_MS;

// ======================================================
// LIGHT
// ======================================================

bool lightOn = false;

bool lightCycleComplete = false;

bool bedtimeWarning = false;
bool nightMode = false;

unsigned long nightLedTimer = 0;
bool nightLedState = false;

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

volatile DoorState doorState = DOOR_STOPPED;

volatile SafetyState safetyState = SAFETY_IDLE;

volatile FaultState faultState = FAULT_NONE;

volatile AutoState autoState = AUTO_IDLE;


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
//const int MAX_OBSTRUCTION_RETRIES = 5;

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
  if (nowMinutes >= openTime && nowMinutes <= closeTime) {
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
  if (rssi >= -50) return "BEST -50 dBm";
  if (rssi >= -62) return "GOOD -66 dBm";
  if (rssi >= -67) return "FAIR -71 dBm";
  if (rssi >= -72) return "OKAY -76 dBm";
  if (rssi >= -77) return "POOR -81 dBm";
  if (rssi >= -82) return "WEAK -86 dBm";
  if (rssi >= -87) return "DIRE -91 dBm";

  return "AWFUL -96 dBm";
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

  const char* wifiText =
    connected ? "WiFi" : "WiFi Lost";

  int wifiWidth =
    u8g2.getStrWidth(wifiText);

  u8g2.drawStr(
    128 - wifiWidth - 1,
    63,
    wifiText);
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
  // ADD BOUNDS CHECK - FIX CIRCULAR BUFFER EDGE CASE
  // =====================================================

  if (viewedEventOffset >= storedEventCount) {
    viewedEventOffset = storedEventCount - 1;
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

void drawEnvironmentPage() {

  if (oledSleeping) return;

  u8g2.clearBuffer();

  // =====================================================
  // ENVIRONMENT PAGE TITLES
  // =====================================================

  u8g2.setFont(u8g2_font_6x13_tf);

  const char* title;

  if (environmentLogIndex == -2) {

    title = "Current Environment";  // page Title
  } else if (environmentLogIndex == -1) {

    title = "Today's Environment";  // Page Title
  } else {

    title = "Environment Log";  // Page Title
  }

  int titleWidth = u8g2.getStrWidth(title);

  u8g2.drawStr((128 - titleWidth) / 2, 12, title);

  u8g2.drawHLine(0, 16, 128);


  // =====================================================
  // CURRENT ENVIRONMENT
  // =====================================================

  if (environmentLogIndex == -2) {

    char buf[32];

    u8g2.setFont(u8g2_font_helvB10_tf);

    snprintf(buf,
             sizeof(buf),
             "Temp: %.1f C",
             currentTemp);

    u8g2.drawStr(2, 35, buf);

    snprintf(buf,
             sizeof(buf),
             "Hum:   %.0f%%",
             currentHumidity);

    u8g2.drawStr(2, 55, buf);

    u8g2.sendBuffer();
  }

  // =====================================================
  // TODAY'S ENVIRONMENT
  // =====================================================

  else if (environmentLogIndex == -1) {

    char buf[32];

    u8g2.setFont(u8g2_font_6x10_tf);

    snprintf(
      buf,
      sizeof(buf),
      "Max T   %.1fC   %s",
      todayMaxTemp,
      todayMaxTempTime);

    u8g2.drawStr(0, 28, buf);

    snprintf(
      buf,
      sizeof(buf),
      "Min T   %.1fC   %s",
      todayMinTemp,
      todayMinTempTime);

    u8g2.drawStr(0, 40, buf);

    snprintf(
      buf,
      sizeof(buf),
      "Max H    %.0f%%    %s",
      todayMaxHum,
      todayMaxHumTime);

    u8g2.drawStr(0, 52, buf);

    snprintf(
      buf,
      sizeof(buf),
      "Min H    %.0f%%    %s",
      todayMinHum,
      todayMinHumTime);

    u8g2.drawStr(0, 64, buf);

    u8g2.sendBuffer();
  }

  // =====================================================
  // ENVIRONMENT LOG
  // =====================================================

  else {

    ClimateDay& d =
      climateLog[environmentLogIndex];

    char buf[32];

    u8g2.setFont(u8g2_font_6x10_tf);

    char dayBuf[16];

    // ====================================
    // DAY NUMBER LEFT
    // ====================================

    snprintf(
      dayBuf,
      sizeof(dayBuf),
      "Day %d/%d",
      environmentLogIndex + 1,
      ENVIRONMENT_LOG_DAYS);

    u8g2.drawStr(0, 28, dayBuf);

    // ====================================
    // DATE RIGHT
    // ====================================

    int dateWidth = u8g2.getStrWidth(d.dateString);

    u8g2.drawStr(128 - dateWidth, 28, d.dateString);


    // ====================================
    // MAX TEMP
    // ====================================

    snprintf(
      buf,
      sizeof(buf),
      "Max T   %.1fC",  // Temp
      d.maxTemp);

    u8g2.drawStr(0, 42, buf);

    snprintf(
      buf,
      sizeof(buf),
      "%s",
      d.maxTempTime);

    u8g2.drawStr(95, 42, buf);  // Time stamp

    // ====================================
    // MIN TEMP
    // ====================================

    snprintf(
      buf,
      sizeof(buf),
      "Min T   %.1fC",  //Temp
      d.minTemp);

    u8g2.drawStr(0, 53, buf);

    snprintf(
      buf,
      sizeof(buf),
      "%s",
      d.minTempTime);

    u8g2.drawStr(95, 53, buf);  // Time stamp

    // ====================================
    // MAX HUMIDITY
    // ====================================

    snprintf(
      buf,
      sizeof(buf),
      "Max H    %.0f%%",  // Humidity
      d.maxHum);

    u8g2.drawStr(0, 64, buf);

    snprintf(
      buf,
      sizeof(buf),
      "%s",
      d.maxHumTime);

    u8g2.drawStr(95, 64, buf);  // Time stamp

    u8g2.sendBuffer();
  }
}
// ======================================================
// SYSTEM TIME PAGE
// ======================================================

void drawSystemTimePage() {

  if (oledSleeping) return;

  u8g2.clearBuffer();

  struct tm timeinfo;

  char timeBuf[16] = "--:--:--";
  bool timeValid = false;

  if (getTime(timeinfo)) {

    timeValid = true;

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

  if (timeValid && isInBST(timeinfo) && useFixedOpenTime) {
    snprintf(
      sunBuf,
      sizeof(sunBuf),
      "Open %d:%02d SS %02d:%02d",
      fixedDoorOpenHour,
      fixedDoorOpenMinute,
      actualSunset / 60,
      actualSunset % 60);
  } else {
    snprintf(
      sunBuf,
      sizeof(sunBuf),
      "SR %02d:%02d  SS %02d:%02d",
      actualSunrise / 60,
      actualSunrise % 60,
      actualSunset / 60,
      actualSunset % 60);
  }

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
  // WIFI QUALITY (CENTERED)
  // =====================================================

  u8g2.setFont(u8g2_font_6x13_tf);

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

  u8g2.setFont(u8g2_font_7x14_tf);

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

  else if (currentPage == PAGE_ENVIRONMENT) {

    drawEnvironmentPage();
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

void updateEnvironment() {

  static unsigned long lastRead = 0;
  static int consecutiveDhtFailures = 0;
  static bool firstReadDone = false;

  // Read after settling delay on first call, then every DHT_READ_INTERVAL_MS
  if (!firstReadDone) {
    if (millis() < DHT_FIRST_READ_DELAY_MS) {
      return;
    }
    firstReadDone = true;
    lastRead = millis();
  } else if (millis() - lastRead < DHT_READ_INTERVAL_MS) {
    return;
  }

  lastRead = millis();

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {

    consecutiveDhtFailures++;

    Serial.print("DHT22 Read Failed (");
    Serial.print(consecutiveDhtFailures);
    Serial.println(" consecutive)");

    // Log fault after MAX_CONSECUTIVE_DHT_FAILURES consecutive failures
    if (consecutiveDhtFailures > MAX_CONSECUTIVE_DHT_FAILURES) {
      setEvent("DHT22 FAULT");
      consecutiveDhtFailures = 0;
    }

    return;
  }

  consecutiveDhtFailures = 0;

  currentTemp = t;
  currentHumidity = h;


  struct tm timeinfo;

  if (!getTime(timeinfo)) {
    return;
  }

  char timeStamp[16];

  snprintf(
    timeStamp,
    sizeof(timeStamp),
    "%02d:%02d",
    timeinfo.tm_hour,
    timeinfo.tm_min);

  if (t > todayMaxTemp) {

    todayMaxTemp = t;

    strncpy(todayMaxTempTime, timeStamp, sizeof(todayMaxTempTime) - 1);
    todayMaxTempTime[sizeof(todayMaxTempTime) - 1] = '\0';

    saveCurrentEnvironmentDay();
  }

  if (t < todayMinTemp) {

    todayMinTemp = t;

    strcpy(
      todayMinTempTime,
      timeStamp);

    saveCurrentEnvironmentDay();
  }

  if (h > todayMaxHum) {

    todayMaxHum = h;

    strcpy(
      todayMaxHumTime,
      timeStamp);

    saveCurrentEnvironmentDay();
  }

  if (h < todayMinHum) {

    todayMinHum = h;

    strcpy(
      todayMinHumTime,
      timeStamp);

    saveCurrentEnvironmentDay();
  }

  displayDirty = true;
}

void rolloverEnvironmentLog() {

  static int loggedDay = -1;

  struct tm timeinfo;

  if (!getTime(timeinfo)) {
    return;
  }

  if (loggedDay == -1) {

    loggedDay = timeinfo.tm_mday;
    return;
  }

  if (timeinfo.tm_mday == loggedDay) {
    return;
  }

  loggedDay = timeinfo.tm_mday;

  // ==========================================
  // SHIFT HISTORY
  // ==========================================

  for (int i = ENVIRONMENT_LOG_DAYS - 1; i > 0; i--) {

    climateLog[i] = climateLog[i - 1];
  }

  // ==========================================
  // STORE YESTERDAY (Calculate previous day)
  // ==========================================

  // Create a time_t for yesterday
  time_t now;
  time(&now);
  time_t yesterday = now - (24 * 60 * 60);  // Subtract 24 hours
  struct tm yesterdayInfo;
  localtime_r(&yesterday, &yesterdayInfo);

  const char* days[] = {
    "Sun", "Mon", "Tue", "Wed",
    "Thu", "Fri", "Sat"
  };

  const char* months[] = {
    "Jan", "Feb", "Mar", "Apr",
    "May", "Jun", "Jul", "Aug",
    "Sep", "Oct", "Nov", "Dec"
  };

  snprintf(
    climateLog[0].dateString,
    sizeof(climateLog[0].dateString),
    "%s %02d %s",
    days[yesterdayInfo.tm_wday],
    yesterdayInfo.tm_mday,  // ← Now uses yesterday's day
    months[yesterdayInfo.tm_mon]);

  climateLog[0].maxTemp = todayMaxTemp;
  climateLog[0].minTemp = todayMinTemp;

  climateLog[0].maxHum = todayMaxHum;
  climateLog[0].minHum = todayMinHum;

  strcpy(
    climateLog[0].maxTempTime,
    todayMaxTempTime);

  strcpy(
    climateLog[0].minTempTime,
    todayMinTempTime);

  strcpy(
    climateLog[0].maxHumTime,
    todayMaxHumTime);

  strcpy(
    climateLog[0].minHumTime,
    todayMinHumTime);

  // ==========================================
  // RESET FOR NEW DAY
  // ==========================================

  todayMaxTemp = -100;
  todayMinTemp = 100;

  todayMaxHum = -100;
  todayMinHum = 100;

  strcpy(todayMaxTempTime, "");
  strcpy(todayMinTempTime, "");

  strcpy(todayMaxHumTime, "");
  strcpy(todayMinHumTime, "");

  preferences.begin("envlog", false);

  preferences.putBytes(
    "history",
    climateLog,
    sizeof(climateLog));

  preferences.end();

  Serial.println("Environment log rolled");
  // Force reload from storage to verify persistence
  preferences.begin("envlog", true);

  ClimateDay tempLog[ENVIRONMENT_LOG_DAYS];
  size_t len = preferences.getBytesLength("history");

  if (len == sizeof(climateLog)) {
    preferences.getBytes("history", tempLog, sizeof(climateLog));
    Serial.println("✓ History verified in storage");
  } else {
    Serial.print("⚠ History size mismatch: ");
    Serial.print(len);
    Serial.print(" vs ");
    Serial.println(sizeof(climateLog));
  }

  preferences.end();
  Serial.print("Stored ");
  Serial.print(ENVIRONMENT_LOG_DAYS);
  Serial.println(" days of history");
}
void saveCurrentEnvironmentDay() {

  preferences.begin("envlog", false);

  preferences.putFloat(
    "todayMaxTemp",
    todayMaxTemp);

  preferences.putFloat(
    "todayMinTemp",
    todayMinTemp);

  preferences.putFloat(
    "todayMaxHum",
    todayMaxHum);

  preferences.putFloat(
    "todayMinHum",
    todayMinHum);

  preferences.putString(
    "maxTempTime",
    todayMaxTempTime);

  preferences.putString(
    "minTempTime",
    todayMinTempTime);

  preferences.putString(
    "maxHumTime",
    todayMaxHumTime);

  preferences.putString(
    "minHumTime",
    todayMinHumTime);

  preferences.end();
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

  if (wifiRetryCount >= MAX_WIFI_RETRIES) {
    setEvent("WiFi Disabled");
    WiFi.mode(WIFI_OFF);  // Save power
    wifiRetryCount = 0;
    return;  // Don't retry this session
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  wifiRetryCount++;
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
// TIMEZONE DETECTION
// ======================================================

bool isInBST(struct tm& timeinfo) {
  // BST runs when daylight saving time is active
  return (timeinfo.tm_isdst > 0);
}

int getSeasonalOffset(int offsetBST, int offsetGMT, struct tm& timeinfo) {
  return isInBST(timeinfo) ? offsetBST : offsetGMT;
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

  // Get seasonal offsets based on BST/GMT
  int seasonalSunsetOffset = getSeasonalOffset(sunsetCloseOffsetBST, sunsetCloseOffsetGMT, timeinfo);

  //======================================================
  // Door opening: winter always sunrise; summer fixed or sunrise
  //======================================================

  if (isInBST(timeinfo)) {
    if (useFixedOpenTime) {
      openTime = (fixedDoorOpenHour * 60) + fixedDoorOpenMinute;
    } else {
      openTime = actualSunrise + sunriseOpenOffsetBST;
    }
  } else {
    openTime = actualSunrise + sunriseOpenOffsetGMT;
  }
  closeTime = actualSunset + seasonalSunsetOffset;

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

  // =====================================================
  // RESET MANUAL OVERRIDE AT MIDNIGHT (NEW DAY)
  // =====================================================
  static int lastDay = -1;
  struct tm timeinfo;
  if (getTime(timeinfo)) {
    if (timeinfo.tm_mday != lastDay) {
      lastDay = timeinfo.tm_mday;
      manualLightOverride = false;
      manualLightOffTime = 0;
    }
  }

  // =====================================================
  // MANUAL OVERRIDE ACTIVE?
  // =====================================================
  if (manualLightOverride) {

    // Only auto-reset if manual ON timeout expired (10 minutes)
    if (manualLightOffTime > 0 && millis() >= manualLightOffTime) {
      manualLightOverride = false;
      manualLightOffTime = 0;
      // Resume auto control (fall through to auto logic below)
    } else {
      // Manual override still active, stay locked
      return;
    }
  }

  // =====================================================
  // AUTO LIGHT CONTROL (ONLY IF NO ACTIVE OVERRIDE)
  // =====================================================

  struct tm timeinfo2;
  if (!getTime(timeinfo2)) {
    return;
  }

  int seasonalLightOnOffset = getSeasonalOffset(lightOnOffsetBST, lightOnOffsetGMT, timeinfo2);
  int seasonalLightOffMinutes = getSeasonalOffset(lightOffMinutesBST, lightOffMinutesGMT, timeinfo2);

  int onTime = actualSunset + seasonalLightOnOffset;
  int offTime = actualSunset + seasonalLightOffMinutes;

  // TURN ON
  if (!lightOn && !lightCycleComplete && nowMinutes >= onTime && nowMinutes < offTime) {
    digitalWrite(LIGHT_PIN, HIGH);

    lightOn = true;

    bedtimeWarning = true;
    nightMode = false;

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

    if (!manualMoveActive && manualEncoderAccum >= ENCODER_DETENTS) {

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

    if (!manualMoveActive && manualEncoderAccum <= -ENCODER_DETENTS) {

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

        nightMode = false;

        bedtimeWarning = false;

        char buf[32];

        snprintf(
          buf,
          sizeof(buf),
          "Door Open %.1fs",
          lastOpenTravelTime);

        setEvent(buf);
      }

      else if (millis() - motorStartTime > (safetyState == SAFETY_OBSTRUCTION ? MOTOR_TIMEOUT_RECOVERY : MOTOR_TIMEOUT)) {

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

        if (bedtimeWarning) {

          nightMode = true;
        }

        char buf[32];

        snprintf(
          buf,
          sizeof(buf),
          "Door Close %.1fs",
          lastCloseTravelTime);

        setEvent(buf);

        //setEvent("Night Mode");

        displayDirty = true;
      }

      // ==========================================
      // CLOSE TIMEOUT
      // POSSIBLE CHICKEN OBSTRUCTION
      // ==========================================
      else if (millis() - motorStartTime > MOTOR_TIMEOUT) {
        stopMotor();
        obstructionRetries++;
        setEvent("Close Obstruction");

        if (obstructionRetries >= MAX_OBSTRUCTION_RETRIES) {
          safetyState = SAFETY_IDLE;
          triggerFault("Door blocked");
        } else {

          safetyStartTime = millis();
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

    case SAFETY_IDLE:
      break;

    case SAFETY_OBSTRUCTION:
      // WAIT UNTIL FULLY OPEN
      if (doorState == DOOR_OPEN) {
        safetyOpenReachedTime = millis();
        safetyState = SAFETY_WAITING;
      }
      break;

    case SAFETY_WAITING:
      // WAIT before retry close
      if (millis() - safetyOpenReachedTime > SAFETY_WAIT_AFTER_REOPEN_MS) {
        startClosing();
        safetyState = SAFETY_IDLE;
      }
      break;
  }

  // ==========================================
  // GLOBAL SAFETY TIMEOUT
  // ==========================================
  // Only enforce timeout during active recovery, not waiting phase
  if (safetyState == SAFETY_OBSTRUCTION) {
    if (millis() - safetyStartTime > SAFETY_TIMEOUT) {
      safetyState = SAFETY_IDLE;
      triggerFault("Safety timeout - obstruction unresolved");
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
    .timeout_ms = WATCHDOG_TIMEOUT_MS,
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
    .trigger_panic = true
  };

  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);


  esp_reset_reason_t resetReason = esp_reset_reason();

  Wire.begin();

  u8g2.begin();

  dht.begin();

  preferences.begin("envlog", true);

  size_t len =
    preferences.getBytesLength("history");

  if (len == sizeof(climateLog)) {

    preferences.getBytes(
      "history",
      climateLog,
      sizeof(climateLog));

    // Debug: Verify loaded data
    int validDays = 0;
    for (int i = 0; i < ENVIRONMENT_LOG_DAYS; i++) {
      if (strlen(climateLog[i].dateString) > 0) {
        validDays++;
      }
    }
    Serial.print("Loaded ");
    Serial.print(validDays);
    Serial.println(" days from storage");
  }

  preferences.end();

  preferences.begin("envlog", true);

  todayMaxTemp =
    preferences.getFloat(
      "todayMaxTemp",
      -100);

  todayMinTemp =
    preferences.getFloat(
      "todayMinTemp",
      100);

  todayMaxHum =
    preferences.getFloat(
      "todayMaxHum",
      -100);

  todayMinHum =
    preferences.getFloat(
      "todayMinHum",
      100);

  String s;

  s = preferences.getString(
    "maxTempTime",
    "");

  strncpy(
    todayMaxTempTime,
    s.c_str(),
    sizeof(todayMaxTempTime));

  s = preferences.getString(
    "minTempTime",
    "");

  strncpy(
    todayMinTempTime,
    s.c_str(),
    sizeof(todayMinTempTime));

  s = preferences.getString(
    "maxHumTime",
    "");

  strncpy(
    todayMaxHumTime,
    s.c_str(),
    sizeof(todayMaxHumTime));

  s = preferences.getString(
    "minHumTime",
    "");

  strncpy(
    todayMinHumTime,
    s.c_str(),
    sizeof(todayMinHumTime));

  preferences.end();

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

  pinMode(NIGHT_LED1_PIN, OUTPUT);
  pinMode(NIGHT_LED2_PIN, OUTPUT);

  digitalWrite(NIGHT_LED1_PIN, LOW);
  digitalWrite(NIGHT_LED2_PIN, LOW);
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

  bool buttonDown = rotaryEncoder.isEncoderButtonDown();

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

      displayedEventNumber = storedEventCount - viewedEventOffset;
      viewingHistory = true;
      historyViewStart = millis();
      lastOledActivity = millis();
      displayDirty = true;
      drawEventsPage();
    }

    lastEncoderValue = encoderValue;
  }

  // =====================================================
  // ENVIRONMENT PAGE SCROLLING
  // =====================================================

  if (currentPage == PAGE_ENVIRONMENT) {

    int encoderValue = rotaryEncoder.readEncoder();

    if (encoderValue != lastEncoderValue) {

      int delta = encoderValue - lastEncoderValue;

      // CLOCKWISE = OLDER LOGS
      if (delta > 0) {

        environmentLogIndex++;

        if (environmentLogIndex > ENVIRONMENT_LOG_DAYS - 1) {
          environmentLogIndex = -1;
        }
      }

      // COUNTERCLOCKWISE = TOWARD CURRENT
      if (delta < 0) {

        environmentLogIndex--;

        if (environmentLogIndex < -(ENVIRONMENT_LOG_DAYS - 1)) {
          environmentLogIndex = -(ENVIRONMENT_LOG_DAYS - 1);
        }
      }

      lastOledActivity = millis();
      displayDirty = true;
      drawEnvironmentPage();
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

    unsigned long holdTime = millis() - buttonDownTime;

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

    unsigned long pressTime = millis() - buttonDownTime;

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
      currentPage = (DisplayPage)((currentPage + 1) % NUM_DISPLAY_PAGES);

      viewedEventOffset = 0;
      environmentLogIndex = -2;  //which page to show first,-0=log,-1=today,-2=current

      rotaryEncoder.setEncoderValue(0);
      lastEncoderValue = 0;

      viewingHistory = false;

      lastOledActivity = millis();
      displayDirty = true;
      refreshDisplayIfNeeded();
    }

    // =====================================================
    // MANUAL PAGE LIGHT TOGGLE (Hold button = Light)
    // =====================================================

    else if (currentPage == PAGE_MANUAL && pressTime >= 700 && pressTime < 2000) {

      // Toggle light state
      if (lightOn) {
        // Light is ON, turn it OFF (permanently)
        lightOn = false;
        digitalWrite(LIGHT_PIN, LOW);
        manualLightOverride = true;  // Lock out auto control
        manualLightOffTime = 0;      // No timeout - stays off permanently
        setEvent("Manual Light OFF");
      } else {
        // Light is OFF, turn it ON (for 10 minutes)
        lightOn = true;
        digitalWrite(LIGHT_PIN, HIGH);
        manualLightOverride = true;
        manualLightOffTime = millis() + manualLightTimeout;  // Auto-off after 10 min
        setEvent("Manual Light ON");
      }

      lastOledActivity = millis();
      displayDirty = true;
      refreshDisplayIfNeeded();
      buttonWasDown = buttonDown;
      return;
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
//======================================
// Night Door LEDs
//======================================
void updateNightLeds() {
  static unsigned long lastFlash = 0;
  static unsigned long lightOnTime = 0;  // Track when light first came on

  unsigned long flashInterval = NIGHT_FLASH_MS;

  // =====================================================
  // NIGHT MODE - Door is closed for the night
  // =====================================================
  if (nightMode) {
    flashInterval = NIGHT_FLASH_MS;
  }
  // =====================================================
  // BEDTIME WARNING - Progressive 4 stages
  // =====================================================
  else if (bedtimeWarning) {

    struct tm timeinfo;
    if (getTime(timeinfo)) {
      int nowMinutes = (timeinfo.tm_hour * 60) + timeinfo.tm_min;
      int minutesUntilClose = closeTime - nowMinutes;

      // Track when light first came on (for Stage 0 delay)
      if (lightOnTime == 0) {
        lightOnTime = millis();
      }

      unsigned long millisSinceLightOn = millis() - lightOnTime;
      unsigned long minutesSinceLightOn = millisSinceLightOn / 60000;  // Convert to minutes

      // Stage 3:  Quick flash, (continues until door closes)
      if (minutesUntilClose <= BEDTIME_STAGE3_MINS) {
        flashInterval = STAGE3_FLASH_MS;
      }
      // Stage 2: 1-5 minutes until close (Faster flash)
      else if (minutesUntilClose <= BEDTIME_STAGE2_MINS && minutesUntilClose > BEDTIME_STAGE3_MINS) {
        flashInterval = STAGE2_FLASH_MS;
      }
      // Stage 1: 5-9 minutes until close (Medium flash)
      else if (minutesUntilClose <= BEDTIME_STAGE1_MINS && minutesUntilClose > BEDTIME_STAGE2_MINS) {
        flashInterval = STAGE1_FLASH_MS;
      }
      // Stage 0: After light-on delay (Slow flash)
      else if (minutesSinceLightOn >= BEDTIME_STAGE0_DELAY_MINS) {
        flashInterval = STAGE0_FLASH_MS;
      }

      // =====================================================
      // DOOR CLOSING - Keep Stage 3 flash going
      // =====================================================
      else if (!bedtimeWarning && !nightMode) {
        flashInterval = STAGE3_FLASH_MS;
      }

      // Light just came on, before delay
      else {
        digitalWrite(NIGHT_LED1_PIN, LOW);
        digitalWrite(NIGHT_LED2_PIN, LOW);
        return;
      }
    }

  } else {
    // bedtimeWarning is OFF, reset the light timer
    lightOnTime = 0;
    digitalWrite(NIGHT_LED1_PIN, LOW);
    digitalWrite(NIGHT_LED2_PIN, LOW);
    return;
  }

  // =====================================================
  // FLASH TIMING
  // =====================================================
  if (millis() - lastFlash < flashInterval)
    return;

  lastFlash = millis();

  // Night Mode has priority
  if (nightMode) {
    nightLedState = !nightLedState;
    digitalWrite(NIGHT_LED1_PIN, nightLedState);
    digitalWrite(NIGHT_LED2_PIN, !nightLedState);
    return;
  }

  // Bedtime warning - all 4 stages
  if (bedtimeWarning) {
    nightLedState = !nightLedState;
    digitalWrite(NIGHT_LED1_PIN, nightLedState);
    digitalWrite(NIGHT_LED2_PIN, nightLedState);
    return;
  }
}

// ======================================================
// LOOP
// ======================================================

void loop() {

  esp_task_wdt_reset();

  updateEnvironment();

  rolloverEnvironmentLog();

  handleSwitch();

  handleEventScroll();

  updateDoor();

  updateSafetyCycle();

  motorSafetyCheck();

  updateBuzzerPattern();

  updateStatusLEDs();

  updateNightLeds();

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

    // Save environment log to persistent storage daily
    static int lastSavedDay = -1;

    if (timeinfo.tm_mday != lastSavedDay) {
      lastSavedDay = timeinfo.tm_mday;

      preferences.begin("envlog", false);
      preferences.putBytes("history", climateLog, sizeof(climateLog));
      preferences.end();

      Serial.println("Daily history backup saved");
    }

    int nowMinutes =
      (timeinfo.tm_hour * 60) + timeinfo.tm_min;

    autoState = computeAutoState(nowMinutes);

    //===========================================
    // Door Auto State
    //===========================================

    static AutoState lastAutoState = AUTO_IDLE;

    if (autoState != lastAutoState) {

      switch (autoState) {

        case AUTO_IDLE:
          setEvent("Auto_Idle");  //AUTO_IDLE
          break;

        case AUTO_ALLOWED:
          setEvent("Auto_Allowed");  //AUTO_ALLOWED
          break;

        case AUTO_LOCKED_MANUAL:
          setEvent("Auto_Locked");  //AUTO_LOCKED// Auto_Disabled
          break;

        case AUTO_SAFETY_RECOVERY:
          setEvent("Auto_Safety");  //AUTO_SAFETY
          break;
      }

      lastAutoState = autoState;
    }

    updateLight(nowMinutes);

    if (autoState == AUTO_ALLOWED) {

      if (doorState == DOOR_CLOSED && nowMinutes >= openTime && nowMinutes < closeTime) {

        setEvent("Auto Open");  //AUTO OPEN
        startOpening();
      }

      if (doorState != DOOR_CLOSED && nowMinutes >= closeTime) {

        setEvent("Auto Close");  //AUTO CLOSE
        startClosing();
      }
    }
  }
}

