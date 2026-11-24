/* =====================================================================
   CoopCam — ESP32-S3 chicken-coop camera & IR lamp controller
   ---------------------------------------------------------------------
   - DS3231 RTC kept in UTC; NTP corrects it (at most once per 24 h)
   - Sunrise/sunset calculated offline (NOAA) from LATITUDE/LONGITUDE
   - Camera 12 V switched OFF during two daily windows 
   - IR lamp window relative to sunset (default -13 / +20 min)
   - Manual override: button or web dashboard, auto-reverts
   - SHT4x ceiling temp/humidity on the shared I2C bus
   - Optional lens heater: dew point while camera is ON, plus manual timer
   - ArduinoOTA + tiny web dashboard
   ---------------------------------------------------------------------
   Libraries (Arduino IDE > Library Manager):
       RTClib by Adafruit   (pulls in Adafruit BusIO)
       Sensirion I2C SHT4x by Sensirion (pulls in Sensirion Core)
   Board (Tools menu):
       ESP32S3 Dev Module
       Flash Size        : 16MB
       Partition Scheme  : 16MB scheme WITH OTA (two app slots)
       PSRAM             : Disabled
       USB CDC On Boot   : Enabled   (so Serial works over USB)
   ===================================================================== */

#include <WiFi.h>
#include <math.h>
#include <Wire.h>
#include <RTClib.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <SensirionI2cSht4x.h>
#include <esp_arduino_version.h>

#include "secrets.h"
#include "RemoteLog.h"

// =====================================================================
//  USER SETTINGS  
// =====================================================================

constexpr char FW_VERSION[] = "V1.06";

constexpr int SERIAL_BAUD = 115200;

// --- Camera power: switched OFF every day in these two windows, hrs and minutes -------
// Day Time
constexpr int CAM_OFF_FROM_H = 8;  constexpr int CAM_OFF_FROM_M = 45;   // camera auto OFF at 8.45
constexpr int CAM_OFF_TO_H   = 15;  constexpr int CAM_OFF_TO_M   = 30;  // camera auto ON at 15.30

// Night Time
constexpr int CAM_OFF2_FROM_H = 0;  constexpr int CAM_OFF2_FROM_M = 2;  // camera auto OFF at 00.02
constexpr int CAM_OFF2_TO_H   = 5;  constexpr int CAM_OFF2_TO_M   = 30; // camera auto ON at 5.30

// --- Camera stream link (the "View Camera" card) --------------------
// Leave "" to hide the card. One tap opens this URL in a new tab; if the
// camera relay is OFF the dashboard wakes the camera, counts down, then
// opens the view in that same tab.
//
// Recommended (go2rtc on the Linux PC re-serves the camera's RTSP in a form
// every browser can play, so nothing needs installing on the Windows laptop):
//   "http://192.168.0.20:1984/stream.html?src=coop&mode=webrtc"
//   ...&mode=mse  or  ...&mode=mjpeg   if WebRTC is blocked
//
// Also works with no server in between (camera's own web page):
//   "http://192.168.0.11/"
constexpr char CAMERA_VIEW_URL[] = "http://192.168.1.98/index.html";

// --- IR lamp auto: dusk window, relative to sunset ------------------------
constexpr int  IR_ON_BEFORE_SUNSET_MIN = 13;    // ON  13 min before sunset
constexpr int  IR_OFF_AFTER_SUNSET_MIN =  20;   // OFF  20 min after sunset
constexpr bool IR_ALL_NIGHT            = false; // true = IR ON from sunset -> sunrise

// --- IR brightness: permanent default; dashboard changes are temporary --------
constexpr uint8_t IR_DEFAULT_BRIGHTNESS = 50;  // 0-100%, copy final dashboard value here
constexpr uint32_t IR_PWM_FREQUENCY = 5000;    // Hz; verify YX-MOS operation at this frequency
constexpr uint8_t IR_PWM_RESOLUTION = 8;       // 8-bit duty resolution
constexpr uint32_t IR_RAMP_TIME_MS = 4000;     // 4-second ON and OFF fades
static_assert(IR_DEFAULT_BRIGHTNESS <= 100, "IR_DEFAULT_BRIGHTNESS must be 0-100"); // Dashboard adjustment ?

// --- SHT4x ceiling temp/humidity, shared I2C bus ---------------------
// If Mounted at ceiling height by the vents, away from the IR lamp and camera.
// Values live in RAM only: min/max reset at boot and at local midnight.
constexpr bool     SENSOR_ENABLED   = true;
constexpr uint32_t SENSOR_READ_MS   = 7UL * 60UL * 1000UL;  // 7 min between pings
constexpr uint8_t  SENSOR_PRECISION = 1;  // 0 = low, 1 = medium, 2 = high repeatability

// --- Lens heater: dew point control, GPIO6 --------------------------------
// Set HEATER_ENABLED to true only when the 12 V / 7 W pad is fitted.
// While false, GPIO6 is held OFF and the dashboard heater tile is inactive.
constexpr bool  HEATER_ENABLED          = true;   // false or true
constexpr int   HEATER_MANUAL_MINUTES   = 5;      // one-button manual ON timer
constexpr int   HEATER_AUTO_MINUTES     = 5;      // one-shot auto run when the camera powers ON
constexpr float HEATER_MAX_AMBIENT_C    = 15.0f;  // hard cut-out: never heat in warm air in celcius
constexpr float HEATER_AUTO_MAX_AMBIENT_C = 8.0f; // auto heat only at/below this in celcius
constexpr float HEATER_DEW_MARGIN_ON_C  = 2.0f;   // auto fires when temp - dew <= 2 C at camera power-up
constexpr float HEATER_DEW_MARGIN_OFF_C = 5.0f;   // kept for reference; the one-shot ends on its timer
constexpr bool  HEATER_AUTO_FIRE_ON_MANUAL_CAM_ON = true;  // true = also fire on a button/dashboard camera wake
static_assert(HEATER_MANUAL_MINUTES > 0, "HEATER_MANUAL_MINUTES must be positive");
static_assert(HEATER_AUTO_MINUTES > 0, "HEATER_AUTO_MINUTES must be positive");
static_assert(HEATER_DEW_MARGIN_OFF_C > HEATER_DEW_MARGIN_ON_C,
              "heater OFF margin must be greater than ON margin");

// --- Camera & IR Manual override -------------------------------------------------
constexpr int  MANUAL_MINUTES           = 10;  // auto-revert after __ minutes
constexpr bool BUTTON_ALSO_WAKES_CAMERA = true;

// --- OTA -------------------------------------------------------------
constexpr char OTA_HOSTNAME[] = "coopcam";
constexpr char OTA_PASSWORD[] = "higicoopcam";

// =====================================================================
//  OUTPUTS  —  hardware wiring. Edit only if the wiring changes.
// =====================================================================

// Camera 12 V: relay, Normally Open contacts.
// Most relay boards are ACTIVE-LOW: IN low = relay energised
constexpr int     PIN_CAMERA_RELAY = 4;      // GPIO4  -> relay IN1
constexpr uint8_t CAMERA_ON_LEVEL  = LOW;

// IR lamp: YX-MOS PWM trigger input.
constexpr int PIN_IR_LAMP = 5;               // GPIO5 -> YX-MOS control input

// Lens heater: YX-MOS low-side switch. Continuity test: + passes through, - switches.
constexpr int     PIN_HEATER      = 6;       // GPIO6 -> heater YX-MOS control input
constexpr uint8_t HEATER_ON_LEVEL = HIGH;    // HIGH = heater ON

// Status LED. Clone boards vary: 48 is common for the WS2812.
enum LedType { LED_NONE, LED_PLAIN, LED_WS2812 };
constexpr int     PIN_STATUS_LED   = 48;
constexpr LedType STATUS_LED_TYPE  = LED_WS2812;

// Manual IR toggle button.
constexpr int  PIN_BUTTON        = 10;       // GPIO10 -> manual IR toggle -> GND
constexpr bool BUTTON_ACTIVE_LOW = true;     // pressed = pin LOW (button to GND)

// I2C for the DS3231 & SHT4x temp sensor
constexpr int PIN_I2C_SDA = 8;               // GPIO8 -> DS3231 SDA (green)
constexpr int PIN_I2C_SCL = 9;               // GPIO9 -> DS3231 SCL (yellow)

