// System: ESP32S3 N16R8 board.
// Arduino IDE: ESP32S3 Dev Module

// Author HIGI-Styx
// Febuary-September 2026


//  https://github.com/retirement2021/HIGI_Hen_HOUSE for Robust versions, wiring diagrams, photos, 3D printing files, and a complete parts list.

// V2.13
// Remove the current time and 1 second refresh on system time page and replace Cold threshhold temp. 
// Change the title of the page to System.

// V2.12
// Amalgamated inside and outside environmental pages into one, for convenient viewing.

// V2.10
// removed the clear persistent environment logs on the next upload as it was the culprit to NVS logging fails.

// 2.04
// Rewrote the Environment Outside pages inline with the Environment Inside pages for consistency.
// Futher refinement to the cold delay logic to poll the external temperature sensor immediately before the door opens, then reset the polling timer.

// V2.02
// Refined cold delay logic to evaluate temperature immediately prior to door opening, logging a "Cold Delay" event when triggered.
// Added a option to clear internal and external persistent environment logs on the next upload.

// V2.01
// Added support for an SHT30 outdoor temperature probe at address 0x44, complete with dedicated display pages and logging.
// Replaced the Adafruit_SHT31 library with SensirionI2CSHT3x to support dual SHT3x sensors on separate I2C addresses.
// Updated the I2C address for the SHT31 coop sensor to 0x45.
// Implemented a cold weather morning door delay when outdoor temperatures fall below a configurable threshold.
// Added a 24/7 Cold (blue) LED indicator that illuminates when ambient temperature drops below the configured setpoint.

// V2.0
// Upgraded from a DHT22 coop temp sensor to a SHT31 sensor.


//------------------------

// This project has evolved to enable long-term autonomous operation with minimal user intervention.

// An automated coop management system that synchronizes door operation and lighting with natural sunrise/sunset times,
// Continuously tracks environmental metrics, and integrates robust safety protocols to protect the flock and hardware.

// Tailored for UK time zones and DST shifts.
// Only requires a minimal Wi-Fi signal to perform NTP time synchronization.
// All customizable settings are centralized in one block under two headers: *USER SETTINGS* and *USER MAGIC NUMBERS*.
// Integrated motor timeout protection to prevent motor burnout during mechanical door failures.
// Obstruction Protection: Smart door-reversal logic to release a trapped chicken during closure.
// Coop door and light operations sync with local sunrise/sunset times and feature fully customizable settings to accommodate your flock's routine.
// Rooster Crow Control: Optional fixed-time door opening schedule to delay early morning crowing.
// System operation uses a rotary encoder. Perform a long press (≥5 seconds) to reset system faults.



//======================================================
// LIBRARYS
//======================================================
#include <WiFi.h>
#include <time.h>
#include <math.h>
#include <Wire.h>
#include <RTClib.h>
#include "secrets.h"
#include <U8g2lib.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <SensirionI2cSht3x.h>
#include <AiEsp32RotaryEncoder.h>

// ======================================================
// TIME VALUES
// ======================================================
// Converts milliseconds into readable; seconds, minutes and hours.

const unsigned long SECOND = 1000UL;
const unsigned long MINUTE = 60000UL;
const unsigned long HOUR = 3600000UL;

// ******************************************************
// ********USER SETTINGS*********
// ******************************************************

//--------------------------------------
//  SYSTEM
//--------------------------------------
// Displayed firmware version
const char systemVersion[] = "V2.12";

//"""""""""""
// WINTER
//"""""""""""

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// DOOR OPENING OPTION (GMT only)
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// GMT (Winter only):
//   Choose sunrise OR fixed opening time
// * false = Sunrise     (GMT Winter only)
//   true  = Fixed time  (GMT Winter only) ~Rooster Crow Control
bool useFixedOpenTimeGMT = true;

// Fixed opening time for GMT Winter (24-hour clock)
int fixedDoorOpenHourGMT = 8;     // GMT fixed open hour (24-hour)
int fixedDoorOpenMinuteGMT = 00;  // GMT fixed open minute

//-------------------------------------
//  DOOR GMT TIME. (winter)
//-------------------------------------
// Positive offset = AFTER  sunrise/sunset
// Negative offset = BEFORE sunrise/sunset.

//MORNING.
// * Active only when "DOOR OPENING OPTION" is set to "false". (GMT Winter only)
// Door opening offset from sunrise - GMT
int sunriseOpenOffsetGMT = +20;  // __ minutes after sunrise

//EVENING
// Door closing offset from sunset - GMT
int sunsetCloseOffsetGMT = +15;  // __ minutes after sunset

//--------------------------------------
//  LIGHT GMT TIME. (winter)
//--------------------------------------
// Positive offset = AFTER  sunrise/sunset
// Negative offset = BEFORE sunrise/sunset.

//LIGHT ON
// Coop light ON offset from sunset - GMT
int lightOnOffsetGMT = -13;  // __ minutes before sunset

//LIGHT OFF
// Coop light OFF offset from sunset - GMT
int lightOffMinutesGMT = +7;  // __ minutes after sunset

//-------------------------------------------------------
//  OUTSIDE SHT30 TEMPERATURE SENSOR + COLD-OPEN SETTINGS
//-------------------------------------------------------

bool enableOutsideSHT = true;  // Set to "false" if no outdoor temperature sensor is installed.

float outsideTempTriggerC = 13 ;       // Delays scheduled door opening when outside temp is under ___ °C (also triggers 24/7 Cold LED).
int doorOpenDelayIfColdMinutes = +10;  // Minutes to delay door opening time.

bool forceOpenDespiteCold = false;  // set true to ignore door cold-delay (persistent)


//""""""""""""
// SUMMER
//""""""""""""

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// DOOR OPENING OPTION (BST only)
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// BST (summer only):
//   Choose sunrise OR fixed opening time
// * false = Sunrise     (BST summer only)
//   true  = Fixed time  (BST summer only) ~Rooster Crow Control
bool useFixedOpenTime = true;

// Fixed opening time for BST summer (24-hour clock)
int fixedDoorOpenHour = 7;
int fixedDoorOpenMinute = 55;

//------------------------------------
//  DOOR BST TIME (summer)
//------------------------------------
// Positive offset = AFTER  sunrise/sunset
// Negative offset = BEFORE sunrise/sunset.

//  MORNING -
//  * Active only when "DOOR OPENING OPTION" is set to "false". (BST summer only)
//  Door opening offset from sunrise - BST
int sunriseOpenOffsetBST = +30;

//EVENING
// Door closing offset from sunset - BST
int sunsetCloseOffsetBST = +20;

//------------------------------------
//  LIGHT BST TIME (Summer)
//------------------------------------
// Positive offset = AFTER  sunrise/sunset
// Negative offset = BEFORE sunrise/sunset.

//LIGHT ON
// Coop light ON offset from sunset - BST
int lightOnOffsetBST = -7;

//LIGHT OFF
// Coop light OFF offset from sunset - BST
int lightOffMinutesBST = +13;

//--------------------------------------
// Coop Door LED indications
//--------------------------------------
// Visual Chicken Homing
// Progressive bedtime flash stages.

const unsigned long BEDTIME_STAGE0_DELAY_MINS = 0;  // Stage 0: Minutes after coop light comes ON. default 0
const unsigned long BEDTIME_STAGE1_MINS = 12;       // Stage 1: Minutes before door close. default 12
const unsigned long BEDTIME_STAGE2_MINS = 6;        // Stage 2: Minutes before door close. default 6
const unsigned long BEDTIME_STAGE3_MINS = 1;        // stage 3: Minutes before door close. default 1

// Progressive bedtime flash speed. (1000  milli-seconds = 1 second)

const unsigned long STAGE0_FLASH_MS = 1000;  // Stage 0: Slower flash. default 1000. Bug Note 1: Must end in *00 to avoid a known software issue.
const unsigned long STAGE1_FLASH_MS = 700;   // Stage 1: Medium flash. default 700.  Bug Note 1.
const unsigned long STAGE2_FLASH_MS = 400;   // Stage 2: Faster flash. default 400.  Bug Note 1.
const unsigned long STAGE3_FLASH_MS = 200;   // Stage 3: Quick  flash. default 200.  Bug note 1.


