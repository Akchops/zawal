#!/usr/bin/env python3
"""The approach: ten seconds of scroll from the sun's own line down onto the
day's first frame.

  0.0-2.0 s  on the sun's line. The camera stands where the sun is, looking
             along its rays at the courtyard house, and dollies in. From there
             every shadow hides behind the thing that casts it, so the quarter
             reads flat and shadowless.
  2.0-5.6 s  it peels off the line and spirals down, east, south, west, north,
             the shadows opening as it leaves the sun.
  5.6-7.0 s  from the north it skims the roofs; the wind tower passes on the left.
  7.0-8.4 s  over the courtyard it tips down and dives through one star of the
             canopy lattice (holes ~23 cm across; the camera is a point).
  8.4-10 s   under the canopy it pulls back north and down and lands exactly on
             SEQ-A frame 0 (08:00, the courtyard camera).

    plan_approach.py spec       write the portrait path into sequences/seq_b.json
    plan_approach.py preview    rough low-resolution phone preview of the whole move
                                -> <scratch>/approach2/p/NNN.png, approach.webp/.gif, sheet

Frames are planned as data (pos, target, fov, shift, ev, minute) like
plan_sequences.py; the last frame IS SEQ-A frame 0 (the full render copies
its image), so the hand-over to the day has no jump.
"""
import json
import math
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(__file__))
from solar import clock, solar_position, sun_dir  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
BIN = os.environ.get("ZAWAL_BIN", os.path.join(ROOT, "bin", "zawal"))
SEQ_B = os.path.join(ROOT, "sequences", "seq_b.json")
SEQ_A = os.path.join(ROOT, "sequences", "seq_a.json")

DURATION = 10.0
T0_MIN, T1_MIN = 450.0, 480.0          # 07:30 -> 08:00, as the street walk did
C = (4.0, 2.0, 6.6)                    # what the opening and the orbit look at: the courtyard
SX, SY, SZ = 3.6, 5.25, 3.96           # the star: a canopy hole clear of the teak beams
END = {"pos": (3.6, 1.55, 0.9), "tgt": (3.6, 1.55, 14.0)}   # SEQ-A frame 0
END_FOV = {"l": 84.0, "p": 88.0}
END_SHIFT = {"l": 0.22, "p": 0.16}
END_EV = 1.2
SUNLINE_T = 1.6                        # up to here the camera is exactly on the sun's line

# Frames per segment (uniform in time inside a segment). Dense where the
# picture changes fastest: the tower pass and the dive.
SEGMENTS = [(0.0, 2.0, 20), (2.0, 5.6, 44), (5.6, 7.0, 26), (7.0, 8.4, 34), (8.4, 10.0, 26)]


def minutes_at(t):
    return T0_MIN + (T1_MIN - T0_MIN) * t / DURATION


def sun_at(t):
    alt, az = solar_position(2026, 6, 21, minutes_at(t))
    return alt, az


def on_sunline(t, dist):
    alt, az = sun_at(t)
    d = sun_dir(alt, az)
    return tuple(C[k] + dist * d[k] for k in range(3))


def sph(phi, r, el):
    """phi: 0 = east, 90 = south (+z), 180 = west, 270 = north. el: degrees up."""
    p, e = math.radians(phi), math.radians(el)
    return (C[0] + r * math.cos(e) * math.cos(p), C[1] + r * math.sin(e), C[2] + r * math.cos(e) * math.sin(p))


def sun_phi_el(t):
    alt, az = sun_at(t)
    d = sun_dir(alt, az)
    return math.degrees(math.atan2(d[2], d[0])), alt


def sunline_dist(t):
    # 330 m -> 190 m over the opening, easing out into the peel-off
    u = min(1.0, t / 2.0)
    return 330.0 - 140.0 * (1.0 - (1.0 - u) ** 1.6)


