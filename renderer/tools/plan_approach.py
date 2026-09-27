#!/usr/bin/env python3
"""The approach (proposal): the camera spirals down around the courtyard house
from high above the quarter, skims the roofs past the wind tower, drops into
the courtyard through its open north end and lands on the day's first frame
(SEQ-A frame 0: 08:00, the courtyard camera), so the film can hand straight
over to the day.

    plan_approach.py preview [frames=150]   rough, low resolution, both orientations
                                            -> <scratch>/approach/{l,p}/NNN.png,
                                               approach.webp / approach.gif, sheet

Frames are planned as data (pos, target, fov, shift, ev, minute) exactly like
plan_sequences.py, so a full render can use the same path later.
"""
import json
import math
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(__file__))
from solar import clock  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
BIN = os.environ.get("ZAWAL_BIN", os.path.join(ROOT, "bin", "zawal"))

C = (4.0, 1.0, 6.3)                       # what the orbit looks at: the courtyard, low
END = {"pos": (3.6, 1.55, 0.9), "tgt": (3.6, 1.55, 14.0)}   # SEQ-A frame 0
END_FOV = {"l": 84.0, "p": 88.0}          # hfov landscape / vfov portrait, as SEQ-A
END_SHIFT = {"l": 0.22, "p": 0.16}


def orbit_point(phi_deg, r, y):
    a = math.radians(phi_deg)             # 0 = east, 90 = south (+z), 180 = west, 270 = north
    return (C[0] + r * math.cos(a), y, C[2] + r * math.sin(a))


def keyframes():
    """(u, pos, tgt, fov_l, fov_p, shift_scale, ev): u runs 0..1 along the move."""
    k = []
    # the spiral: 315 degrees, from 125 m out and 105 m up to 21 m out, 16 m up
    orbit = [(-45, 125, 105), (0, 108, 90), (45, 88, 72), (90, 70, 57), (135, 54, 43),
             (180, 41, 32), (225, 30, 23), (270, 21, 16)]
    for j, (phi, r, y) in enumerate(orbit):
        u = 0.64 * j / (len(orbit) - 1)
        k.append((u, orbit_point(phi, r, y), C, 46 + 12 * j / 7, 50 + 12 * j / 7, 0.0, 0.8))
    # over the north wing (the wind tower passes on the right), then down into the courtyard
    k.append((0.73, (3.7, 12.5, -3.5), (3.6, 0.0, 4.5), 64, 66, 0.0, 0.85))
    k.append((0.82, (3.6, 9.2, 1.1), (3.6, 0.0, 5.6), 70, 72, 0.0, 0.95))
    k.append((0.92, (3.6, 4.4, 1.0), (3.6, 1.1, 12.0), 78, 82, 0.5, 1.1))
    k.append((1.0, END["pos"], END["tgt"], END_FOV["l"], END_FOV["p"], 1.0, 1.2))
    return k


def catmull(p0, p1, p2, p3, t):
    t2, t3 = t * t, t * t * t
    return tuple(0.5 * ((2 * b) + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3)
                 for a, b, c, d in zip(p0, p1, p2, p3))


def smooth(x):
    return x * x * (3 - 2 * x)


def plan(n):
    ks = keyframes()
    frames = []
    for i in range(n):
        s = i / (n - 1)
        u = 0.5 - 0.5 * math.cos(math.pi * s)          # ease in and out over the whole move
        j = max(0, min(len(ks) - 2, next((q for q in range(len(ks) - 1) if ks[q][0] <= u <= ks[q + 1][0]), len(ks) - 2)))
        a, b = ks[j], ks[j + 1]
        t = (u - a[0]) / (b[0] - a[0]) if b[0] > a[0] else 0.0
        P = [ks[max(0, j - 1)], a, b, ks[min(len(ks) - 1, j + 2)]]
        pos = catmull(*[q[1] for q in P], t)
        tgt = catmull(*[q[2] for q in P], t)
        ts = smooth(t)
        lerp = lambda x, y: x + (y - x) * ts                                    # noqa: E731
        minutes = 450.0 + 30.0 * s                      # 07:30 -> 08:00, as the walk did
        frames.append({"i": i, "u": round(u, 5), "pos": [round(v, 4) for v in pos], "tgt": [round(v, 4) for v in tgt],
                       "fov_l": round(lerp(a[3], b[3]), 3), "fov_p": round(lerp(a[4], b[4]), 3),
                       "shiftScale": round(lerp(a[5], b[5]), 4), "ev": round(lerp(a[6], b[6]), 3),
                       "minutes": minutes, "time": clock(minutes)})
    frames[-1].update({"pos": list(END["pos"]), "tgt": list(END["tgt"]), "fov_l": END_FOV["l"], "fov_p": END_FOV["p"], "shiftScale": 1.0, "ev": 1.2})
    return frames


def render(frames, variant, W, H, spp, out):
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
        print(variant, f["i"], f["time"], flush=True)


def encode(frames, root):
    from PIL import Image, ImageDraw
    L = [Image.open(os.path.join(root, "l", f"{f['i']:03d}.png")).convert("RGB") for f in frames]
    P = [Image.open(os.path.join(root, "p", f"{f['i']:03d}.png")).convert("RGB") for f in frames]
    out = []
    for f, l, p in zip(frames, L, P):
        W = l.width + 12 + p.width
        H = max(p.height, l.height) + 26
        im = Image.new("RGB", (W, H), (15, 20, 24))
        im.paste(l, (0, 26 + (p.height - l.height) // 2)); im.paste(p, (l.width + 12, 26))
        g = ImageDraw.Draw(im)
        g.text((6, 7), f"ROUGH PREVIEW - frame {f['i'] + 1}/{len(frames)} - 21 June {f['time'][:5]} - desktop (left), phone (right)", fill=(200, 196, 188))
        out.append(im)
    out[0].save(os.path.join(root, "approach.webp"), save_all=True, append_images=out[1:], duration=1000 // 24, loop=0, quality=72, method=4)
    small = [im.resize((im.width * 3 // 4, im.height * 3 // 4)).quantize(colors=128, method=Image.Quantize.MEDIANCUT) for im in out[::2]]
    small[0].save(os.path.join(root, "approach.gif"), save_all=True, append_images=small[1:], duration=1000 // 12, loop=0, optimize=True)
    picks = [0, 20, 40, 60, 80, 96, 108, 120, 132, len(frames) - 1]
    th = [out[i].resize((out[i].width // 2, out[i].height // 2)) for i in picks]
    sheet = Image.new("RGB", (5 * th[0].width + 24, 2 * th[0].height + 6), (240, 236, 228))
    for k, t in enumerate(th):
        sheet.paste(t, ((k % 5) * (t.width + 6), (k // 5) * (t.height + 6)))
    sheet.save(os.path.join(root, "approach_sheet.png"))


def main():
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 150
    root = os.path.join(os.environ.get("ZAWAL_SCRATCH", "/tmp/zawal-seq"), "approach")
    os.makedirs(root, exist_ok=True)
    frames = plan(n)
    json.dump(frames, open(os.path.join(root, "path.json"), "w"), indent=1)
    if sys.argv[1] == "preview":
        render(frames, "l", 480, 270, 6, os.path.join(root, "l"))
        render(frames, "p", 270, 480, 6, os.path.join(root, "p"))
        encode(frames, root)
    print("done", root)


if __name__ == "__main__":
    main()