// Visual Door Closed.
// Night Alternating Flash will only start once the lower limit switch detects that the door is fully closed.

const unsigned long NIGHT_FLASH_MS = 1200;  //  Flashes continuously overnight until the morning door opening sequence triggers.. Default 1200. Bug Note 1.

//======================================
// Manual coop light auto OFF timer.
//--------------------------------------
unsigned long manualLightTimeout = 10 * MINUTE;  // default 10 minutes


//******************************************************
//*******USER MAGIC NUMBERS*******
//******************************************************

//--------------------------------------
//*** MOTOR IMPORTANT SETUP ***
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// Normal door open/close timing
#define MOTOR_TIMEOUT (15 * SECOND)  // Motor timeout protection, protects the motor from burn out if the door fails in any way.
//                                      Adjust for how long the door motor runs between the lower & upper door limit switches (from fully closed to fully open position) \
//                                      Check Event page, eg "DOOR OPEN 14.1s", add 1-2 seconds. Keep this tight.

// Automatic door obstruction timing. Chicken release logic
#define MOTOR_TIMEOUT_RECOVERY (18 * SECOND)  // Only triggered when a door obstruction is detected (chicken blocking the door way).Time difference between MOTOR_TIMEOUT and MOTOR_TIMEOUT_RECOVERY. eg additional 3 seconds. \
//                                               Allows for the "MOTOR_TIMEOUT" time to finish (full 15 seconds) plus a few seconds for the extra cord to unwind and rewind, eliminating a MOTOR_TIMEOUT fault. \
//                                               Once triggered, check Event page, find the sequence, eg "Close Obstruction", "Auto Safety", "Door Open 15.9s", add 2-4 seconds. \                                              
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


// Door Obstruction Pause
#define SAFETY_WAIT_AFTER_REOPEN_MS (180 * SECOND)  // When the system registers a door obstruction, it re-opens the door (to release the hen)
//                                                   This function holds the door open for a pause of __ seconds before a retry of the door obstruction CLOSE.
//                                                   Allowing the released chicken time to re-enter the coop.

// Door Obstruction Retries
const int MAX_OBSTRUCTION_RETRIES = 3;  // Number of attemps to fully close the door before a fault condition occures.

//-------------------------------------

// SHT31 Inside temperature/humidity Sensor
#define MAX_CONSECUTIVE_SHT_FAILURES 10        // Log fault after 10 consecutive failures
#define SHT_FIRST_READ_DELAY_MS (60 * SECOND)  // Delay before first reading ( __ seconds for sensor settling) Default 60 seconds
#define SHT_READ_INTERVAL_MS (13 * MINUTE)     // Subsequent reading interval every __ minutes. (reads in 24hrs: 9-minutes=160, 13-minutes=110, 17-minutes=85)

// SHT30 Outside temp/humidity sensor. (Offset from the SHT31's schedule to help prevent I2C bus congestion)
#define MAX_CONSECUTIVE_SHT_OUT_FAILURES 10            // Log fault after 10 consecutive failures
#define OUTSIDE_SHT_FIRST_READ_DELAY_MS (75 * SECOND)  // Delay before first reading ( __ seconds for sensor settling) Default 75 seconds
#define OUTSIDE_SHT_READ_INTERVAL_MS (9 * MINUTE)      // Subsequent reading interval every __ minutes. (reads in 24hrs: 9-minutes=160, 13-minutes=110, 17-minutes=85)

// SHT3x temperature sensor addresses
const uint8_t INSIDE_SHT_ADDRESS = 0x45;   // SHT31 (Link VIN to address pin for address 0x45)
const uint8_t OUTSIDE_SHT_ADDRESS = 0x44;  // SHT30 probe style,fixed address

// Evironment persistent Log Entries
const int ENVIRONMENT_LOG_DAYS = 92;          // Inside env log. Default 92 days
const int ENVIRONMENT_LOG_DAYS_OUTSIDE = 92;  // Outside env log. Default 92 days

// Events
#define MAX_EVENTS 200  // Number of events stored. default 200

// WiFi
#define WIFI_RETRY_INTERVAL (90 * SECOND)  // If WiFi fails, retry every __ until connected. Default 90 seconds
#define MAX_WIFI_RETRIES 20                // Stop WiFi retry after this many attempts

// Rotary Encoder
#define ENCODER_DETENTS 3  // Detent threshold for manual door movement (prevents accidental triggers). Default 3

// BUZZER
unsigned long buzzerRepeatInterval = (15 * SECOND);  // Fault buzzer repeat interval. Default 15 seconds

// Display
unsigned long oledTimeout = (1 * MINUTE);  // Display sleeps after no activity. Default 1 minute

// Display Pages
// Change the order of the display pages

enum DisplayPage {

  PAGE_MAIN,         // Basic info
  PAGE_SCHEDULE,     // Today's door & light times. info only
  PAGE_ENVIRONMENT,  // Coop temp & humidity. Scroll info
  PAGE_MANUAL,       // Manual door open/close, light on/off. Interactive
  PAGE_EVENTS,       // Date stamped events. Scroll info
  PAGE_SYSTEM_WIFI,  // IP and signal strength. Info only.
  PAGE_SYSTEM_TIME   // Cold delay trigger temp, SunRise or fixed door open and SunSet time. Info only.
};

// Display Pages
#define NUM_DISPLAY_PAGES 7  // number of display pages.

// Always first display page
DisplayPage currentPage = PAGE_MAIN;

// Event Scroll Sensitivity
#define EVENT_SCROLL_TIMEOUT_MS (10 * SECOND)  // Exit event view after 10 sec inactivity

// Watchdog
#define WATCHDOG_TIMEOUT_MS (35 * SECOND)  // Watchdog timeout

// Safety Timeout
#define SAFETY_TIMEOUT (2 * MINUTE)  // adjust for overall fail safe


// ******************************************************
// ****END OF USER SETTINGS****
// ******************************************************

// ======================================================
// GPIO & connections specific to the ESP32 S3 board
// ======================================================

#define MOTOR_OPEN_PIN 41   // Motor OPEN door. Via a L298N or TB6612FNG controller
#define MOTOR_CLOSE_PIN 42  // Motor CLOSE door. Via a L298N or TB6612FNG controller

#define LIMIT_OPEN_PIN 15   // Door Limit Switch top, Reed switch, Normally open contacts. Connect to GND. (magnet to close circuit)
#define LIMIT_CLOSE_PIN 16  // Door Limit Switch bottom, Reed switch, Normally open contacts. Connect to GND. (magnet to close circuit)

#define LIGHT_PIN 17  // Auto Coop Light. To control a logic level relay board module. 3.3v or 5v coil

#define BUZZER_PIN 3  // 3V Active Buzzer for alert fault. Connect to GND

#define STATUS_LED_PIN 6  // 3mm GREEN LED connect to GND. System healthy. 2k+ resistor. High resistor to keep LED dim
#define FAULT_LED_PIN 7   // 3mm RED LED connect to GND. Fault warning. 1k resistor.

#define NIGHT_LED1_PIN 11  // 5mm RED LED connect to GND. Installed above the Coop door for chicken homing & visual referance. 100-330R resistor. (forward voltage 1.6v-2.1v)
#define NIGHT_LED2_PIN 12  // 5mm RED LED connect to GND. Installed above the Coop door for chicken homing & visual referance. 100-330R resistor.

#define COLD_LED_PIN 13  // 5mm Blue LED connect to GND. (outside cold indicator) Installed above the Coop door visual referance. 5R-10R resistor.(forward voltage 3v-3.6v)

// Rotary Encoder
#define ENCODER_CLK_PIN 4  // KY-040
#define ENCODER_DT_PIN 5   // KY-040
#define ENCODER_SW_PIN 18  // KY-040

// Temp & Humidity
SensirionI2cSht3x sht3xInside;   // Inside SHT31 - 0x45
SensirionI2cSht3x sht3xOutside;  // Outside SHT30 - 0x44

// =======================================================
// ADDITIONAL GPI0 PIN INFO
// =======================================================

// INFO: 1.3" OLED display, RTC DS3231
//       SHT31 coop temperature sensor & SHT30 outside temp sensor
//       All are I2C protocol, Use the same connection pins for all four devices.
// Pin     8 = SDA
// Pin     9 = SCK/SCL
// VCC/VIN 3.3v
// GND