// ── Wiring map ───────────────────────────────────────────────────────
// 12V+ -- fuse -- relay COM ; relay NO -- camera +12V
// 12V+ ------------------------------------ IR lamp +ve, heater +ve
// 12V+ -- YX-MOS OUT+ ; IR lamp -ve -- YX-MOS OUT-
// 12V+ -- YX-MOS OUT+ ; heater -ve   -- YX-MOS OUT- (GPIO6)
// 12V+ -- buck -- 5V -- ESP32 5V, relay VCC, YX-MOS VCC

// All GNDs common (PSU, ESP32, relay, YX-MOS, DS3231, camera)

// RELAY
//-----------
// GPIO4  -> relay data       (10k pull-up to keep relay off at boot)
// 3.3-5.0V =VCC
// GND

// YX-MOS SWITCH
// ---------
// GPIO5  -> YX-MOS Data      (10k pull-down to keep lamp off at boot)
// GND
//
// HEATER YX-MOS SWITCH
// ---------
// GPIO6  -> YX-MOS Data      (10k pull-down to keep heater off at boot)
// GND

// DS3231 RTC + SHT4x temp/humidity, shared I2C
// -----------
// GPIO8 -> DS3231 SDA + SHT4x SDA (green)
// GPIO9 -> DS3231 SCL + SHT4x SCL (yellow)
// 3.3V  -> VCC
// GND

// MOMENTARY SWITCH
// ------------
// GPIO10 -> manual IR toggle button 
// GND

// RESET
// ----------
// GPIO RST
// GND

// GPIO48 -> onboard WS2812 status LED (check your board variant)

constexpr int TZ_MODE = 1;
constexpr int LOCAL_UTC_OFFSET_MIN = 0;   // only used when TZ_MODE = 0

// The M-H5SVXW needs roughly this long after 12 V returns before RTSP answers.
constexpr uint8_t CAMERA_WAKE_SECONDS = 30;

// --- TEMP: preview the Watch card before the hardware arrives ---------
// true  = always show the card, even with CAMERA_VIEW_URL empty (tap is inert:
//         no relay override, no countdown, no navigation)
// false = hide the card whenever CAMERA_VIEW_URL is empty
constexpr bool SHOW_WATCH_CARD_ANYWAY = true;

// --- Network ---------------------------------------------------------
constexpr char NTP_SERVER_1[] = "pool.ntp.org";
constexpr char NTP_SERVER_2[] = "time.google.com";
constexpr unsigned long NTP_RTC_WRITE_INTERVAL_S = 24UL * 60UL * 60UL;

// --- Solar fallback (polar day / night) ------------------------------
constexpr int SOLAR_FALLBACK_SUNRISE_MIN = 6 * 60;
constexpr int SOLAR_FALLBACK_SUNSET_MIN  = 18 * 60;

// =====================================================================
//  Internals — nothing below here normally needs editing
// =====================================================================

RTC_DS3231 rtc;
SensirionI2cSht4x sht4x;
WebServer  server(80);

// RTClib keeps this private to its .cpp, so the low-level status reads below
// need their own copy.
constexpr uint8_t DS3231_I2C_ADDR = 0x68;
constexpr uint8_t SHT4X_I2C_ADDR  = 0x44;

// ---- state ----------------------------------------------------------
bool   rtcOk        = false;
bool   rtcTimeValid = false;
bool   haveTime     = false;
bool   solarOk      = false;

DateTime lastUtc;
DateTime lastLocal;
int      localMinNow  = 0;
int      sunriseLocal = 0;
int      sunsetLocal  = 0;
int      irStart      = 0;
int      irEnd        = 0;
int      cachedDay    = -1;
int      cachedOffset = -999;
int      localOffsetNow = 0;

bool   appliedCam    = false;
bool   appliedLamp   = false;
bool   appliedHeater = false;
bool   heaterWant    = false;   // latest decision, before it is applied

// Lens heater state. Auto is a one-shot fired on a camera OFF->ON edge when the
// dew point calls for it; it never re-fires while the camera stays on. Manual is
// a separate one-button timer that takes over from auto. Sensor failure or the
// high-temperature cut-out always forces the heater OFF.
bool     heaterManualActive  = false;
uint32_t heaterManualUntilMs = 0;
bool     heaterAutoActive    = false;
uint32_t heaterAutoUntilMs   = 0;
bool     heaterCamWasOn      = false;   // previous camera state, for edge detection
bool     heaterSensorOk      = false;
float    heaterDewPointC     = NAN;
float    heaterGapC          = NAN;
String   heaterReason        = "off";

uint8_t irBrightness = IR_DEFAULT_BRIGHTNESS;  // runtime target; resets to .ino setting at boot
bool irPwmReady = false;

// Current PWM output and non-blocking fade state (percentage, 0-100).
float irOutputPercent = 0.0f;
float irRampStartPercent = 0.0f;
float irRampTargetPercent = 0.0f;
uint32_t irRampStartMs = 0;
uint32_t irRampLastUpdateMs = 0;
bool irRampActive = false;

bool   overrideActive = false;
bool   overrideLamp   = false;
bool   overrideCamera = false;
time_t overrideUntil  = 0;

int    simMinutes   = -1;        // -1 = off
time_t lastRtcWrite = 0;

bool     ntpStarted       = false;
bool     otaStarted       = false;
bool     wifiWasConnected = false;
uint32_t lastWifiAttempt  = 0;
uint32_t lastTickMs       = 0;

String timeStr, sunriseStr, sunsetStr, nextEventStr, dateStr;

// ---- SHT4x state, RAM only ------------------------------------------
// Seven samples are kept so the trend can compare the newest with the oldest
// of the last three. At a 7-minute read that window spans 14 minutes.
enum Trend : uint8_t { TREND_NONE, TREND_UP, TREND_DOWN, TREND_STEADY };
struct SensorSample { float t; float h; uint32_t ms; };

constexpr uint8_t  SENSOR_HISTORY      = 7;
constexpr uint8_t  SENSOR_TREND_SAMPLES = 3;
constexpr uint32_t SENSOR_TREND_MIN_MS = 12UL * 60UL * 1000UL;
constexpr float    SENSOR_TEMP_DEADBAND = 0.3f;   // deg C
constexpr float    SENSOR_HUM_DEADBAND  = 1.5f;   // %RH

// The heater must not rely on a reading older than one normal read plus margin.
const uint32_t HEATER_SENSOR_MAX_AGE_MS = SENSOR_READ_MS + 60000UL;

bool     sensorOk       = false;   // last read succeeded
bool     sensorProbed   = false;   // answered at boot
uint32_t sensorSerial   = 0;
float    sensorTempC    = NAN;
float    sensorHum      = NAN;
float    sensorTempMin  = NAN;
float    sensorTempMax  = NAN;
float    sensorHumMin   = NAN;
float    sensorHumMax   = NAN;
int      sensorTempMinAtMin = -1;
int      sensorTempMaxAtMin = -1;
int      sensorHumMinAtMin  = -1;
int      sensorHumMaxAtMin  = -1;
int      sensorLastAtMin    = -1;
int      sensorDayKey       = -1;   // year * 10000 + month * 100 + day
uint32_t sensorLastReadMs   = 0;

Trend sensorTempTrend = TREND_NONE;
Trend sensorHumTrend  = TREND_NONE;
SensorSample sensorHist[SENSOR_HISTORY];
uint8_t sensorHistCount = 0;
uint8_t sensorHistHead  = 0;

// ---- small helpers --------------------------------------------------
static inline int mod1440(int x) { x %= 1440; if (x < 0) x += 1440; return x; }
static inline double deg2rad(double d) { return d * M_PI / 180.0; }
static inline double rad2deg(double r) { return r * 180.0 / M_PI; }

static inline bool inWindow(int now, int start, int end) {
  if (start == end) return false;
  if (start < end)  return now >= start && now < end;
  return now >= start || now < end;          // wraps midnight
}

String fmtMin(int m) {
  char b[8];
  snprintf(b, sizeof(b), "%02d:%02d", (m / 60) % 24, m % 60);
  return String(b);
}

// "10:00-15:00", or "" when the window is disabled (start == end).
String windowStr(int start, int end) {
  if (start == end) return String();
  return fmtMin(start) + "-" + fmtMin(end);
}

// Magnus approximation: dew point in degrees C from air temp and RH.
float dewPointC(float t, float rh) {
  if (isnan(t) || isnan(rh)) return NAN;
  rh = constrain(rh, 1.0f, 100.0f);
  const float a = 17.62f;
  const float b = 243.12f;
  float g = logf(rh / 100.0f) + (a * t) / (b + t);
  return (b * g) / (a - g);
}

