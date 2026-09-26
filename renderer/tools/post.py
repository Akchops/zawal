#!/usr/bin/env python3
"""ZAWAL post pipeline: denoise -> glare -> exposure -> filmic tone map -> grade -> encode.

Usage:
  post.py <render_dir> <out.png|.jpg|.webp> [--ev 0.0] [--no-denoise] [--grade zawal]
          [--haze 0] [--vignette 0.18] [--width 1600]

Everything happens in linear light until the tone map. The grade is a gentle
split-tone toward the brand palette: shade leans to the blue-black "deep shade",
sunlit plaster to warm "noon white". Numbers are locked per sequence so a
scrubbed sequence never pumps.
"""
import json
import sys
import argparse
import numpy as np
from PIL import Image


def load(render_dir):
    meta = json.load(open(f"{render_dir}/meta.json"))
    w, h = meta["width"], meta["height"]

    def rd(name, c):
        a = np.fromfile(f"{render_dir}/{name}.f32", dtype=np.float32)
        return a.reshape(h, w, c) if c > 1 else a.reshape(h, w)

    return meta, rd("color", 3), rd("albedo", 3), rd("normal", 3), rd("depth", 1), rd("sunvis", 1)


def denoise(color, albedo, normal):
    import pyoidn
    out = np.zeros_like(color)
    color = np.ascontiguousarray(color, dtype=np.float32)
    albedo = np.ascontiguousarray(np.clip(albedo, 0, 1), dtype=np.float32)
    normal = np.ascontiguousarray(normal, dtype=np.float32)
    with pyoidn.Device(pyoidn.OIDN_DEVICE_TYPE_CPU) as d:
        d.commit()
        with pyoidn.Filter(d, pyoidn.OIDN_FILTER_TYPE_RT) as f:
            f.set_image(pyoidn.OIDN_IMAGE_COLOR, color, pyoidn.OIDN_FORMAT_FLOAT3)
            f.set_image(pyoidn.OIDN_IMAGE_ALBEDO, albedo, pyoidn.OIDN_FORMAT_FLOAT3)
            f.set_image(pyoidn.OIDN_IMAGE_NORMAL, normal, pyoidn.OIDN_FORMAT_FLOAT3)
            f.set_image(pyoidn.OIDN_IMAGE_OUTPUT, out, pyoidn.OIDN_FORMAT_FLOAT3)
            f.set_bool("hdr", True)
            f.set_quality(pyoidn.OIDN_QUALITY_HIGH)
            f.commit()
            f.execute()
        err = d.get_error()
        if err:
            print("oidn:", err, file=sys.stderr)
    return out


def gaussian_blur(img, sigma):
    from scipy.ndimage import gaussian_filter
    return np.stack([gaussian_filter(img[..., c], sigma, mode="nearest") for c in range(3)], -1)


def glare(img, strength=1.0):
    """Veiling glare: a physically-motivated long-tailed PSF built from a sum of
    Gaussians (camera/eye scatter). Applied in linear light so only genuinely
    bright sources (the sun, sunlit limewash) spread, never mid-tones."""
    h, w, _ = img.shape
    s = max(h, w) / 1600.0
    out = img * (1.0 - 0.035 * strength)
    # Only energy above a soft knee feeds the wide lobes, so ordinary lit plaster
    # does not haze the whole frame.
    lum = img @ np.array([0.2126, 0.7152, 0.0722], np.float32)
    knee = np.clip((lum - 2.0) / 6.0, 0, 1)[..., None]
    hot = img * knee
    for sigma, wgt in ((1.5, 0.018), (6.0, 0.010), (24.0, 0.005), (80.0, 0.002)):
        out += gaussian_blur(img if sigma < 3 else hot, sigma * s) * (wgt * strength)
    return out


AGX_IN = np.array([[0.842479062253094, 0.0423282422610123, 0.0423756549057051],
                   [0.0784335999999992, 0.878468636469772, 0.0784336],
                   [0.0792237451477643, 0.0791661274605434, 0.879142973793104]], np.float32)
AGX_OUT = np.array([[1.19687900512017, -0.0528968517574562, -0.0529716355144438],
                    [-0.0980208811401368, 1.15190312990417, -0.0980434501171241],
                    [-0.0990297440797205, -0.0989611768448433, 1.15107367264116]], np.float32)