//INFO: Rotary Encoder. KY-040
// Pin   4  = CLK
// Pin   5  = DT
// Pin   18 = SW
// VIN/+ 3.3v
// GND

//INFO: Logic Level Relay Board. (Switch for coop light)
// pin 17 = data
// DC+ 3.3v or 5v depending relay coil voltage
// GND

// INFO: ESP32 board RESET. Momentary push button. Recommeded in the final build.
// Pin   RST
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
  AUTO_LOCKED_MANUAL,   // user has taken override control
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

// Outside history uses its own configured size
ClimateDay climateLogOutside[ENVIRONMENT_LOG_DAYS_OUTSIDE];

float currentTempOutside = NAN;
float currentHumidityOutside = NAN;

float todayMaxTempOutside = -100;
float todayMinTempOutside = 100;

char todayMaxTempTimeOutside[16] = "";
char todayMinTempTimeOutside[16] = "";

int environmentLogIndexOutside = -2;  // used by the outside environment page
int environmentViewPos = 0;           // -2 = NVS IN, -1 = TODAY IN, 0 = NOW, 1 = TODAY OUT, 2

// Outside humidity (today)
float todayMaxHumOutside = -100;
float todayMinHumOutside = 100;

char todayMaxHumTimeOutside[16] = "";
char todayMinHumTimeOutside[16] = "";

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
unsigned long lastOutsideReadMillis = 0;  // last successful outside SHT30 read (used to reset poll interval)

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

int openRetryCount = 0;
int closeRetryCount = 0;

int obstructionRetries = 0;

bool systemLockout = false;


// ======================================================
// OVERRIDES
// ======================================================

bool manualMoveActive = false;

bool dailyResetDone = false;

bool manualOpenedDoor = false;
bool manualClosedDoor = false;

int manualOpenDay = -1;
int manualCloseDay = -1;

// Cold-delay helpers
int delayedOpenTime = -1;
int coldDelayDay = -1;
int coldDelayCheckedDay = -1;
bool coldDelayLogged = false;  // prevent repeated log messages

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
  if (rssi >= -87) return "DIRE -91 dBm";  //enough for system requirements.

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

  // Choose title based on current environment view
  const char* title;
  if (environmentViewPos == 0) {
    title = "Environment Now";
  } else if (environmentViewPos == -1) {
    title = "Today's Internal";
  } else if (environmentViewPos == 1) {
    title = "Today's External";
  } else if (environmentViewPos == -2) {
    title = "Internal Log";
  } else { // environmentViewPos == 2
    title = "External Log";
  }

  // TITLE
  u8g2.setFont(u8g2_font_6x13_tf);
  int titleWidth = u8g2.getStrWidth(title);
  u8g2.drawStr((128 - titleWidth) / 2, 12, title);
  u8g2.drawHLine(0, 16, 128);

  // ===== NOW view (center) =====
  if (environmentViewPos == 0) {

    // Two-column stacked layout: labels in a left column, temps in a right column
    u8g2.setFont(u8g2_font_6x10_tf);

    const int labelX = 8;     // left column for labels
    const int tempX  = 72;    // right column for temps
    const int yTop   = 34;    // IN row Y
    const int yBot   = 50;    // OUT row Y

    // IN row
    const char* inLabel = " Temp INT";
    char inTempBuf[24];
    if (!isnan(currentTemp)) {
      snprintf(inTempBuf, sizeof(inTempBuf), "%.1f C", currentTemp);
    } else {
      snprintf(inTempBuf, sizeof(inTempBuf), "  --.- C");
    }
    u8g2.drawStr(labelX, yTop, inLabel);
    u8g2.drawStr(tempX,  yTop, inTempBuf);

    // OUT row
    const char* outLabel = " Temp EXT";
    char outTempBuf[24];
    if (enableOutsideSHT && !isnan(currentTempOutside)) {
      snprintf(outTempBuf, sizeof(outTempBuf), "%.1f C", currentTempOutside);
    } else {
      snprintf(outTempBuf, sizeof(outTempBuf), " --.- C");
    }
    u8g2.drawStr(labelX, yBot, outLabel);
    u8g2.drawStr(tempX,  yBot, outTempBuf);

    // Bottom centered hint 
    u8g2.setFont(u8g2_font_6x10_tf);
    const char* centerHint = "Ext << Scroll >> Int";
    int hintW = u8g2.getStrWidth(centerHint);
    int hintX = (128 - hintW) / 2;
    u8g2.drawStr(hintX, 64, centerHint);

    u8g2.sendBuffer();
    return;
  }

  // ===== TODAY INSIDE =====
  if (environmentViewPos == -1) {

    u8g2.setFont(u8g2_font_6x10_tf);

    char buf[32];

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
    return;
  }

  // ===== TODAY OUTSIDE =====
  if (environmentViewPos == 1) {

    u8g2.setFont(u8g2_font_6x10_tf);

    char buf[32];

    snprintf(
      buf,
      sizeof(buf),
      "Max T   %.1fC   %s",
      todayMaxTempOutside,
      todayMaxTempTimeOutside);
    u8g2.drawStr(0, 28, buf);

    snprintf(
      buf,
      sizeof(buf),
      "Min T   %.1fC   %s",
      todayMinTempOutside,
      todayMinTempTimeOutside);
    u8g2.drawStr(0, 40, buf);

    snprintf(
      buf,
      sizeof(buf),
      "Max H    %.0f%%    %s",
      todayMaxHumOutside,
      todayMaxHumTimeOutside);
    u8g2.drawStr(0, 52, buf);

    snprintf(
      buf,
      sizeof(buf),
      "Min H    %.0f%%    %s",
      todayMinHumOutside,
      todayMinHumTimeOutside);
    u8g2.drawStr(0, 64, buf);

    u8g2.sendBuffer();
    return;
  }

  // ===== INSIDE LOG (NVS) =====
  if (environmentViewPos == -2) {

    // Clamp index inside valid bounds
    if (environmentLogIndex < 0) environmentLogIndex = 0;
    if (environmentLogIndex > ENVIRONMENT_LOG_DAYS - 1) environmentLogIndex = ENVIRONMENT_LOG_DAYS - 1;

    ClimateDay& d = climateLog[environmentLogIndex];

    char buf[32];

    u8g2.setFont(u8g2_font_6x10_tf);

    char dayBuf[16];
    snprintf(
      dayBuf,
      sizeof(dayBuf),
      "Day %d/%d",
      environmentLogIndex + 1,
      ENVIRONMENT_LOG_DAYS);
    u8g2.drawStr(0, 28, dayBuf);

    int dateWidth = u8g2.getStrWidth(d.dateString);
    u8g2.drawStr(128 - dateWidth, 28, d.dateString);

    snprintf(
      buf,
      sizeof(buf),
      "Max T   %.1fC",
      d.maxTemp);
    u8g2.drawStr(0, 42, buf);

    snprintf(
      buf,
      sizeof(buf),
      "%s",
      d.maxTempTime);
    u8g2.drawStr(95, 42, buf);

    snprintf(
      buf,
      sizeof(buf),
      "Min T   %.1fC",
      d.minTemp);
    u8g2.drawStr(0, 53, buf);

    snprintf(
      buf,
      sizeof(buf),
      "%s",
      d.minTempTime);
    u8g2.drawStr(95, 53, buf);

    snprintf(
      buf,
      sizeof(buf),
      "Max H    %.0f%%",
      d.maxHum);
    u8g2.drawStr(0, 64, buf);

    snprintf(
      buf,
      sizeof(buf),
      "%s",
      d.maxHumTime);
    u8g2.drawStr(95, 64, buf);

    u8g2.sendBuffer();
    return;
  }

  // ===== OUTSIDE LOG (NVS) =====
  if (environmentViewPos == 2) {

    // Clamp outside index within valid bounds
    if (environmentLogIndexOutside < 0) environmentLogIndexOutside = 0;
    if (environmentLogIndexOutside > ENVIRONMENT_LOG_DAYS_OUTSIDE - 1) environmentLogIndexOutside = ENVIRONMENT_LOG_DAYS_OUTSIDE - 1;

    ClimateDay& d = climateLogOutside[environmentLogIndexOutside];

    char buf[32];

    u8g2.setFont(u8g2_font_6x10_tf);

    char dayBuf[16];
    snprintf(
      dayBuf,
      sizeof(dayBuf),
      "Day %d/%d",
      environmentLogIndexOutside + 1,
      ENVIRONMENT_LOG_DAYS_OUTSIDE);
    u8g2.drawStr(0, 28, dayBuf);

    int dateWidth = u8g2.getStrWidth(d.dateString);
    u8g2.drawStr(128 - dateWidth, 28, d.dateString);

    snprintf(
      buf,
      sizeof(buf),
      "Max T   %.1fC",
      d.maxTemp);
    u8g2.drawStr(0, 42, buf);

    snprintf(
      buf,
      sizeof(buf),
      "%s",
      d.maxTempTime);
    u8g2.drawStr(95, 42, buf);

    snprintf(
      buf,
      sizeof(buf),
      "Min T   %.1fC",
      d.minTemp);
    u8g2.drawStr(0, 53, buf);

    snprintf(
      buf,
      sizeof(buf),
      "%s",
      d.minTempTime);
    u8g2.drawStr(95, 53, buf);

    snprintf(
      buf,
      sizeof(buf),
      "Max H    %.0f%%",
      d.maxHum);
    u8g2.drawStr(0, 64, buf);

    snprintf(
      buf,
      sizeof(buf),
      "%s",
      d.maxHumTime);
    u8g2.drawStr(95, 64, buf);

    u8g2.sendBuffer();
    return;
  }
}

