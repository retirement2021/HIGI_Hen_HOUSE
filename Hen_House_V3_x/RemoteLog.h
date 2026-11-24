// ======================================================
// RemoteLog.h - RAM ring buffer that copies everything printed with Log.print / Log.println / Log.printf
// Log.* also forwards to Serial, so the USB serial monitor behaves exactly as before.
// The buffer is read over WiFi at http://<ip>/log (handlers live in Dashboard.ino).
// Single-threaded by design: only call Log from loop() and the functions it calls (never from an ISR).
// ======================================================
#pragma once
#include <Arduino.h>

#define REMOTE_LOG_SIZE 8192  // bytes held; must be a power of two

bool getTime(struct tm& ti);  // defined in main sketch

class RemoteLog : public Print {
public:
  using Print::write;

  size_t write(uint8_t c) override {
    put(c);
    Serial.write(c);
    return 1;
  }

  size_t write(const uint8_t* b, size_t n) override {
    for (size_t i = 0; i < n; i++) put(b[i]);
    Serial.write(b, n);
    return n;
  }

  // Appends the bytes from position `since` onwards (at most maxBytes) to `out`.
  // If `since` is older than what is still held (or in the future) it starts at the oldest byte held.
  // `from` = position the returned text really starts at. Returns the position to ask for next time.
  uint32_t readFrom(uint32_t since, String& out, uint32_t maxBytes, uint32_t& from) const {
    uint32_t t = _total;
    uint32_t oldest = (t > REMOTE_LOG_SIZE) ? (t - REMOTE_LOG_SIZE) : 0;
    if (since < oldest || since > t) since = oldest;
    uint32_t n = t - since;
    if (n > maxBytes) n = maxBytes;
    out.reserve(n);
    for (uint32_t i = 0; i < n; i++) out += (char)_buf[(since + i) & (REMOTE_LOG_SIZE - 1)];
    from = since;
    return since + n;
  }

private:
  void raw(uint8_t c) {
    _buf[_total & (REMOTE_LOG_SIZE - 1)] = c;
    _total++;
  }

  void put(uint8_t c) {
    if (c == '\r') return;  // keep the buffer LF-only
    if (_lineStart) {       // stamp each line with clock time
      _lineStart = false;
      if (!_stamping) {     // guard: getTime() must not re-enter the logger
        _stamping = true;
        uint32_t sec = millis() / 1000UL;
        if (sec != _lastSec || _stampLen == 0) {  // call getTime at most once per second
          _lastSec = sec;
          struct tm ti;
          int n;
          if (getTime(ti)) n = snprintf(_stamp, sizeof(_stamp), "[%02d:%02d:%02d] ", ti.tm_hour, ti.tm_min, ti.tm_sec);
          else n = snprintf(_stamp, sizeof(_stamp), "[--:--:--] ");
          _stampLen = (n > 0 && n < (int)sizeof(_stamp)) ? n : 0;
        }
        for (uint8_t i = 0; i < _stampLen; i++) raw((uint8_t)_stamp[i]);
        _stamping = false;
      }
    }
    raw(c);
    if (c == '\n') _lineStart = true;
  }

  uint8_t _buf[REMOTE_LOG_SIZE];
  uint32_t _total = 0;  // bytes ever written (wraps harmlessly after 4 GB)
  bool _lineStart = true;
  char _stamp[16];
  uint8_t _stampLen = 0;
  uint32_t _lastSec = 0;
  bool _stamping = false;
};

extern RemoteLog Log;