// True only for a fresh, valid SHT4x sample. Stale counts as sensor failure.
bool heaterSensorFresh() {
  if (!SENSOR_ENABLED || !sensorOk) return false;
  if (isnan(sensorTempC) || isnan(sensorHum)) return false;
  if (sensorTempC < -40.0f || sensorTempC > 125.0f) return false;
  if (sensorHum < 0.0f || sensorHum > 100.0f) return false;
  return (millis() - sensorLastReadMs) <= HEATER_SENSOR_MAX_AGE_MS;
}

String uptimeStr() {
  uint32_t s = millis() / 1000;
  char b[32];
  snprintf(b, sizeof(b), "%lud %02lu:%02lu:%02lu",
           (unsigned long)(s / 86400), (unsigned long)((s / 3600) % 24),
           (unsigned long)((s / 60) % 60), (unsigned long)(s % 60));
  return String(b);
}

// ---- solar (NOAA) ---------------------------------------------------
double julianDay(int y, int m, int d) {
  if (m <= 2) { y -= 1; m += 12; }
  int A = y / 100;
  int B = 2 - A + A / 4;
  return floor(365.25 * (y + 4716)) + floor(30.6001 * (m + 1)) + d + B - 1524.5;
}

// Returns minutes since 00:00 UTC. False if the sun never rises/sets.
bool solarTimes(int y, int m, int d, double lat, double lon,
                int &sunriseUTC, int &sunsetUTC) {
  double jd = julianDay(y, m, d);
  double T  = (jd - 2451545.0) / 36525.0;

  double L0 = fmod(280.46646 + T * (36000.76983 + T * 0.0003032), 360.0);
  if (L0 < 0) L0 += 360.0;
  double M  = 357.52911 + T * (35999.05029 - 0.0001537 * T);
  double e  = 0.016708634 - T * (0.000042037 + 0.0000001267 * T);
  double C  = sin(deg2rad(M)) * (1.914602 - T * (0.004817 + 0.000014 * T))
            + sin(deg2rad(2 * M)) * (0.019993 - 0.000101 * T)
            + sin(deg2rad(3 * M)) * 0.000289;
  double trueLong = L0 + C;
  double omega  = 125.04 - 1934.136 * T;
  double lambda = trueLong - 0.00569 - 0.00478 * sin(deg2rad(omega));
  double eps0 = 23.0 + (26.0 + (21.448 - T * (46.815 + T * (0.00059 - T * 0.001813))) / 60.0) / 60.0;
  double eps  = eps0 + 0.00256 * cos(deg2rad(omega));
  double decl = rad2deg(asin(sin(deg2rad(eps)) * sin(deg2rad(lambda))));

  double yy = tan(deg2rad(eps / 2.0)); yy *= yy;
  double eqTime = 4.0 * rad2deg(
        yy * sin(2 * deg2rad(L0))
      - 2 * e * sin(deg2rad(M))
      + 4 * e * yy * sin(deg2rad(M)) * cos(2 * deg2rad(L0))
      - 0.5 * yy * yy * sin(4 * deg2rad(L0))
      - 1.25 * e * e * sin(2 * deg2rad(M)));

  double cosHA = cos(deg2rad(90.833)) / (cos(deg2rad(lat)) * cos(deg2rad(decl)))
               - tan(deg2rad(lat)) * tan(deg2rad(decl));
  if (cosHA > 1.0 || cosHA < -1.0) return false;   // polar night / day

  double ha   = rad2deg(acos(cosHA));
  double noon = 720.0 - 4.0 * lon - eqTime;        // minutes UTC
  sunriseUTC = (int)lround(noon - 4.0 * ha);
  sunsetUTC  = (int)lround(noon + 4.0 * ha);
  return true;
}

// ---- local time zone ------------------------------------------------
// Day of week, 0 = Sunday .. 6 = Saturday (Sakamoto).
int dayOfWeek(int y, int m, int d) {
  static const uint8_t t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

int daysInMonth(int y, int m) {
  static const uint8_t d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
  return d[m - 1];
}

// Offset from UTC in minutes for this UTC instant.
int localOffsetMin(const DateTime &utc) {
  if (TZ_MODE != 1) return LOCAL_UTC_OFFSET_MIN;

  int y = utc.year();

  int lastMar = daysInMonth(y, 3);
  lastMar -= dayOfWeek(y, 3, lastMar);          // last Sunday of March
  int lastOct = daysInMonth(y, 10);
  lastOct -= dayOfWeek(y, 10, lastOct);          // last Sunday of October

  DateTime bstStart(y, 3,  lastMar, 1, 0, 0);    // 01:00 UTC
  DateTime bstEnd  (y, 10, lastOct, 1, 0, 0);    // 01:00 UTC

  return (utc >= bstStart && utc < bstEnd) ? 60 : 0;
}

const char *tzName() {
  if (TZ_MODE != 1) return "fixed";
  return localOffsetNow ? "BST" : "GMT";
}

void computeSolarFor(const DateTime &local, int offMin) {
  int key = local.year() * 10000 + local.month() * 100 + local.day();
  if (key == cachedDay && offMin == cachedOffset) return;
  cachedDay    = key;
  cachedOffset = offMin;

  int sr, ss;
  if (solarTimes(local.year(), local.month(), local.day(),
                 LATITUDE, LONGITUDE, sr, ss)) {
    solarOk      = true;
    sunriseLocal = mod1440(sr + offMin);
    sunsetLocal  = mod1440(ss + offMin);
  } else {
    solarOk      = false;
    sunriseLocal = SOLAR_FALLBACK_SUNRISE_MIN;
    sunsetLocal  = SOLAR_FALLBACK_SUNSET_MIN;
  }
  irStart = mod1440(sunsetLocal - IR_ON_BEFORE_SUNSET_MIN);
  irEnd   = mod1440(sunsetLocal + IR_OFF_AFTER_SUNSET_MIN);
}

// The firmware build date is a reliable anchor: it is flashed within days of
// being compiled, so a believable RTC time lies from just before that day to a
// couple of decades after. A blank or corrupt module happily returns nonsense
// (this one returned 2051), which must never become the system clock.
DateTime buildAnchor() {
  static DateTime anchor(2024, 1, 1, 0, 0, 0);
  static bool ready = false;
  if (ready) return anchor;
  ready = true;

  static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char *s = __DATE__;                 // e.g. "Nov 13 2026"
  int mon = 1;
  for (int i = 0; i < 12; i++) {
    if (strncmp(s, months + i * 3, 3) == 0) { mon = i + 1; break; }
  }
  int day  = atoi(s + 4);
  int year = atoi(s + 7);
  if (year >= 2024) anchor = DateTime(year, mon, day, 0, 0, 0);
  return anchor;
}

// A reading is believable only if it sits near the build date. A stopped
// oscillator also keeps returning the same second forever, which the write
// verification below catches.
bool rtcTimeSane(const DateTime &d) {
  if (d.year() < 2024) return false;
  int64_t ahead = (int64_t)d.unixtime() - (int64_t)buildAnchor().unixtime();
  return ahead > -(int64_t)86400 && ahead < (int64_t)20 * 365 * 86400;
}

// Reads the DS3231 control (0x0E) and status (0x0F) registers. Only called when
// a time write does not stick, so the log can say why. Returns 0xFF on failure,
// otherwise the control byte; the status byte is returned through `status`.
uint8_t rtcReadControl(uint8_t &status) {
  status = 0;
  Wire.beginTransmission(DS3231_I2C_ADDR);
  Wire.write((uint8_t)0x0E);
  if (Wire.endTransmission(false) != 0) return 0xFF;
  if (Wire.requestFrom((uint8_t)DS3231_I2C_ADDR, (uint8_t)2) != 2) return 0xFF;
  uint8_t ctrl = Wire.read();
  status = Wire.read();
  return ctrl;
}

// Raw dump of the seven time registers, for the log when a write will not
// stick. A real DS3231 puts a BCD year (e.g. 26) in the last byte.
bool rtcReadRaw(uint8_t out[7]) {
  Wire.beginTransmission(DS3231_I2C_ADDR);
  Wire.write((uint8_t)0x00);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)DS3231_I2C_ADDR, (uint8_t)7) != 7) return false;
  for (int i = 0; i < 7; i++) out[i] = Wire.read();
  return true;
}

// The ESP32 clock, once NTP has set it, is the authority: it always advances
// and NTP keeps correcting it. The DS3231 is persistence — it carries the time
// across a power cut, nothing more. A bad RTC reading must never be allowed to
// drag the whole sketch's clock backwards, which is what used to happen.
bool getNow(DateTime &utc) {
  time_t t = time(nullptr);
  if (t > 1700000000) { utc = DateTime((uint32_t)t); return true; }

  // Before the system clock exists, the RTC is the only possible source.
  if (rtcOk) {
    DateTime d = rtc.now();
    if (rtcTimeSane(d)) { utc = d; return true; }
  }
  return false;
}

