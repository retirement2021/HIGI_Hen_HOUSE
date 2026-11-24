#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <Wire.h>
#include <RTClib.h>
#include <esp_sntp.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>

#include "config.h"
#include "solar.h"
#include "web_ui.h"

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------
WebServer server(80);
RTC_DS3231 rtc;
Preferences prefs;

struct Config {
  float lat = DEFAULT_LATITUDE;
  float lon = DEFAULT_LONGITUDE;
  uint16_t camBefore = DEFAULT_CAM_BEFORE;
  uint16_t camAfter = DEFAULT_CAM_AFTER;
  uint16_t lampBefore = DEFAULT_LAMP_BEFORE;
  uint16_t lampAfter = DEFAULT_LAMP_AFTER;
  uint16_t manualMinutes = DEFAULT_MANUAL_MINUTES;
  bool lampFollowsCamera = true;
  bool dayMode = false;
  bool camAlwaysOn = false;
  char hostname[32] = DEFAULT_HOSTNAME;
};

Config cfg;

enum class Ov : uint8_t { AUTO, ON, OFF };
Ov camOv = Ov::AUTO;
Ov lampOv = Ov::AUTO;
time_t ovUntil = 0;

bool rtcOk = false;
bool timeValid = false;
bool apMode = false;
String wifiSsid;
String wifiPass;

bool camState = false;
bool lampState = false;
time_t camOnSince = 0;
time_t lampOnSince = 0;

bool pendingReboot = false;
unsigned long rebootAt = 0;
time_t nextNtp = 0;

struct ScheduleResult {
  bool valid = false;
  bool polar = false;
  bool polarDay = false;
  bool polarNight = false;
  bool cam = false;
  bool lamp = false;
  time_t windowSunset = 0;
  time_t nextSunrise = 0;
  time_t nextSunset = 0;
};

// -----------------------------------------------------------------------------
// Small helpers
// -----------------------------------------------------------------------------
static const char *ovName(Ov o) {
  switch (o) {
    case Ov::ON: return "on";
    case Ov::OFF: return "off";
    default: return "auto";
  }
}

static bool applyOverride(bool scheduled, Ov o) {
  if (o == Ov::ON) return true;
  if (o == Ov::OFF) return false;
  return scheduled;
}

static String jsonEscape(const String &s) {
  String o;
  o.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '"': o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n"; break;
      case '\r': o += "\\r"; break;
      case '\t': o += "\\t"; break;
      default:
        if ((unsigned char)c < 0x20) {
          char buf[7];
          snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
          o += buf;
        } else {
          o += c;
        }
    }
  }
  return o;
}

static String sanitizeHostname(String h) {
  h.trim();
  String out;
  for (size_t i = 0; i < h.length(); i++) {
    char c = h[i];
    if (isalnum((unsigned char)c) || c == '-') out += c;
    else if (c == ' ' || c == '_' || c == '.') out += '-';
  }
  if (out.length() == 0) out = DEFAULT_HOSTNAME;
  if (out.length() > 31) out = out.substring(0, 31);
  return out;
}

