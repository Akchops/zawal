#!/usr/bin/env python3
"""Captioned grid of the realism-pass close-ups.

    detail_sheet.py DETAILS_DIR OUT.jpg
"""
import os
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(__file__))
from before_after import BG, INK, MUTED, font  # noqa: E402

CAPTIONS = [
    ("gate_lantern_0730", "07:30  The gate lantern prints its star pattern, doubled by the drum's two walls, on the facade"),
    ("street_end_0730", "07:30  The street's end: cars, lamps, cables, a far house, a ghaf"),
    ("lantern_noon", "12:20:42  Zawal: through the pierced dome, a disc of small stars inside the canopy's large ones"),
    ("pool_noon", "12:20:42  Caustics inside each star on the pool floor; the verdigris coping"),
    ("olives_noon", "12:20:42  Potted olives: silvery crowns, dappled shade, salt bloom on terracotta"),
    ("door_1700", "17:00  Hand grime at the jambs, stress cracks off the door head, a damp tide line"),
    ("bench_0800", "08:00  A folded palm mat and a linen cushion on the teak bench; stone joints and grit"),
]


def main():
    src, out = sys.argv[1:3]
    tiles = [(os.path.join(src, n + ".jpg"), c) for n, c in CAPTIONS if os.path.exists(os.path.join(src, n + ".jpg"))]
    tw, th, cap, gap = 800, 600, 40, 12
    cols = 2
    rows = (len(tiles) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * tw + (cols + 1) * gap, 64 + rows * (th + cap + gap)), BG)
    d = ImageDraw.Draw(sheet)
    d.text((gap, 20), "ZAWAL  realism pass: close-ups at 100 %  (all rendered by code: no photos, no stock)", fill=INK, font=font(22))
    f = font(15)
    for k, (path, caption) in enumerate(tiles):
        x = gap + (k % cols) * (tw + gap)
        y = 64 + (k // cols) * (th + cap + gap)
        im = Image.open(path).convert("RGB").resize((tw, th), Image.LANCZOS)
        sheet.paste(im, (x, y))
        d.text((x, y + th + 10), caption, fill=MUTED, font=f)
    sheet.save(out, quality=88)
    print(out, sheet.size)


if __name__ == "__main__":
    main()
