#!/usr/bin/env python3
"""Check 2 metrics: per-step change of the picture during a slow scroll.

    scrub_metrics.py <blend-dir> <noblend-dir> <out.png> [frames-per-beat anchors as json]

A slideshow shows up as long runs of zero change broken by spikes at frame
boundaries; smooth playback changes a little at every step. Reported per run:
  median / max per-step change (mean |diff|, 0-255), max/median, the share of
  "frozen" steps (change < 10% of the run's max), and the largest single jump.
"""
import json, os, sys
import numpy as np
from PIL import Image, ImageDraw


def load(d):
    meta = json.load(open(os.path.join(d, "meta.json")))
    ims = []
    for s in meta["steps"]:
        im = Image.open(os.path.join(d, f"s{s['k']:04d}.png")).convert("L")
        ims.append(np.asarray(im, np.float32))
    return meta, ims


def stats(ims):
    mad = np.array([np.abs(ims[k] - ims[k - 1]).mean() for k in range(1, len(ims))])
    mx = mad.max() if len(mad) else 0
    frozen = float((mad < 0.1 * mx).mean()) if mx > 0 else 1.0
    med = float(np.median(mad))
    return mad, {"steps": len(mad), "median": round(med, 3), "max": round(float(mx), 3),
                 "max_over_median": round(float(mx / med), 1) if med > 1e-6 else float("inf"),
                 "frozen_share": round(frozen, 3), "cv": round(float(mad.std() / (mad.mean() + 1e-9)), 2)}


def main():
    a, b, out = sys.argv[1], sys.argv[2], sys.argv[3]
    ma, ia = load(a)
    mb, ib = load(b)
    da, sa = stats(ia)
    db, sb = stats(ib)
    print("blend  ", json.dumps(sa))
    print("noblend", json.dumps(sb))
    # Chart: per-step change, blend (top) and slideshow (bottom), same scale.
    W, H, pad = 1200, 520, 40
    img = Image.new("RGB", (W, H), (247, 243, 234))
    g = ImageDraw.Draw(img)
    top = max(da.max(), db.max()) * 1.05 + 1e-6
    for row, (d, name, col, st) in enumerate([(da, "blended (as shipped)", (54, 96, 79), sa), (db, "nearest frame only (slideshow)", (140, 60, 50), sb)]):
        y0 = pad + row * (H // 2)
        h = H // 2 - pad - 20
        g.text((pad, y0 - 28), f"{name}: median {st['median']}, max {st['max']}, max/median {st['max_over_median']}, frozen steps {int(st['frozen_share'] * 100)}%", fill=(15, 20, 24))
        n = len(d)
        bw = (W - 2 * pad) / max(1, n)
        for k, v in enumerate(d):
            x = pad + k * bw
            g.rectangle([x, y0 + h - v / top * h, x + max(1, bw - 1), y0 + h], fill=col)
        g.line([pad, y0 + h, W - pad, y0 + h], fill=(15, 20, 24))
    img.save(out)
    json.dump({"blend": sa, "noblend": sb}, open(os.path.splitext(out)[0] + ".json", "w"), indent=1)


if __name__ == "__main__":
    main()
