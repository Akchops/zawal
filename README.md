# ZAWAL — Architecture measured in shade

**ZAWAL is a fictional studio. This repository is a design demonstration.** Made by Aarav Chopra.
No real clients, awards, developers, people or buildings appear anywhere in it.

A cinematic, scroll-driven site for an invented Dubai architecture practice, where scroll moves the real
sun across 21 June (25.2°N) and every image is rendered by code in this repository: no stock, no photos,
no AI image services.

## Status

**Phase 1 — direction + renderer proof.** Waiting for approval before any site is built.

- [`docs/DIRECTION.md`](docs/DIRECTION.md): art direction, palette, type, motion principles, page
  architecture, home storyboard, the four signature moments, the render plan
- [`docs/phase1/`](docs/phase1/): proof frames from the renderer, each with the exact sun it was lit by
- [`docs/proof/shader-traps.png`](docs/proof/shader-traps.png): lattice / falloff / crease checks on the
  renderer's own fields against deliberately broken controls

## The renderer (`renderer/`)

A CPU path tracer written for this project (C++17, OpenMP): NOAA solar geometry, a physically based dusty
Gulf atmosphere, exact CSG boxes with rounded arrises, lattice screens traced through their real depth,
procedural limewash / rammed earth / limestone / teak / brass / verdigris, OIDN denoising, AgX tone mapping.

```bash
cd renderer && make
./bin/zawal --scene house --cam court --time 12:20:42 --w 1600 --h 900 --spp 64 --out /tmp/zawal/zawal
python3 tools/post.py /tmp/zawal/zawal zawal.jpg --ev 0.4        # needs numpy, pillow, scipy, pyoidn
./tools/render_phase1.sh                                         # all Phase 1 proof frames
```

Scenes: `house` (street, gate, courtyard), `qudra` (desert pavilion). Cameras: `court`, `court_p`,
`street`, `street_p`, `qudra`. Times are local GST; `--lat/--lon` move the sun to another site.
