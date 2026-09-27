#!/usr/bin/env python3
"""The studio at zawal (12:20:42 on 21 June), for the Studio page: the Al Quoz
warehouse's lattice ceiling throws its stars on the floor and the long tables.

    render_studio.py [spp] [binary]
        -> site/public/img/studio/studio-l{1920,1280,960}.webp   (16:9)
           site/public/img/studio/studio-s{1080,720}.webp        (4:5, phones)

Reuses the project-hero pipeline (render, grade, export). Resumable: a render
whose PNG master exists is not redone.
"""
import os
import sys

import render_projects as rp

SPEC = dict(scene="studio", time="12:20:42", ev=0.7, extra=[],
            l=dict(pos=(7.0, 1.6, 0.8), tgt=(7.0, 1.6, 30.0), fov=80, shift=0.18),
            s=dict(pos=(7.0, 1.6, 0.6), tgt=(7.0, 1.6, 30.0), fov=78, shift=0.12))


def main():
    spp = int(sys.argv[1]) if len(sys.argv) > 1 else 40
    if len(sys.argv) > 2:
        rp.BIN = os.path.abspath(sys.argv[2])
    rp.OUT = os.path.join(rp.ROOT, "site", "public", "img", "studio")
    rp.SIZES["s"] = (1080, 1350)
    rp.WIDTHS["s"] = [1080, 720]
    scratch = os.path.join(os.environ.get("ZAWAL_SCRATCH", "/tmp/zawal-seq"), "studio")
    os.makedirs(scratch, exist_ok=True)
    os.makedirs(rp.OUT, exist_ok=True)
    for kind in ("l", "s"):
        raw = os.path.join(scratch, f"studio-{kind}")
        master = rp.render("studio", SPEC, SPEC[kind], kind, spp, raw)
        rp.export(master, f"studio-{kind}", kind)
    print("done studio")


if __name__ == "__main__":
    main()
