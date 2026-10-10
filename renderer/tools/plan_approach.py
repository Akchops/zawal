#!/usr/bin/env python3
"""The approach: ten seconds of scroll from the sun's own line down onto the
day's first frame.

  0.0-1.8 s  on the sun's line. The camera stands where the sun is, looking
             along its rays at the courtyard house, framed tight on it (its east
             facade, gate, ghaf, canopy and wind tower fill the phone's width),
             and dollies in on a long lens, pushing in slowly. From there
             every shadow hides behind the thing that casts it, so the quarter
             reads flat and shadowless.
  1.8-4.8 s  it peels off the line and spirals down, east, south, west, north,
             the shadows opening as it leaves the sun.
  4.8-7.0 s  from the north it skims the roofs, slowly; the wind tower sweeps
             past on the left.
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
C = (4.0, 3.5, 6.4)                    # what the opening and the orbit look at: the house's centre
SX, SY, SZ = 3.6, 5.25, 3.96           # the star: a canopy hole clear of the teak beams
END = {"pos": (3.6, 1.55, 0.9), "tgt": (3.6, 1.55, 14.0)}   # SEQ-A frame 0
END_FOV = {"l": 84.0, "p": 88.0}
END_SHIFT = {"l": 0.22, "p": 0.16}
END_EV = 1.2
SUNLINE_T = 1.5                        # up to here the camera is exactly on the sun's line
SPIRAL_END = 4.8                       # the aerial part: the house keeps its size in the frame until here
# Half the frame's height, in metres at the house: a long lens on the sun's line
# makes the house fill the phone's width from the first frame, then it widens as
# the camera closes in, so the house keeps its size while the quarter's
# perspective changes around it.
K_OPEN, K_PUSH, K_WIDE, K_END = 20.0, 16.0, 28.0, 13.0
K_WIDE_T = 2.9

# Frames per segment (uniform in time inside a segment). Dense where the
# picture changes fastest: the tower pass and the dive.
SEGMENTS = [(0.0, 1.8, 18), (1.8, SPIRAL_END, 60), (SPIRAL_END, 7.0, 38), (7.0, 8.4, 34), (8.4, 10.0, 26)]


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
    # 330 m -> 160 m over the opening at a steady pace; the spiral takes over
    # that inward speed, so there is no stall where the camera leaves the line
    return 330.0 - 170.0 * min(1.0, t / 1.8)


def frame_k(t):
    """Half-height of the frame at the house (m) during the aerial part: tight
    on the house's own east facade while on the sun's line (the gate, the
    lantern, the ghaf, the wind tower above: unmistakably the film's house),
    opening out through the spiral so the orbit reads, tightening again as it
    comes down to the roofs."""
    def ss(a, b, x):
        u = min(1.0, max(0.0, (x - a) / (b - a)))
        return u * u * (3 - 2 * u)
    if t <= SUNLINE_T:
        return K_OPEN + (K_PUSH - K_OPEN) * t / SUNLINE_T      # a slow push in
    if t <= K_WIDE_T:
        return K_PUSH + (K_WIDE - K_PUSH) * ss(SUNLINE_T, K_WIDE_T, t)
    return K_WIDE + (K_END - K_WIDE) * ss(K_WIDE_T, SPIRAL_END, t)


def keys():
    """(t, pos, tgt, fov_l, fov_p, shiftScale, ev)"""
    k = []
    for t in (0.0, 0.5, 1.0, SUNLINE_T):
        k.append((t, on_sunline(t, sunline_dist(t)), C, 20.0, 12.0, 0.0, 0.75))
    # The spiral: off the sun's line, east -> south -> west -> north, closing
    # in and descending. Its turning eases out as it reaches due north, so its
    # last motion is inward, which there is south: straight into the run along
    # the roofs, with no swerve. Written as dense keys from smooth functions.
    # (fov here is a placeholder: plan() sets it from frame_k.)
    ps, es = sun_phi_el(1.8)
    r0, r1 = sunline_dist(1.8), 20.0
    T = SPIRAL_END - 1.8
    m0 = (170.0 / 1.8) * T / (r0 - r1)        # leaves the line at the dolly's speed
    m1 = 6.0 * T / (r0 - r1)                  # arrives at the roof run's speed
    n = 24
    for j in range(n + 1):
        u = j / n
        t = 1.8 + T * u
        sm = u * u * (3 - 2 * u)              # the turn eases in and out
        phi = ps + (270.0 - ps) * sm
        S = (3 * u * u - 2 * u ** 3) + m0 * (u ** 3 - 2 * u * u + u) + m1 * (u ** 3 - u * u)
        r = r0 + (r1 - r0) * S
        el = es + (22.0 - es) * sm
        pos = sph(phi, r, el)
        fov_p = 2 * math.degrees(math.atan(frame_k(t) / math.dist(pos, C)))
        fov_l = 2 * math.degrees(math.atan(1.25 * frame_k(t) * 16 / 9 / math.dist(pos, C)))
        k.append((t, pos, C, fov_l, fov_p, 0.0, 0.78 + 0.13 * u))
    # North of the house at roof height, then slowly along the roofs past the
    # wind tower (x 6-9, z -3.95..-0.95, 13 m tall): it passes on the left,
    # ~2.3 m off. The camera turns to it as it comes, holds it near the middle
    # of the frame, then pans back to the courtyard so the tower sweeps out
    # past the lens. 2.2 s: the move's best moment is not its shortest.
    k.append((5.25, (3.85, 10.9, -9.35), (7.2, 9.6, -1.5), 66, 65, 0.0, 0.93))
    k.append((5.7, (3.7, 10.8, -6.6), (7.4, 9.2, -0.2), 67, 65, 0.0, 0.94))
    k.append((6.15, (3.68, 10.5, -4.4), (6.6, 7.0, 2.2), 67, 65.5, 0.0, 0.95))
    k.append((6.55, (3.65, 10.2, -2.0), (4.6, 3.2, 4.6), 68, 66, 0.0, 0.97))
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


def times(k=None):
    """Frame times. Uniform inside each segment, except the spiral, whose frames
    are spaced by how much the picture turns (view-direction change over the
    field of view), so its fast middle is as finely sampled as its ends."""
    ts = []
    for (a, b, n) in SEGMENTS:
        if k is not None and a == 1.8:
            fine = [a + (b - a) * j / 2000 for j in range(2001)]
            cum, acc, prev = [0.0], 0.0, None
            for t in fine:
                p, g = at(k, t, 1), at(k, t, 2)
                v = [g[q] - p[q] for q in range(3)]
                nv = math.sqrt(sum(x * x for x in v))
                v = [x / nv for x in v]
                fov = 2 * math.degrees(math.atan(frame_k(t) / math.dist(p, C)))
                if prev is not None:
                    dot = max(-1.0, min(1.0, sum(x * y for x, y in zip(v, prev))))
                    acc += math.degrees(math.acos(dot)) / fov + 0.002   # + a floor, so slow parts still move on
                    cum.append(acc)
                prev = v
            for i in range(n):
                goal = acc * i / n
                j = next(q for q in range(len(cum)) if cum[q] >= goal - 1e-12)
                ts.append(fine[j])
        else:
            ts += [a + (b - a) * i / n for i in range(n)]
    return ts + [DURATION]


def plan():
    k = keys()
    out = []
    for i, t in enumerate(times(k)):
        pos, tgt = at(k, t, 1), at(k, t, 2)
        if t <= SUNLINE_T + 1e-9:
            pos, tgt = on_sunline(t, sunline_dist(t)), C   # exactly on the sun's line
        fov_l, fov_p = scalar(k, t, 3), scalar(k, t, 4)
        if t <= SPIRAL_END + 1e-9:
            # the house keeps its size: half-height frame_k(t) at the distance of C
            d = math.dist(pos, C)
            fov_p = 2 * math.degrees(math.atan(frame_k(t) / d))
            fov_l = 2 * math.degrees(math.atan(1.25 * frame_k(t) * 16 / 9 / d))
        m = minutes_at(t)
        alt, az = solar_position(2026, 6, 21, m)
        f = {"i": i, "t": round(t, 4), "pos": [round(v, 4) for v in pos], "tgt": [round(v, 4) for v in tgt],
             "fov_l": round(fov_l, 3), "fov_p": round(fov_p, 3),
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