// Used by RemoteLog.h to stamp each log line. Normally reads the cached local
// time that tick() already calculated. Before the first tick — or at boot when
// only the DS3231 knows the time — it falls back to the RTC directly, so the
// earliest log lines can still carry a real time.
bool getTime(struct tm& ti) {
  if (haveTime) {
    ti.tm_hour = lastLocal.hour();
    ti.tm_min  = lastLocal.minute();
    ti.tm_sec  = lastLocal.second();
    return true;
  }

  DateTime utc;
  if (!getNow(utc)) return false;
  DateTime local = utc + TimeSpan((int32_t)localOffsetMin(utc) * 60);
  ti.tm_hour = local.hour();
  ti.tm_min  = local.minute();
  ti.tm_sec  = local.second();
  return true;
}

// ---- outputs --------------------------------------------------------
void writeIrDuty(float percent) {
  if (!irPwmReady) {
    digitalWrite(PIN_IR_LAMP, LOW);  // fail safe: IR lamp remains off
    return;
  }

  if (percent < 0.0f) percent = 0.0f;
  if (percent > 100.0f) percent = 100.0f;
  uint32_t duty = (uint32_t)(percent * 255.0f / 100.0f + 0.5f);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_IR_LAMP, duty);      // Arduino-ESP32 3.x uses the pin
#else
  ledcWrite(0, duty);                // Arduino-ESP32 2.x uses the channel
#endif
}

void beginIrRamp(float targetPercent) {
  irRampStartPercent = irOutputPercent;
  irRampTargetPercent = targetPercent;
  irRampStartMs = millis();
  irRampLastUpdateMs = irRampStartMs;
  irRampActive = (irRampStartPercent != irRampTargetPercent);
  if (!irRampActive) {
    irOutputPercent = irRampTargetPercent;
    writeIrDuty(irOutputPercent);
  }
}

void serviceIrRamp() {
  if (!irRampActive || !irPwmReady) return;

  uint32_t nowMs = millis();
  uint32_t elapsed = nowMs - irRampStartMs;
  if (elapsed < IR_RAMP_TIME_MS && (nowMs - irRampLastUpdateMs) < 20U) return;
  irRampLastUpdateMs = nowMs;

  if (elapsed >= IR_RAMP_TIME_MS) {
    irOutputPercent = irRampTargetPercent;
    irRampActive = false;
  } else {
    float progress = (float)elapsed / (float)IR_RAMP_TIME_MS;
    irOutputPercent = irRampStartPercent
                    + (irRampTargetPercent - irRampStartPercent) * progress;
  }
  writeIrDuty(irOutputPercent);
}

bool adjustIrBrightness(int change) {
  if (!appliedLamp) return false;    // brightness may only be adjusted while lamp is on

  int next = (int)irBrightness + change;
  if (next < 0) next = 0;
  if (next > 100) next = 100;
  if (next == irBrightness) return true;

  irBrightness = (uint8_t)next;
  beginIrRamp((float)irBrightness);  // smoothly move to the new target brightness
  return true;
}

void applyOutputs(bool cam, bool lamp, bool heat) {
  static bool curCam = false, curLamp = false, curHeat = false;
  if (!irPwmReady) lamp = false;       // never report or request IR ON if PWM setup failed
  if (cam != curCam) {
    digitalWrite(PIN_CAMERA_RELAY, cam ? CAMERA_ON_LEVEL : !CAMERA_ON_LEVEL);
    Log.printf("Camera: %s (%s)\n", cam ? "ON" : "OFF",
               overrideActive ? "manual" : "schedule");
    curCam = cam;
  }
  if (lamp != curLamp) {
    beginIrRamp(lamp ? (float)irBrightness : 0.0f);
    curLamp = lamp;
  }
  if (heat != curHeat) {
    digitalWrite(PIN_HEATER, heat ? HEATER_ON_LEVEL : !HEATER_ON_LEVEL);
    Log.printf("Heat: %s (%s)\n", heat ? "ON" : "OFF", heaterReason.c_str());
    curHeat = heat;
  }
  appliedCam    = cam;
  appliedLamp   = lamp;
  appliedHeater = heat;
}

// ---- lens heater -----------------------------------------------------
static bool timerExpired(bool active, uint32_t untilMs) {
  return active && (int32_t)(millis() - untilMs) >= 0;
}

static int timerMinutesLeft(bool active, uint32_t untilMs) {
  if (!active) return 0;
  long rem = (long)(untilMs - millis());
  if (rem < 0) rem = 0;
  return (int)((rem + 59999L) / 60000L);
}

// Remaining minutes of whichever timer is currently driving the heater.
int heaterMinutesLeft() {
  int manual = timerMinutesLeft(heaterManualActive, heaterManualUntilMs);
  int autoM  = timerMinutesLeft(heaterAutoActive, heaterAutoUntilMs);
  return manual > autoM ? manual : autoM;
}

int heaterAutoMinutesLeft() {
  return timerMinutesLeft(heaterAutoActive, heaterAutoUntilMs);
}

// The heater tile is the only control. Tap once for a timed manual ON, tap
// again to cancel. A manual press takes over from any running auto one-shot.
// Sensor failure and the high-temperature cut-out both win.
void toggleHeaterManual() {
  if (!HEATER_ENABLED) return;

  if (heaterManualActive) {
    heaterManualActive = false;
    Log.println(F("Heat: manual cancelled"));
    return;
  }

  if (!heaterSensorFresh()) {
    Log.println(F("Heat: manual refused (sensor invalid)"));
    return;
  }
  if (sensorTempC >= HEATER_MAX_AMBIENT_C) {
    Log.println(F("Heat: manual refused (too warm)"));
    return;
  }

  heaterAutoActive    = false;   // manual press replaces the auto one-shot
  heaterManualActive  = true;
  heaterManualUntilMs = millis() + (uint32_t)HEATER_MANUAL_MINUTES * 60000UL;
  Log.printf("Heat: manual ON (%dm)\n", HEATER_MANUAL_MINUTES);
}

// Called every second from tick() (and from the web handlers via tick()).
// Auto is ONE timed run on a camera OFF->ON edge when the dew point calls for
// it; a falling dew gap never re-triggers it while the camera stays on.
// `allowAutoOnEdge` says whether this edge may start auto heat. Scheduled and
// manual/button wakes both allow it; the no-clock fallback does not.
void heaterEvaluate(bool camOn, bool allowAutoOnEdge) {
  heaterSensorOk  = heaterSensorFresh();
  heaterDewPointC = NAN;
  heaterGapC      = NAN;

  bool camRose = camOn && !heaterCamWasOn;
  heaterCamWasOn = camOn;
  bool autoEdge = camRose && allowAutoOnEdge;

  bool manualExpired = timerExpired(heaterManualActive, heaterManualUntilMs);
  bool autoExpired   = timerExpired(heaterAutoActive, heaterAutoUntilMs);
  if (manualExpired) heaterManualActive = false;
  if (autoExpired)   heaterAutoActive   = false;

  bool  want   = false;
  String reason;

  if (!HEATER_ENABLED) {
    reason = "disabled";
  } else if (!heaterSensorOk) {
    if (heaterManualActive || heaterAutoActive) {
      heaterManualActive = false;
      heaterAutoActive   = false;
      reason = "sensor invalid, heat cancelled";
    } else {
      reason = "sensor invalid";
    }
  } else {
    heaterDewPointC = dewPointC(sensorTempC, sensorHum);

    if (isnan(heaterDewPointC)) {
      if (heaterManualActive || heaterAutoActive) {
        heaterManualActive = false;
        heaterAutoActive   = false;
        reason = "sensor invalid, heat cancelled";
      } else {
        reason = "sensor invalid";
      }
    } else if (sensorTempC >= HEATER_MAX_AMBIENT_C) {
      heaterGapC = sensorTempC - heaterDewPointC;
      if (heaterManualActive || heaterAutoActive) {
        heaterManualActive = false;
        heaterAutoActive   = false;
        reason = "too warm, heat cancelled";
      } else {
        reason = "too warm";
      }
    } else {
      heaterGapC = sensorTempC - heaterDewPointC;

      if (autoEdge && !heaterManualActive && !heaterAutoActive &&
          sensorTempC <= HEATER_AUTO_MAX_AMBIENT_C &&
          heaterGapC <= HEATER_DEW_MARGIN_ON_C) {
        heaterAutoActive  = true;
        heaterAutoUntilMs = millis() + (uint32_t)HEATER_AUTO_MINUTES * 60000UL;
        Log.printf("Heat: auto ON (%dm, gap %.1fC)\n", HEATER_AUTO_MINUTES, heaterGapC);
      }

      if (heaterManualActive) {
        want   = true;
        reason = "manual " + String(HEATER_MANUAL_MINUTES) + "m";
      } else if (heaterAutoActive) {
        want   = true;
        reason = "auto " + String(HEATER_AUTO_MINUTES) + "m";
      } else if (!camOn) {
        reason = "camera off";
      } else if (sensorTempC > HEATER_AUTO_MAX_AMBIENT_C) {
        reason = "auto temp high";
      } else if (heaterGapC > HEATER_DEW_MARGIN_ON_C) {
        reason = "dry";
      } else {
        reason = "auto spent";
      }
    }
  }

  if (manualExpired && !want && appliedHeater) reason = "manual timer expired";
  if (autoExpired   && !want && appliedHeater) reason = "auto timer expired";
  heaterReason = reason;
  heaterWant   = want;
}

