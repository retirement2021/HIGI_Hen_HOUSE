#include "solar.h"

#include <math.h>

// -----------------------------------------------------------------------------
// Calendar helpers (Howard Hinnant's civil-from-days algorithms)
// -----------------------------------------------------------------------------
int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int64_t)doe - 719468;
}

void civilFromDays(int64_t z, int &y, unsigned &m, unsigned &d) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = (unsigned)(z - era * 146097);
  const unsigned yoe =
      (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  y = (int)yoe + (int)(era * 400);
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp + (mp < 10 ? 3 : -9);
  y += (m <= 2);
}

// -----------------------------------------------------------------------------
// NOAA solar position / sunrise-sunset calculation.
// This is the same algorithm used by the NOAA Solar Calculator spreadsheet.
// Accuracy is within a few seconds for normal latitudes.
// -----------------------------------------------------------------------------
static constexpr double PI_D = 3.14159265358979323846;

static double rad(double degrees) { return degrees * PI_D / 180.0; }
static double deg(double radians) { return radians * 180.0 / PI_D; }

SolarDay solarForDay(int64_t dayIndex, double lat, double lon) {
  SolarDay out;
  if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
    return out;
  }

  // Julian day at 00:00 UTC of this civil date.
  const double jd = 2440587.5 + (double)dayIndex;
  const double T = (jd - 2451545.0) / 36525.0;

  // Geometric mean longitude of the Sun (degrees).
  double L0 = fmod(280.46646 + T * (36000.76983 + T * 0.0003032), 360.0);
  if (L0 < 0.0) L0 += 360.0;

  // Geometric mean anomaly (degrees).
  const double M = 357.52911 + T * (35999.05029 - 0.0001537 * T);

  // Eccentricity of Earth's orbit.
  const double e = 0.016708634 - T * (0.000042037 + 0.0000001267 * T);

  // Equation of the center (degrees).
  const double C =
      sin(rad(M)) * (1.914602 - T * (0.004817 + 0.000014 * T)) +
      sin(rad(2 * M)) * (0.019993 - 0.000101 * T) +
      sin(rad(3 * M)) * 0.000289;

  const double trueLong = L0 + C;

  // Apparent longitude, corrected for nutation and aberration.
  const double lambda =
      trueLong - 0.00569 - 0.00478 * sin(rad(125.04 - 1934.136 * T));

  // Mean obliquity of the ecliptic (degrees).
  const double eps0 =
      23.0 + (26.0 + (21.448 - T * (46.815 + T * (0.00059 - T * 0.001813))) /
                            60.0) /
                 60.0;
  const double eps = eps0 + 0.00256 * cos(rad(125.04 - 1934.136 * T));

  // Solar declination (degrees).
  const double decl = deg(asin(sin(rad(eps)) * sin(rad(lambda))));

  // Equation of time (minutes).
  double y = tan(rad(eps / 2.0));
  y *= y;
  const double eot =
      4.0 * deg(y * sin(rad(2 * L0)) - 2.0 * e * sin(rad(M)) +
                4.0 * e * y * sin(rad(M)) * cos(rad(2 * L0)) -
                0.5 * y * y * sin(rad(4 * L0)) -
                1.25 * e * e * sin(rad(2 * M)));

  // Solar noon in minutes from 00:00 UTC. Longitude is positive east.
  const double noonMin = 720.0 - 4.0 * lon - eot;

  // Hour angle at sunrise/sunset. 90.833 deg includes refraction + solar radius.
  const double cosHA = cos(rad(90.833)) /
                           (cos(rad(lat)) * cos(rad(decl))) -
                       tan(rad(lat)) * tan(rad(decl));

  out.valid = true;
  out.noon = (time_t)(dayIndex * 86400 + llround(noonMin * 60.0));

  if (cosHA > 1.0) {
    // Sun never rises.
    out.polarNight = true;
    out.sunrise = out.sunset = out.noon;
    return out;
  }
  if (cosHA < -1.0) {
    // Sun never sets.
    out.polarDay = true;
    out.sunrise = out.sunset = out.noon;
    return out;
  }

  const double ha = deg(acos(cosHA));
  out.sunrise =
      (time_t)(dayIndex * 86400 + llround((noonMin - 4.0 * ha) * 60.0));
  out.sunset =
      (time_t)(dayIndex * 86400 + llround((noonMin + 4.0 * ha) * 60.0));
  return out;
}