static int clampInt(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

// -----------------------------------------------------------------------------
// Config persistence
// -----------------------------------------------------------------------------
static void loadConfig() {
  prefs.begin("coopcam", false);
  cfg.lat = prefs.getFloat("lat", DEFAULT_LATITUDE);
  cfg.lon = prefs.getFloat("lon", DEFAULT_LONGITUDE);
  cfg.camBefore = prefs.getUShort("camBefore", DEFAULT_CAM_BEFORE);
  cfg.camAfter = prefs.getUShort("camAfter", DEFAULT_CAM_AFTER);
  cfg.lampBefore = prefs.getUShort("lampBefore", DEFAULT_LAMP_BEFORE);
  cfg.lampAfter = prefs.getUShort("lampAfter", DEFAULT_LAMP_AFTER);
  cfg.manualMinutes = prefs.getUShort("manualMinutes", DEFAULT_MANUAL_MINUTES);
  cfg.lampFollowsCamera = prefs.getBool("lampFollows", true);
  cfg.dayMode = prefs.getBool("dayMode", false);
  cfg.camAlwaysOn = prefs.getBool("camAlwaysOn", false);
  String h = prefs.getString("hostname", DEFAULT_HOSTNAME);
  h.toCharArray(cfg.hostname, sizeof(cfg.hostname));
  wifiSsid = prefs.getString("ssid", "");
  wifiPass = prefs.getString("pass", "");
}

static void saveConfig() {
  prefs.putFloat("lat", cfg.lat);
  prefs.putFloat("lon", cfg.lon);
  prefs.putUShort("camBefore", cfg.camBefore);
  prefs.putUShort("camAfter", cfg.camAfter);
  prefs.putUShort("lampBefore", cfg.lampBefore);
  prefs.putUShort("lampAfter", cfg.lampAfter);
  prefs.putUShort("manualMinutes", cfg.manualMinutes);
  prefs.putBool("lampFollows", cfg.lampFollowsCamera);
  prefs.putBool("dayMode", cfg.dayMode);
  prefs.putBool("camAlwaysOn", cfg.camAlwaysOn);
  prefs.putString("hostname", cfg.hostname);
}

// -----------------------------------------------------------------------------
// Time
// -----------------------------------------------------------------------------
static void setSystemTime(time_t t) {
  struct timeval tv;
  tv.tv_sec = t;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
}

static void setTimeFromRtc() {
  if (!rtcOk) return;
  DateTime n = rtc.now();
  if (n.year() >= 2024) {
    setSystemTime((time_t)n.unixtime());
    timeValid = true;
    Serial.printf("[time] DS3231: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                  n.year(), n.month(), n.day(), n.hour(), n.minute(), n.second());
  } else {
    Serial.println("[time] DS3231 has no valid time");
  }
}

static void syncNtp() {
  if (WiFi.status() != WL_CONNECTED) return;
  Serial.println("[time] NTP sync...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
  unsigned long start = millis();
  while (millis() - start < 15000) {
    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) break;
    delay(250);
  }
  time_t now = time(nullptr);
  if (now > 1700000000) {
    timeValid = true;
    if (rtcOk) rtc.adjust(DateTime((uint32_t)now));
    Serial.printf("[time] NTP: %lu\n", (unsigned long)now);
  } else {
    Serial.println("[time] NTP failed");
  }
  nextNtp = now + 6 * 3600;
}

// -----------------------------------------------------------------------------
// Solar schedule
// -----------------------------------------------------------------------------
static ScheduleResult computeSchedule(time_t now) {
  ScheduleResult r;
  if (cfg.lat == 0.0f && cfg.lon == 0.0f) return r;

  int64_t today = (int64_t)now / 86400;
  // Four days so the "next sunrise" display still works just after midnight
  // UTC in far-east time zones. The extra day does not affect the schedule.
  SolarDay days[4] = {
      solarForDay(today - 1, cfg.lat, cfg.lon),
      solarForDay(today, cfg.lat, cfg.lon),
      solarForDay(today + 1, cfg.lat, cfg.lon),
      solarForDay(today + 2, cfg.lat, cfg.lon),
  };

  r.polarDay = days[1].polarDay;
  r.polarNight = days[1].polarNight;
  r.polar = r.polarDay || r.polarNight;

  bool anyValid = false;
  bool camSched = false;
  bool lampSched = false;
  time_t bestSunset = 0;
  time_t nextSunrise = 0;
  time_t nextSunset = 0;
  const int maxAfter = (cfg.camAfter > cfg.lampAfter) ? cfg.camAfter : cfg.lampAfter;

  for (int i = 0; i < 4; i++) {
    SolarDay &d = days[i];
    if (!d.valid) continue;
    anyValid = true;
    if (d.polarDay || d.polarNight) continue;

    const time_t camStart = d.sunset - (time_t)cfg.camBefore * 60;
    const time_t camEnd = d.sunset + (time_t)cfg.camAfter * 60;
    const time_t lampStart = d.sunset - (time_t)cfg.lampBefore * 60;
    const time_t lampEnd = d.sunset + (time_t)cfg.lampAfter * 60;

    if (now >= camStart && now <= camEnd) camSched = true;
    if (now >= lampStart && now <= lampEnd) lampSched = true;
    if (cfg.dayMode && now >= d.sunrise && now <= d.sunset) camSched = true;

    if (now <= d.sunset + (time_t)maxAfter * 60 &&
        (bestSunset == 0 || d.sunset < bestSunset)) {
      bestSunset = d.sunset;
    }
    if (d.sunrise > now && (nextSunrise == 0 || d.sunrise < nextSunrise)) {
      nextSunrise = d.sunrise;
    }
    if (d.sunset > now && (nextSunset == 0 || d.sunset < nextSunset)) {
      nextSunset = d.sunset;
    }
  }

  r.valid = anyValid;
  r.cam = camSched;
  r.lamp = lampSched;
  r.windowSunset = bestSunset;
  r.nextSunrise = nextSunrise;
  r.nextSunset = nextSunset;
  return r;
}

// -----------------------------------------------------------------------------
// Outputs
// -----------------------------------------------------------------------------
static void initOutputs() {
  // Preload the latch to OFF before making the pin an output, so the load does
  // not glitch on during boot.
  digitalWrite(PIN_CAM_SWITCH, CAM_ACTIVE_HIGH ? LOW : HIGH);
  pinMode(PIN_CAM_SWITCH, OUTPUT);
  digitalWrite(PIN_LAMP_SWITCH, LAMP_ACTIVE_HIGH ? LOW : HIGH);
  pinMode(PIN_LAMP_SWITCH, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
}

static void setOutputs(bool cam, bool lamp) {
  if (cam != camState) {
    digitalWrite(PIN_CAM_SWITCH, (cam == CAM_ACTIVE_HIGH) ? HIGH : LOW);
    camState = cam;
    camOnSince = cam ? time(nullptr) : 0;
    Serial.printf("[out] camera %s\n", cam ? "ON" : "OFF");
  }
  if (lamp != lampState) {
    digitalWrite(PIN_LAMP_SWITCH, (lamp == LAMP_ACTIVE_HIGH) ? HIGH : LOW);
    lampState = lamp;
    lampOnSince = lamp ? time(nullptr) : 0;
    Serial.printf("[out] lamp %s\n", lamp ? "ON" : "OFF");
  }
}

static void updateOutputs(time_t now) {
  bool camSched = false;
  bool lampSched = false;

  if (timeValid && !(cfg.lat == 0.0f && cfg.lon == 0.0f)) {
    ScheduleResult s = computeSchedule(now);
    camSched = s.cam;
    lampSched = s.lamp;
  }
  if (cfg.camAlwaysOn) camSched = true;

  const bool cam = applyOverride(camSched, camOv);
  if (cfg.lampFollowsCamera) lampSched = lampSched && cam;
  const bool lamp = applyOverride(lampSched, lampOv);

  setOutputs(cam, lamp);
}

// -----------------------------------------------------------------------------
// WiFi
// -----------------------------------------------------------------------------
static void startAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  apMode = true;
  Serial.printf("[wifi] AP \"%s\" password \"%s\" -> http://%s\n", AP_SSID,
                AP_PASSWORD, WiFi.softAPIP().toString().c_str());
}

static void startWiFi() {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);

  if (wifiSsid.length() == 0) {
    Serial.println("[wifi] no credentials, starting setup AP");
    startAP();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(cfg.hostname);
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());

  Serial.printf("[wifi] connecting to \"%s\"", wifiSsid.c_str());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    apMode = false;
    Serial.printf("[wifi] connected: %s\n", WiFi.localIP().toString().c_str());
    if (MDNS.begin(cfg.hostname)) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("[wifi] http://%s.local/\n", cfg.hostname);
    }
  } else {
    Serial.println("[wifi] connection failed, starting setup AP");
    startAP();
  }
}