String heaterHeatLabel(bool on) {
  return on ? String("Heat ON") : String("Heat OFF");
}

// ---- override -------------------------------------------------------
String overrideStr() {
  if (!overrideActive) return "auto";
  time_t now = haveTime ? (time_t)lastUtc.unixtime() : 0;
  long rem = now ? (long)(overrideUntil - now) : 0;
  if (rem < 0) rem = 0;
  String s = overrideLamp ? "lamp ON" : "lamp OFF";
  s += ", cam "; s += overrideCamera ? "ON" : "OFF";
  s += ", "; s += String(rem / 60); s += "m left";
  return s;
}

int overrideMinutesLeft() {
  if (!overrideActive) return 0;
  time_t now = haveTime ? (time_t)lastUtc.unixtime() : 0;
  long rem = now ? (long)(overrideUntil - now) : 0;
  if (rem < 0) rem = 0;
  return (int)((rem + 59) / 60);          // round up so it never shows 0m while active
}

void startOverride(bool lamp, bool cam) {
  overrideActive = true;
  overrideLamp   = lamp;
  overrideCamera = cam;
  time_t now = haveTime ? (time_t)lastUtc.unixtime() : 0;
  overrideUntil = now ? (now + (time_t)MANUAL_MINUTES * 60) : (time_t)0x7FFFFFFF;
}

// ---- status LED -----------------------------------------------------
void setLed(uint8_t r, uint8_t g, uint8_t b) {
  static uint8_t lr = 255, lg = 255, lb = 255;
  if (r == lr && g == lg && b == lb) return;
  lr = r; lg = g; lb = b;
  if (STATUS_LED_TYPE == LED_WS2812)     neopixelWrite(PIN_STATUS_LED, r, g, b);
  else if (STATUS_LED_TYPE == LED_PLAIN) digitalWrite(PIN_STATUS_LED, (r | g | b) ? HIGH : LOW);
}

void ledTask() {
  uint32_t t = millis();
  uint8_t r = 0, g = 0, b = 0;

  if (!haveTime) {
    if ((t / 200) % 2) r = 60;                       // fast red = no time
  } else if (overrideActive) {
    b = 60;                                          // solid blue = override
  } else if (WiFi.status() != WL_CONNECTED) {
    uint32_t p = t % 2000;
    if (p < 100 || (p > 220 && p < 320)) { r = 50; g = 25; }  // double amber
  } else {
    if ((t % 3000) < 70) g = 40;                     // heartbeat green
  }
  setLed(r, g, b);
}

// ---- button ---------------------------------------------------------
void onButton() {
  bool newLamp = !appliedLamp;
  bool newCam  = newLamp ? true : appliedCam;
  if (BUTTON_ALSO_WAKES_CAMERA && newLamp) newCam = true;
  startOverride(newLamp, newCam);
  Log.printf("Button: override lamp=%d cam=%d\n", newLamp, newCam);
}

void buttonTask() {
  static bool     rawLast      = HIGH;
  static bool     pressedLatch = false;
  static uint32_t lastChange   = 0;

  bool raw = digitalRead(PIN_BUTTON);
  if (raw != rawLast) { rawLast = raw; lastChange = millis(); }
  if (millis() - lastChange < 40) return;

  bool pressed = BUTTON_ACTIVE_LOW ? (raw == LOW) : (raw == HIGH);
  if (pressed && !pressedLatch) { pressedLatch = true; onButton(); }
  if (!pressed) pressedLatch = false;
}

// ---- next event / status strings ------------------------------------
void computeNextEvent(int nowMin) {
  int evOn  = IR_ALL_NIGHT ? sunsetLocal : irStart;
  int evOff = IR_ALL_NIGHT ? sunriseLocal : irEnd;

  struct Ev { int at; const char *name; };
  Ev evs[6];
  int n = 0;

  int offStart  = CAM_OFF_FROM_H  * 60 + CAM_OFF_FROM_M;
  int offEnd    = CAM_OFF_TO_H    * 60 + CAM_OFF_TO_M;
  int off2Start = CAM_OFF2_FROM_H * 60 + CAM_OFF2_FROM_M;
  int off2End   = CAM_OFF2_TO_H   * 60 + CAM_OFF2_TO_M;

  if (offStart != offEnd) {
    evs[n++] = { offStart, "Camera OFF" };
    evs[n++] = { offEnd,   "Camera ON"  };
  }
  if (off2Start != off2End) {
    evs[n++] = { off2Start, "Camera OFF" };
    evs[n++] = { off2End,   "Camera ON"  };
  }
  evs[n++] = { evOn,  "IR ON"  };
  evs[n++] = { evOff, "IR OFF" };

  int best = 99999;
  const char *bestName = "";
  for (int i = 0; i < n; i++) {
    int d = mod1440(evs[i].at - nowMin);
    if (d == 0) d = 1440;
    if (d < best) { best = d; bestName = evs[i].name; }
  }
  nextEventStr = String(bestName) + " in " + String(best / 60) + "h" + String(best % 60) + "m";
}

