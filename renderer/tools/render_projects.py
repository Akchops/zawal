#!/usr/bin/env python3
"""Project imagery: the four heroes of signature D and two details each for the
project pages. Every image is rendered at the project's signature hour.

    render_projects.py heroes [spp]    -> site/public/img/projects/<slug>-l{1920,1280}.webp
                                          site/public/img/projects/<slug>-p{1080,720}.webp
                                          site/public/img/projects/<slug>-{l,p}-sun.webp
    render_projects.py details [spp]   -> site/public/img/projects/<slug>-d{1,2}-{1000,640}.webp
    render_projects.py heroes 40 ghaf-house    (one project)

The -sun.webp files feed the morph shader: R = signed distance to the edge of
the sunlit area (128 = edge, lower = in sun, 4 px per step at mask size),
G = sun visibility (the renderer's sunvis AOV), both at 1/4 size, lossless.
Resumable: a render whose PNG master exists is not redone.
"""
import json
import os
import subprocess
import sys

import numpy as np
from PIL import Image
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
BIN = os.path.join(ROOT, "renderer", "bin", "zawal")
OUT = os.path.join(ROOT, "site", "public", "img", "projects")
Q = 84

# Cameras: pos, tgt, fov (h for landscape, v for portrait and details), shift, level.
PROJECTS = {
    "ghaf-house": dict(scene="ghaf", time="16:10", ev=0.4, extra=[],
        l=dict(pos=(-1.9, 1.6, 7.0), tgt=(14.0, 1.6, 8.2), fov=82, shift=0.32),
        p=dict(pos=(-2.6, 1.6, 6.8), tgt=(14.0, 1.6, 7.6), fov=74, shift=0.22),
        details=[dict(pos=(8.4, 1.6, 7.8), tgt=(14.0, 1.6, 6.2), fov=58, shift=0.42),        # the shade on the limewash
                 dict(pos=(-2.9, 1.45, 12.9), tgt=(-1.2, 1.45, 1.0), fov=70, shift=0.12, ev=0.9)]),  # the west loggia
    "hotel-sikka": dict(scene="sikka", time="11:10", ev=1.5, extra=[],
        l=dict(pos=(1.5, 1.6, 7.5), tgt=(-2.6, 1.6, -6.0), fov=82, shift=0.72),
        p=dict(pos=(1.35, 1.6, 8.0), tgt=(-1.2, 1.6, -8.0), fov=80, shift=0.3),
        details=[dict(pos=(1.2, 1.6, 4.0), tgt=(0.2, 9.0, 1.2), fov=64, shift=0.0, level=False),  # the slats from below
                 dict(pos=(0.9, 1.6, -3.2), tgt=(0.9, 10.5, -15.0), fov=66, shift=0.0, level=False, ev=0.9)]),  # the wind tower
    "qudra-canopy": dict(scene="qudra", time="13:40", ev=0.5, extra=["--lat", "24.84", "--lon", "55.37"],
        l=dict(pos=(-3.0, 1.5, 17.2), tgt=(14.0, 1.5, -4.0), fov=80, shift=0.24),
        p=dict(pos=(-13.5, 1.55, 12.5), tgt=(6.0, 1.55, -6.0), fov=92, shift=0.3),
        details=[dict(pos=(3.0, 1.6, 6.0), tgt=(7.0, -0.2, 2.5), fov=60, shift=0.0, level=False),   # triangles on the sand
                 dict(pos=(2.0, 1.5, 4.0), tgt=(3.6, 9.0, 1.0), fov=66, shift=0.0, level=False)]),   # the lattice from below
    "mushrif-reading-rooms": dict(scene="mushrif", time="18:05", ev=2.2, extra=[],
        l=dict(pos=(6.3, 1.45, 0.75), tgt=(3.4, 1.15, 16.0), fov=84, shift=0.14),
        p=dict(pos=(6.9, 1.45, 0.75), tgt=(4.8, 1.25, 16.0), fov=76, shift=0.05),
        details=[dict(pos=(5.4, 1.5, 4.6), tgt=(9.0, 1.5, 9.8), fov=62, shift=0.22),                    # stars above the shelves
                 dict(pos=(0.9, 1.55, 9.8), tgt=(-0.2, 2.3, 5.4), fov=60, shift=0.1, ev=2.8)]),      # a screen, edge on
}
SIZES = {"l": (1920, 1080), "p": (1080, 1920), "d": (1000, 1250)}
WIDTHS = {"l": [1920, 1280, 960], "p": [1080, 720, 540], "d": [1000, 640]}
FIRST = {960, 540}        # first-paint tiers (inside the first-load budget), lighter


