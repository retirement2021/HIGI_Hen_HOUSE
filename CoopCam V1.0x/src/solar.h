#pragma once

#include <Arduino.h>
#include <time.h>

// One calendar day of solar events, computed for a given latitude/longitude.
// Times are UTC epoch seconds. For polar day/night, sunrise/sunset are set to
// solar noon and the corresponding flag is true.
struct SolarDay {
  bool valid = false;
  bool polarDay = false;
  bool polarNight = false;
  time_t sunrise = 0;
  time_t sunset = 0;
  time_t noon = 0;
};

// Days since 1970-01-01 for a proleptic Gregorian date.
int64_t daysFromCivil(int y, unsigned m, unsigned d);

// Inverse of daysFromCivil().
void civilFromDays(int64_t days, int &y, unsigned &m, unsigned &d);

// Compute sunrise/sunset for the UTC civil date that starts at
// dayIndex * 86400 seconds. lat is +north, lon is +east.
SolarDay solarForDay(int64_t dayIndex, double lat, double lon);
