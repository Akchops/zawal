#!/usr/bin/env python3
"""Shader-trap proof sheet.

Reads the fields and profiles written by bin/trap_proof (which evaluates the
renderer's real functions) and plots derivatives, beside broken controls.

  python3 tools/trap_proof.py <datadir> <out.png>

Reading the sheet:
  Trap 1  A lattice shows as an axis-aligned GRID in the curvature image.
  Trap 3  A crease shows as a JUMP in the slope curve (h'), and as a thin
          bright LINE along the diagonal of a joint crossing.
  Trap 2  A plateau shows as a FLAT disc with no contours; a kink as contours
          that stop abruptly at the rim instead of converging.
"""
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont

src, dst = sys.argv[1], sys.argv[2]
N = 512
P = 280           # panel size
PAD = 18
BG = (14, 19, 24)
INK = (233, 223, 206)
BAD = (222, 124, 104)
GOOD = (127, 179, 160)
DIM = (120, 128, 134)

try:
    F = ImageFont.truetype("DejaVuSans.ttf", 12)
    FB = ImageFont.truetype("DejaVuSans-Bold.ttf", 14)
except Exception:
    F = FB = ImageFont.load_default()


def load(name):
    return np.fromfile(f"{src}/{name}.f32", dtype=np.float32).reshape(N, N).astype(np.float64)


def grad_mag(a, h):
    gy, gx = np.gradient(a, h)
    return np.hypot(gx, gy)


def laplacian(a, h):
    return (np.roll(a, 1, 0) + np.roll(a, -1, 0) + np.roll(a, 1, 1) + np.roll(a, -1, 1) - 4 * a)[1:-1, 1:-1] / (h * h)


def curvature_jump(a, h):
    """|grad(laplacian)|: a curvature DISCONTINUITY (C1-not-C2 fade) becomes a
    spike here, so a lattice shows as a crisp grid."""
    lap = laplacian(a, h)
    return grad_mag(lap, h)


def to_img(v, pct=99.0, gamma=0.6):
    s = np.percentile(v, pct) + 1e-12
    x = np.clip(v / s, 0, 1) ** gamma
    im = Image.fromarray((x * 255).astype(np.uint8)).convert("RGB")
    return im.resize((P, P), Image.LANCZOS)


def plot(profiles, title_lines, xlabel):
    """profiles: list of (xs, ys, colour, label). Draws h' (slope) curves,
    normalised, with h as a faint underlay."""
    im = Image.new("RGB", (P, P), (20, 26, 32))
    d = ImageDraw.Draw(im)
    for k, (x, y, col, lab) in enumerate(profiles):
        dy = np.gradient(y, x)
        for series, alpha, lw in ((y, 0.35, 1), (dy, 1.0, 2)):
            s = series - series.min()
            s = s / (s.max() + 1e-12)
            px = (x - x.min()) / (x.max() - x.min()) * (P - 20) + 10
            py = P - 30 - s * (P - 70)
            c = tuple(int(cc * alpha + 20 * (1 - alpha)) for cc in col)
            d.line(list(zip(px.tolist(), py.tolist())), fill=c, width=lw)
        d.text((10, 8 + 14 * k), lab, fill=col, font=F)
    d.text((10, P - 20), xlabel, fill=DIM, font=F)
    return im


def falloff_panel(kind):
    n = 600
    y, x = np.mgrid[0:n, 0:n] / n * 2 - 1
    d = np.hypot(x, y)
    r = 0.8
    if kind == "smoothstep":
        t = np.clip((d - 0.3) / (r - 0.3), 0, 1)
        f = 1 - t * t * (3 - 2 * t)
    elif kind == "linear":
        f = np.clip(1 - d / r, 0, 1)
    else:
        q = np.clip(1 - d * d / (r * r), 0, 1)
        f = q ** 3
    c = f * 40
    frac = c - np.floor(c)
    # anti-aliased contour lines where fract crosses 0
    w = np.fwidth if hasattr(np, "fwidth") else None
    gy, gx = np.gradient(c)
    fw = np.hypot(gx, gy) + 1e-6
    line = np.clip(1 - np.minimum(frac, 1 - frac) / (1.2 * fw), 0, 1) * (f > 0)
    base = 0.12 + 0.6 * f
    v = np.clip(base + 0.35 * line, 0, 1)
    im = Image.fromarray((v * 255).astype(np.uint8)).convert("RGB")
    return im.resize((P, P), Image.LANCZOS)