void updateStatus(const DateTime &local) {
  static const char *WD[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char *MO[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                             "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  char b[16];
  snprintf(b, sizeof(b), "%02d:%02d:%02d", local.hour(), local.minute(), local.second());
  timeStr    = b;
  sunriseStr = fmtMin(sunriseLocal);
  sunsetStr  = fmtMin(sunsetLocal);
  snprintf(b, sizeof(b), "%s %d %s", WD[local.dayOfTheWeek()], local.day(),
           MO[local.month() - 1]);
  dateStr = b;
}

// Colour band for the dashboard header (same phases as the Hen House page).
const char *dayPhaseName() {
  if (!haveTime || !solarOk) return "night";
  int now = localMinNow;
  int sr  = sunriseLocal;
  int ss  = sunsetLocal;

  if (now < sr - 60 || now >= ss + 30) return "night";
  if (now < sr)      return "preDawn";
  if (now < sr + 30) return "sunrise";

  int midday = (sr + ss) / 2;
  if (now < midday - 60) return "morning";
  if (now < midday + 60) return "midday";
  if (now < ss - 30)     return "afternoon";
  return "sunset";
}

String buildJson() {
  String j;
  j.reserve(1400);
  j += "{\"version\":\"";   j += FW_VERSION;
  j += "\",\"time\":\"";    j += timeStr;
  j += "\",\"tz\":\"";      j += tzName();
  j += "\",\"date\":\"";    j += dateStr;
  j += "\",\"sunrise\":\""; j += sunriseStr;
  j += "\",\"sunset\":\"";  j += sunsetStr;
  j += "\",\"next\":\"";    j += nextEventStr;
  j += "\",\"camOff1\":\""; j += windowStr(CAM_OFF_FROM_H  * 60 + CAM_OFF_FROM_M,
                                             CAM_OFF_TO_H    * 60 + CAM_OFF_TO_M);
  j += "\",\"camOff2\":\""; j += windowStr(CAM_OFF2_FROM_H * 60 + CAM_OFF2_FROM_M,
                                             CAM_OFF2_TO_H   * 60 + CAM_OFF2_TO_M);
  j += "\",\"irOn\":\"";    j += fmtMin(irStart);
  j += "\",\"irOff\":\"";   j += fmtMin(irEnd);
  j += "\",\"camera\":";    j += appliedCam  ? "true" : "false";
  j += ",\"lamp\":";        j += appliedLamp ? "true" : "false";
  j += ",\"heater\":";       j += appliedHeater  ? "true" : "false";
  j += ",\"heaterEnabled\":"; j += HEATER_ENABLED ? "true" : "false";
  j += ",\"heaterManualMinutes\":"; j += HEATER_MANUAL_MINUTES;
  j += ",\"heaterAutoMinutes\":";   j += HEATER_AUTO_MINUTES;
  j += ",\"heaterManual\":"; j += heaterManualActive ? "true" : "false";
  j += ",\"heaterAuto\":";   j += heaterAutoActive ? "true" : "false";
  j += ",\"heaterMin\":";    j += heaterMinutesLeft();
  j += ",\"heaterAutoMin\":"; j += heaterAutoMinutesLeft();
  j += ",\"heaterReason\":\""; j += heaterReason; j += "\"";
  j += ",\"dewPoint\":";     j += isnan(heaterDewPointC) ? "null"
                                                         : String(heaterDewPointC, 1);
  j += ",\"heaterGap\":";    j += isnan(heaterGapC) ? "null"
                                                    : String(heaterGapC, 1);
  j += ",\"irBrightness\":"; j += String(irBrightness);
  j += ",\"overrideActive\":"; j += overrideActive ? "true" : "false";
  j += ",\"override\":\"";  j += overrideStr();
  j += "\",\"overrideMin\":"; j += overrideMinutesLeft();
  j += ",\"rssi\":";      j += String(WiFi.RSSI());
  j += ",\"uptime\":\"";    j += uptimeStr();
  j += "\",\"rtc\":\"";     j += rtcOk ? (rtcTimeValid ? "ok" : "invalid")
                                          : (haveTime ? "none, NTP" : "missing");
  j += "\",\"solar\":\"";   j += solarOk ? "ok" : "fallback";
  j += "\",\"sim\":\"";     j += (simMinutes >= 0) ? fmtMin(simMinutes) : "off";
  j += "\",\"dayPhase\":\""; j += dayPhaseName();
  j += "\",\"sensor\":\"";  j += !SENSOR_ENABLED ? "off" : (sensorOk ? "ok" : "missing");
  j += "\",\"sensorAt\":\""; j += sensorLastAtMin >= 0 ? fmtMin(sensorLastAtMin)
                                                       : String("--:--");
  j += "\",\"temp\":";      j += isnan(sensorTempC) ? "null" : String(sensorTempC, 1);
  j += ",\"hum\":";         j += isnan(sensorHum)   ? "null" : String((int)lroundf(sensorHum));
  j += ",\"tempMin\":";     j += isnan(sensorTempMin) ? "null" : String(sensorTempMin, 1);
  j += ",\"tempMinAt\":\""; j += sensorTempMinAtMin >= 0 ? fmtMin(sensorTempMinAtMin)
                                                         : String("--:--");
  j += "\",\"tempMax\":";   j += isnan(sensorTempMax) ? "null" : String(sensorTempMax, 1);
  j += ",\"tempMaxAt\":\""; j += sensorTempMaxAtMin >= 0 ? fmtMin(sensorTempMaxAtMin)
                                                         : String("--:--");
  j += "\",\"humMin\":";    j += isnan(sensorHumMin) ? "null" : String((int)lroundf(sensorHumMin));
  j += ",\"humMinAt\":\"";  j += sensorHumMinAtMin >= 0 ? fmtMin(sensorHumMinAtMin)
                                                         : String("--:--");
  j += "\",\"humMax\":";    j += isnan(sensorHumMax) ? "null" : String((int)lroundf(sensorHumMax));
  j += ",\"humMaxAt\":\"";  j += sensorHumMaxAtMin >= 0 ? fmtMin(sensorHumMaxAtMin)
                                                         : String("--:--");
  j += "\",\"tempTrend\":\""; j += trendName(sensorTempTrend);
  j += "\",\"humTrend\":\"";  j += trendName(sensorHumTrend);
  j += "\"}";
  return j;
}

// ---- network / NTP --------------------------------------------------
void netTask() {
  uint32_t ms = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if (wifiWasConnected) { wifiWasConnected = false; ntpStarted = false; }
    if (ms - lastWifiAttempt > 30000) {
      lastWifiAttempt = ms;
      Log.println(F("WiFi: connecting..."));
      WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
    return;
  }

  if (!wifiWasConnected) {
    wifiWasConnected = true;
    Log.print(F("WiFi: ")); Log.println(WiFi.localIP());
    if (otaStarted) ArduinoOTA.end();
    ArduinoOTA.begin();
    otaStarted = true;
  }

  if (!ntpStarted) {
    configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);
    ntpStarted = true;
    Log.println(F("NTP: started"));
  }

  // Reconcile the DS3231 with the ESP32 clock. NTP owns the time; the RTC only
  // needs an occasional write so it survives a power cut. A module that answers
  // but cannot hold a time must never be written every few seconds — that is
  // what produced the endless "NTP: RTC set (drift -818987200 s)" lines.
  static uint32_t lastNtpCheck   = 0;
  static uint32_t lastRtcAttempt = 0;
  static uint8_t  rtcWriteFails  = 0;
  static bool     rtcWarned      = false;

  if (ms - lastNtpCheck < 10000) return;
  lastNtpCheck = ms;

  time_t t = time(nullptr);
  if (t <= 1700000000 || !rtcOk) return;

  DateTime rn = rtc.now();
  bool rtcSane = rtcTimeSane(rn);

  int64_t drift = rtcSane ? (int64_t)t - (int64_t)rn.unixtime() : 0;
  bool due = !rtcTimeValid
          || !rtcSane
          || ((int64_t)t - (int64_t)lastRtcWrite >= (int64_t)NTP_RTC_WRITE_INTERVAL_S)
          || (drift > 120 || drift < -120);
  if (!due) return;

  // First sync is immediate. After that a write happens at most every 10 min,
  // and once the module has failed three times only every 6 hours.
  uint32_t minGap = (rtcWriteFails >= 3) ? 6UL * 60UL * 60UL * 1000UL
                                         : 10UL * 60UL * 1000UL;
  if (rtcTimeValid && lastRtcWrite != 0 &&
      (uint32_t)(ms - lastRtcAttempt) < minGap) return;
  lastRtcAttempt = ms;

  bool wasValid = rtcTimeValid;
  bool firstSet = !wasValid;

  // Compares the module against the value just written. A single I2C glitch
  // must not condemn it, so read a few times before believing a failure.
  auto storedOk = [&]() -> bool {
    for (int i = 0; i < 3; i++) {
      delay(5);
      DateTime v = rtc.now();
      int64_t d = (int64_t)t - (int64_t)v.unixtime();
      if (rtcTimeSane(v) && d >= -5 && d <= 5) return true;
    }
    return false;
  };

  // RTClib 2.x returns void from adjust(), so the read-back is the only way to
  // know whether the module actually stored the time.
  rtc.adjust(DateTime((uint32_t)t));
  bool stored = storedOk();

  if (!stored) {
    uint8_t status;
    uint8_t ctrl = rtcReadControl(status);
    // EOSC set (0x80 in the control register) means the oscillator is switched
    // off. Clear it and try once more before giving up.
    if (ctrl != 0xFF && (ctrl & 0x80)) {
      Wire.beginTransmission(DS3231_I2C_ADDR);
      Wire.write((uint8_t)0x0E);
      Wire.write((uint8_t)(ctrl & 0x7F));
      Wire.endTransmission();
      rtc.adjust(DateTime((uint32_t)t));
      stored = storedOk();
    }
    if (!stored) {
      // Say it on the very first attempt, then stay quiet unless it keeps
      // failing. The register dump is what tells a dead module from a clone.
      if (!rtcWarned && (firstSet || ++rtcWriteFails >= 3)) {
        rtcWarned = true;
        Log.printf("RTC: will not hold time (control=0x%02X status=0x%02X); check coin cell and module\n",
                   ctrl == 0xFF ? 0 : ctrl, status);
        uint8_t raw[7];
        if (rtcReadRaw(raw)) {
          Log.printf("RTC: registers %02X %02X %02X %02X %02X %02X %02X\n",
                     raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6]);
        } else {
          Log.println(F("RTC: register read failed"));
        }
      }
      return;
    }
  }

  rtcWriteFails = 0;
  lastRtcWrite  = t;
  rtcTimeValid  = true;

  // Log an actual, verified change — never a rejected one.
  if (firstSet) {
    if (rtcSane) Log.printf("NTP: RTC set (drift %lld s)\n", (long long)drift);
    else         Log.println(F("NTP: RTC set (was blank)"));
  } else if (rtcSane) {
    Log.printf("NTP: RTC corrected (drift %lld s)\n", (long long)drift);
  }
}