// Helper: format minutes-since-midnight into "HH:MM" or "   N/A   "
void formatMinutesToHHMM(int mins, char* buf, size_t buflen) {
  if (mins < 0) {
    snprintf(buf, buflen, "   N/A   ");
  } else {
    int nm = (mins % (24 * 60) + (24 * 60)) % (24 * 60);
    int hh = nm / 60;
    int mm = nm % 60;
    snprintf(buf, buflen, "%02d:%02d", hh, mm);
  }
}
// ==========================================
// Today's Schedule Page
// ==========================================

void drawSchedulePage() {

  if (oledSleeping) return;

  u8g2.clearBuffer();

  // Title
  u8g2.setFont(u8g2_font_6x13_tf);
  const char* title = "Today's Schedule";
  int titleWidth = u8g2.getStrWidth(title);
  u8g2.drawStr((128 - titleWidth) / 2, 12, title);
  u8g2.drawHLine(0, 16, 128);

  // Need current time for DST selection (and to ensure offsets are chosen correctly)
  struct tm timeinfo;
  if (!getTime(timeinfo)) {
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(10, 40, "Time unavailable");
    u8g2.sendBuffer();
    return;
  }

  // Compute seasonal offsets for light on/off
  int seasonalLightOnOffset = getSeasonalOffset(lightOnOffsetBST, lightOnOffsetGMT, timeinfo);
  int seasonalLightOffMinutes = getSeasonalOffset(lightOffMinutesBST, lightOffMinutesGMT, timeinfo);

  // Calculate times (minutes since midnight). Seasonal door openTime & closeTime are maintained by updateSunTimes()
  int doorOpenMinutes = openTime;    // may be fixed or sunrise+offset depending on config
  int doorCloseMinutes = closeTime;  // scheduled close+offset
  int lightOnMinutes = -1;
  int lightOffMinutes = -1;

  if (actualSunset >= 0) {
    lightOnMinutes = actualSunset + seasonalLightOnOffset;
    lightOffMinutes = actualSunset + seasonalLightOffMinutes;
  }

  char bufOpen[16], bufClose[16], bufLightOn[16], bufLightOff[16];
  formatMinutesToHHMM(doorOpenMinutes, bufOpen, sizeof(bufOpen));
  formatMinutesToHHMM(doorCloseMinutes, bufClose, sizeof(bufClose));
  formatMinutesToHHMM(lightOnMinutes, bufLightOn, sizeof(bufLightOn));
  formatMinutesToHHMM(lightOffMinutes, bufLightOff, sizeof(bufLightOff));

  u8g2.setFont(u8g2_font_6x12_tf);

  const int xLabel = 6;
  const int rightPad = 6;
  const int yStart = 28;
  const int yStep = 12;

  // Line 1: Door Open
  u8g2.drawStr(xLabel, yStart + 0 * yStep, "Door Open");
  int w = u8g2.getStrWidth(bufOpen);
  u8g2.drawStr(128 - w - rightPad, yStart + 0 * yStep, bufOpen);

  // Line 2: Door Close
  u8g2.drawStr(xLabel, yStart + 1 * yStep, "Door Close");
  w = u8g2.getStrWidth(bufClose);
  u8g2.drawStr(128 - w - rightPad, yStart + 1 * yStep, bufClose);

  // Line 3: Light ON
  u8g2.drawStr(xLabel, yStart + 2 * yStep, "Light ON");
  w = u8g2.getStrWidth(bufLightOn);
  u8g2.drawStr(128 - w - rightPad, yStart + 2 * yStep, bufLightOn);

  // Line 4: Light OFF
  u8g2.drawStr(xLabel, yStart + 3 * yStep, "Light OFF");
  w = u8g2.getStrWidth(bufLightOff);
  u8g2.drawStr(128 - w - rightPad, yStart + 3 * yStep, bufLightOff);

  u8g2.sendBuffer();

  displayDirty = false;
}

// ======================================================
// DAYLIGHT & COLD PAGE
// ======================================================

