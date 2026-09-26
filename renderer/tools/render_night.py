#!/usr/bin/env python3
"""The night courtyard (home scene 14): the day's last camera at 20:30, the
sun 16 degrees down, lit only by the night sky and its lanterns, which switch
on one group at a time. Each state is rendered whole (not summed), so every
image is a true path-traced frame; the site crossfades between neighbours in
linear light, which is exactly one lantern brightening.

    render_night.py portrait|landscape [spp]
 -> site/public/night/night-{p|l}-{0,1,2,3}.webp
    0 = lamps off, 1 = + the hanging lantern, 2 = + the east wall lantern,
    3 = + the west wall lantern
Resumable: a state whose PNG master exists is not re-rendered.
"""
import json
import os
import subprocess
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
BIN = os.path.join(ROOT, "renderer", "bin", "zawal")
OUT = os.path.join(ROOT, "site", "public", "night")
STATES = ["0", "1", "1,2", "1,2,3"]      # group 0 does not exist: lamps built, all off
GLOW = "4e-6,3.5e-6,3e-6"
EV = float(os.environ.get("ZAWAL_NIGHT_EV", "10.2"))


def main():
    variant = sys.argv[1]
    spp = int(sys.argv[2]) if len(sys.argv) > 2 else 64
    scratch = os.path.join(os.environ.get("ZAWAL_SCRATCH", "/tmp/zawal-seq"), "night")
    os.makedirs(scratch, exist_ok=True)
    os.makedirs(OUT, exist_ok=True)
    seqa = json.load(open(os.path.join(ROOT, "renderer", "sequences", "seq_a.json")))
    cam = seqa[variant]["frames"][-1]["cam"]
    W, H = (720, 1280) if variant == "portrait" else (1600, 900)
    v = variant[0]
    for k, lights in enumerate(STATES):
        raw = os.path.join(scratch, f"night-{v}-{k}")
        master = raw + ".png"
        if not os.path.exists(master):
            cmd = [BIN, "--scene", "house", "--cam", "court", "--time", "20:30", "--w", str(W), "--h", str(H), "--spp", str(spp),
                   "--lights", lights, "--nightglow", GLOW, "--out", raw,
                   "--campos", ",".join(map(str, cam["pos"])), "--camtgt", ",".join(map(str, cam["tgt"])), "--shift", str(cam["shift"])]
            cmd += ["--vfov", str(cam["fov"])] if cam["fovAxis"] == "v" else ["--hfov", str(cam["fov"])]
            subprocess.run(cmd, check=True, stderr=subprocess.DEVNULL)
            subprocess.run([sys.executable, os.path.join(HERE, "post.py"), raw, master, "--ev", str(EV), "--glare", "1.0"], check=True)
        dst = os.path.join(OUT, f"night-{v}-{k}.webp")
        Image.open(master).convert("RGB").save(dst, "WEBP", quality=84, method=6)
        print(os.path.relpath(dst, ROOT), os.path.getsize(dst) // 1024, "kB", flush=True)
    print("night done", variant)


if __name__ == "__main__":
    main()