// ---- SHT4x ceiling sensor -------------------------------------------
const char *trendName(Trend t) {
  switch (t) {
    case TREND_UP:     return "up";
    case TREND_DOWN:   return "down";
    case TREND_STEADY: return "steady";
    default:           return "";
  }
}

void sensorHistoryPush(float t, float h) {
  sensorHist[sensorHistHead] = { t, h, millis() };
  sensorHistHead = (uint8_t)((sensorHistHead + 1) % SENSOR_HISTORY);
  if (sensorHistCount < SENSOR_HISTORY) sensorHistCount++;
}

// age 0 = newest sample.
const SensorSample &sensorHistoryAt(uint8_t age) {
  uint8_t idx = (uint8_t)((sensorHistHead + SENSOR_HISTORY - 1 - age) % SENSOR_HISTORY);
  return sensorHist[idx];
}

Trend sensorTrendOf(bool temperature) {
  if (sensorHistCount < 2) return TREND_NONE;

  uint8_t n = sensorHistCount < SENSOR_TREND_SAMPLES
                ? sensorHistCount : SENSOR_TREND_SAMPLES;
  const SensorSample &newest = sensorHistoryAt(0);
  const SensorSample &oldest = sensorHistoryAt((uint8_t)(n - 1));

  // A 7-minute read gives 14 minutes across three samples. Twelve is the
  // floor, so the arrow appears on the third reading and not before.
  uint32_t span = newest.ms - oldest.ms;
  if (span < SENSOR_TREND_MIN_MS) return TREND_NONE;

  float delta = temperature ? (newest.t - oldest.t) : (newest.h - oldest.h);
  float band  = temperature ? SENSOR_TEMP_DEADBAND : SENSOR_HUM_DEADBAND;
  if (delta >  band) return TREND_UP;
  if (delta < -band) return TREND_DOWN;
  return TREND_STEADY;
}

// Clear the daily extremes when the local date changes, and take a fresh
// reading so the new day's min/max starts immediately rather than up to seven
// minutes late. RAM only, exactly like the values themselves.
void sensorRollDay() {
  if (!haveTime) return;
  int key = lastLocal.year() * 10000 + lastLocal.month() * 100 + lastLocal.day();
  if (sensorDayKey < 0) {
    sensorDayKey = key;
    // If the first reading happened before the clock existed, its extremes
    // have no timestamp. Clear them and read again now that time is known.
    if (SENSOR_ENABLED && sensorOk) {
      sensorTempMin = NAN;
      sensorTempMax = NAN;
      sensorHumMin  = NAN;
      sensorHumMax  = NAN;
      sensorTempMinAtMin = -1;
      sensorTempMaxAtMin = -1;
      sensorHumMinAtMin  = -1;
      sensorHumMaxAtMin  = -1;
      sensorLastReadMs = millis() - SENSOR_READ_MS;
    }
    return;
  }
  if (key == sensorDayKey) return;

  sensorDayKey = key;
  sensorTempMin = NAN;
  sensorTempMax = NAN;
  sensorHumMin  = NAN;
  sensorHumMax  = NAN;
  sensorTempMinAtMin = -1;
  sensorTempMaxAtMin = -1;
  sensorHumMinAtMin  = -1;
  sensorHumMaxAtMin  = -1;
  if (SENSOR_ENABLED) sensorLastReadMs = millis() - SENSOR_READ_MS;
}

bool sensorRead() {
  if (!SENSOR_ENABLED) return false;

  float t = NAN, h = NAN;
  int16_t err;
  if (SENSOR_PRECISION == 0)      err = sht4x.measureLowestPrecision(t, h);
  else if (SENSOR_PRECISION >= 2) err = sht4x.measureHighPrecision(t, h);
  else                            err = sht4x.measureMediumPrecision(t, h);

  bool rangeOk = !isnan(t) && !isnan(h) &&
                 t >= -40.0f && t <= 125.0f && h >= 0.0f && h <= 100.0f;
  if (err != 0 || !rangeOk) {
    if (sensorOk) Log.printf("SHT4x: read failed (%s)\n", err ? "I2C" : "range");
    sensorOk = false;
    sensorHistCount = 0;
    sensorTempTrend = TREND_NONE;
    sensorHumTrend  = TREND_NONE;
    return false;
  }

  sensorOk     = true;
  sensorProbed = true;
  sensorTempC  = t;
  sensorHum    = h;
  sensorLastAtMin = haveTime ? localMinNow : -1;

  if (isnan(sensorTempMin) || sensorTempC < sensorTempMin) {
    sensorTempMin = sensorTempC;
    sensorTempMinAtMin = sensorLastAtMin;
  }
  if (isnan(sensorTempMax) || sensorTempC > sensorTempMax) {
    sensorTempMax = sensorTempC;
    sensorTempMaxAtMin = sensorLastAtMin;
  }
  if (isnan(sensorHumMin) || sensorHum < sensorHumMin) {
    sensorHumMin = sensorHum;
    sensorHumMinAtMin = sensorLastAtMin;
  }
  if (isnan(sensorHumMax) || sensorHum > sensorHumMax) {
    sensorHumMax = sensorHum;
    sensorHumMaxAtMin = sensorLastAtMin;
  }

  sensorHistoryPush(sensorTempC, sensorHum);
  sensorTempTrend = sensorTrendOf(true);
  sensorHumTrend  = sensorTrendOf(false);
  return true;
}

void sensorInit() {
  if (!SENSOR_ENABLED) return;

  sht4x.begin(Wire, SHT4X_I2C_ADDR);
  sht4x.softReset();

  uint32_t serial = 0;
  int16_t err = sht4x.serialNumber(serial);
  sensorProbed = (err == 0);
  if (sensorProbed) sensorSerial = serial;

  sensorRollDay();
  sensorLastReadMs = millis();
  sensorRead();
}

void sensorTask() {
  if (!SENSOR_ENABLED) return;
  uint32_t now = millis();
  if (sensorLastReadMs != 0 &&
      (uint32_t)(now - sensorLastReadMs) < SENSOR_READ_MS) return;
  sensorLastReadMs = now;
  sensorRead();
}

// ---- one-second scheduler -------------------------------------------
void tick() {
  bool hadTime = haveTime;
  haveTime = getNow(lastUtc);
  if (!haveTime) {
    heaterEvaluate(true, false);        // safe fallback: camera on, lamp off; never auto-fire without a clock
    applyOutputs(true, false, heaterWant);
    timeStr      = "--:--:--";
    nextEventStr = HEATER_ENABLED ? heaterHeatLabel(heaterWant) : String("no time");
    return;
  }

  localOffsetNow = localOffsetMin(lastUtc);
  lastLocal = lastUtc + TimeSpan((int32_t)localOffsetNow * 60);

  // Say so once, the moment the clock becomes usable. This is the line that
  // replaces the dashes after a first NTP sync on a blank RTC. The wording
  // follows where the time actually came from, not what the RTC flag says.
  if (!hadTime) {
    Log.invalidateStamp();
    bool fromRtc = (time(nullptr) <= 1700000000);
    Log.printf("Clock %s: %04d-%02d-%02d %02d:%02d:%02d %s\n",
               fromRtc ? "from RTC" : "set by NTP",
               lastLocal.year(), lastLocal.month(), lastLocal.day(),
               lastLocal.hour(), lastLocal.minute(), lastLocal.second(), tzName());
  }

  computeSolarFor(lastLocal, localOffsetNow);

  localMinNow = lastLocal.hour() * 60 + lastLocal.minute();
  int eff = (simMinutes >= 0) ? simMinutes : localMinNow;

  time_t nowEpoch = (time_t)lastUtc.unixtime();
  if (overrideActive && nowEpoch >= overrideUntil) overrideActive = false;

  int offStart  = CAM_OFF_FROM_H  * 60 + CAM_OFF_FROM_M;
  int offEnd    = CAM_OFF_TO_H    * 60 + CAM_OFF_TO_M;
  int off2Start = CAM_OFF2_FROM_H * 60 + CAM_OFF2_FROM_M;
  int off2End   = CAM_OFF2_TO_H   * 60 + CAM_OFF2_TO_M;
  bool camSched = !inWindow(eff, offStart, offEnd) &&
                  !inWindow(eff, off2Start, off2End);

  bool lampSched = IR_ALL_NIGHT
                 ? inWindow(eff, sunsetLocal, sunriseLocal)
                 : inWindow(eff, irStart, irEnd);

  bool camOn  = overrideActive ? overrideCamera : camSched;
  bool lampOn = overrideActive ? overrideLamp   : lampSched;

  if (lampOn) camOn = true;             // never light the coop with the camera off
  if (!camOn) lampOn = false;

  // Any real power-up qualifies: scheduled, button or dashboard WAKE.
  heaterEvaluate(camOn, !overrideActive || HEATER_AUTO_FIRE_ON_MANUAL_CAM_ON);
  applyOutputs(camOn, lampOn, heaterWant);
  updateStatus(lastLocal);
  sensorRollDay();
  if (HEATER_ENABLED) nextEventStr = heaterHeatLabel(heaterWant);
  else                computeNextEvent(eff);
}

