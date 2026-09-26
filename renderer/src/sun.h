// Solar position — NOAA / Meeus low-precision algorithm (accurate to ~0.01 deg
// for 1901-2099, far below the 0.53 deg the solar disc subtends).
//
// The site uses the identical algorithm in JavaScript (src/lib/solar.js) so
// the HUD numbers and the rendered shadows can never disagree.
#pragma once
#include "common.h"

namespace zw {

struct SunPos {
  double altitudeDeg;   // true altitude above the horizon, refraction-corrected
  double azimuthDeg;    // clockwise from true north
  double declinationDeg;
  double eqTimeMin;     // equation of time, minutes
  V3 dir;               // unit vector pointing TO the sun, world space (+X E, +Y up, +Z S)
};

// Dubai (city reference point, ~25.2 N as the brief specifies). Timezone GST =
// UTC+4, no daylight saving. Individual project sites can pass their own lat/lon.
constexpr double DUBAI_LAT = 25.2048;
constexpr double DUBAI_LON = 55.2708;
constexpr double DUBAI_TZ = 4.0;

inline double deg2rad(double d) { return d * 3.14159265358979323846 / 180.0; }
inline double rad2deg(double r) { return r * 180.0 / 3.14159265358979323846; }

// Julian day for a Gregorian calendar date at 00:00 UTC.
inline double julianDay(int y, int m, int d) {
  if (m <= 2) { y -= 1; m += 12; }
  int A = y / 100;
  int B = 2 - A + A / 4;
  return std::floor(365.25 * (y + 4716)) + std::floor(30.6001 * (m + 1)) + d + B - 1524.5;
}

// localMinutes: minutes after local midnight (GST). Returns the full position.
inline SunPos solarPosition(int year, int month, int day, double localMinutes,
                            double latDeg = DUBAI_LAT, double lonDeg = DUBAI_LON,
                            double tz = DUBAI_TZ) {
  double jd = julianDay(year, month, day) + (localMinutes / 1440.0) - tz / 24.0;
  double T = (jd - 2451545.0) / 36525.0;  // Julian centuries since J2000.0

  double L0 = std::fmod(280.46646 + T * (36000.76983 + T * 0.0003032), 360.0);  // mean longitude
  if (L0 < 0) L0 += 360.0;
  double M = 357.52911 + T * (35999.05029 - 0.0001537 * T);                      // mean anomaly
  double e = 0.016708634 - T * (0.000042037 + 0.0000001267 * T);                 // orbit eccentricity
  double Mr = deg2rad(M);
  double C = std::sin(Mr) * (1.914602 - T * (0.004817 + 0.000014 * T)) +
             std::sin(2 * Mr) * (0.019993 - 0.000101 * T) + std::sin(3 * Mr) * 0.000289;  // equation of centre
  double trueLong = L0 + C;
  double omega = 125.04 - 1934.136 * T;
  double lambda = trueLong - 0.00569 - 0.00478 * std::sin(deg2rad(omega));  // apparent longitude
  double eps0 = 23.0 + (26.0 + (21.448 - T * (46.815 + T * (0.00059 - T * 0.001813))) / 60.0) / 60.0;
  double eps = eps0 + 0.00256 * std::cos(deg2rad(omega));  // corrected obliquity
  double decl = rad2deg(std::asin(std::sin(deg2rad(eps)) * std::sin(deg2rad(lambda))));

  double y = std::tan(deg2rad(eps / 2.0));
  y *= y;
  double L0r = deg2rad(L0);
  double eqTime = 4.0 * rad2deg(y * std::sin(2 * L0r) - 2 * e * std::sin(Mr) +
                                4 * e * y * std::sin(Mr) * std::cos(2 * L0r) -
                                0.5 * y * y * std::sin(4 * L0r) - 1.25 * e * e * std::sin(2 * Mr));

  double trueSolarTime = std::fmod(localMinutes + eqTime + 4.0 * lonDeg - 60.0 * tz, 1440.0);
  if (trueSolarTime < 0) trueSolarTime += 1440.0;
  double hourAngle = trueSolarTime / 4.0 - 180.0;  // degrees, negative in the morning

  double latr = deg2rad(latDeg), declr = deg2rad(decl), har = deg2rad(hourAngle);
  double cosZen = std::sin(latr) * std::sin(declr) + std::cos(latr) * std::cos(declr) * std::cos(har);
  cosZen = std::max(-1.0, std::min(1.0, cosZen));
  double zenith = rad2deg(std::acos(cosZen));
  double elev = 90.0 - zenith;

  // Azimuth measured from north, clockwise.
  double az = rad2deg(std::atan2(std::sin(har),
                                 std::cos(har) * std::sin(latr) - std::tan(declr) * std::cos(latr))) + 180.0;
  az = std::fmod(az + 360.0, 360.0);

  // Atmospheric refraction (NOAA piecewise fit), degrees.
  double refr = 0.0;
  if (elev <= 85.0) {
    double te = std::tan(deg2rad(elev));
    if (elev > 5.0) refr = 58.1 / te - 0.07 / (te * te * te) + 0.000086 / std::pow(te, 5);
    else if (elev > -0.575) refr = 1735.0 + elev * (-518.2 + elev * (103.4 + elev * (-12.79 + elev * 0.711)));
    else refr = -20.772 / te;
    refr /= 3600.0;
  }
  double alt = elev + refr;

  SunPos s;
  s.altitudeDeg = alt;
  s.azimuthDeg = az;
  s.declinationDeg = decl;
  s.eqTimeMin = eqTime;
  double a = deg2rad(alt), A = deg2rad(az);
  // East = sin(A)cos(a), North = cos(A)cos(a), Up = sin(a); +Z is South.
  s.dir = normalize(V3((float)(std::cos(a) * std::sin(A)), (float)std::sin(a), (float)(-std::cos(a) * std::cos(A))));
  return s;
}

// Solar noon (zawal) in local minutes, found by golden-section search on altitude.
inline double solarNoonMinutes(int y, int m, int d) {
  double lo = 600, hi = 840;
  for (int i = 0; i < 80; i++) {
    double m1 = lo + (hi - lo) * 0.382, m2 = lo + (hi - lo) * 0.618;
    if (solarPosition(y, m, d, m1).altitudeDeg < solarPosition(y, m, d, m2).altitudeDeg) lo = m1; else hi = m2;
  }
  return 0.5 * (lo + hi);
}

}  // namespace zw
