# ZAWAL — Architecture measured in shade

**ZAWAL is a fictional studio. This repository is a design demonstration.** Made by Aarav Chopra.
No real clients, awards, developers, people or buildings appear anywhere in it.

A cinematic, scroll-driven site for an invented Dubai architecture practice, where scroll moves the real
sun across 21 June (25.2°N) and every image is rendered by code in this repository: no stock, no photos,
no AI image services.

## Status

**Built (Phases 2–4).** The phone set is rendered and in the site; the desktop (landscape) sequences are
rendering (resumable, committed every six frames) and go in when they land. See
[`docs/DIRECTION.md`](docs/DIRECTION.md) §9 for the checks, measurements and decisions.

- Four signatures on the home page: **A** a day in one scroll (a scrubbed, blended frame sequence),
  **B** street to courtyard (camera travel, depth-reprojected between frames), **C** you are the sun
  (a live relight of the courtyard by pointer, touch, keys or tilt), **D** projects morph through their
  own shadow patterns (a full-screen shader).
- First load under 100 kB gzip on every page (measured, `site/scripts/check-budget.mjs`); no three.js, no
  GSAP; raw WebGL loaded after first paint. WebGL → Canvas 2D → static stills; reduced motion and no-JS
  get the whole story as a readable article.
- [`docs/checks/`](docs/checks/): the evidence for the three pre-render checks (samples, smooth scrubbing,
  the first frame at 100 %).

## The renderer (`renderer/`)

A CPU path tracer written for this project (C++17, OpenMP): NOAA solar geometry, a physically based dusty
Gulf atmosphere, exact CSG boxes with rounded arrises, lattice screens traced through their real depth
(with holes carved at an angle), procedural limewash / rammed earth / coral stone / limestone / teak /
brass / verdigris, leaf-cell foliage, OIDN denoising, AgX tone mapping.

```bash
cd renderer && make
./bin/zawal --scene house --cam court --time 12:20:42 --w 1600 --h 900 --spp 64 --out /tmp/zawal/zawal
python3 tools/post.py /tmp/zawal/zawal zawal.jpg --ev 0.4        # needs numpy, pillow, scipy, pyoidn
python3 tools/plan_sequences.py                                  # plan SEQ-A / SEQ-B (writes sequences/*.json)
python3 tools/render_seq.py --seq b --variant portrait --frames all --commit   # resumable sequence render
bash tools/render_desktop_set.sh                                 # both landscape sequences
python3 tools/render_projects.py heroes 40                       # the four project heroes + sun masks (D)
python3 tools/render_projects.py details 32                      # two details per project
python3 tools/render_night.py                                    # the lantern states for the night scene
python3 tools/render_studio.py 40                                # the studio warehouse at 12:20
```

Scenes: `house` (street, gate, courtyard), `qudra` (desert pavilion), `ghaf`, `sikka`, `mushrif`
(project heroes), `studio` (the Al Quoz warehouse). Times are local GST; `--lat/--lon` move the sun to
another site; `--indirect 1`, `--lights` and `--nightglow` produce the relight basis and night states.

## The site (`site/`)

Astro, static output. Content lives in `site/src/content/*.json`; the film's clock and scenes in
`site/src/lib/` (`scroll.js`, `home.js`, `player.js`, `sun.js`, `compositor.js`, `study.js`).

```bash
cd site && npm install
npm run build              # pack frames, posters and masks; astro build; first-load budget check
npm run preview-site       # a copy of dist/ with relative URLs, for hosting in a sub-folder
node qa/pages.mjs http://localhost:4400 /tmp/qa        # every page at 320/390/430/768/1440/1728 + axe
node qa/matrix.mjs http://localhost:4400 /tmp/matrix   # the home film at nine depths, four modes
node qa/focus.mjs http://localhost:4400/ /tmp/focus 390x844   # every Tab stop visible and ringed
node qa/signatures.mjs http://localhost:4400 /tmp/sig 390x844@2 --gl   # the four signatures as strips
```

`?forcegl` accepts a software WebGL context (headless QA only; real visitors on software GL get the
Canvas 2D tier), `?nogl` forces Canvas 2D, `?noguard` disables the frame-drop guard, `?noblend` shows bare
frames (the check-2 baseline).

## Earlier phases

- [`docs/phase1/`](docs/phase1/): the Phase 1 proof frames, each with the exact sun it was lit by
- [`docs/realism/`](docs/realism/): the realism pass, before | after and close-ups at 100 %
- [`docs/proof/shader-traps.png`](docs/proof/shader-traps.png): lattice / falloff / crease checks on the
  renderer's own fields against deliberately broken controls
