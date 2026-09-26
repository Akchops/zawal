#!/usr/bin/env python3
"""Plans the two pre-rendered sequences frame by frame and writes them as data:

    sequences/seq_a.json   A · the courtyard's day (fixed camera, warped clock)
    sequences/seq_b.json   B · street -> gate -> dahliz -> courtyard (camera path)

Each file holds a landscape and a portrait variant. A frame is
{i, file, minutes, time, alt, az, cam: {pos, tgt, fov, fovAxis, shift}, ev,
hold_of?}. `hold_of` marks a frame that repeats an earlier one (the zawal
hold), which the render driver copies instead of rendering.

The site reads the same files (minute, sun and exposure per frame), so the HUD
and the pixels come from one source.
"""
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from solar import clock, solar_noon_minutes, solar_position, sun_dir  # noqa: E402

DATE = (2026, 6, 21)
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "sequences")

# Courtyard geometry (scenes.h): canopy lattice at HC over X 0..CW, Z KZ0..KZ1;
# walls rise to H; the loggia front is at CL.
CW, CL, H, HC, KZ0, KZ1 = 7.2, 13.2, 7.4, 5.2, 3.0, 10.0

# The courtyard camera, landscape and portrait: SEQ-B ends on it, SEQ-A holds it.
COURT = {"pos": [3.6, 1.55, 0.9], "tgt": [3.6, 1.55, 14.0], "fov": 84.0, "fovAxis": "h", "shift": 0.22}
COURT_P = {"pos": [3.6, 1.55, 0.9], "tgt": [3.6, 1.55, 14.0], "fov": 88.0, "fovAxis": "v", "shift": 0.16}


# ---------------------------------------------------------------------------
# SEQ-A: the day, time-warped by how fast the canopy's pattern moves.
def pattern_speed(minutes, dt=0.5):
    """Mean speed (m/min) of the canopy pattern where it lands in the courtyard."""
    def landings(t):
        alt, az = solar_position(*DATE, t)
        s = sun_dir(alt, az)
        out = {}
        if s[1] <= 0.01:
            return out
        for ix in range(8):
            for iz in range(8):
                p = (0.45 + ix * (CW - 0.9) / 7, HC, KZ0 + 0.4 + iz * (KZ1 - KZ0 - 0.8) / 7)
                # Is the canopy point lit? The ray to the sun must clear the walls (top at H).
                k = (H - p[1]) / s[1]
                q = (p[0] + s[0] * k, p[2] + s[2] * k)
                if not (0.0 <= q[0] <= CW and -0.45 <= q[1] <= CL + 3.45):
                    continue
                # Where does its shadow land: floor, or one of the walls?
                best = None
                d = (-s[0], -s[1], -s[2])
                for axis, plane in ((1, 0.0), (0, 0.0), (0, CW), (2, 0.0), (2, CL)):
                    if abs(d[axis]) < 1e-9:
                        continue
                    t_hit = (plane - p[axis]) / d[axis]
                    if t_hit <= 0:
                        continue
                    x = [p[a] + d[a] * t_hit for a in range(3)]
                    if -1e-6 <= x[0] <= CW + 1e-6 and -1e-6 <= x[1] <= H and -1e-6 <= x[2] <= CL + 1e-6:
                        if best is None or t_hit < best[0]:
                            best = (t_hit, x)
                if best:
                    out[(ix, iz)] = best[1]
        return out

    a, b = landings(minutes - dt), landings(minutes + dt)
    common = [k for k in a if k in b]
    if not common:
        return 0.0
    return sum(math.dist(a[k], b[k]) for k in common) / len(common) / (2 * dt)


def seq_a_times(n=121, hold=6, t0=8 * 60, t1=18 * 60 + 50):
    """Frame minutes: equal steps of visual change, zawal exact and held."""
    noon = solar_noon_minutes(*DATE)
    ts = [t0 + i * 0.5 for i in range(int((t1 - t0) / 0.5) + 1)]
    # Visual change per minute: the pattern's speed, floored so the light
    # still gets frames after the pattern has left (colour, bounce, dusk).
    rate = [max(pattern_speed(t), 0.018) for t in ts]
    cum = [0.0]
    for k in range(1, len(ts)):
        cum.append(cum[-1] + 0.5 * (rate[k] + rate[k - 1]) * 0.5)

    def minute_at(s):
        for k in range(1, len(cum)):
            if cum[k] >= s:
                f = (s - cum[k - 1]) / max(cum[k] - cum[k - 1], 1e-12)
                return ts[k - 1] + f * (ts[k] - ts[k - 1])
        return ts[-1]

    s_noon = next(cum[k] for k in range(len(ts)) if ts[k] >= noon)
    moving = n - hold          # distinct frames besides the held copies (zawal counted once)
    # Split the moving frames before/after zawal in proportion to arc length.
    before = round((moving - 1) * s_noon / cum[-1])
    after = moving - 1 - before
    frames = [minute_at(s_noon * i / before) for i in range(before)]
    frames += [noon] * (hold + 1)
    frames += [minute_at(s_noon + (cum[-1] - s_noon) * i / after) for i in range(1, after + 1)]
    frames[0], frames[-1] = float(t0), float(t1)
    assert len(frames) == n, len(frames)
    return frames, before


