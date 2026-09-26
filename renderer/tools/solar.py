"""NOAA / Meeus solar position: a line-for-line port of src/sun.h, so frame
schedules planned in Python land on exactly the sun the renderer draws."""
import math

DUBAI_LAT, DUBAI_LON, DUBAI_TZ = 25.2048, 55.2708, 4.0


def julian_day(y, m, d):
    if m <= 2:
        y -= 1
        m += 12
    A = y // 100
    B = 2 - A + A // 4
    return math.floor(365.25 * (y + 4716)) + math.floor(30.6001 * (m + 1)) + d + B - 1524.5


def solar_position(year, month, day, local_minutes, lat=DUBAI_LAT, lon=DUBAI_LON, tz=DUBAI_TZ):
    """Returns (altitude_deg, azimuth_deg) with refraction, azimuth clockwise from north."""
    jd = julian_day(year, month, day) + local_minutes / 1440.0 - tz / 24.0
    T = (jd - 2451545.0) / 36525.0
    L0 = math.fmod(280.46646 + T * (36000.76983 + T * 0.0003032), 360.0)
    if L0 < 0:
        L0 += 360.0
    M = 357.52911 + T * (35999.05029 - 0.0001537 * T)
    e = 0.016708634 - T * (0.000042037 + 0.0000001267 * T)
    Mr = math.radians(M)
    C = (math.sin(Mr) * (1.914602 - T * (0.004817 + 0.000014 * T)) + math.sin(2 * Mr) * (0.019993 - 0.000101 * T)
         + math.sin(3 * Mr) * 0.000289)
    omega = 125.04 - 1934.136 * T
    lam = L0 + C - 0.00569 - 0.00478 * math.sin(math.radians(omega))
    eps0 = 23.0 + (26.0 + (21.448 - T * (46.815 + T * (0.00059 - T * 0.001813))) / 60.0) / 60.0
    eps = eps0 + 0.00256 * math.cos(math.radians(omega))
    decl = math.degrees(math.asin(math.sin(math.radians(eps)) * math.sin(math.radians(lam))))
    y = math.tan(math.radians(eps / 2.0)) ** 2
    L0r = math.radians(L0)
    eq = 4.0 * math.degrees(y * math.sin(2 * L0r) - 2 * e * math.sin(Mr) + 4 * e * y * math.sin(Mr) * math.cos(2 * L0r)
                            - 0.5 * y * y * math.sin(4 * L0r) - 1.25 * e * e * math.sin(2 * Mr))
    tst = math.fmod(local_minutes + eq + 4.0 * lon - 60.0 * tz, 1440.0)
    if tst < 0:
        tst += 1440.0
    ha = tst / 4.0 - 180.0
    latr, declr, har = math.radians(lat), math.radians(decl), math.radians(ha)
    cz = math.sin(latr) * math.sin(declr) + math.cos(latr) * math.cos(declr) * math.cos(har)
    cz = max(-1.0, min(1.0, cz))
    elev = 90.0 - math.degrees(math.acos(cz))
    az = math.degrees(math.atan2(math.sin(har), math.cos(har) * math.sin(latr) - math.tan(declr) * math.cos(latr))) + 180.0
    az = math.fmod(az + 360.0, 360.0)
    refr = 0.0
    if elev <= 85.0:
        te = math.tan(math.radians(elev))
        if elev > 5.0:
            refr = 58.1 / te - 0.07 / te ** 3 + 0.000086 / te ** 5
        elif elev > -0.575:
            refr = 1735.0 + elev * (-518.2 + elev * (103.4 + elev * (-12.79 + elev * 0.711)))
        else:
            refr = -20.772 / te
        refr /= 3600.0
    return elev + refr, az


def sun_dir(alt, az):
    """Unit vector to the sun in scene space (+X east, +Y up, +Z south)."""
    a, A = math.radians(alt), math.radians(az)
    return (math.cos(a) * math.sin(A), math.sin(a), -math.cos(a) * math.cos(A))


def solar_noon_minutes(y, m, d):
    lo, hi = 600.0, 840.0
    for _ in range(80):
        m1, m2 = lo + (hi - lo) * 0.382, lo + (hi - lo) * 0.618
        if solar_position(y, m, d, m1)[0] < solar_position(y, m, d, m2)[0]:
            lo = m1
        else:
            hi = m2
    return 0.5 * (lo + hi)


def clock(minutes):
    """Minutes after midnight -> 'HH:MM:SS.s' as the renderer's --time parses it."""
    h = int(minutes // 60)
    m = int(minutes - h * 60)
    s = (minutes - h * 60 - m) * 60.0
    return f"{h:02d}:{m:02d}:{s:04.1f}"


if __name__ == "__main__":
    for t in (7 * 60 + 30, 8 * 60, 17 * 60, 12 * 60 + 20 + 43 / 60):
        print(clock(t), ["%.2f" % v for v in solar_position(2026, 6, 21, t)])
    n = solar_noon_minutes(2026, 6, 21)
    print("zawal", clock(n), ["%.3f" % v for v in solar_position(2026, 6, 21, n)])