void drawSystemTimePage() {

  if (oledSleeping) return;

  u8g2.clearBuffer();

  struct tm timeinfo;
  bool timeValid = false;

  if (getTime(timeinfo)) {
    timeValid = true;
  }

  // =====================================================
  // TITLE DAYLIGHT & COLD
  // =====================================================

  u8g2.setFont(u8g2_font_6x13_tf);

  const char* title = "Daylight & Cold";

  int titleWidth = u8g2.getStrWidth(title);

  u8g2.drawStr(
    (128 - titleWidth) / 2,
    12,
    title);

  u8g2.drawHLine(0, 16, 128);

  // =====================================================
  // COLD SETPOINT 
  // =====================================================

  u8g2.setFont(u8g2_font_7x13_tf);

  // Show the cold delay only if the external sensor is enabled.
  // If no external sensor (enableOutsideSHT == false) the line is omitted.
  if (enableOutsideSHT) {
    char coldBuf[32];
    snprintf(coldBuf, sizeof(coldBuf), "Cold Delay: %.1f C", outsideTempTriggerC);
    int coldWidth = u8g2.getStrWidth(coldBuf);
    u8g2.drawStr(
      (128 - coldWidth) / 2,
      36,
      coldBuf);
  }

  // =====================================================
  // SUNRISE / SUNSET or Fixed Open time
  // =====================================================

  char sunBuf[32];

  // Show fixed opening time if configured for the current timezone:
  if (timeValid && isInBST(timeinfo) && useFixedOpenTime) {
    // BST and BST fixed open enabled
    snprintf(
      sunBuf,
      sizeof(sunBuf),
      "Open %d:%02d SS %02d:%02d",
      fixedDoorOpenHour,
      fixedDoorOpenMinute,
      actualSunset / 60,
      actualSunset % 60);
  } else if (timeValid && !isInBST(timeinfo) && useFixedOpenTimeGMT) {
    // GMT and GMT fixed open enabled
    snprintf(
      sunBuf,
      sizeof(sunBuf),
      "Open %d:%02d SS %02d:%02d",
      fixedDoorOpenHourGMT,
      fixedDoorOpenMinuteGMT,
      actualSunset / 60,
      actualSunset % 60);
  } else {
    // Default: show SR/SS
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



  u8g2.sendBuffer();

  displayDirty = false;
}

void refreshDisplayIfNeeded() {

  static unsigned long lastRefresh = 0;

  if (oledSleeping) return;

  unsigned long now = millis();


  // All pages only refresh when something has changed
  if (!displayDirty) {
    return;
  }

  // Optional very small throttle to avoid excessive redraws during bursts
  if (now - lastRefresh < 100) {
    return;
  }

  lastRefresh = now;

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

  else if (currentPage == PAGE_SCHEDULE) {
    drawSchedulePage();
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
  static int consecutiveShtFailures = 0;
  static bool firstReadDone = false;

  // Read after settling delay on first call, then every SHT_READ_INTERVAL_MS
  if (!firstReadDone) {
    if (millis() < SHT_FIRST_READ_DELAY_MS) {
      return;
    }
    firstReadDone = true;
    lastRead = millis();
  } else if (millis() - lastRead < SHT_READ_INTERVAL_MS) {
    return;
  }

  lastRead = millis();

  // Try multiple reads to avoid transient failures
  const int SHT_READ_RETRIES = 3;
  const unsigned long SHT_RETRY_DELAY_MS = 250;

  float t = NAN;
  float h = NAN;

  for (int attempt = 0; attempt < SHT_READ_RETRIES; attempt++) {

    float temperature = NAN;
    float humidity = NAN;

    int16_t error =
      sht3xInside.measureSingleShot(REPEATABILITY_HIGH, false, temperature, humidity);

    if (error != 0) {
      t = NAN;
      h = NAN;
    } else {
      t = temperature;
      h = humidity;
      break;
    }

    delay(10);
  }

  bool valid = true;

  if (isnan(t) || isnan(h) || !isfinite(t) || !isfinite(h)) valid = false;
  if (t < -40.0 || t > 60.0) valid = false;
  if (h < 0.0 || h > 100.0) valid = false;

  if (!valid) {
    consecutiveShtFailures++;
    Serial.print("SHT3x Read Invalid (attempts=");
    Serial.print(SHT_READ_RETRIES);
    Serial.print(") temp=");
    Serial.print(t);
    Serial.print(" hum=");
    Serial.println(h);

    // Log fault after MAX_CONSECUTIVE_SHT_FAILURES consecutive failures
    if (consecutiveShtFailures > MAX_CONSECUTIVE_SHT_FAILURES) {
      setEvent("SHT31 IN FAULT");
      consecutiveShtFailures = 0;
    }
    return;
  }

  // Valid reading — reset failure counter
  consecutiveShtFailures = 0;

  currentTemp = t;
  currentHumidity = h;

  struct tm timeinfo;
  if (!getTime(timeinfo)) {
    return;
  }

  char timeStamp[16];
  snprintf(timeStamp, sizeof(timeStamp), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

  // Update daily max/min only when reading is valid
  if (t > todayMaxTemp) {
    todayMaxTemp = t;
    strncpy(todayMaxTempTime, timeStamp, sizeof(todayMaxTempTime) - 1);
    todayMaxTempTime[sizeof(todayMaxTempTime) - 1] = '\0';
    saveCurrentEnvironmentData();
  }

  if (t < todayMinTemp) {
    todayMinTemp = t;
    strncpy(todayMinTempTime, timeStamp, sizeof(todayMinTempTime) - 1);
    todayMinTempTime[sizeof(todayMinTempTime) - 1] = '\0';
    saveCurrentEnvironmentData();
  }

  if (h > todayMaxHum) {
    todayMaxHum = h;
    strncpy(todayMaxHumTime, timeStamp, sizeof(todayMaxHumTime) - 1);
    todayMaxHumTime[sizeof(todayMaxHumTime) - 1] = '\0';
    saveCurrentEnvironmentData();
  }

  if (h < todayMinHum) {
    todayMinHum = h;
    strncpy(todayMinHumTime, timeStamp, sizeof(todayMinHumTime) - 1);
    todayMinHumTime[sizeof(todayMinHumTime) - 1] = '\0';
    saveCurrentEnvironmentData();
  }

  displayDirty = true;
}

// Update the Blue LED that indicates outside is colder than outsideTempTriggerC
void updateOutsideColdLed() {
  // If outside sensor disabled, ensure LED is off
  if (!enableOutsideSHT) {
    digitalWrite(COLD_LED_PIN, LOW);
    return;
  }

  // If we have a valid outside temperature reading and it's below trigger -> ON
  if (!isnan(currentTempOutside) && currentTempOutside < outsideTempTriggerC) {
    digitalWrite(COLD_LED_PIN, HIGH);
  } else {
    digitalWrite(COLD_LED_PIN, LOW);
  }
}

// Read and update outside SHT30

void updateOutsideEnvironment() {

  if (!enableOutsideSHT) return;

  static int consecutiveShtFailures = 0;
  static bool firstReadDone = false;

  // Use shared lastOutsideReadMillis so other code can reset the outside poll timer
  if (!firstReadDone) {
    if (millis() < OUTSIDE_SHT_FIRST_READ_DELAY_MS) return;
    firstReadDone = true;
    lastOutsideReadMillis = millis();
  } else if (millis() - lastOutsideReadMillis < OUTSIDE_SHT_READ_INTERVAL_MS) {
    return;
  }

  // Mark the read time now (so interval counts from start of attempt)
  lastOutsideReadMillis = millis();

  const int SHT_READ_RETRIES = 3;
  const unsigned long SHT_RETRY_DELAY_MS = 250;

  float t = NAN;
  float h = NAN;

  for (int attempt = 0; attempt < SHT_READ_RETRIES; attempt++) {

    float temperature_out = NAN;
    float humidity_out = NAN;

    int16_t error =
      sht3xOutside.measureSingleShot(REPEATABILITY_HIGH, false, temperature_out, humidity_out);

    if (error == 0) {
      t = temperature_out;
      h = humidity_out;
      break;
    }

    // Wait a short time before retrying
    delay(SHT_RETRY_DELAY_MS);
  }

  // Validate reading
  bool valid = true;
  if (isnan(t) || isnan(h) || !isfinite(t) || !isfinite(h)) valid = false;
  if (t < -40.0 || t > 60.0) valid = false;
  if (h < 0.0 || h > 100.0) valid = false;

  if (!valid) {
    consecutiveShtFailures++;
    Serial.print("SHT30 OUT Read Invalid temp=");
    Serial.print(t);
    Serial.print(" hum=");
    Serial.println(h);

    if (consecutiveShtFailures > MAX_CONSECUTIVE_SHT_OUT_FAILURES) {
      setEvent("SHT30 OUT FAULT");
      consecutiveShtFailures = 0;
    }
    return;
  }

  // Valid reading — reset failure counter
  consecutiveShtFailures = 0;

  currentTempOutside = t;
  currentHumidityOutside = h;

  struct tm timeinfo;
  if (!getTime(timeinfo)) {
    // still update cold LED state based on last known values
    updateOutsideColdLed();
    displayDirty = true;
    return;
  }

  char timeStamp[16];
  snprintf(timeStamp, sizeof(timeStamp), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

  // Temperature min/max (persist if changed)
  if (t > todayMaxTempOutside) {
    todayMaxTempOutside = t;
    strncpy(todayMaxTempTimeOutside, timeStamp, sizeof(todayMaxTempTimeOutside) - 1);
    todayMaxTempTimeOutside[sizeof(todayMaxTempTimeOutside) - 1] = '\0';
    saveCurrentEnvironmentData();
  }

  if (t < todayMinTempOutside) {
    todayMinTempOutside = t;
    strncpy(todayMinTempTimeOutside, timeStamp, sizeof(todayMinTempTimeOutside) - 1);
    todayMinTempTimeOutside[sizeof(todayMinTempTimeOutside) - 1] = '\0';
    saveCurrentEnvironmentData();
  }

  // Humidity min/max (persist if changed)
  if (h > todayMaxHumOutside) {
    todayMaxHumOutside = h;
    strncpy(todayMaxHumTimeOutside, timeStamp, sizeof(todayMaxHumTimeOutside) - 1);
    todayMaxHumTimeOutside[sizeof(todayMaxHumTimeOutside) - 1] = '\0';
    saveCurrentEnvironmentData();
  }

  if (h < todayMinHumOutside) {
    todayMinHumOutside = h;
    strncpy(todayMinHumTimeOutside, timeStamp, sizeof(todayMinHumTimeOutside) - 1);
    todayMinHumTimeOutside[sizeof(todayMinHumTimeOutside) - 1] = '\0';
    saveCurrentEnvironmentData();
  }

  // Update display state & cold LED only after processing the valid reading
  displayDirty = true;
  updateOutsideColdLed();
}

// Read the outside SHT30 immediately.
bool pollOutsideSensorNow(int maxAttempts = 3) {
  if (!enableOutsideSHT) return false;

  const int attempts = (maxAttempts > 0) ? maxAttempts : 3;
  const unsigned long retryDelayMs = 200;

  for (int attempt = 0; attempt < attempts; attempt++) {
    float temperature = NAN;
    float humidity = NAN;
    int16_t err = sht3xOutside.measureSingleShot(REPEATABILITY_HIGH, false, temperature, humidity);

    if (err == 0) {
      // Validate reading
      if (!isnan(temperature) && isfinite(temperature) && temperature >= -40.0 && temperature <= 60.0 && !isnan(humidity) && isfinite(humidity) && humidity >= 0.0 && humidity <= 100.0) {

        // Update the live values
        currentTempOutside = temperature;
        currentHumidityOutside = humidity;

        // Reset outside read timer to now (fresh interval)
        lastOutsideReadMillis = millis();

        // If we have time, update today's min/max and persist times (same semantics as regular reader)
        struct tm timeinfo;
        if (getTime(timeinfo)) {
          char timeStamp[16];
          snprintf(timeStamp, sizeof(timeStamp), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

          if (temperature > todayMaxTempOutside) {
            todayMaxTempOutside = temperature;
            strncpy(todayMaxTempTimeOutside, timeStamp, sizeof(todayMaxTempTimeOutside) - 1);
            todayMaxTempTimeOutside[sizeof(todayMaxTempTimeOutside) - 1] = '\0';
            saveCurrentEnvironmentData();
          }

          if (temperature < todayMinTempOutside) {
            todayMinTempOutside = temperature;
            strncpy(todayMinTempTimeOutside, timeStamp, sizeof(todayMinTempTimeOutside) - 1);
            todayMinTempTimeOutside[sizeof(todayMinTempTimeOutside) - 1] = '\0';
            saveCurrentEnvironmentData();
          }

          if (humidity > todayMaxHumOutside) {
            todayMaxHumOutside = humidity;
            strncpy(todayMaxHumTimeOutside, timeStamp, sizeof(todayMaxHumTimeOutside) - 1);
            todayMaxHumTimeOutside[sizeof(todayMaxHumTimeOutside) - 1] = '\0';
            saveCurrentEnvironmentData();
          }

          if (humidity < todayMinHumOutside) {
            todayMinHumOutside = humidity;
            strncpy(todayMinHumTimeOutside, timeStamp, sizeof(todayMinHumTimeOutside) - 1);
            todayMinHumTimeOutside[sizeof(todayMinHumTimeOutside) - 1] = '\0';
            saveCurrentEnvironmentData();
          }

          // Persist the updated today's values (one call persists all)
          saveCurrentEnvironmentData();
        }

        // Update cold LED and redraw when needed
        updateOutsideColdLed();
        displayDirty = true;

        return true;
      }
    }

    // brief delay before retrying
    delay(retryDelayMs);
  }

  // no valid reading obtained
  return false;
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
  // SHIFT HISTORY (inside)
  // ==========================================
  for (int i = ENVIRONMENT_LOG_DAYS - 1; i > 0; i--) {
    climateLog[i] = climateLog[i - 1];
  }

  // SHIFT HISTORY (outside)
  for (int i = ENVIRONMENT_LOG_DAYS_OUTSIDE - 1; i > 0; i--) {
    climateLogOutside[i] = climateLogOutside[i - 1];
  }

  // ==========================================
  // STORE YESTERDAY (Calculate previous day)
  // ==========================================
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

  // Inside: store yesterday's date & stats
  snprintf(
    climateLog[0].dateString,
    sizeof(climateLog[0].dateString),
    "%s %02d %s",
    days[yesterdayInfo.tm_wday],
    yesterdayInfo.tm_mday,
    months[yesterdayInfo.tm_mon]);

  climateLog[0].maxTemp = todayMaxTemp;
  climateLog[0].minTemp = todayMinTemp;
  climateLog[0].maxHum = todayMaxHum;
  climateLog[0].minHum = todayMinHum;

  strncpy(climateLog[0].maxTempTime, todayMaxTempTime, sizeof(climateLog[0].maxTempTime) - 1);
  climateLog[0].maxTempTime[sizeof(climateLog[0].maxTempTime) - 1] = '\0';
  strncpy(climateLog[0].minTempTime, todayMinTempTime, sizeof(climateLog[0].minTempTime) - 1);
  climateLog[0].minTempTime[sizeof(climateLog[0].minTempTime) - 1] = '\0';
  strncpy(climateLog[0].maxHumTime, todayMaxHumTime, sizeof(climateLog[0].maxHumTime) - 1);
  climateLog[0].maxHumTime[sizeof(climateLog[0].maxHumTime) - 1] = '\0';
  strncpy(climateLog[0].minHumTime, todayMinHumTime, sizeof(climateLog[0].minHumTime) - 1);
  climateLog[0].minHumTime[sizeof(climateLog[0].minHumTime) - 1] = '\0';
  climateLogOutside[0].minTempTime[sizeof(climateLogOutside[0].minTempTime) - 1] = '\0';

  // Outside: store yesterday's date & stats
  snprintf(
    climateLogOutside[0].dateString,
    sizeof(climateLogOutside[0].dateString),
    "%s %02d %s",
    days[yesterdayInfo.tm_wday],
    yesterdayInfo.tm_mday,
    months[yesterdayInfo.tm_mon]);

  // Temperatures
  climateLogOutside[0].maxTemp = todayMaxTempOutside;
  climateLogOutside[0].minTemp = todayMinTempOutside;

  // copy temperature timestamps
  strncpy(climateLogOutside[0].maxTempTime, todayMaxTempTimeOutside, sizeof(climateLogOutside[0].maxTempTime) - 1);
  climateLogOutside[0].maxTempTime[sizeof(climateLogOutside[0].maxTempTime) - 1] = '\0';
  strncpy(climateLogOutside[0].minTempTime, todayMinTempTimeOutside, sizeof(climateLogOutside[0].minTempTime) - 1);
  climateLogOutside[0].minTempTime[sizeof(climateLogOutside[0].minTempTime) - 1] = '\0';

  // Humidity copy values and timestamps
  climateLogOutside[0].maxHum = todayMaxHumOutside;
  climateLogOutside[0].minHum = todayMinHumOutside;

  strncpy(climateLogOutside[0].maxHumTime, todayMaxHumTimeOutside, sizeof(climateLogOutside[0].maxHumTime) - 1);
  climateLogOutside[0].maxHumTime[sizeof(climateLogOutside[0].maxHumTime) - 1] = '\0';
  strncpy(climateLogOutside[0].minHumTime, todayMinHumTimeOutside, sizeof(climateLogOutside[0].minHumTime) - 1);
  climateLogOutside[0].minHumTime[sizeof(climateLogOutside[0].minHumTime) - 1] = '\0';
  // ==========================================
  // RESET FOR NEW DAY (inside)
  // ==========================================
  todayMaxTemp = -100;
  todayMinTemp = 100;
  todayMaxHum = -100;
  todayMinHum = 100;
  strcpy(todayMaxTempTime, "");
  strcpy(todayMinTempTime, "");
  strcpy(todayMaxHumTime, "");
  strcpy(todayMinHumTime, "");

  // Reset outside day (temp + humidity)
  todayMaxTempOutside = -100;
  todayMinTempOutside = 100;
  todayMaxHumOutside = -100;
  todayMinHumOutside = 100;
  strcpy(todayMaxTempTimeOutside, "");
  strcpy(todayMinTempTimeOutside, "");
  strcpy(todayMaxHumTimeOutside, "");
  strcpy(todayMinHumTimeOutside, "");

  // ==========================================
  // PERSIST BOTH HISTORIES
  // ==========================================
  preferences.begin("envlog", false);

  preferences.putBytes("history_in", climateLog, sizeof(climateLog));
  preferences.putBytes("history_out", climateLogOutside, sizeof(climateLogOutside));

  preferences.end();

  Serial.println("Environment log rolled");

  // Verification: read back and verify sizes for both keys
  preferences.begin("envlog", true);

  size_t lenIn = preferences.getBytesLength("history_in");
  if (lenIn == sizeof(climateLog)) {
    preferences.getBytes("history_in", climateLog, sizeof(climateLog));
    Serial.println("✓ History (in) verified in storage");
  } else {
    Serial.print("⚠ History (in) size mismatch: ");
    Serial.print(lenIn);
    Serial.print(" vs ");
    Serial.println(sizeof(climateLog));
  }

  size_t lenOut = preferences.getBytesLength("history_out");
  if (lenOut == sizeof(climateLogOutside)) {
    preferences.getBytes("history_out", climateLogOutside, sizeof(climateLogOutside));
    Serial.println("✓ History (out) verified in storage");
  } else {
    Serial.print("⚠ History (out) size mismatch: ");
    Serial.print(lenOut);
    Serial.print(" vs ");
    Serial.println(sizeof(climateLogOutside));
  }

  preferences.end();

  Serial.print("Stored ");
  Serial.print(ENVIRONMENT_LOG_DAYS);
  Serial.println(" days of history");
}

// ======================================================
// UNIFIED PERSISTENCE: Inside + Outside Daily Data
// ======================================================
void saveCurrentEnvironmentData() {
  preferences.begin("envlog", false);

  // TODAY'S INSIDE
  preferences.putFloat("todayMaxTemp", todayMaxTemp);
  preferences.putFloat("todayMinTemp", todayMinTemp);
  preferences.putFloat("todayMaxHum", todayMaxHum);
  preferences.putFloat("todayMinHum", todayMinHum);
  preferences.putString("maxTempTime", todayMaxTempTime);
  preferences.putString("minTempTime", todayMinTempTime);
  preferences.putString("maxHumTime", todayMaxHumTime);
  preferences.putString("minHumTime", todayMinHumTime);

  // TODAY'S OUTSIDE
  preferences.putFloat("todayMaxTemp_out", todayMaxTempOutside);
  preferences.putFloat("todayMinTemp_out", todayMinTempOutside);
  preferences.putFloat("todayMaxHum_out", todayMaxHumOutside);
  preferences.putFloat("todayMinHum_out", todayMinHumOutside);
  preferences.putString("todayMaxTempTime_out", todayMaxTempTimeOutside);
  preferences.putString("todayMinTempTime_out", todayMinTempTimeOutside);
  preferences.putString("todayMaxHumTime_out", todayMaxHumTimeOutside);
  preferences.putString("todayMinHumTime_out", todayMinHumTimeOutside);

  preferences.end();
}

// ======================================================
// DAILY BACKUP: Persist Histories to NVS
// ======================================================
void persistEnvironmentHistoriesToPrefs() {
  preferences.begin("envlog", false);

  // Histories
  preferences.putBytes("history_in", climateLog, sizeof(climateLog));
  preferences.putBytes("history_out", climateLogOutside, sizeof(climateLogOutside));
  // Legacy key for backward compatibility
  preferences.putBytes("history", climateLog, sizeof(climateLog));

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
  // Door opening: choose between fixed time or sunrise+offset
  // for BST and GMT separately
  //======================================================

  if (isInBST(timeinfo)) {
    // BST (summer)
    if (useFixedOpenTime) {
      openTime = (fixedDoorOpenHour * 60) + fixedDoorOpenMinute;
    } else {
      openTime = actualSunrise + sunriseOpenOffsetBST;
    }
  } else {
    // GMT (winter)
    if (useFixedOpenTimeGMT) {
      openTime = (fixedDoorOpenHourGMT * 60) + fixedDoorOpenMinuteGMT;
    } else {
      openTime = actualSunrise + sunriseOpenOffsetGMT;
    }
  }

  closeTime = actualSunset + seasonalSunsetOffset;

  // force a display update (no event)
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

  Wire.begin();           //primary I2C bus (Display,RTC & SHT3x sensors)
  Wire.setClock(100000);  // reduce I2C clock speed
  u8g2.begin();

  // Initialise inside SHT31 temperature sensor
  sht3xInside.begin(Wire, INSIDE_SHT_ADDRESS);

  //setEvent("SHT31 Initialised"); // commented out to reduce Event logging
    Serial.println("SHT31 initialised at address 0x45");

  // ======================================================
  // LOAD INSIDE HISTORY
  // ======================================================
  preferences.begin("envlog", true);
  size_t lenIn = preferences.getBytesLength("history_in");
  if (lenIn == sizeof(climateLog)) {
    preferences.getBytes("history_in", climateLog, sizeof(climateLog));
    Serial.println("Loaded history_in");
  } else {
    size_t lenLegacy = preferences.getBytesLength("history");
    if (lenLegacy == sizeof(climateLog)) {
      preferences.getBytes("history", climateLog, sizeof(climateLog));
      Serial.println("Loaded legacy history");
    } else {
      Serial.println("No inside history found");
    }
  }
  preferences.end();

  // ======================================================
  // LOAD TODAY'S INSIDE DATA
  // ======================================================
  preferences.begin("envlog", true);
  todayMaxTemp = preferences.getFloat("todayMaxTemp", -100);
  todayMinTemp = preferences.getFloat("todayMinTemp", 100);
  todayMaxHum = preferences.getFloat("todayMaxHum", -100);
  todayMinHum = preferences.getFloat("todayMinHum", 100);

  String s;
  s = preferences.getString("maxTempTime", "");
  strncpy(todayMaxTempTime, s.c_str(), sizeof(todayMaxTempTime) - 1);
  todayMaxTempTime[sizeof(todayMaxTempTime) - 1] = '\0';

  s = preferences.getString("minTempTime", "");
  strncpy(todayMinTempTime, s.c_str(), sizeof(todayMinTempTime) - 1);
  todayMinTempTime[sizeof(todayMinTempTime) - 1] = '\0';

  s = preferences.getString("maxHumTime", "");
  strncpy(todayMaxHumTime, s.c_str(), sizeof(todayMaxHumTime) - 1);
  todayMaxHumTime[sizeof(todayMaxHumTime) - 1] = '\0';

  s = preferences.getString("minHumTime", "");
  strncpy(todayMinHumTime, s.c_str(), sizeof(todayMinHumTime) - 1);
  todayMinHumTime[sizeof(todayMinHumTime) - 1] = '\0';

  preferences.end();

  // ======================================================
  // LOAD OUTSIDE HISTORY
  // ======================================================
  preferences.begin("envlog", true);
  size_t lenOut = preferences.getBytesLength("history_out");
  if (lenOut == sizeof(climateLogOutside)) {
    preferences.getBytes("history_out", climateLogOutside, sizeof(climateLogOutside));
    Serial.println("Loaded outside history");
  } else {
    Serial.println("No outside history found");
  }
  preferences.end();
 

  // Load today's outside min/max (if present)
  todayMaxTempOutside = preferences.getFloat("todayMaxTemp_out", todayMaxTempOutside);
  todayMinTempOutside = preferences.getFloat("todayMinTemp_out", todayMinTempOutside);

  // Load outside humidity today's min/max if present
  todayMaxHumOutside = preferences.getFloat("todayMaxHum_out", todayMaxHumOutside);
  todayMinHumOutside = preferences.getFloat("todayMinHum_out", todayMinHumOutside);

  s = preferences.getString("todayMaxTempTime_out", "");
  strncpy(todayMaxTempTimeOutside, s.c_str(), sizeof(todayMaxTempTimeOutside) - 1);
  todayMaxTempTimeOutside[sizeof(todayMaxTempTimeOutside) - 1] = '\0';

  s = preferences.getString("todayMinTempTime_out", "");
  strncpy(todayMinTempTimeOutside, s.c_str(), sizeof(todayMinTempTimeOutside) - 1);
  todayMinTempTimeOutside[sizeof(todayMinTempTimeOutside) - 1] = '\0';

  // humidity times
   s = preferences.getString("todayMaxHumTime_out", "");
  strncpy(todayMaxHumTimeOutside, s.c_str(), sizeof(todayMaxHumTimeOutside) - 1);
  todayMaxHumTimeOutside[sizeof(todayMaxHumTimeOutside) - 1] = '\0';

  s = preferences.getString("todayMinHumTime_out", "");
  strncpy(todayMinHumTimeOutside, s.c_str(), sizeof(todayMinHumTimeOutside) - 1);
  todayMinHumTimeOutside[sizeof(todayMinHumTimeOutside) - 1] = '\0';

  // Initialise outside SHT30 temperature sensor if enabled
  if (enableOutsideSHT) {

    sht3xOutside.begin(Wire, OUTSIDE_SHT_ADDRESS);

    //setEvent("SHT30 Initialised"); // commented out to reduce event logging
    Serial.println("SHT30 OUT initialised at address 0x44");
  }

  // After outside sensor initialised, set LED initial state
  updateOutsideColdLed();

  // Load persistent setting for forceOpenDespiteCold
  preferences.begin("settings", true);
  forceOpenDespiteCold = preferences.getBool("forceOpenDespiteCold", forceOpenDespiteCold);
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

    setEvent("Reset");
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

  // INITIALISE DEBOUNCE STATES
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

  pinMode(COLD_LED_PIN, OUTPUT);
  digitalWrite(COLD_LED_PIN, LOW);  // initial off

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

      // CLOCKWISE = newer events
      if (delta > 0) {

        viewedEventOffset++;

        if (viewedEventOffset >= storedEventCount) {
          viewedEventOffset = storedEventCount - 1;
        }
      }

      // COUNTERCLOCKWISE = older events
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
  // ENVIRONMENT PAGE (combined)
  // =====================================================
  if (currentPage == PAGE_ENVIRONMENT) {

    int encoderValue = rotaryEncoder.readEncoder();

    if (encoderValue != lastEncoderValue) {

      int delta = encoderValue - lastEncoderValue;

      // Normalize to single-step increments
      if (delta > 0) delta = 1;
      if (delta < 0) delta = -1;

      // If we're in an IN NVS log (left-most)
      if (environmentViewPos == -2) {
        // delta < 0 : stepping AWAY from NOW (further CCW) => go to older days (increment index)
        if (delta < 0) {
          environmentLogIndex++;
          if (environmentLogIndex > ENVIRONMENT_LOG_DAYS - 1) environmentLogIndex = ENVIRONMENT_LOG_DAYS - 1;
        }
        // delta > 0 : stepping TOWARD NOW (CW)
        else {
          // If we are not at the most recent day, move to a newer day first
          if (environmentLogIndex > 0) {
            environmentLogIndex--;
          } else {
            // at most-recent day and user turned toward NOW -> step out to TODAY IN
            environmentViewPos = -1;
          }
        }
      }
      // If we're in an OUT NVS log (right-most)
      else if (environmentViewPos == 2) {
        // delta > 0 : stepping AWAY from NOW (further CW) => older outside days
        if (delta > 0) {
          environmentLogIndexOutside++;
          if (environmentLogIndexOutside > ENVIRONMENT_LOG_DAYS_OUTSIDE - 1) environmentLogIndexOutside = ENVIRONMENT_LOG_DAYS_OUTSIDE - 1;
        }
        // delta < 0 : stepping TOWARD NOW (CCW)
        else {
          if (environmentLogIndexOutside > 0) {
            environmentLogIndexOutside--;
          } else {
            // at most-recent day and user turned toward NOW -> step out to TODAY OUT
            environmentViewPos = 1;
          }
        }
      }
      // Not in a NVS log: rotation moves view position stepwise (-2..2)
      else {
        environmentViewPos += delta;
        if (environmentViewPos < -2) environmentViewPos = -2;
        if (environmentViewPos > 2) environmentViewPos = 2;

        // When entering a log, default to most recent day
        if (environmentViewPos == -2) environmentLogIndex = 0;
        if (environmentViewPos == 2) environmentLogIndexOutside = 0;
      }

      lastOledActivity = millis();
      displayDirty = true;
      drawEnvironmentPage();
    }

    lastEncoderValue = encoderValue;
  }
  // ============================================================================================================

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
      environmentViewPos = 0;          // default to NOW when entering Environment page
      environmentLogIndex = 0;         // default NVS inside to most recent day
      environmentLogIndexOutside = 0;  // default NVS outside to most recent day

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
      // Stage 2:  minutes until close (Faster flash)
      else if (minutesUntilClose <= BEDTIME_STAGE2_MINS && minutesUntilClose > BEDTIME_STAGE3_MINS) {
        flashInterval = STAGE2_FLASH_MS;
      }
      // Stage 1:  minutes until close (Medium flash)
      else if (minutesUntilClose <= BEDTIME_STAGE1_MINS && minutesUntilClose > BEDTIME_STAGE2_MINS) {
        flashInterval = STAGE1_FLASH_MS;
      }
      // Stage 0: After coop light-on delay (Slow flash)
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

  updateOutsideEnvironment();

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

    // Save environment log to persistent storage daily (inside + outside + today's values)
    static int lastSavedDay = -1;
    if (timeinfo.tm_mday != lastSavedDay) {
      lastSavedDay = timeinfo.tm_mday;
      persistEnvironmentHistoriesToPrefs();
      Serial.println("Daily history backup saved (in + out)");
    }

    int nowMinutes = (timeinfo.tm_hour * 60) + timeinfo.tm_min;

    autoState = computeAutoState(nowMinutes);

    //===========================================
    // Door Auto State
    //===========================================

    static AutoState lastAutoState = AUTO_IDLE;

    if (autoState != lastAutoState) {

      switch (autoState) {

        case AUTO_IDLE:
          setEvent("Auto_Idle");
          break;

        case AUTO_ALLOWED:
          setEvent("Auto_Allowed");
          break;

        case AUTO_LOCKED_MANUAL:
          setEvent("Auto_Locked");
          break;

        case AUTO_SAFETY_RECOVERY:
          setEvent("Auto_Safety");
          break;
      }

      lastAutoState = autoState;
    }

    updateLight(nowMinutes);

    if (autoState == AUTO_ALLOWED) {

      // ---------- AUTO OPEN: one-time morning check at scheduled openTime ----------
      if (doorState == DOOR_CLOSED) {

        // We already have timeinfo above
        int today = timeinfo.tm_yday;

        // Perform the one-time morning check only once per day
        if (coldDelayCheckedDay != today) {

          // Only evaluate when we've reached (or passed) the scheduled open time
          if (nowMinutes >= openTime) {

            // Mark we've performed the morning check so we won't do it again today
            coldDelayCheckedDay = today;

            // Decide whether to apply the cold delay (only now, once per day)
            // First: try to refresh the outside reading up to 3 attempts and reset its read timer
            if (enableOutsideSHT && !forceOpenDespiteCold) {
              pollOutsideSensorNow(3);
            }

            bool applyDelay = false;

            if (enableOutsideSHT && !forceOpenDespiteCold && !isnan(currentTempOutside)) {
              if (currentTempOutside < outsideTempTriggerC) applyDelay = true;
            }

            if (applyDelay) {
              // Set the delayed open minute and record the day
              delayedOpenTime = openTime + doorOpenDelayIfColdMinutes;
              if (delayedOpenTime >= 24 * 60) delayedOpenTime -= 24 * 60;
              coldDelayDay = today;

              // Log a single "Cold Delay" event now that the morning opening was postponed
              setEvent("Cold Delay");
              coldDelayLogged = true;
            } else {
              // Temperature is OK (or sensor missing) — open immediately
              setEvent("Auto Open");
              startOpening();
            }
          }
        }

        // If we've already performed today's check and a delayedOpenTime exists, handle it
        else {
          if (delayedOpenTime >= 0) {

            // Allow cancellation of the previously-applied delay if temp warms before delayed time
            if (enableOutsideSHT && !forceOpenDespiteCold && !isnan(currentTempOutside) && currentTempOutside >= outsideTempTriggerC) {
              // Cancel the delay silently (no event)
              delayedOpenTime = -1;
              coldDelayDay = -1;
              coldDelayLogged = false;

              // If we're within the operating window, open immediately
              if (nowMinutes >= openTime && nowMinutes < closeTime) {
                setEvent("Auto Open");
                startOpening();
              }
            }
            // If not cancelled, open when delayed time is reached
            else if (nowMinutes >= delayedOpenTime && nowMinutes < closeTime) {
              delayedOpenTime = -1;
              coldDelayDay = -1;
              coldDelayLogged = false;
              setEvent("Auto Open");
              startOpening();
            }
          }
        }
      }

      // ---------- AUTO CLOSE (unchanged) ----------
      if (doorState != DOOR_CLOSED && nowMinutes >= closeTime) {
        setEvent("Auto Close");
        startClosing();
      }
    }
  }
}