def keys():
    """(t, pos, tgt, fov_l, fov_p, shiftScale, ev)"""
    k = []
    for t in (0.0, 0.5, 1.0, SUNLINE_T):
        k.append((t, on_sunline(t, sunline_dist(t)), C, 24.0 + 4.0 * t, 20.0 + 4.0 * t, 0.0, 0.75))
    ps, es = sun_phi_el(2.0)
    # The spiral: off the sun's line, east -> south -> west -> north, descending
    # at a near-constant angle of view, closing in.
    spiral = [(2.1, ps + 6, 176, es + 0.5), (2.7, 20, 150, 26), (3.3, 70, 118, 26.5), (3.9, 125, 90, 26.5),
              (4.5, 180, 66, 26), (5.1, 228, 46, 25), (5.6, 258, 32, 23)]
    for j, (t, phi, r, el) in enumerate(spiral):
        a = j / (len(spiral) - 1)
        k.append((t, sph(phi, r, el), C, 40 + 22 * a, 36 + 26 * a, 0.0, 0.78 + 0.12 * a))
    # North of the house at roof height; along the roofs past the wind tower
    # (x 6-9, z -3.95..-0.95, 13 m tall: it passes on the left, ~2.3 m off).
    k.append((6.1, (3.9, 11.2, -11.5), (3.7, 6.5, 4.0), 66, 64, 0.0, 0.92))
    k.append((6.55, (3.7, 10.4, -4.2), (3.6, 4.8, 4.2), 68, 66, 0.0, 0.95))
    k.append((7.0, (3.6, 9.3, 1.5), (3.6, 0.0, 4.6), 70, 68, 0.0, 1.0))
    # The dive: straight down onto the star and through it.
    k.append((7.5, (SX, 7.2, 3.62), (SX, 0.0, SZ + 0.35), 72, 70, 0.0, 1.05))
    k.append((7.9, (SX, 5.75, SZ - 0.005), (SX, 0.0, SZ + 0.3), 74, 72, 0.0, 1.08))
    k.append((8.1, (SX, SY - 0.025, SZ), (SX, 0.0, SZ + 0.32), 76, 74, 0.0, 1.1))
    k.append((8.35, (SX, 4.55, SZ - 0.06), (SX, 0.2, SZ + 1.6), 78, 76, 0.0, 1.12))
    # Under the canopy: pull back north and down, turning to look south.
    k.append((9.1, (3.6, 2.6, 2.05), (3.6, 1.15, 10.5), 82, 84, 0.55, 1.17))
    k.append((10.0, END["pos"], END["tgt"], END_FOV["l"], END_FOV["p"], 1.0, END_EV))
    return k


def cr_nonuniform(P, T, t):
    """Barry-Goldman Catmull-Rom through 4 points with time knots T, at time t."""
    p0, p1, p2, p3 = P
    t0, t1, t2, t3 = T

    def lerp(a, b, ta, tb):
        if tb - ta < 1e-9:
            return a
        w = (t - ta) / (tb - ta)
        return tuple(x + (y - x) * w for x, y in zip(a, b))
    a1, a2, a3 = lerp(p0, p1, t0, t1), lerp(p1, p2, t1, t2), lerp(p2, p3, t2, t3)
    b1, b2 = lerp(a1, a2, t0, t2), lerp(a2, a3, t1, t3)
    return lerp(b1, b2, t1, t2)


def at(k, t, field):
    j = max(0, min(len(k) - 2, next((q for q in range(len(k) - 1) if k[q][0] <= t <= k[q + 1][0]), len(k) - 2)))
    i0, i3 = max(0, j - 1), min(len(k) - 1, j + 2)
    P = [k[i0][field], k[j][field], k[j + 1][field], k[i3][field]]
    T = [k[i0][0], k[j][0], k[j + 1][0], k[i3][0]]
    if i0 == j:   # end conditions: mirror the knot
        T[0] = T[1] - (T[2] - T[1]); P[0] = tuple(2 * a - b for a, b in zip(P[1], P[2]))
    if i3 == j + 1:
        T[3] = T[2] + (T[2] - T[1]); P[3] = tuple(2 * a - b for a, b in zip(P[2], P[1]))
    return cr_nonuniform(P, T, t)


def scalar(k, t, field):
    j = max(0, min(len(k) - 2, next((q for q in range(len(k) - 1) if k[q][0] <= t <= k[q + 1][0]), len(k) - 2)))
    a, b = k[j], k[j + 1]
    u = (t - a[0]) / (b[0] - a[0])
    u = u * u * (3 - 2 * u)
    return a[field] + (b[field] - a[field]) * u


def times():
    ts = []
    for (a, b, n) in SEGMENTS:
        ts += [a + (b - a) * i / n for i in range(n)]
    return ts + [DURATION]


def plan():
    k = keys()
    out = []
    for i, t in enumerate(times()):
        pos, tgt = at(k, t, 1), at(k, t, 2)
        if t <= SUNLINE_T + 1e-9:
            pos, tgt = on_sunline(t, sunline_dist(t)), C   # exactly on the sun's line
        m = minutes_at(t)
        alt, az = solar_position(2026, 6, 21, m)
        f = {"i": i, "t": round(t, 4), "pos": [round(v, 4) for v in pos], "tgt": [round(v, 4) for v in tgt],
             "fov_l": round(scalar(k, t, 3), 3), "fov_p": round(scalar(k, t, 4), 3),
             "shiftScale": round(scalar(k, t, 5), 4), "ev": round(scalar(k, t, 6), 3),
             "minutes": round(m, 4), "time": clock(m), "alt": round(alt, 3), "az": round(az, 3)}
        out.append(f)
    # The landing: the last frame is SEQ-A frame 0, to the digit.
    a0 = json.load(open(SEQ_A))
    for v, key in (("landscape", "l"), ("portrait", "p")):
        f0 = a0[v]["frames"][0]
        assert tuple(f0["cam"]["pos"]) == END["pos"] and tuple(f0["cam"]["tgt"]) == END["tgt"], "SEQ-A frame 0 moved"
        assert f0["cam"]["fov"] == END_FOV[key] and f0["cam"]["shift"] == END_SHIFT[key] and f0["ev"] == END_EV
    f0 = a0["portrait"]["frames"][0]
    out[-1].update({"pos": list(END["pos"]), "tgt": list(END["tgt"]), "fov_l": END_FOV["l"], "fov_p": END_FOV["p"],
                    "shiftScale": 1.0, "ev": END_EV, "minutes": f0["minutes"], "time": f0["time"], "alt": f0["alt"], "az": f0["az"]})
    check(out)
    return out