// ---- boot summary ---------------------------------------------------
void printConfig() {
  Log.println(F("---------------- configuration ----------------"));
  Log.printf("Firmware        : %s\n", FW_VERSION);
  Log.printf("Time zone       : %s (UTC%+d min)\n", tzName(), localOffsetNow);
  Log.printf("Location        : %.4f, %.4f\n", LATITUDE, LONGITUDE);
  Log.printf("Camera OFF      : %02d:%02d - %02d:%02d\n",
                CAM_OFF_FROM_H, CAM_OFF_FROM_M, CAM_OFF_TO_H, CAM_OFF_TO_M);
  if ((CAM_OFF2_FROM_H * 60 + CAM_OFF2_FROM_M) != (CAM_OFF2_TO_H * 60 + CAM_OFF2_TO_M))
    Log.printf("Camera OFF (2)  : %02d:%02d - %02d:%02d\n",
                  CAM_OFF2_FROM_H, CAM_OFF2_FROM_M, CAM_OFF2_TO_H, CAM_OFF2_TO_M);
  Log.printf("IR lamp         : ON %d min before sunset, OFF %d min after%s\n",
                IR_ON_BEFORE_SUNSET_MIN, IR_OFF_AFTER_SUNSET_MIN,
                IR_ALL_NIGHT ? " (also all night)" : "");
  Log.printf("Manual override : %d min\n", MANUAL_MINUTES);
  Log.printf("Lens heater     : %s, GPIO%d, manual %dm, max %.1fC\n",
             HEATER_ENABLED ? "enabled" : "disabled", PIN_HEATER,
             HEATER_MANUAL_MINUTES, HEATER_MAX_AMBIENT_C);
  if (HEATER_ENABLED) {
    Log.printf("Heater auto     : one-shot %dm on camera power-up (gap <= %.1fC, temp <= %.1fC)%s\n",
               HEATER_AUTO_MINUTES, HEATER_DEW_MARGIN_ON_C,
               HEATER_AUTO_MAX_AMBIENT_C,
               HEATER_AUTO_FIRE_ON_MANUAL_CAM_ON ? " (incl. manual wakes)" : " (schedule only)");
    Log.println(F("Heater repeat   : none while the camera stays on; manual press takes over"));
  }
  Log.printf("Camera relay    : GPIO%d (ON level %d)\n", PIN_CAMERA_RELAY, CAMERA_ON_LEVEL);
  Log.printf("IR lamp         : GPIO%d PWM, brightness %u%%\n", PIN_IR_LAMP, (unsigned)irBrightness);
  Log.printf("IR button       : GPIO%d (press = lamp on/off + %d min override)\n",
                PIN_BUTTON, MANUAL_MINUTES);
  if (CAMERA_VIEW_URL[0])
    Log.printf("view camera    : %s\n", CAMERA_VIEW_URL);
  else if (SHOW_WATCH_CARD_ANYWAY)
    Log.println(F("view camera    : no stream URL, tile shown"));
  else
    Log.println(F("view camera    : no stream URL, card hidden"));
  if (!SENSOR_ENABLED) {
    Log.println(F("SHT4x ceiling   : disabled"));
  } else if (sensorProbed) {
    if (sensorSerial) {
      Log.printf("SHT4x ceiling   : 0x%02X ok, S/N 0x%08lX, %lu min\n",
                    SHT4X_I2C_ADDR, (unsigned long)sensorSerial,
                    (unsigned long)(SENSOR_READ_MS / 60000UL));
    } else {
      Log.printf("SHT4x ceiling   : 0x%02X ok, %lu min\n",
                    SHT4X_I2C_ADDR,
                    (unsigned long)(SENSOR_READ_MS / 60000UL));
    }
  } else {
    Log.printf("SHT4x ceiling   : no response at 0x%02X\n", SHT4X_I2C_ADDR);
  }
  Log.printf("RTC             : %s\n", rtcOk ? (rtcTimeValid ? "ok" : "present, time invalid") : "not found");
  if (haveTime) {
    Log.printf("Sunrise         : %s\n", fmtMin(sunriseLocal).c_str());
    Log.printf("Sunset          : %s\n", fmtMin(sunsetLocal).c_str());
    Log.printf("IR window       : %s - %s\n", fmtMin(irStart).c_str(), fmtMin(irEnd).c_str());
  }
  Log.println(F("-----------------------------------------------"));
}

// ---- Arduino --------------------------------------------------------
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);

  // Outputs to a safe state before anything else.
  pinMode(PIN_CAMERA_RELAY, OUTPUT);
  digitalWrite(PIN_CAMERA_RELAY, !CAMERA_ON_LEVEL);
  pinMode(PIN_IR_LAMP, OUTPUT);
  digitalWrite(PIN_IR_LAMP, LOW);  // keep YX-MOS trigger inactive during PWM setup
  pinMode(PIN_HEATER, OUTPUT);
  digitalWrite(PIN_HEATER, !HEATER_ON_LEVEL);  // heater safely OFF at boot

  // DS3231 before the first log line: when it already holds a valid time, the
  // logger reads it directly and every boot line carries a real timestamp.
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  Log.println();
  Log.printf("CoopCam %s\n", FW_VERSION);

  // I2C scan: proves whether the DS3231 is answering. It lives at 0x68, so any
  // other address here is something unexpected on the bus.
  uint8_t i2cFound = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Log.printf("I2C: device at 0x%02X\n", addr);
      i2cFound++;
    }
  }
  if (!i2cFound) {
    Log.println(F("I2C: no devices found (check SDA=GPIO8, SCL=GPIO9, VCC=3.3V, GND)"));
  }

  if (rtc.begin(&Wire)) {
    rtcOk = true;
    rtcTimeValid = rtcTimeSane(rtc.now());
  } else {
    rtcOk = false;
  }

  if (!rtcOk) {
    Log.println(F("RTC: not found"));
  } else if (rtc.lostPower()) {
    Log.println(F("RTC: lost power, waiting for NTP"));
  }

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  irPwmReady = ledcAttach(PIN_IR_LAMP, IR_PWM_FREQUENCY, IR_PWM_RESOLUTION);
#else
  irPwmReady = (ledcSetup(0, IR_PWM_FREQUENCY, IR_PWM_RESOLUTION) > 0);
  if (irPwmReady) ledcAttachPin(PIN_IR_LAMP, 0);
#endif
  if (irPwmReady) {
    writeIrDuty(0.0f);
    Log.printf("IR PWM         : GPIO%d, %lu Hz, %u-bit, brightness %u%%\n",
                  PIN_IR_LAMP, (unsigned long)IR_PWM_FREQUENCY,
                  IR_PWM_RESOLUTION, IR_DEFAULT_BRIGHTNESS);
  } else {
    Log.println(F("ERROR: IR PWM setup failed; IR lamp output disabled"));
  }
  if (STATUS_LED_TYPE == LED_PLAIN) {
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);
  }
  pinMode(PIN_BUTTON, BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(OTA_HOSTNAME);
  WiFi.setSleep(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  lastWifiAttempt = millis();

  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() { Log.println(F("OTA: start")); });
  ArduinoOTA.onEnd([]()   { Log.println(F("\nOTA: done")); });
  ArduinoOTA.onProgress([](unsigned int p, unsigned int t) { Log.printf("OTA: %u%%\r", p * 100 / t); });
  ArduinoOTA.onError([](ota_error_t e) { Log.printf("OTA: error %u\n", e); });

  dashboardSetup();

  lastTickMs = millis();
  sensorInit();           // probe 0x44 first, so a camera-on edge at boot can auto-heat
  tick();                 // apply the schedule once at boot
  printConfig();
}

void loop() {
  if (otaStarted) ArduinoOTA.handle();
  server.handleClient();
  netTask();
  buttonTask();
  ledTask();
  serviceIrRamp();                 // update PWM fade without blocking other tasks

  if (millis() - lastTickMs >= 1000) {
    lastTickMs = millis();
    tick();
  }

  sensorTask();
}