// -----------------------------------------------------------------------------
// Web handlers
// -----------------------------------------------------------------------------
static void handleStatus() {
  time_t now = time(nullptr);
  ScheduleResult s;
  if (timeValid && !(cfg.lat == 0.0f && cfg.lon == 0.0f)) {
    s = computeSchedule(now);
  }

  bool camSched = s.cam || cfg.camAlwaysOn;
  bool lampSched = s.lamp && (!cfg.lampFollowsCamera || camSched);

  time_t camStart = 0, camEnd = 0, lampStart = 0, lampEnd = 0;
  if (s.windowSunset) {
    camStart = s.windowSunset - (time_t)cfg.camBefore * 60;
    camEnd = s.windowSunset + (time_t)cfg.camAfter * 60;
    lampStart = s.windowSunset - (time_t)cfg.lampBefore * 60;
    lampEnd = s.windowSunset + (time_t)cfg.lampAfter * 60;
  }

  String j;
  j.reserve(1500);
  j += "{";
  j += "\"now\":" + String((unsigned long)now);
  j += ",\"timeValid\":" + String(timeValid ? "true" : "false");
  j += ",\"rtcOk\":" + String(rtcOk ? "true" : "false");
  j += ",\"rtcTemp\":" + String(rtcOk ? rtc.getTemperature() : 0.0f, 1);
  j += ",\"apSsid\":\"" + jsonEscape(String(AP_SSID)) + "\"";

  j += ",\"wifi\":{";
  j += "\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
  j += ",\"ap\":" + String(apMode ? "true" : "false");
  j += ",\"rssi\":" + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0);
  j += ",\"ip\":\"" +
       jsonEscape(apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) +
       "\"";
  j += ",\"ssid\":\"" +
       jsonEscape(apMode ? String(AP_SSID) : WiFi.SSID()) + "\"";
  j += "}";

  j += ",\"sun\":{";
  j += "\"valid\":" + String(s.valid ? "true" : "false");
  j += ",\"polar\":" + String(s.polar ? "true" : "false");
  j += ",\"polarDay\":" + String(s.polarDay ? "true" : "false");
  j += ",\"polarNight\":" + String(s.polarNight ? "true" : "false");
  j += ",\"windowSunset\":" + String((unsigned long)s.windowSunset);
  j += ",\"nextSunrise\":" + String((unsigned long)s.nextSunrise);
  j += ",\"nextSunset\":" + String((unsigned long)s.nextSunset);
  j += ",\"camStart\":" + String((unsigned long)camStart);
  j += ",\"camEnd\":" + String((unsigned long)camEnd);
  j += ",\"lampStart\":" + String((unsigned long)lampStart);
  j += ",\"lampEnd\":" + String((unsigned long)lampEnd);
  j += "}";

  j += ",\"cam\":{";
  j += "\"on\":" + String(camState ? "true" : "false");
  j += ",\"scheduled\":" + String(camSched ? "true" : "false");
  j += ",\"override\":\"" + String(ovName(camOv)) + "\"";
  j += ",\"onSince\":" + String((unsigned long)camOnSince);
  j += "}";

  j += ",\"lamp\":{";
  j += "\"on\":" + String(lampState ? "true" : "false");
  j += ",\"scheduled\":" + String(lampSched ? "true" : "false");
  j += ",\"override\":\"" + String(ovName(lampOv)) + "\"";
  j += ",\"onSince\":" + String((unsigned long)lampOnSince);
  j += "}";

  j += ",\"override\":{";
  j += "\"active\":" +
       String((camOv != Ov::AUTO || lampOv != Ov::AUTO) ? "true" : "false");
  j += ",\"until\":" + String((unsigned long)ovUntil);
  j += ",\"left\":" +
       String((unsigned long)(ovUntil && ovUntil > now ? ovUntil - now : 0));
  j += "}";

  j += ",\"config\":{";
  j += "\"lat\":" + String(cfg.lat, 6);
  j += ",\"lon\":" + String(cfg.lon, 6);
  j += ",\"camBefore\":" + String(cfg.camBefore);
  j += ",\"camAfter\":" + String(cfg.camAfter);
  j += ",\"lampBefore\":" + String(cfg.lampBefore);
  j += ",\"lampAfter\":" + String(cfg.lampAfter);
  j += ",\"manualMinutes\":" + String(cfg.manualMinutes);
  j += ",\"lampFollowsCamera\":" +
       String(cfg.lampFollowsCamera ? "true" : "false");
  j += ",\"dayMode\":" + String(cfg.dayMode ? "true" : "false");
  j += ",\"camAlwaysOn\":" + String(cfg.camAlwaysOn ? "true" : "false");
  j += ",\"hostname\":\"" + jsonEscape(String(cfg.hostname)) + "\"";
  j += "}";

  j += "}";

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

