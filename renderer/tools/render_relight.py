#!/usr/bin/env python3
"""Buffers for signature C, "You are the sun": a relightable courtyard.

For each aspect (portrait / landscape), at the camera of the day's last frame:
  gbuffer: depth (16-bit in two bytes), normal, albedo  -> lossless / q90 WebP
  basis:   12 renders of everything EXCEPT direct sun at the first surface
           (--indirect), one per sun direction, linear, each normalised by its
           own scale and gamma-encoded -> WebP q86
  sun.json: the directions, scales, camera, the sun-transmittance table and the
           exposure curve, so the shader can add direct sun with analytic
           shadows and tone-map exactly like tools/post.py.
Basis #0 is the sun of the day's last frame, so the hand-over is pixel-exact.

    render_relight.py portrait [spp]
Resumable: a finished direction (its .webp) is skipped.
"""
import json
import os
import subprocess
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
BIN = os.path.join(ROOT, "renderer", "bin", "zawal")
OUT = os.path.join(ROOT, "site", "public", "sun")
DEPTH_MAX = 40.0

# Sun directions (alt, az) the pointer can reach, left (east) to right (west).
BASIS = [(8, 70), (8, 115), (8, 245), (35, 80), (35, 140), (35, 220), (35, 280), (62, 100), (62, 180), (62, 260), (88, 180)]


def run(cmd, log):
    subprocess.run(cmd, check=True, stdout=log, stderr=log)


def main():
    variant = sys.argv[1]
    spp = int(sys.argv[2]) if len(sys.argv) > 2 else 24
    scratch = os.path.join(os.environ.get("ZAWAL_SCRATCH", "/tmp/zawal-seq"), "relight", variant)
    os.makedirs(scratch, exist_ok=True)
    os.makedirs(OUT, exist_ok=True)
    log = open(os.path.join(scratch, "log.txt"), "a")
    seqa = json.load(open(os.path.join(ROOT, "renderer", "sequences", "seq_a.json")))
    last = seqa[variant]["frames"][-1]
    cam = last["cam"]
    W, H = (720, 1280) if variant == "portrait" else (1600, 900)
    v = variant[0]
    camargs = ["--campos", ",".join(map(str, cam["pos"])), "--camtgt", ",".join(map(str, cam["tgt"])), "--shift", str(cam["shift"])]
    camargs += ["--vfov", str(cam["fov"])] if cam["fovAxis"] == "v" else ["--hfov", str(cam["fov"])]
    base = [BIN, "--scene", "house", "--cam", "court", "--w", str(W), "--h", str(H)] + camargs

    dirs = [(last["alt"], last["az"])] + BASIS
    meta = {"variant": variant, "size": [W, H], "cam": cam, "handover": {"time": last["time"], "alt": last["alt"], "az": last["az"], "ev": last["ev"]},
            "depthMax": DEPTH_MAX, "basis": []}

    # --- G-buffer (from the first basis render; AOVs do not depend on the sun) ---
    for k, (alt, az) in enumerate(dirs):
        name = f"basis{k:02d}-{v}"
        raw = os.path.join(scratch, name)
        dst = os.path.join(OUT, name + ".webp")
        lin = raw + ".npy"
        if not os.path.exists(dst):
            if not os.path.exists(lin):
                if not os.path.exists(os.path.join(raw, "color.f32")):
                    run(base + ["--time", "12:00", "--sunalt", str(alt), "--sunaz", str(az), "--indirect", "1", "--spp", str(spp), "--out", raw], log)
                run([sys.executable, os.path.join(HERE, "post.py"), raw, raw + ".png", "--save-linear", lin], log)
            L = np.load(lin).astype(np.float32)
            lum = L @ np.array([0.2126, 0.7152, 0.0722], np.float32)
            scale = float(np.percentile(lum, 99.7)) * 1.15 + 1e-9
            enc = np.clip(L / scale, 0, 1) ** (1 / 2.2)
            Image.fromarray((enc * 255 + 0.5).astype(np.uint8)).save(dst, "WEBP", quality=86, method=6)
            json.dump({"scale": scale}, open(raw + ".json", "w"))
        scale = json.load(open(raw + ".json"))["scale"]
        meta["basis"].append({"file": f"/sun/{name}.webp", "alt": alt, "az": az, "scale": scale})
        if k == 0 and os.path.exists(os.path.join(raw, "depth.f32")):
            d = np.fromfile(os.path.join(raw, "depth.f32"), np.float32).reshape(H, W)
            q = np.clip(np.nan_to_num(d, nan=DEPTH_MAX, posinf=DEPTH_MAX) / DEPTH_MAX, 0, 1)
            q16 = np.round(q * 65535).astype(np.uint32)
            rgb = np.stack([(q16 >> 8).astype(np.uint8), (q16 & 255).astype(np.uint8), np.zeros_like(q16, np.uint8)], -1)
            Image.fromarray(rgb).save(os.path.join(OUT, f"gb-depth-{v}.webp"), "WEBP", lossless=True, quality=100, method=6)
            n = np.fromfile(os.path.join(raw, "normal.f32"), np.float32).reshape(H, W, 3)
            nn = n / np.maximum(np.linalg.norm(n, axis=-1, keepdims=True), 1e-6)
            Image.fromarray(np.round((nn * 0.5 + 0.5) * 255).astype(np.uint8)).save(os.path.join(OUT, f"gb-normal-{v}.webp"), "WEBP", lossless=True, quality=100, method=6)
            al = np.fromfile(os.path.join(raw, "albedo.f32"), np.float32).reshape(H, W, 3)
            srgb = np.where(al <= 0.0031308, al * 12.92, 1.055 * np.power(np.clip(al, 0, 1), 1 / 2.4) - 0.055)
            Image.fromarray(np.round(np.clip(srgb, 0, 1) * 255).astype(np.uint8)).save(os.path.join(OUT, f"gb-albedo-{v}.webp"), "WEBP", quality=90, method=6)
        print(name, alt, az, f"scale {scale:.4g}", flush=True)

    table = json.loads(subprocess.run([BIN, "--scene", "house", "--suntable", "1"], check=True, capture_output=True, text=True).stdout)
    meta["sunTrans"] = [row[1:] for row in table]
    meta["gbuffer"] = {"depth": f"/sun/gb-depth-{v}.webp", "normal": f"/sun/gb-normal-{v}.webp", "albedo": f"/sun/gb-albedo-{v}.webp"}
    json.dump(meta, open(os.path.join(OUT, f"sun-{v}.json"), "w"), indent=1)
    print("relight buffers done", variant)


if __name__ == "__main__":
    main()
