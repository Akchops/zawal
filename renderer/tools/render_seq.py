#!/usr/bin/env python3
"""Resumable renderer for the pre-rendered sequences (sequences/seq_*.json).

    render_seq.py --seq b --variant portrait --frames even --commit

For each selected frame that is not already exported:
  render (bin/zawal) -> denoise + grade (tools/post.py, PNG master in scratch)
  -> WebP tiers in site/public/seq/<seq>/<l|p>/<tier>/<file>.webp
  -> drop the raw float buffers (they are ~55 MB a frame).
Frames are processed in chunks; with --commit each finished chunk is
committed and pushed, so an interrupted session loses at most one chunk.
A frame counts as done when every tier file exists, so reruns skip it.
Held frames (the zawal hold) are copied, never rendered.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
RENDERER = os.path.join(ROOT, "renderer")
SITE_SEQ = os.path.join(ROOT, "site", "public", "seq")

# Delivery tiers per variant: (tier name, width, height, WebP quality).
TIERS = {
    "landscape": [("1600", 1600, 900, 72), ("1280", 1280, 720, 72), ("ladder", 320, 180, 50)],
    "portrait": [("720", 720, 1280, 72), ("ladder", 144, 256, 50)],
}
SHORT = {"landscape": "l", "portrait": "p"}


def tier_path(seq, variant, tier, file):
    return os.path.join(SITE_SEQ, seq, SHORT[variant], tier, file + ".webp")


def done(seq, variant, file):
    return all(os.path.exists(tier_path(seq, variant, t[0], file)) for t in TIERS[variant])


def render_frame(seq, variant, f, spp, scratch, log):
    from PIL import Image
    cam = f["cam"]
    w, h = (1600, 900) if variant == "landscape" else (720, 1280)
    raw = os.path.join(scratch, f"{seq}{SHORT[variant]}", f["file"])
    spp = int(round(spp * f.get("sppScale", 1.0)))
    cmd = [os.path.join(RENDERER, "bin", "zawal"), "--scene", "house", "--cam", "court", "--time", f["time"],
           "--w", str(w), "--h", str(h), "--spp", str(spp), "--out", raw,
           "--campos", ",".join(str(v) for v in cam["pos"]), "--camtgt", ",".join(str(v) for v in cam["tgt"]),
           "--shift", str(cam["shift"])]
    cmd += ["--vfov", str(cam["fov"])] if cam["fovAxis"] == "v" else ["--hfov", str(cam["fov"])]
    t0 = time.time()
    if not os.path.exists(os.path.join(raw, "color.f32")):
        subprocess.run(cmd, check=True, stdout=log, stderr=log)
    master = raw + ".png"
    subprocess.run([sys.executable, os.path.join(HERE, "post.py"), raw, master, "--ev", str(f["ev"])],
                   check=True, stdout=log, stderr=log)
    img = Image.open(master).convert("RGB")
    for name, tw, th, q in TIERS[variant]:
        p = tier_path(seq, variant, name, f["file"])
        os.makedirs(os.path.dirname(p), exist_ok=True)
        im = img if (tw, th) == img.size else img.resize((tw, th), Image.LANCZOS)
        im.save(p + ".tmp", "WEBP", quality=q, method=6)
        os.replace(p + ".tmp", p)          # a half-written tier never counts as done
    # Keep the PNG master and meta; drop the float buffers.
    shutil.copy(os.path.join(raw, "meta.json"), raw + ".json")
    shutil.rmtree(raw, ignore_errors=True)
    return time.time() - t0


def git(*args):
    return subprocess.run(["git", "-C", ROOT] + list(args), check=False, capture_output=True, text=True)


def commit_and_push(msg, paths):
    git("add", *paths)
    if not git("diff", "--cached", "--quiet").returncode:
        return
    r = git("commit", "-m", msg + "\n\nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>\n"
            "Claude-Session: https://claude.ai/code/session_01RUXPNPi5mF87s2muAzjWwM")
    if r.returncode:
        print(r.stdout, r.stderr)
        return
    branch = git("rev-parse", "--abbrev-ref", "HEAD").stdout.strip()
    for wait in (0, 2, 4, 8, 16):
        time.sleep(wait)
        if git("push", "-u", "origin", branch).returncode == 0:
            return
    print("push failed after retries; commits are local and will go with the next push")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seq", choices=["a", "b"], required=True)
    ap.add_argument("--variant", choices=["landscape", "portrait"], required=True)
    ap.add_argument("--frames", choices=["all", "even", "odd"], default="all")
    ap.add_argument("--first", type=int, nargs="*", default=[], help="frames to do before the rest")
    ap.add_argument("--spp", type=int, default=32)
    ap.add_argument("--chunk", type=int, default=6)
    ap.add_argument("--commit", action="store_true")
    ap.add_argument("--scratch", default=os.environ.get("ZAWAL_SCRATCH", "/tmp/zawal-seq"))
    args = ap.parse_args()

    spec = json.load(open(os.path.join(RENDERER, "sequences", f"seq_{args.seq}.json")))
    frames = spec[args.variant]["frames"]
    pick = [f for f in frames if args.frames == "all" or (f["i"] % 2 == 0) == (args.frames == "even")]
    order = [f for f in pick if f["i"] in args.first] + [f for f in pick if f["i"] not in args.first]
    os.makedirs(args.scratch, exist_ok=True)
    log = open(os.path.join(args.scratch, f"render_{args.seq}{SHORT[args.variant]}.log"), "a")
    pending, chunk_files = [f for f in order if not done(args.seq, args.variant, f["file"])], []
    print(f"SEQ-{args.seq.upper()} {args.variant} {args.frames}: {len(pick) - len(pending)} done, {len(pending)} to go", flush=True)
    for n, f in enumerate(pending):
        if "hold_of" in f:
            src = frames[f["hold_of"]]["file"]
            if not done(args.seq, args.variant, src):
                render_frame(args.seq, args.variant, frames[f["hold_of"]], args.spp, args.scratch, log)
            for name, *_ in TIERS[args.variant]:
                shutil.copy(tier_path(args.seq, args.variant, name, src), tier_path(args.seq, args.variant, name, f["file"]))
            dt = 0.0
        else:
            dt = render_frame(args.seq, args.variant, f, args.spp, args.scratch, log)
        chunk_files.append(f["file"])
        print(f"  {f['file']} {f['time']} ev {f['ev']}  {dt:.0f}s  ({n + 1}/{len(pending)})", flush=True)
        if args.commit and (len(chunk_files) >= args.chunk or n == len(pending) - 1):
            paths = [os.path.relpath(tier_path(args.seq, args.variant, t[0], c), ROOT)
                     for c in chunk_files for t in TIERS[args.variant]]
            commit_and_push(f"Render SEQ-{args.seq.upper()} {args.variant} frames {chunk_files[0]}-{chunk_files[-1]}", paths)
            chunk_files = []


if __name__ == "__main__":
    main()
