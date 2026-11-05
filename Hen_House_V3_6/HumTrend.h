#pragma once
#include <Arduino.h>

struct HumTrend {
  static const uint8_t N = 7;
  float v[N];
  uint32_t t[N];
  uint8_t n = 0;
};