static void handleControl() {
  bool anySet = false;
  Ov newCam = camOv;
  Ov newLamp = lampOv;

  if (server.hasArg("cam")) {
    String v = server.arg("cam");
    if (v == "on") { newCam = Ov::ON; anySet = true; }
    else if (v == "off") { newCam = Ov::OFF; anySet = true; }
    else if (v == "auto") { newCam = Ov::AUTO; anySet = true; }
  }
  if (server.hasArg("lamp")) {
    String v = server.arg("lamp");
    if (v == "on") { newLamp = Ov::ON; anySet = true; }
    else if (v == "off") { newLamp = Ov::OFF; anySet = true; }
    else if (v == "auto") { newLamp = Ov::AUTO; anySet = true; }
  }

  if (!anySet) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"no target\"}");
    return;
  }

  int minutes = cfg.manualMinutes;
  if (server.hasArg("minutes")) {
    int m = server.arg("minutes").toInt();
    if (m >= 0 && m <= 1440) minutes = m;
  }

  camOv = newCam;
  lampOv = newLamp;
  if (camOv == Ov::AUTO && lampOv == Ov::AUTO) {
    ovUntil = 0;
  } else {
    ovUntil = (timeValid && minutes > 0) ? time(nullptr) + (time_t)minutes * 60 : 0;
  }

  updateOutputs(time(nullptr));
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleSettings() {
  if (!server.hasArg("lat") || !server.hasArg("lon")) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing location\"}");
    return;
  }

  float lat = server.arg("lat").toFloat();
  float lon = server.arg("lon").toFloat();
  if (!(lat >= -90.0f && lat <= 90.0f) || !(lon >= -180.0f && lon <= 180.0f) ||
      (lat == 0.0f && lon == 0.0f)) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid location\"}");
    return;
  }

  cfg.lat = lat;
  cfg.lon = lon;
  if (server.hasArg("camBefore")) cfg.camBefore = clampInt(server.arg("camBefore").toInt(), 0, 720);
  if (server.hasArg("camAfter")) cfg.camAfter = clampInt(server.arg("camAfter").toInt(), 0, 720);
  if (server.hasArg("lampBefore")) cfg.lampBefore = clampInt(server.arg("lampBefore").toInt(), 0, 720);
  if (server.hasArg("lampAfter")) cfg.lampAfter = clampInt(server.arg("lampAfter").toInt(), 0, 720);
  if (server.hasArg("manualMinutes")) cfg.manualMinutes = clampInt(server.arg("manualMinutes").toInt(), 1, 1440);
  if (server.hasArg("lampFollowsCamera")) cfg.lampFollowsCamera = server.arg("lampFollowsCamera") != "0";
  if (server.hasArg("dayMode")) cfg.dayMode = server.arg("dayMode") != "0";
  if (server.hasArg("camAlwaysOn")) cfg.camAlwaysOn = server.arg("camAlwaysOn") != "0";

  bool rebootNeeded = false;

  if (server.hasArg("hostname")) {
    String h = sanitizeHostname(server.arg("hostname"));
    if (h != String(cfg.hostname)) {
      h.toCharArray(cfg.hostname, sizeof(cfg.hostname));
      rebootNeeded = true;
    }
  }

  if (server.hasArg("ssid")) {
    String s = server.arg("ssid");
    s.trim();
    if (s.length() > 0) {
      wifiSsid = s;
      wifiPass = server.arg("pass");
      prefs.putString("ssid", wifiSsid);
      prefs.putString("pass", wifiPass);
      rebootNeeded = true;
    }
  }

  saveConfig();
  updateOutputs(time(nullptr));

  server.send(200, "application/json",
              rebootNeeded ? "{\"ok\":true,\"reboot\":true}" : "{\"ok\":true}");

  if (rebootNeeded) {
    pendingReboot = true;
    rebootAt = millis() + 900;
  }
}

