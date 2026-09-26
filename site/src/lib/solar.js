// Solar position: NOAA / Meeus low-precision algorithm, a line-for-line port
// of renderer/src/sun.h, so the numbers on screen and the shadows in the
// rendered frames come from the same equations.
//
// Angles in degrees. Azimuth is clockwise from true north. Times are local
// minutes after midnight in Gulf Standard Time (UTC+4, no DST).

export const DUBAI = { lat: 25.2048, lon: 55.2708, tz: 4 };

const rad = (d) => (d * Math.PI) / 180;
const deg = (r) => (r * 180) / Math.PI;

function julianDay(y, m, d) {
  if (m <= 2) { y -= 1; m += 12; }
  const A = Math.floor(y / 100);
  const B = 2 - A + Math.floor(A / 4);
  return Math.floor(365.25 * (y + 4716)) + Math.floor(30.6001 * (m + 1)) + d + B - 1524.5;
}

/** Sun altitude/azimuth (refraction-corrected) for a local date and minute. */
export function solarPosition(year, month, day, minutes, site = DUBAI) {
  const { lat, lon, tz } = site;
  const jd = julianDay(year, month, day) + minutes / 1440 - tz / 24;
  const T = (jd - 2451545.0) / 36525.0;
  let L0 = (280.46646 + T * (36000.76983 + T * 0.0003032)) % 360;
  if (L0 < 0) L0 += 360;
  const M = 357.52911 + T * (35999.05029 - 0.0001537 * T);
  const e = 0.016708634 - T * (0.000042037 + 0.0000001267 * T);
  const Mr = rad(M);
  const C = Math.sin(Mr) * (1.914602 - T * (0.004817 + 0.000014 * T)) +
    Math.sin(2 * Mr) * (0.019993 - 0.000101 * T) + Math.sin(3 * Mr) * 0.000289;
  const omega = 125.04 - 1934.136 * T;
  const lambda = L0 + C - 0.00569 - 0.00478 * Math.sin(rad(omega));
  const eps0 = 23 + (26 + (21.448 - T * (46.815 + T * (0.00059 - T * 0.001813))) / 60) / 60;
  const eps = eps0 + 0.00256 * Math.cos(rad(omega));
  const decl = deg(Math.asin(Math.sin(rad(eps)) * Math.sin(rad(lambda))));
  let y = Math.tan(rad(eps / 2));
  y *= y;
  const L0r = rad(L0);
  const eqTime = 4 * deg(y * Math.sin(2 * L0r) - 2 * e * Math.sin(Mr) + 4 * e * y * Math.sin(Mr) * Math.cos(2 * L0r) -
    0.5 * y * y * Math.sin(4 * L0r) - 1.25 * e * e * Math.sin(2 * Mr));
  let tst = (minutes + eqTime + 4 * lon - 60 * tz) % 1440;
  if (tst < 0) tst += 1440;
  const ha = tst / 4 - 180;
  const latr = rad(lat), declr = rad(decl), har = rad(ha);
  let cz = Math.sin(latr) * Math.sin(declr) + Math.cos(latr) * Math.cos(declr) * Math.cos(har);
  cz = Math.max(-1, Math.min(1, cz));
  const elev = 90 - deg(Math.acos(cz));
  let az = deg(Math.atan2(Math.sin(har), Math.cos(har) * Math.sin(latr) - Math.tan(declr) * Math.cos(latr))) + 180;
  az = (az + 360) % 360;
  let refr = 0;
  if (elev <= 85) {
    const te = Math.tan(rad(elev));
    if (elev > 5) refr = 58.1 / te - 0.07 / te ** 3 + 0.000086 / te ** 5;
    else if (elev > -0.575) refr = 1735 + elev * (-518.2 + elev * (103.4 + elev * (-12.79 + elev * 0.711)));
    else refr = -20.772 / te;
    refr /= 3600;
  }
  return { alt: elev + refr, az, decl, eqTime };
}

/** Unit vector to the sun in the scene frame (+X east, +Y up, +Z south). */
export function sunDir(alt, az) {
  const a = rad(alt), A = rad(az);
  return [Math.cos(a) * Math.sin(A), Math.sin(a), -Math.cos(a) * Math.cos(A)];
}

/** Local solar noon (zawal), minutes after midnight, by golden-section search. */
export function solarNoon(year, month, day, site = DUBAI) {
  let lo = 600, hi = 840;
  for (let i = 0; i < 60; i++) {
    const m1 = lo + (hi - lo) * 0.382, m2 = lo + (hi - lo) * 0.618;
    if (solarPosition(year, month, day, m1, site).alt < solarPosition(year, month, day, m2, site).alt) lo = m1; else hi = m2;
  }
  return 0.5 * (lo + hi);
}

/** "12:20:42" from minutes after midnight. */
export function clock(minutes, withSeconds = false) {
  const total = Math.round(minutes * 60);
  const h = Math.floor(total / 3600) % 24, m = Math.floor((total % 3600) / 60), s = total % 60;
  const p = (n) => String(n).padStart(2, "0");
  return withSeconds ? `${p(h)}:${p(m)}:${p(s)}` : `${p(h)}:${p(m)}`;
}

const MONTHS = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"];

/**
 * Inverse problem for "You are the sun": which day and minute of the year in
 * Dubai puts the sun at (alt, az)? Searches the year on a coarse grid, then
 * refines. Returns null when no day has that sun (angular error > tolerance).
 */
export function whenIsTheSun(alt, az, year = 2026, site = DUBAI, tolDeg = 1.2) {
  const target = sunDir(alt, az);
  const dist = (p) => {
    const d = sunDir(p.alt, p.az);
    return Math.acos(Math.max(-1, Math.min(1, d[0] * target[0] + d[1] * target[1] + d[2] * target[2])));
  };
  let best = { err: Infinity, month: 6, day: 21, minutes: 720 };
  const daysIn = (m) => new Date(Date.UTC(year, m, 0)).getUTCDate();
  for (let m = 1; m <= 12; m++) {
    for (let d = 1; d <= daysIn(m); d += 3) {
      for (let t = 300; t <= 1200; t += 10) {
        const err = dist(solarPosition(year, m, d, t, site));
        if (err < best.err) best = { err, month: m, day: d, minutes: t };
      }
    }
  }
  // Refine around the best coarse hit: day +-3, minute +-10.
  const b0 = best;
  for (let dd = -3; dd <= 3; dd++) {
    const date = new Date(Date.UTC(year, b0.month - 1, b0.day + dd));
    const m = date.getUTCMonth() + 1, d = date.getUTCDate();
    for (let t = b0.minutes - 10; t <= b0.minutes + 10; t += 0.5) {
      const err = dist(solarPosition(year, m, d, t, site));
      if (err < best.err) best = { err, month: m, day: d, minutes: t };
    }
  }
  if (deg(best.err) > tolDeg) return null;
  return { ...best, errDeg: deg(best.err), label: `${best.day} ${MONTHS[best.month - 1]}, ${clock(best.minutes)}` };
}