def agx(x, punch=1.08):
    """AgX base transform (Blender 4.x / Troy Sobotka), then a mild 'punch'
    look. AgX desaturates toward white as luminance rises, the way film does,
    so sunlit limewash reaches warm white instead of the orange a per-channel
    curve produces. Input linear Rec.709, output display sRGB (0..1)."""
    min_ev, max_ev = -12.47393, 4.026069
    v = np.maximum(x, 1e-10) @ AGX_IN
    v = np.clip(np.log2(v), min_ev, max_ev)
    v = (v - min_ev) / (max_ev - min_ev)
    v2 = v * v
    v4 = v2 * v2
    v = 15.5 * v4 * v2 - 40.14 * v4 * v + 31.96 * v4 - 6.868 * v2 * v + 0.4298 * v2 + 0.1191 * v - 0.00232
    # Look: slight saturation lift around luminance, in the curve's display space.
    lum = v @ np.array([0.2126, 0.7152, 0.0722], np.float32)
    v = lum[..., None] + punch * (v - lum[..., None])
    v = np.clip(v @ AGX_OUT, 0, None)
    lin = np.power(v, 2.2)                      # AgX outputs a 2.2-encoded signal
    srgb = np.where(lin <= 0.0031308, lin * 12.92, 1.055 * np.power(lin, 1 / 2.4) - 0.055)
    return np.clip(srgb, 0, 1)


def grade(img, lum_lin):
    """Split tone toward the ZAWAL palette. img is display-referred 0..1."""
    shade = np.array([0.965, 0.985, 1.03], np.float32)  # toward deep shade blue-black
    noon = np.array([1.015, 1.0, 0.975], np.float32)    # toward warm noon white
    t = np.clip(lum_lin, 0, 1)[..., None]
    tint = shade * (1 - t) + noon * t
    return np.clip(img * tint, 0, 1)


def vignette(img, amount):
    h, w, _ = img.shape
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    nx, ny = (x / w - 0.5) * 2, (y / h - 0.5) * 2
    r2 = (nx * nx * (w / h) ** 2 + ny * ny) / (1 + (w / h) ** 2)
    v = 1 - amount * r2 ** 1.4
    return img * v[..., None]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dir")
    ap.add_argument("out")
    ap.add_argument("--ev", type=float, default=0.0)
    ap.add_argument("--no-denoise", action="store_true")
    ap.add_argument("--vignette", type=float, default=0.16)
    ap.add_argument("--glare", type=float, default=1.0)
    ap.add_argument("--width", type=int, default=0)
    ap.add_argument("--quality", type=int, default=88)
    ap.add_argument("--save-linear", default="")
    ap.add_argument("--wb", default="0.930,1.0,1.110",
                    help="white-balance gains. Default = Dubai June noon global illuminant "
                         "(sun + sky on a horizontal plane, from the renderer's own atmosphere): "
                         "a photographer's locked daylight preset, so morning and dusk stay warm")
    args = ap.parse_args()

    meta, color, albedo, normal, depth, sunvis = load(args.dir)
    color = np.nan_to_num(color, nan=0.0, posinf=0.0, neginf=0.0)
    img = color if args.no_denoise else denoise(color, albedo, normal)
    if args.save_linear:
        np.save(args.save_linear, img.astype(np.float16))
    # Exposure: relative scene units -> photographic. EV 0 puts sunlit limewash
    # near the top of the curve's linear section at noon.
    exposure = 2.0 ** args.ev * 6.0
    wb = np.array([float(v) for v in args.wb.split(",")], np.float32)
    img = img * exposure * wb
    img = glare(img, args.glare)
    lum_lin = img @ np.array([0.2126, 0.7152, 0.0722], np.float32)
    img = agx(img)
    img = grade(img, lum_lin * 0.6)
    if args.vignette > 0:
        img = vignette(img, args.vignette)
    # Blue-noise-ish dither below one 8-bit step: removes banding in the sky.
    rng = np.random.default_rng(7)
    img = img + (rng.random(img.shape, dtype=np.float32) - 0.5) / 255.0
    out = Image.fromarray((np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8))
    if args.width and args.width != out.width:
        out = out.resize((args.width, round(out.height * args.width / out.width)), Image.LANCZOS)
    kw = {}
    if args.out.endswith(".jpg"):
        kw = dict(quality=args.quality, optimize=True, progressive=True, subsampling=0)
    elif args.out.endswith(".webp"):
        kw = dict(quality=args.quality, method=6)
    out.save(args.out, **kw)
    print(f"{args.out}  sun alt {meta['sunAltitudeDeg']:.2f} az {meta['sunAzimuthDeg']:.2f}  "
          f"render {meta['renderSeconds']:.0f}s")


if __name__ == "__main__":
    main()
