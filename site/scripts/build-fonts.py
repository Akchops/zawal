#!/usr/bin/env python3
"""Self-hosted, subset fonts (all OFL): Archivo variable, IBM Plex Mono,
IBM Plex Sans Arabic. Run once; outputs are committed in public/fonts/.

Archivo keeps its two axes but only the ranges the design uses (wght 200-600,
wdth 100-125), and only Latin + the typographic marks the copy needs.
Plex Sans Arabic keeps only the letters of the Arabic words on the site."""
import os
from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

HERE = os.path.dirname(os.path.abspath(__file__))
NM = os.path.join(HERE, "..", "node_modules")
OUT = os.path.join(HERE, "..", "public", "fonts")
LATIN = "".join(chr(c) for c in range(0x20, 0x7F)) + "–—‘’“”·°×…→←↑↓′″€£§©®™•½¼¾±≈≤≥µ²³éèêàâäçîïôöüûñÉÀ"
ARABIC_WORDS = "زوال ظل ظلال شمس"


def build(src, dst, text, limits=None):
    font = TTFont(src)
    if limits:
        font = instancer.instantiateVariableFont(font, limits)
    opts = subset.Options()
    opts.flavor = "woff2"
    opts.layout_features = ["kern", "liga", "calt", "tnum", "lnum", "case", "init", "medi", "fina", "isol", "rlig"]
    opts.name_IDs = ["*"]
    opts.notdef_outline = True
    s = subset.Subsetter(options=opts)
    s.populate(text=text)
    s.subset(font)
    font.flavor = "woff2"
    font.save(dst)
    print(os.path.basename(dst), os.path.getsize(dst), "bytes")


os.makedirs(OUT, exist_ok=True)
build(os.path.join(NM, "@fontsource-variable/archivo/files/archivo-latin-wdth-normal.woff2"),
      os.path.join(OUT, "archivo-var.woff2"), LATIN, {"wght": (200, 600), "wdth": (100, 125)})
build(os.path.join(NM, "@fontsource/ibm-plex-mono/files/ibm-plex-mono-latin-400-normal.woff2"),
      os.path.join(OUT, "plex-mono-400.woff2"), LATIN)
build(os.path.join(NM, "@fontsource/ibm-plex-mono/files/ibm-plex-mono-latin-500-normal.woff2"),
      os.path.join(OUT, "plex-mono-500.woff2"), LATIN)
build(os.path.join(NM, "@fontsource/ibm-plex-sans-arabic/files/ibm-plex-sans-arabic-arabic-300-normal.woff2"),
      os.path.join(OUT, "plex-arabic-300.woff2"), ARABIC_WORDS)