def check(frames):
    """The path must never pass through solid things: a few known volumes."""
    tower = ((6.0, 7.0, -3.95), (9.0, 13.2, -0.95))
    for f in frames:
        x, y, z = f["pos"]
        lo, hi = tower
        assert not (lo[0] - 0.3 < x < hi[0] + 0.3 and lo[1] < y < hi[1] and lo[2] - 0.3 < z < hi[2] + 0.3), ("tower", f["i"])
        if 5.15 < y < 5.3:   # in the canopy's thickness: must be inside the star
            assert abs(x - SX) < 0.05 and abs(z - SZ) < 0.05, ("canopy", f["i"], x, z)
        if 0 < x < 7.2 and 0 < z < 13.2:
            pass
        elif y < 7.6:
            raise AssertionError(("inside a wall or wing", f["i"], f["pos"]))


def to_spec(frames, variant):
    key = "l" if variant == "landscape" else "p"
    res = []
    for f in frames:
        res.append({"i": f["i"], "file": f"b{f['i']:03d}", "t": f["t"], "minutes": f["minutes"], "time": f["time"],
                    "alt": f["alt"], "az": f["az"],
                    "cam": {"pos": f["pos"], "tgt": f["tgt"], "fov": f["fov_" + key], "fovAxis": "h" if key == "l" else "v",
                            "shift": round(END_SHIFT[key] * f["shiftScale"], 4), "level": False},
                    "ev": f["ev"], "scene": "approach", "sppScale": 1.0})
    # the landing frame is a copy of a000 (rendered by the day film's own scene)
    res[-1]["cam"]["level"] = True
    res[-1]["copy_of"] = "a/a000"
    return res


def write_spec(frames):
    spec = json.load(open(SEQ_B))
    # depth from 2 cm (inside the canopy star) to the far town
    spec["portrait"] = {"size": [720, 1280], "depthRange": [0.02, 1500.0], "frames": to_spec(frames, "portrait")}
    spec["title"] = "The approach (phone); the street walk (desktop, interim)"
    json.dump(spec, open(SEQ_B, "w"), indent=1)
    print("wrote", SEQ_B, len(frames), "portrait frames")


def render(frames, W, H, spp, out, variant="p"):
    os.makedirs(out, exist_ok=True)
    for f in frames:
        png = os.path.join(out, f"{f['i']:03d}.png")
        if os.path.exists(png):
            continue
        raw = os.path.join(out, f"raw{f['i']:03d}")
        fov = f["fov_l"] if variant == "l" else f["fov_p"]
        cmd = [BIN, "--scene", "approach", "--cam", "court", "--time", f["time"], "--w", str(W), "--h", str(H),
               "--spp", str(spp), "--out", raw, "--campos", ",".join(map(str, f["pos"])), "--camtgt", ",".join(map(str, f["tgt"])),
               "--shift", str(END_SHIFT[variant] * f["shiftScale"]), "--level", "0",
               "--hfov" if variant == "l" else "--vfov", str(fov)]
        subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run([sys.executable, os.path.join(HERE, "post.py"), raw, png, "--ev", str(f["ev"])], check=True, stdout=subprocess.DEVNULL)
        subprocess.run(["rm", "-rf", raw])
        print(f["i"], f["time"], flush=True)


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "spec"
    frames = plan()
    if cmd == "spec":
        write_spec(frames)
    elif cmd == "preview":
        root = os.path.join(os.environ.get("ZAWAL_SCRATCH", "/tmp/zawal-seq"), "approach2")
        os.makedirs(root, exist_ok=True)
        json.dump(frames, open(os.path.join(root, "path.json"), "w"), indent=1)
        W, H = (int(v) for v in os.environ.get("PREVIEW_SIZE", "270x480").split("x"))
        render(frames[:-1], W, H, int(os.environ.get("PREVIEW_SPP", "8")), os.path.join(root, "p"))
        print("done", root)
    elif cmd == "path":
        for f in frames:
            print(f["i"], f["t"], f["time"], f["pos"], f["tgt"], f["fov_p"], f["ev"])


if __name__ == "__main__":
    main()
