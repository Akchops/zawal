#!/usr/bin/env python3
"""Side-by-side before/after sheets for the realism pass.

    before_after.py BEFORE_DIR AFTER_DIR OUT_DIR

For every NAME.jpg present in both directories writes OUT_DIR/NAME.jpg: the
two frames side by side at the same size, labelled, on the site's night
colour. Also writes OUT_DIR/overview.jpg with every pair stacked.
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

BG = (22, 20, 18)
INK = (233, 223, 206)
MUTED = (150, 140, 126)


def font(size):
    for path in ("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                 "/usr/share/fonts/dejavu/DejaVuSans.ttf"):
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def pair(before, after, title, cell_w):
    a = Image.open(before).convert("RGB")
    b = Image.open(after).convert("RGB")
    h = round(a.height * cell_w / a.width)
    a = a.resize((cell_w, h), Image.LANCZOS)
    b = b.resize((cell_w, h), Image.LANCZOS)
    gap, pad, head = 12, 16, 46
    sheet = Image.new("RGB", (pad * 2 + cell_w * 2 + gap, head + h + pad), BG)
    d = ImageDraw.Draw(sheet)
    f = font(20)
    d.text((pad, 13), "BEFORE  phase 1", fill=MUTED, font=f)
    d.text((pad + cell_w + gap, 13), "AFTER  realism pass", fill=INK, font=f)
    tw = d.textlength(title, font=f)
    d.text((pad + cell_w * 2 + gap - tw, 13), title, fill=MUTED, font=f)
    sheet.paste(a, (pad, head))
    sheet.paste(b, (pad + cell_w + gap, head))
    return sheet


def main():
    before_dir, after_dir, out_dir = sys.argv[1:4]
    os.makedirs(out_dir, exist_ok=True)
    names = sorted(n for n in os.listdir(after_dir)
                   if n.endswith(".jpg") and os.path.exists(os.path.join(before_dir, n)))
    sheets = []
    for n in names:
        a = Image.open(os.path.join(after_dir, n))
        portrait = a.height > a.width
        s = pair(os.path.join(before_dir, n), os.path.join(after_dir, n), n[:-4], 620 if portrait else 1200)
        s.save(os.path.join(out_dir, n), quality=88)
        sheets.append((n, s))
        print(n, s.size)
    # Overview: landscape pairs full width, portrait pairs two to a row.
    land = [s for n, s in sheets if s.width > 2000]
    port = [s for n, s in sheets if s.width <= 2000]
    width = max(s.width for _, s in sheets)
    rows = [s.resize((width, round(s.height * width / s.width)), Image.LANCZOS) for s in land]
    for i in range(0, len(port), 2):
        chunk = port[i:i + 2]
        cw = width // 2
        scaled = [s.resize((cw, round(s.height * cw / s.width)), Image.LANCZOS) for s in chunk]
        row = Image.new("RGB", (width, max(s.height for s in scaled)), BG)
        for k, s in enumerate(scaled):
            row.paste(s, (k * cw, 0))
        rows.append(row)
    ov = Image.new("RGB", (width, sum(r.height for r in rows)), BG)
    y = 0
    for r in rows:
        ov.paste(r, (0, y))
        y += r.height
    ov = ov.resize((1600, round(ov.height * 1600 / ov.width)), Image.LANCZOS)
    ov.save(os.path.join(out_dir, "overview.jpg"), quality=85)
    print("overview", ov.size)


if __name__ == "__main__":
    main()
