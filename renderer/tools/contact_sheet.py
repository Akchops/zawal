#!/usr/bin/env python3
"""Phase 1 contact sheet: landscape frames in a column, portraits beside, each labelled with its sun."""
import json, sys, os
from PIL import Image, ImageDraw, ImageFont

d = sys.argv[1]
out = sys.argv[2]
land = ["street_0730", "court_0800", "court_zawal", "court_1700", "qudra_1340"]
port = ["street_0730_p", "court_zawal_p"]
labels = {
    "street_0730": "THE STREET · 07:30",
    "court_0800": "COURTYARD · 08:00",
    "court_zawal": "COURTYARD · ZAWAL 12:20:42",
    "court_1700": "COURTYARD · 17:00",
    "qudra_1340": "QUDRA CANOPY · 13:40",
    "street_0730_p": "PHONE · STREET 07:30",
    "court_zawal_p": "PHONE · ZAWAL",
}
LW, LH = 960, 540
PW, PH = 405, 720
pad, cap = 14, 34
try:
    F = ImageFont.truetype("DejaVuSansMono.ttf", 14)
except Exception:
    F = ImageFont.load_default()
W = pad + LW + pad + PW + pad
H = pad + len(land) * (LH + cap) + pad
sheet = Image.new("RGB", (W, H), (15, 20, 24))
dr = ImageDraw.Draw(sheet)
y = pad
for n in land:
    p = f"{d}/{n}.jpg"
    if os.path.exists(p):
        im = Image.open(p).resize((LW, LH), Image.LANCZOS)
        sheet.paste(im, (pad, y))
        m = json.load(open(f"{d}/{n}.json"))
        dr.text((pad, y + LH + 8), f"{labels[n]}   ☉ alt {m['sunAltitudeDeg']:.2f}°  az {m['sunAzimuthDeg']:.2f}°", fill=(233, 223, 206), font=F)
    y += LH + cap
y = pad
for n in port:
    p = f"{d}/{n}.jpg"
    if os.path.exists(p):
        im = Image.open(p).resize((PW, PH), Image.LANCZOS)
        sheet.paste(im, (pad + LW + pad, y))
        dr.text((pad + LW + pad, y + PH + 8), labels[n], fill=(233, 223, 206), font=F)
    y += PH + cap + 20
sheet.save(out, quality=88)
print(out)