def ev_for_minutes(m):
    """Exposure authored along the day (never automatic)."""
    keys = [(8 * 60, 1.2), (10 * 60, 0.8), (solar_noon_minutes(*DATE), 0.4), (14 * 60 + 30, 0.7),
            (17 * 60, 1.3), (18 * 60, 1.8), (18 * 60 + 30, 2.3), (18 * 60 + 50, 2.8)]
    for (a, ea), (b, eb) in zip(keys, keys[1:]):
        if m <= b:
            f = (m - a) / (b - a)
            f = f * f * (3 - 2 * f)
            return round(ea + (eb - ea) * f, 3)
    return keys[-1][1]


def plan_a():
    mins, zawal_index = seq_a_times()
    out = {"id": "A", "title": "A day in one scroll", "date": "2026-06-21", "tz": "+04:00", "zawal_index": zawal_index}
    for name, cam in (("landscape", COURT), ("portrait", COURT_P)):
        frames = []
        for i, m in enumerate(mins):
            alt, az = solar_position(*DATE, m)
            f = {"i": i, "file": f"a{i:03d}", "minutes": round(m, 4), "time": clock(m), "alt": round(alt, 3), "az": round(az, 3),
                 "cam": cam, "ev": ev_for_minutes(m)}
            if i > 0 and abs(m - mins[i - 1]) < 1e-9:
                f["hold_of"] = frames[i - 1].get("hold_of", i - 1)
            frames.append(f)
        out[name] = {"size": [1600, 900] if name == "landscape" else [720, 1280], "frames": frames}
    return out


# ---------------------------------------------------------------------------
# SEQ-B: the walk. Waypoints by frame; position on a centripetal Catmull-Rom,
# heading and lens interpolated with smoothstep between waypoints.
def yaw_of(pos, tgt):
    return math.degrees(math.atan2(tgt[0] - pos[0], tgt[2] - pos[2]))


def catmull(p0, p1, p2, p3, t):
    def tj(ti, a, b):
        return ti + max(math.dist(a, b), 1e-4) ** 0.5
    t0 = 0.0
    t1 = tj(t0, p0, p1)
    t2 = tj(t1, p1, p2)
    t3 = tj(t2, p2, p3)
    u = t1 + (t2 - t1) * t

    def lerp(a, b, ta, tb):
        return [a[k] + (b[k] - a[k]) * (u - ta) / (tb - ta) for k in range(3)]
    A1, A2, A3 = lerp(p0, p1, t0, t1), lerp(p1, p2, t1, t2), lerp(p2, p3, t2, t3)
    B1, B2 = lerp(A1, A2, t0, t2), lerp(A2, A3, t1, t3)
    return lerp(B1, B2, t1, t2)


