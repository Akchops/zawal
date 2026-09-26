#!/usr/bin/env python3
"""Contact sheets for qa/matrix.mjs: one row per (viewport, mode), its nine
scroll depths side by side, labelled.
    sheet.py <matrixDir> <out.png> [viewport-width ...]
"""
import json, os, sys
from PIL import Image, ImageDraw

d, out = sys.argv[1], sys.argv[2]
only = set(sys.argv[3:])
log = json.load(open(os.path.join(d, "log.json")))
rows = [e for e in log if not only or e["viewport"].split("x")[0] in only]
TH = 300                                   # thumbnail height
thumbs = []
for e in rows:
    ims = [Image.open(os.path.join(d, s)).convert("RGB") for s in e["shots"]]
    ims = [im.resize((max(1, round(im.width * TH / im.height)), TH)) for im in ims]
    thumbs.append((e, ims))
W = max(sum(i.width + 6 for i in ims) for _, ims in thumbs) + 150
H = len(thumbs) * (TH + 30) + 10
sheet = Image.new("RGB", (W, H), (240, 236, 228))
g = ImageDraw.Draw(sheet)
y = 10
for e, ims in thumbs:
    bad = len(e["console"]) + len(e["network"])
    g.text((8, y + 4), f"{e['viewport']}\n{e['mode']}\nconsole {len(e['console'])}\nnetwork {len(e['network'])}", fill=(160, 40, 30) if bad else (20, 20, 20))
    x = 150
    for im in ims:
        sheet.paste(im, (x, y)); x += im.width + 6
    y += TH + 30
sheet.save(out)
print(out, sheet.size)