rows = []
# ---- Trap 1 ------------------------------------------------------------------
h = 8.0 / N
bad = load("t1_bad")
good = load("t1_good")
lime = load("t1_lime")
rows.append(("TRAP 1 · value-noise lattice — curvature-jump |∇(∇²h)| over 8 lattice cells", [
    (to_img(curvature_jump(bad, h), 99.5, 0.5), "CONTROL value noise, cubic fade, 2.0×", BAD),
    (to_img(curvature_jump(good, h), 99.5, 0.5), "ZAWAL gradient noise, quintic, rot 1.97×", GOOD),
    (to_img(grad_mag(bad, h), 99.5, 0.8), "CONTROL slope |∇h|: blobs sit on grid", BAD),
    (to_img(grad_mag(good, h), 99.5, 0.8), "ZAWAL slope |∇h|: isotropic", GOOD),
    (to_img(grad_mag(lime, 0.5 / N), 99.5, 0.8), "ZAWAL limewash bump, 0.5 m of wall", GOOD),
]))

# ---- Trap 3 ------------------------------------------------------------------
def csv(name):
    a = np.loadtxt(f"{src}/{name}.csv", delimiter=",")
    return a[:, 0], a[:, 1]

jb, jg = load("t3_joint_bad"), load("t3_joint_good")
hz = 0.012 / N
xb, yb = csv("p_joint_bad")
xg, yg = csv("p_joint_good")
cb_x, cb_y = csv("p_crest_bad")
cg_x, cg_y = csv("p_crest_good")
gx_, gy_ = csv("p_groove")
rows.append(("TRAP 3 · clamp creases — slope h′ (bold) and height (faint) across joints and crests", [
    (plot([(xb, yb, BAD, "CONTROL clamp ramp: h′ jumps")], [], "across a floor joint, 10 mm"), "joint profile, clamp + max", BAD),
    (plot([(xg, yg, GOOD, "ZAWAL smootherstep: h′ smooth")], [], "across a floor joint, 10 mm"), "joint profile, smootherstep + smax", GOOD),
    (to_img(grad_mag(jb, hz), 99.7, 0.7), "CONTROL joint crossing: max() crease", BAD),
    (to_img(grad_mag(jg, hz), 99.7, 0.7), "ZAWAL joint crossing: smax, no diagonal", GOOD),
    (plot([(cb_x, cb_y, BAD, "CONTROL |sin| crest: h′ jumps"), (cg_x, cg_y, GOOD, "ZAWAL √(sin²+e): continuous")], [],
          "across a dune crest, ±0.25 rad"), "dune crest profiles", GOOD),
]))

# ---- Trap 2 ------------------------------------------------------------------
rows.append(("TRAP 2 · flat-top falloff — contours of fract(falloff × 40)", [
    (falloff_panel("smoothstep"), "CONTROL 1−smoothstep: plateau", BAD),
    (falloff_panel("linear"), "CONTROL clamp(1−d/r): kink at rim", BAD),
    (falloff_panel("compact"), "ZAWAL live shader (1−d²/r²)³", GOOD),
    (plot([(gx_, gy_, GOOD, "ZAWAL rammed-earth lift groove")], [], "across a lift line, 40 mm"), "groove profile: h′ continuous", GOOD),
]))

W = 5 * (P + PAD) + PAD
H = PAD + sum(34 + P + 30 for _ in rows) + 60
sheet = Image.new("RGB", (W, H), BG)
d = ImageDraw.Draw(sheet)
y = PAD
for title, panels in rows:
    d.text((PAD, y + 8), title, fill=INK, font=FB)
    y += 34
    for k, (im, cap, col) in enumerate(panels):
        x = PAD + k * (P + PAD)
        sheet.paste(im, (x, y))
        d.text((x, y + P + 8), cap, fill=col, font=F)
    y += P + 30
d.text((PAD, y + 8), "Fields are the renderer's own functions (src/noise.h, src/materials.h, src/geometry.h), "
       "evaluated by tools/trap_proof.cpp. Red = deliberately broken control. Green = shipped.", fill=DIM, font=F)
sheet.save(dst)
print(dst)