def plan_b():
    GC = -2.45
    # frame, position, heading (deg, 0 = +Z south, 180 = north, -90 = west), fov (landscape h), shift, ev
    street0 = [16.4, 1.62, 8.2]
    way = [
        (0, street0, yaw_of(street0, [10.4, 1.62, -20.0]) % 360.0, 76.0, 0.46, 1.0),
        (36, [15.2, 1.62, 0.9], 196.0, 76.0, 0.42, 1.0),
        (48, [14.1, 1.62, -1.5], 228.0, 78.0, 0.36, 1.2),
        # Into the passage: dark, the eye adapting (exposure pushed, but it
        # must still read as shade, not as a lit room).
        (60, [12.7, 1.62, GC], 268.0, 80.0, 0.30, 1.8),
        (72, [8.4, 1.62, GC], 270.0, 80.0, 0.28, 2.2),
        # The turn: the opening to the courtyard glows on the left and pulls
        # the camera round toward it, so the turn happens looking at light.
        (80, [5.5, 1.61, GC + 0.05], 296.0, 82.0, 0.26, 2.3),
        (86, [3.62, 1.59, -1.75], 360.0, 84.0, 0.24, 1.9),
        (96, COURT["pos"], 360.0, 84.0, 0.22, 1.2),
    ]
    # Unwrap headings so each segment turns the short way (-168 and 192 are
    # the same direction; interpolating between them would spin the camera).
    for k in range(1, len(way)):
        y = way[k][2]
        while y - way[k - 1][2] > 180.0:
            y -= 360.0
        while y - way[k - 1][2] < -180.0:
            y += 360.0
        way[k] = (way[k][0], way[k][1], y) + tuple(way[k][3:])
    n = 97
    minutes = [7 * 60 + 30 + 30.0 * i / (n - 1) for i in range(n)]

    # Portrait starts on the approved portrait street composition (street_p)
    # and joins the common path by frame 36.
    street0_p = [16.2, 1.62, 8.6]
    way_p = [(0, street0_p, yaw_of(street0_p, [11.6, 1.62, -20.0]) % 360.0) + tuple(way[0][3:])] + way[1:]

    def at(i, portrait):
        way_ = way_p if portrait else way
        for k in range(len(way_) - 1):
            fa, fb = way_[k][0], way_[k + 1][0]
            if fa <= i <= fb:
                t = (i - fa) / (fb - fa)
                p0 = way_[max(k - 1, 0)][1]
                p1, p2 = way_[k][1], way_[k + 1][1]
                p3 = way_[min(k + 2, len(way_) - 1)][1]
                pos = catmull(p0, p1, p2, p3, t) if p1 != p2 else list(p1)
                s = t * t * (3 - 2 * t)
                ya, yb = way_[k][2], way_[k + 1][2]
                yaw = ya + (yb - ya) * s
                fov = way_[k][3] + (way_[k + 1][3] - way_[k][3]) * s
                shift = way_[k][4] + (way_[k + 1][4] - way_[k][4]) * s
                ev = way_[k][5] + (way_[k + 1][5] - way_[k][5]) * s
                return pos, yaw, fov, shift, ev
        return way_[-1][1], way_[-1][2], way_[-1][3], way_[-1][4], way_[-1][5]

    out = {"id": "B", "title": "Street to courtyard", "date": "2026-06-21", "tz": "+04:00"}
    for name in ("landscape", "portrait"):
        frames = []
        for i in range(n):
            pos, yaw, fov, shift, ev = at(i, name == "portrait")
            y = math.radians(yaw)
            tgt = [pos[0] + math.sin(y) * 20.0, pos[1], pos[2] + math.cos(y) * 20.0]
            if name == "landscape":
                cam = {"pos": [round(v, 4) for v in pos], "tgt": [round(v, 4) for v in tgt], "fov": round(fov, 3), "fovAxis": "h",
                       "shift": round(shift, 4)}
            else:
                # Portrait: a vertical lens from 84 deg on the street to the court_p lens.
                f = i / (n - 1)
                cam = {"pos": [round(v, 4) for v in pos], "tgt": [round(v, 4) for v in tgt], "fov": round(84.0 + 4.0 * f, 3), "fovAxis": "v",
                       "shift": round(0.22 + (0.16 - 0.22) * f, 4)}
            if i == n - 1:
                cam = dict(COURT if name == "landscape" else COURT_P)
            m = minutes[i]
            alt, az = solar_position(*DATE, m)
            frames.append({"i": i, "file": f"b{i:03d}", "minutes": round(m, 4), "time": clock(m), "alt": round(alt, 3), "az": round(az, 3),
                           "cam": cam, "ev": round(ev, 3),
                           # Frames pushed by eye adaptation show their noise: more samples in the dark.
                           "sppScale": round(min(4.0, 2.0 ** (0.6 * max(0.0, ev - 1.2))), 3)})
        out[name] = {"size": [1600, 900] if name == "landscape" else [720, 1280], "frames": frames}
    return out


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    a, b = plan_a(), plan_b()
    for name, data in (("seq_a.json", a), ("seq_b.json", b)):
        with open(os.path.join(OUT, name), "w") as fh:
            json.dump(data, fh, indent=1)
    fa = a["landscape"]["frames"]
    print("A:", len(fa), "frames; zawal at", a["zawal_index"], "held",
          sum(1 for f in fa if "hold_of" in f), "; first/last", fa[0]["time"], fa[-1]["time"])
    gaps = [fa[i + 1]["minutes"] - fa[i]["minutes"] for i in range(len(fa) - 1) if fa[i + 1]["minutes"] != fa[i]["minutes"]]
    print("   step min/max minutes: %.1f / %.1f" % (min(gaps), max(gaps)))
    print("   ", " ".join(f["time"][:5] for f in fa[::10]))
    fb = b["landscape"]["frames"]
    print("B:", len(fb), "frames", fb[0]["time"], "->", fb[-1]["time"])
