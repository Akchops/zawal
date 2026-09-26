#!/usr/bin/env python3
"""Hi-res stills for the frames the scroll rests on (the first frame above all):
renders a sequence frame at a large size and exports the delivery tiers.

    render_stills.py b 0 portrait     -> site/public/img/hero/b000-p{720,1080,1440}.webp
    render_stills.py b 0 landscape    -> site/public/img/hero/b000-l{1600,2560}.webp

Resumable: skips the render when the PNG master exists.
"""
import json
import os
import subprocess
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "site", "public", "img", "hero")
SIZES = {"portrait": ((1440, 2560), [720, 1080, 1440]), "landscape": ((2560, 1440), [1600, 2560])}
Q = 84


def main():
    seq, idx, variant = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    spp = int(sys.argv[4]) if len(sys.argv) > 4 else 64
    scratch = os.environ.get("ZAWAL_SCRATCH", "/tmp/zawal-seq")
    f = json.load(open(os.path.join(ROOT, "renderer", "sequences", f"seq_{seq}.json")))[variant]["frames"][idx]
    (W, H), widths = SIZES[variant]
    raw = os.path.join(scratch, "stills", f"{f['file']}-{variant[0]}")
    master = raw + ".png"
    if not os.path.exists(master):
        c = f["cam"]
        cmd = [os.path.join(ROOT, "renderer", "bin", "zawal"), "--scene", "house", "--cam", "court", "--time", f["time"],
               "--w", str(W), "--h", str(H), "--spp", str(spp), "--out", raw,
               "--campos", ",".join(map(str, c["pos"])), "--camtgt", ",".join(map(str, c["tgt"])), "--shift", str(c["shift"])]
        cmd += ["--vfov", str(c["fov"])] if c["fovAxis"] == "v" else ["--hfov", str(c["fov"])]
        subprocess.run(cmd, check=True)
        subprocess.run([sys.executable, os.path.join(HERE, "post.py"), raw, master, "--ev", str(f["ev"])], check=True)
    os.makedirs(OUT, exist_ok=True)
    img = Image.open(master).convert("RGB")
    for w in widths:
        h = round(w * img.height / img.width)
        im = img if w == img.width else img.resize((w, h), Image.LANCZOS)
        p = os.path.join(OUT, f"{f['file']}-{variant[0]}{w}.webp")
        im.save(p, "WEBP", quality=Q, method=6)
        print(p, os.path.getsize(p) // 1024, "kB")


if __name__ == "__main__":
    main()
