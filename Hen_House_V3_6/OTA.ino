// ======================================================
// OTA - firmware updates over the LAN (separate tab: OTA.ino)
// Password protected. Invitations are ignored while the door is moving.
// The upload runs inside ArduinoOTA.handle(), so loop() is paused until it finishes.
// ======================================================
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>

#ifndef OTA_PASSWORD
#error Add this line to secrets.h: #define OTA_PASSWORD "your-long-password"
#endif

static bool otaRunning = false;

static const char* otaErrorText(int e) {  // int, not ota_error_t: the IDE hoists this prototype above the #include
  switch (e) {
    case OTA_AUTH_ERROR: return "wrong password";
    case OTA_BEGIN_ERROR: return "begin failed (no room for the update?)";
    case OTA_CONNECT_ERROR: return "connect failed";
    case OTA_RECEIVE_ERROR: return "receive failed (upload interrupted?)";
    case OTA_END_ERROR: return "end failed (image rejected)";
    default: return "unknown error";
  }
}

void otaSetup() {  // once, in setup()
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    Log.println("OTA: update starting");
  });

  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    esp_task_wdt_reset();  // keep the 35 s watchdog fed during the upload
    static unsigned int lastStep = 99;
    unsigned int step = total ? (done / (total / 10 + 1)) : 0;  // 0..9, one print per ~10%
    if (step != lastStep) {
      lastStep = step;
      Log.printf("OTA: %u%%\n", total ? (unsigned)((uint64_t)done * 100ULL / total) : 0U);
    }
  });

  ArduinoOTA.onEnd([]() {
    Log.println("OTA: complete, rebooting");
  });

  ArduinoOTA.onError([](ota_error_t e) {
    Log.print("OTA: error - ");
    Log.println(otaErrorText((int)e));
  });
}

void otaOnWifiConnected() {  // from connectWiFi(): "just connected"
  if (otaRunning) ArduinoOTA.end();
  ArduinoOTA.setHostname(WiFi.getHostname());
  ArduinoOTA.begin();
  otaRunning = true;
  Log.print("OTA ready: ");
  Log.print(WiFi.getHostname());
  Log.print(" at ");
  Log.println(WiFi.localIP());
}

void otaOnWifiLost() {  // from connectWiFi(): "just disconnected"
  if (otaRunning) ArduinoOTA.end();
  otaRunning = false;
}

void otaLoop() {  // once per loop()
  if (!otaRunning) return;
  if (doorState == DOOR_OPENING || doorState == DOOR_CLOSING) return;  // refuse while moving
  ArduinoOTA.handle();
}

// void otaLoop() {                        // once per loop()
//   if (!otaRunning) return;
//   bool moving = (doorState == DOOR_OPENING || doorState == DOOR_CLOSING);
//   if (millis() < 90000UL) moving = true;   // TEST ONLY: pretend the door is moving for 90 s after boot. DELETE FOR THE COOP.
//   if (moving) return;                   // refuse while moving
//   ArduinoOTA.handle();
// }