static void handleSetTime() {
  if (!server.hasArg("epoch")) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing epoch\"}");
    return;
  }
  unsigned long e = strtoul(server.arg("epoch").c_str(), nullptr, 10);
  if (e < 1700000000UL) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid epoch\"}");
    return;
  }
  setSystemTime((time_t)e);
  timeValid = true;
  if (rtcOk) rtc.adjust(DateTime((uint32_t)e));
  nextNtp = (time_t)e + 6 * 3600;
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleReboot() {
  server.send(200, "application/json", "{\"ok\":true}");
  pendingReboot = true;
  rebootAt = millis() + 900;
}

static void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html", INDEX_HTML);
  });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/control", HTTP_POST, handleControl);
  server.on("/api/settings", HTTP_POST, handleSettings);
  server.on("/api/settime", HTTP_POST, handleSetTime);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });
  server.begin();
}

// -----------------------------------------------------------------------------
// Button
// -----------------------------------------------------------------------------
static void handleButton() {
  static bool last = HIGH;
  static unsigned long downAt = 0;
  static bool longSent = false;

  const bool cur = digitalRead(PIN_BUTTON);

  if (cur == LOW && last == HIGH) {
    downAt = millis();
    longSent = false;
  }

  if (cur == LOW && !longSent && millis() - downAt >= 3000) {
    longSent = true;
    camOv = lampOv = Ov::AUTO;
    ovUntil = 0;
    updateOutputs(time(nullptr));
    Serial.println("[btn] automatic");
  }

  if (cur == HIGH && last == LOW && !longSent) {
    unsigned long dur = millis() - downAt;
    if (dur >= 50) {
      if (camOv == Ov::ON && lampOv == Ov::ON) {
        camOv = lampOv = Ov::AUTO;
        ovUntil = 0;
        Serial.println("[btn] off / automatic");
      } else {
        camOv = lampOv = Ov::ON;
        ovUntil = (timeValid && cfg.manualMinutes > 0)
                      ? time(nullptr) + (time_t)cfg.manualMinutes * 60
                      : 0;
        Serial.println("[btn] night view");
      }
      updateOutputs(time(nullptr));
    }
  }

  last = cur;
}

