#!/usr/bin/env python3
"""Rough preview of a variant of the approach's last part: instead of dropping
in through the courtyard's open north end, the camera dives through ONE star
of the canopy lattice (holes about 23 cm across; the camera is a point, so it
fits), then pulls back north and down onto the day's first frame.

    preview_stardive.py [frames=56]  -> <scratch>/stardive/{l,p}/NNN.png, stardive.webp/.gif
"""
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import plan_approach as pa  # noqa: E402

# Star centres of the canopy (courtyardStar, period 0.36, offset 0.18) sit at
# x, z = multiples of 0.36 m; (3.6, 3.96) is clear of the teak beams below it.
SX, SZ = 3.6, 3.96
KEYS = [  # (u, pos, tgt, fov_l, fov_p, shiftScale, ev)
    (0.00, (3.7, 14.0, -8.0), (3.6, 0.0, 4.0), 60, 62, 0.0, 0.85),
    (0.22, (3.6, 10.0, 3.0), (3.6, 0.0, 4.2), 62, 64, 0.0, 0.9),
    (0.42, (SX, 6.4, SZ), (SX, 0.0, SZ + 0.02), 64, 66, 0.0, 0.95),
    (0.52, (SX, 5.55, SZ), (SX, 0.0, SZ + 0.02), 70, 72, 0.0, 1.0),
    (0.60, (SX, 5.1, SZ), (SX, 0.0, SZ + 0.03), 74, 76, 0.0, 1.05),
    (0.74, (3.6, 3.9, 3.0), (3.6, 0.6, 9.5), 78, 82, 0.4, 1.12),
    (1.00, pa.END["pos"], pa.END["tgt"], pa.END_FOV["l"], pa.END_FOV["p"], 1.0, 1.2),
]


def plan(n):
    frames = []
    for i in range(n):
        s = i / (n - 1)
        u = 0.5 - 0.5 * math.cos(math.pi * s)
        j = next((q for q in range(len(KEYS) - 1) if KEYS[q][0] <= u <= KEYS[q + 1][0]), len(KEYS) - 2)
        a, b = KEYS[j], KEYS[j + 1]
        t = (u - a[0]) / (b[0] - a[0])
        P = [KEYS[max(0, j - 1)], a, b, KEYS[min(len(KEYS) - 1, j + 2)]]
        pos = pa.catmull(*[q[1] for q in P], t)
        tgt = pa.catmull(*[q[2] for q in P], t)
        ts = pa.smooth(t)
        lerp = lambda x, y: x + (y - x) * ts                                    # noqa: E731
        minutes = 470.0 + 10.0 * s
        frames.append({"i": i, "pos": [round(v, 4) for v in pos], "tgt": [round(v, 4) for v in tgt],
                       "fov_l": lerp(a[3], b[3]), "fov_p": lerp(a[4], b[4]), "shiftScale": lerp(a[5], b[5]),
                       "ev": lerp(a[6], b[6]), "minutes": minutes, "time": pa.clock(minutes)})
    frames[-1].update({"pos": list(pa.END["pos"]), "tgt": list(pa.END["tgt"]), "fov_l": pa.END_FOV["l"], "fov_p": pa.END_FOV["p"],
                       "shiftScale": 1.0, "ev": 1.2})
    return frames


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 56
    root = os.path.join(os.environ.get("ZAWAL_SCRATCH", "/tmp/zawal-seq"), "stardive")
    os.makedirs(root, exist_ok=True)
    frames = plan(n)
    json.dump(frames, open(os.path.join(root, "path.json"), "w"), indent=1)
    pa.render(frames, "l", 480, 270, 8, os.path.join(root, "l"))
    pa.render(frames, "p", 270, 480, 8, os.path.join(root, "p"))
    from PIL import Image, ImageDraw
    out = []
    for f in frames:
        l = Image.open(os.path.join(root, "l", f"{f['i']:03d}.png")).convert("RGB")
        p = Image.open(os.path.join(root, "p", f"{f['i']:03d}.png")).convert("RGB")
        im = Image.new("RGB", (l.width + 12 + p.width, p.height + 26), (15, 20, 24))
        im.paste(l, (0, 26 + (p.height - l.height) // 2)); im.paste(p, (l.width + 12, 26))
        ImageDraw.Draw(im).text((6, 7), f"ROUGH PREVIEW - through a star of the canopy - frame {f['i'] + 1}/{n}", fill=(200, 196, 188))
        out.append(im)
    out[0].save(os.path.join(root, "stardive.webp"), save_all=True, append_images=out[1:], duration=1000 // 16, loop=0, quality=72, method=4)
    small = [im.resize((im.width * 3 // 4, im.height * 3 // 4)).quantize(colors=128, method=Image.Quantize.MEDIANCUT) for im in out]
    small[0].save(os.path.join(root, "stardive.gif"), save_all=True, append_images=small[1:], duration=1000 // 14, loop=0, optimize=True)
    print("done", root)


if __name__ == "__main__":
    main()