def render(slug, spec, cam, kind, spp, raw):
    W, H = SIZES[kind]
    master = raw + ".png"
    if os.path.exists(master):
        return master
    cmd = [BIN, "--scene", spec["scene"], "--cam", spec["scene"], "--time", spec["time"], "--w", str(W), "--h", str(H),
           "--spp", str(spp), "--out", raw, "--campos", ",".join(map(str, cam["pos"])), "--camtgt", ",".join(map(str, cam["tgt"])),
           "--shift", str(cam["shift"]), "--level", "1" if cam.get("level", True) else "0"]
    cmd += ["--hfov", str(cam["fov"])] if kind == "l" else ["--vfov", str(cam["fov"])]
    cmd += spec["extra"]
    subprocess.run(cmd, check=True, stderr=subprocess.DEVNULL)
    subprocess.run([sys.executable, os.path.join(HERE, "post.py"), raw, master, "--ev", str(cam.get("ev", spec["ev"]))], check=True)
    return master


def export(master, stem, kind):
    img = Image.open(master).convert("RGB")
    for w in WIDTHS[kind]:
        h = round(w * img.height / img.width)
        im = img if w == img.width else img.resize((w, h), Image.LANCZOS)
        p = os.path.join(OUT, f"{stem}{w}.webp")
        im.save(p, "WEBP", quality=70 if (kind in "lp" and w in FIRST) else Q, method=6)
        print(" ", os.path.relpath(p, ROOT), os.path.getsize(p) // 1024, "kB", flush=True)


def sun_mask(raw, W, H, dst):
    sv = np.fromfile(os.path.join(raw, "sunvis.f32"), np.float32).reshape(H, W)
    m = sv.reshape(H // 4, 4, W // 4, 4).mean(axis=(1, 3))
    lit = m > 0.5
    d = ndimage.distance_transform_edt(~lit) - ndimage.distance_transform_edt(lit)
    r = np.clip(128 + d * 4, 0, 255).astype(np.uint8)
    g = np.clip(m * 255 + 0.5, 0, 255).astype(np.uint8)
    Image.fromarray(np.stack([r, g, np.zeros_like(r)], -1)).save(dst, "WEBP", lossless=True, quality=100, method=6)
    print(" ", os.path.relpath(dst, ROOT), os.path.getsize(dst) // 1024, "kB", f"lit {lit.mean():.2f}", flush=True)


def main():
    what = sys.argv[1]
    spp = int(sys.argv[2]) if len(sys.argv) > 2 else (40 if what == "heroes" else 32)
    only = sys.argv[3] if len(sys.argv) > 3 else None
    scratch = os.path.join(os.environ.get("ZAWAL_SCRATCH", "/tmp/zawal-seq"), "projects")
    os.makedirs(scratch, exist_ok=True)
    os.makedirs(OUT, exist_ok=True)
    for slug, spec in PROJECTS.items():
        if only and slug != only:
            continue
        if what == "heroes":
            for kind in ("l", "p"):
                raw = os.path.join(scratch, f"{slug}-{kind}")
                master = render(slug, spec, spec[kind], kind, spp, raw)
                export(master, f"{slug}-{kind}", kind)
                W, H = SIZES[kind]
                sun_mask(raw, W, H, os.path.join(OUT, f"{slug}-{kind}-sun.webp"))
        else:
            for i, cam in enumerate(spec["details"], 1):
                raw = os.path.join(scratch, f"{slug}-d{i}")
                master = render(slug, spec, cam, "d", spp, raw)
                export(master, f"{slug}-d{i}-", "d")
    print("done", what)


if __name__ == "__main__":
    main()