// -----------------------------------------------------------------------------
// Setup / loop
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== Chicken coop camera controller ===");

  loadConfig();
  initOutputs();

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  rtcOk = rtc.begin();
  if (rtcOk) {
    if (rtc.lostPower()) Serial.println("[rtc] lost power / time invalid");
    setTimeFromRtc();
  } else {
    Serial.println("[rtc] DS3231 not found");
  }

  startWiFi();
  if (!apMode) {
    syncNtp();
    ArduinoOTA.setHostname(cfg.hostname);
    ArduinoOTA.setPassword("coopcam");
    ArduinoOTA.begin();
  }
  if (nextNtp == 0) nextNtp = time(nullptr) + 6 * 3600;

  setupWebServer();
  updateOutputs(time(nullptr));

  Serial.printf("[sys] time valid: %s, AP mode: %s\n",
                timeValid ? "yes" : "no", apMode ? "yes" : "no");
  Serial.printf("[sys] camera pin %d, lamp pin %d\n", PIN_CAM_SWITCH,
                PIN_LAMP_SWITCH);
}

void loop() {
  if (!apMode) ArduinoOTA.handle();
  server.handleClient();
  handleButton();

  const time_t now = time(nullptr);

  static unsigned long lastNtpTry = 0;
  if (WiFi.status() == WL_CONNECTED) {
    const bool ntpDue =
        timeValid ? (now >= nextNtp) : (millis() - lastNtpTry >= 300000UL);
    if (ntpDue) {
      lastNtpTry = millis();
      syncNtp();
    }
  }

  if (ovUntil && now >= ovUntil) {
    camOv = lampOv = Ov::AUTO;
    ovUntil = 0;
    Serial.println("[ov] expired -> automatic");
  }

  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate >= 500) {
    lastUpdate = millis();
    updateOutputs(now);
  }

  if (pendingReboot && (long)(millis() - rebootAt) >= 0) {
    Serial.println("[sys] rebooting");
    ESP.restart();
  }

  if (!apMode && WiFi.status() != WL_CONNECTED) {
    static unsigned long lastTry = 0;
    if (millis() - lastTry > 30000) {
      lastTry = millis();
      Serial.println("[wifi] reconnecting");
      WiFi.reconnect();
    }
  }

  delay(10);
}
