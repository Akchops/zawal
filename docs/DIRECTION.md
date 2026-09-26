# ZAWAL — Direction & Render Proof (Phase 1)

> ZAWAL is a fictional studio. This document and the site are a design demonstration by Aarav Chopra.
> Status: **Phase 1, waiting for approval.** Nothing below is built yet except the offline renderer and the proof frames in `docs/phase1/`.

---

## 0. The idea in one line

**Your scroll is the sun.** The home page is one real day: 21 June, Dubai, 25.2°N. Every image is lit by the true sun for its minute. Scrolling moves the sun, shadows sweep, shrink to almost nothing at 12:20:42 (*zawal*), and stretch into dusk. Then the page hands the sun to the visitor.

Spectacle thesis (what a normal architecture site would never do): *the site doesn't show photographs of shade, it performs shade. The visitor watches a real day happen to a building, then holds the sun themselves.*

The recognisable device, visible without the logo: **a small solar instrument (the HUD)** that always reads the real sun for whatever is on screen (`21·06 · 12:20:42 · ☉ 88.23° / 180.0° · shadow 0.03 × h`). Every number on the site comes from the same solar algorithm the renderer uses. The site cannot lie about the light.

**Who it is for and why it sells.** Prospects are Dubai architecture and interior practices. Their work is sold on images of light and material, and their websites are almost all the same grid of project photos. A site where their own subject, light on built form, is *performed live* with correct solar geometry is something they recognise instantly as expensive and impossible to template. It demonstrates art direction, 3D rendering, scroll choreography, shader work, data-driven content and mobile engineering at once.

**Scope to build:** home film; four project pages from one template; Shade Study page with the full instrument; Studio + contact page; page transitions; all fallbacks.
**Deliberately not built:** CMS, blog/journal, careers, press, search, language switcher (Arabic is used only for the name), a map embed, any form backend (contact is `mailto:` + `tel:`), invented team photos.

**Biggest sales risk:** a prospect judges the imagery as "CG" in the first second. **Biggest visual risk:** vegetation (see §11).

---

## 1. Art direction

**Visual world.** Sun-struck and dense. Full-bleed architectural imagery, rendered by our own path tracer, is the page. Type lives *in the shade* of the image, set into the dark bands that the real shadows make. There are three materially different states across the page: blazing white (street, noon floor), deep blue-black shade (passage, dusk, night), and warm tactile close-ups (rammed earth, teak, brass).

**Adjectives:** exact · sun-struck · tactile · cool-in-heat · monumental · intimate · measured.

**It must not look like:**
- luxury real estate: gold, marble, script fonts, "exclusive", champagne beige with whitespace
- the "minimal architecture studio" template: white void, tiny grey sans, a grid of project cards
- Dubai clichés: skyline, Burj, dunes-with-camels, calligraphy pastiche, lanterns as ornament
- Islamic pattern used as wallpaper. Every pattern on this site casts a shadow or it doesn't appear.
- glassmorphism, gradient blobs, bento grids, rounded cards, fade-up everything

**Composition rules.**
1. Image first, full-bleed, always. No image sits in a card.
2. Type sits in shade. Headline blocks are placed where the render is darkest, measured from the frame's own luminance map, never over busy light.
3. One-point perspective and level verticals (architectural shift-lens discipline) for every architectural frame. Asymmetry comes from light, not from tilting the camera.
4. Density rhythm: IMPACT (street) → RELEASE (dark passage) → BUILD (morning) → IMPACT (zawal) → QUIET (dusk) → PLAY (you are the sun) → IMPACT ×4 (projects) → CALM (method, studio) → FINAL IMPACT (night lantern) → contact.
5. Captions are drawn like architectural annotation: hairline leaders, times, altitudes, dimensions. Never paragraphs describing craft.

**Imagery bible (web-image-director rules applied to rendered imagery):**

| | rule |
|---|---|
| Subjects | fictional ZAWAL buildings only; no people, no cars, no signage or text in images, no real buildings |
| Light | the true sun for the stated site, date and minute (NOAA solar algorithm), dusty Gulf summer atmosphere (AOD ≈ 0.38) |
| Lens | 20–24 mm equivalent for courtyards and interiors, 28–35 mm for street and details; eye height 1.45–1.62 m |
| Perspective | verticals always vertical (lens shift, never pitch), except the one aerial project frame |
| Colour | locked daylight white balance (noon global illuminant), so morning and dusk stay warm, shade stays blue |
| Texture | every surface has hand-made irregularity: limewash clouding, rammed-earth courses, stone lippage, teak grain |
| Grade | AgX filmic, a whisper of split-tone toward the palette (shade → blue-black, sun → noon white), sub-LSB dither |
| Never | lens flare stickers, fake depth of field on architecture, HDR halos, stock-looking "golden hour" orange |

---

## 2. Palette (final)

The site lives in the shade, so dark is the default ground.

| name | hex | role | use / never |
|---|---|---|---|
| **Deep Shade** | `#0F1418` | page ground, the colour of the passage | behind all imagery and dark sections. Never pure black (#000 kills the blue of real shade) |
| **Noon White** | `#F7F3EA` | primary text on shade; the zawal flash | headlines, body on dark, the one full-white flash at 12:20:42. Never as a large light background |
| **Limewash Sand** | `#E6DAC6` | light sections (Shade Study sheet, Studio) | the "paper" of drawings. Never as a tint over imagery |
| **Dust** | `#8C857A` | secondary text, hairlines (at 35% alpha) | captions, HUD labels, rules. Never for body copy |
| **Weathered Copper** | `#5E9483` | the one accent: the sun dot, links, focus rings, the CTA | small quantities only. On sand use its deep tint `#36604F` (AA 5.2:1) |

Contrast on Deep Shade: Noon White 17:1 · Copper 5.3:1 · Dust 5.1:1 (all ≥ AA). The same copper-green exists *in the buildings*: the pool lining, waterspouts, the west door. The accent is a material, not a UI colour.

---

## 3. Typography

| role | face | settings | why |
|---|---|---|---|
| Display + text | **Archivo** (variable, wdth 62–125, wght 100–900, OFL, Google Fonts) | wordmark and statements at **wdth 125**, wght 300, tight leading; text at wdth 100, wght 400, 17/26 | Expanded grotesk letterforms are long and low, like shadows at 08:00. Rational enough for architects, not the overused serif-luxury or Space-Grotesk-tech voices |
| Instrument | **IBM Plex Mono** (OFL) | 11–13 px, caps, +0.08em, tabular figures | the daylight engineer's voice: every time, angle and dimension |
| Arabic | **IBM Plex Sans Arabic** (OFL), subset to a handful of glyphs | زوال, ظل | the name and one or two words, set with care, never decoration |

Scale: wordmark `clamp(44px, 9vw, 160px)`; statements `clamp(34px, 6vw, 104px)`; section heads `clamp(26px, 3.4vw, 56px)`; body 17–18 px, max 38 ch; HUD 11–13 px. Fonts are self-hosted, subset, `font-display: swap`, and only Archivo text weights are preloaded.

Grid: 12 columns, 24 px gutters desktop / 4 columns, 16 px gutters mobile. **No radius anywhere** (limewash arrises are rounded by the renderer, not by CSS). Borders are 1 px Dust hairlines. Icons: none, except the sun-path glyph in the HUD (a drawn SVG arc).

---

## 4. Motion principles

1. **The sun is the only animator.** Anything that moves is either light moving (time) or the viewer moving (camera). Nothing floats, bounces or fades for decoration.
2. **Scroll = time, linearly.** Scrubbed motion uses `ease: none`. Within a scene, a pixel of scroll is always the same slice of the day. Easing is reserved for UI (hover, menu, focus): 180–240 ms, `cubic-bezier(0.2, 0, 0, 1)`.
3. **Reveal like shade, never fade.** Text and UI appear behind a hard-edged mask that sweeps at the *current sun azimuth* of the scene. Headlines split by line (GSAP SplitText) and each line is uncovered by that shadow edge. Numbers roll to their value like an instrument settling.
4. **Every scene change happens through a shadow.** Camera travel, a shared frame, or a lattice-pattern morph. No section ever fades in as a separate rectangle.
5. **One ambient motion only: heat.** A real-time shimmer over sunlit ground, strongest near the horizon, and slow dust in light shafts. Both stop entirely under reduced motion.
6. **Pins declare their beats.** 700 px/beat desktop, 420 px/beat mobile (≤767), recomputed on resize; no pin longer than 1200 px/beat.
7. **Reduced motion is a designed state.** Still photographs at the key minutes, every word present, no pins, no scrub, no smooth scroll.

**Interaction map**

| element | behaviour | desktop | touch | reduced motion |
|---|---|---|---|---|
| first load | frame 0 of SEQ-B paints immediately from a small inline poster; the wordmark is uncovered by a shade edge at the scene's sun angle (0.9 s); shimmer starts | same | same | poster + text, no shimmer |
| scroll | scrubs the current sequence; HUD counts the minute | wheel smoothed by Lenis | native momentum | normal scroll through stills |
| headlines | SplitText lines uncovered by a hard shade edge moving at the sun's azimuth | same | same | visible, static |
| images | never "enter": they are the scene, or they arrive through a morph | same | same | static stills |
| links / buttons | underline is a thin shadow that slides in from the sun side (180 ms); copper focus ring 2 px | hover + focus | press state (copper) | colour change only |
| nav | wordmark + four items; on scroll the bar condenses to the HUD line | hover underline | full-screen menu, items uncovered by a shadow sweep | instant menu |
| cursor | system cursor everywhere except C, where it becomes a small sun ring | yes | n/a | system cursor |
| page transition | the eclipse (lattice-shadow mask), ~700 ms | View Transitions | same | instant |
| signature moments | A, B, C, D (§7) | — | touch versions in §7 | stills + captions |

Technology (lowest rung that delivers it): pre-rendered frame sequences on `<canvas>` via CSS `position: sticky` (native pinning, no pin-spacer thrash); **raw WebGL** (~3 kB) for every real-time effect, because each is a full-screen shader over pre-rendered buffers. **No three.js**: there is no real-time 3D camera or scene graph to justify 120 kB. If a later phase needs one it is lazy-loaded. **GSAP + ScrollTrigger + SplitText + DrawSVG + MorphSVG**, lazy-loaded after first paint, for text choreography, the Shade Study plot and the sun-path morph. **Lenis** on desktop wheel only (scrubbed frames judder on discrete wheel steps); never on touch, never under reduced motion. The site must still work with Lenis removed.

---

## 5. Page architecture

| page | purpose | notes |
|---|---|---|
| `/` **The Day** | the film: proof of the idea in 30 s, then the work | everything below in §6 |
| `/work/<slug>/` ×4 | case studies from one template: Ghaf House, Hotel Sikka, Qudra Canopy, Mushrif Reading Rooms | hero render at its signature hour, the project's own Shade Study plot, 2 detail renders, facts, *next project* through its shadow |
| `/shade-study/` | the method, and the standalone consultancy for developers and landscape architects | the full "you are the sun" instrument with a date control (not only 21 June), how a study is delivered, CTA "Commission a Shade Study" |
| `/studio/` | 31 people in Al Quoz, founders, services, contact | text portraits (no invented photos of people), the warehouse, map-free address block |

All content lives in `src/content/*.json` (studio, people, projects, services, copy strings, per-scene sun metadata), separate from components, so the calmer "premium" edition can be built from the same files.

**Page transitions (the eclipse).** Cross-document View Transitions (`@view-transition { navigation: auto }`, Chromium 126+ and Safari 18.2+): the outgoing page darkens *through* a lattice-shadow mask that sweeps across it, and the incoming page emerges through the same pattern; ~700 ms. Project → next project uses that project's own pattern. Browsers without support navigate instantly (no fake fade). Reduced motion: instant.

Navigation: wordmark left; `Work · Shade Study · Studio · Contact` right in Plex Mono caps. Mobile menu: full-screen Deep Shade sheet whose items are uncovered by a shadow sweep. The persistent fiction mark sits bottom-left in 11 px mono: *Fictional studio · design demonstration*.

---

## 6. Home storyboard (beat by beat)

Beat = 700 px desktop / 420 px mobile. Times are GST on 21 June 2026, from the solar algorithm.

| # | scene | beats | sun (time · alt · az) | on screen | copy (voice: short, precise) | how | boundary out |
|---|---|---|---|---|---|---|---|
| 0 | **Cold open** | — | 07:30 · 24.7° · 74.5° | the blazing east facade of the house, the gate a dark slot, the far wall's shadow band on the street; heat shimmer rising | **ZAWAL** / *Architecture measured in shade.* | SEQ-B frame 0 + live shimmer shader | the first scroll moves the camera |
| 1 | **The street** | 1.5 | 07:30→07:38 | camera walks north up the street, facade blazing | *Dubai. The longest day of the year.* | SEQ-B 0–36 | camera travel |
| 2 | **The gate** | 1 | 07:38→07:44 | camera turns into the recess; the only dark thing in the frame fills it | *The most valuable thing a building gives here is shade.* | SEQ-B 36–60 | camera travel |
| 3 | **The dahliz** | 1 | 07:44→07:52 | the bent passage, almost black, the eye adapting | definition, white on shade: *zawal* · زوال · *the moment the sun crosses its highest point, when shadows are shortest.* | SEQ-B 60–84 | camera turns in darkness |
| 4 | **Emergence** | 0.5 | 07:52→08:00 | the courtyard opens: rammed earth, the lattice canopy, the rill | — | SEQ-B 84–96; **last frame = SEQ-A frame 0** | identical frame |
| 5 | **A · morning** | 2 | 08:00→11:00 · 31°→72° | the canopy's stars stretched across the west wall slide down onto the floor | *08:00 — the canopy throws its pattern 1.65 m for every metre of height.* | SEQ-A 0–42, HUD counts live | continuous frames |
| 6 | **A · ZAWAL** | 1.5 incl. hold | 12:20:42 · 88.23° · 180° | the stars fall straight down onto the floor; walls lose their shadows; the frame holds | **12:20:42.** *Shadows are 3 cm long for every metre. We design every building for this hour.* | SEQ-A 42–70 (zawal held over 6 frames) | continuous frames |
| 7 | **A · afternoon → dusk** | 2 | 13:00→18:50 · 81°→4° | the pattern climbs the east wall and leaves; the far wall glows, then the courtyard goes blue | *The rest of the day is the building's job.* | SEQ-A 70–120 | continuous frames |
| 8 | **Hand-over** | 0.5 | 19:11 sunset | blue-hour courtyard; the pointer becomes a small sun | *The sun has set. Take it.* | SEQ-A 120 == C at dusk (same camera, same pixels) | identical frame |
| 9 | **C · You are the sun** | 2 pinned + free | the visitor's | the same courtyard relit live; the canopy shadow follows the pointer | HUD: *Your sun: 14 April, 09:12 — ALT 51° AZ 98°* (or *No day in Dubai has this sun.*) | real-time WebGL relighting | lattice-shadow morph |
| 10 | **The Shade Study** | 2 | 06:00→19:00 | the shadows a site gets on 21 June, plotted hour by hour, drawing themselves on Limewash Sand; the gaps become a building | *Every project starts with one drawing.* | SVG + DrawSVG, computed from the real sun | the plot's empty region opens as the first project's aperture |
| 11 | **D · Four projects** | 4 × 2 | each at its signature hour | full-screen hero; facts in the shade; morph to the next through its shadow pattern | Ghaf House · Hotel Sikka · Qudra Canopy · Mushrif Reading Rooms | real-time WebGL morph over rendered heroes + sun masks | full-screen shadow morph |
| 12 | **Method & services** | 1.5 | — | Mushrif's carved screen stays pinned while its holes close one by one into solid shade; that shade *is* the ground the services list is set on: Architecture · Interiors · Courtyards & landscape · Shade & daylight studies, each line uncovered by the shade edge | one sentence each | pinned hand-off (both states on screen), pattern threshold | shade-line sweep at the sun's angle |
| 13 | **Studio** | 1.5 | — | Leila Haddad and Tomas Ekberg as text portraits, 31 people, Al Quoz warehouse | *Architects who talk about light the way chefs talk about salt* (in their own words) | pinned hand-off | the page darkens with sunset |
| 14 | **Night · contact** | 1.5 | 20:30 | the courtyard at night: lanterns switch on one by one, the lattice now throws light *outward* | *Tell us about your site. We'll start with its shade.* studio@zawal.example · +971 4 000 0000 · Al Quoz, Dubai | pre-rendered light layers composited by scroll | footer |
| 15 | Footer | — | — | — | *ZAWAL is a fictional studio. This site is a design demonstration.* · *Made by Aarav Chopra* | — | — |

**First 10 seconds on a phone:** a photoreal street in hard morning sun with heat rising off it (moving before any input). The first thumb-scroll walks you up the street, and by the third flick you're turning into a dark doorway. **Within 30 s:** you have walked into a courtyard and watched shadows sweep a wall. Two interaction modes (scrubbed camera travel, scrubbed time), with a third (hold the sun) at about 45 s.

**Screen-recordable moments:** (1) the street-to-courtyard walk, 8–10 s; (2) zawal: stars dropping onto the floor, 6 s; (3) the pointer dragging the sun, 8 s; (4) any project morph, 4 s. Each works as a vertical clip.

---

## 7. Signature moments

### A · A day in one scroll
- **How:** 121-frame pre-rendered sequence, fixed camera in the courtyard, sun from 08:00 to 18:50 on the true path. The frame schedule is time-warped so the canopy shadow moves ≤ 25% of its pattern period per master frame (dense at the ends, where shadows race; sparse at midday), and zawal (12:20:42) is an exact frame, held. Scroll addresses frames; the HUD interpolates the minute between frames.
- **Real-time or pre-rendered:** **pre-rendered**. It is a function of scroll alone, and it needs path-traced bounce light (the warm glow on shaded walls *is* the story) that no phone can compute at 60 fps.
- **QA category:** 4 (scrubbed cinematic frame sequence).
- **Mobile / 320 px:** a separately composed **portrait** master (9:16, taller view: canopy overhead, stars on the floor below), 61 frames (every 2nd master frame) with the matching 61 ladder frames. At 320 px the same sequence, cover-fit; the HUD collapses to one line at the bottom.

### B · Street to courtyard
- **How:** 97-frame pre-rendered camera path: north up the street → turn into the gate recess → through the bent passage (the turn happens in darkness) → into the courtyard, ending on a frame **identical** to A's first frame. The walk is ~30 m and the clock runs 07:30 → 08:00 during it: a time-lapse walk, consistent with the whole page being one compressed day. (Alternative if you prefer literal time: the clock runs 07:30 → 07:32 and A starts at 07:32.) Exposure is authored along the path (eye adaptation from glare to shade), never automatic. Live heat shimmer sits over the street frames only.
- **Real-time or pre-rendered:** **pre-rendered**: fixed camera path, global illumination through a dark passage, a function of scroll alone.
- **QA category:** 1 (camera travel through a 3D environment), delivered as 4 (scrubbed frame sequence).
- **Mobile / 320 px:** portrait master of the same path (the street reads as a tall slot of light, the gate as a vertical dark door). 49 frames with the matching ladder frames.

### C · You are the sun
- **How:** at sunset the courtyard frame hands over to a **relightable** version of the same view. Offline we render a G-buffer (albedo, normal, world position, sky visibility) plus ~12 baked *indirect-light basis* images for sun directions spread over the sky. A raw-WebGL fragment shader computes direct sun per pixel: N·L, analytic ray casts against the courtyard's walls (a handful of boxes) and the canopy lattice. The lattice is the **same 2D pattern SDF as the renderer, ported line-for-line to GLSL**, so live shadows match offline ones. Penumbra comes from the SDF (`distance / (travel × tan 0.27°)`), and bounce light is interpolated from the nearest baked basis images. Pointer x → azimuth (left = east, as seen facing south), pointer y → altitude. The HUD inverts the solar equations and reports the *date and time* your sun belongs to in Dubai, or that it never happens.
- **Real-time or pre-rendered:** **real-time**. The output depends on the pointer, not scroll. Cost per pixel: 3 G-buffer reads, 4 basis reads, one short 2D SDF march, ≤ 6 box tests. That is a mid-range-Android-safe budget; to be verified with 4× CPU throttle and on a real device.
- **Tiers:** WebGL relight → **Canvas 2D** tracing the same behaviour (pointer picks the nearest of 24 pre-rendered sun positions and cross-dissolves; shadows still follow the pointer, stepped) → **static** dusk frame in the DOM.
- **Touch (never fights scroll):** the stage is `touch-action: pan-y`, so vertical swipes always scroll. **Horizontal drag moves the sun** along the real 21 June path; **tap** places it; idle, the sun follows scroll through the pin. An opt-in **Tilt** button (DeviceOrientation, iOS permission prompt on tap) turns the phone into a sundial.
- **QA category:** 3 (shader-based pointer interaction).
- **320 px:** portrait G-buffer; same shader; the canvas is capped at 1.5× DPR.

### D · Projects morph through the shadow
- **How:** each project hero is rendered at its signature hour with its **sun-visibility mask** (a renderer AOV: which pixels the sun actually reaches) and that mask's **signed distance field**. A raw-WebGL shader, driven by scroll: the outgoing project's lit patches *erode along its own shadow geometry* until only its pattern of light remains. The pattern's SDF **morphs into the next project's pattern** (star → palm slats → triangles → carved star), and the incoming project's lit patches bloom from that shape back to the full image. Full-screen, continuous, reversible, never a cross-fade.
- **Real-time or pre-rendered:** **real-time** (4 texture reads + arithmetic per pixel, trivially 60 fps). It is scroll-driven, but the rule's test is frame budget, and this passes easily, while pre-rendering would multiply download by the number of transitions.
- **Tiers:** WebGL → Canvas 2D (the same threshold morph on a ¼-res mask, composited with `destination-in`) → static heroes stacked.
- **QA category:** 5 (full-screen morph between sections).
- **320 px:** portrait heroes and masks; identical shader.

Supporting (not claimed as signatures): heat shimmer; shade-edge text reveals; the drawn Shade Study; the night lantern light-layer scrub.

---

## 8. Render plan

### Renderer (`renderer/`, C++17, written for this project)
- **Path tracer** on the CPU (OpenMP, 4 cores here): next-event estimation for the sun disc and the sky with MIS, GGX + Lambert BSDF, smooth dielectric water, Russian roulette, up to 6 bounces.
- **Sun:** NOAA/Meeus solar position for any lat/lon/minute (`src/sun.h`), with atmospheric refraction; the physical disc (0.533°), so penumbrae widen with distance exactly as they should. Verified: 21 June 2026 Dubai, zawal 12:20:42, alt 88.23°, sunrise 05:30, sunset 19:11.
- **Sky:** single-scattering spherical atmosphere (Rayleigh, dust-heavy Mie at AOD 0.38 with blue-absorbing dust, ozone) plus a multiple-scattering fill, baked per frame to an importance-sampled environment and an aerial-perspective table.
- **Geometry:** exact boxes with CSG cut-outs (openings, recesses) and **rounded arrises** from a smoothed SDF (no CG-sharp edges); **lattice slabs** that sphere-trace a 2D pattern SDF along the ray's in-plane path, so screen thickness really blocks low sun; smooth dune height fields; BVH.
- **Materials:** solid procedural textures: limewash (domain-warped clouding, crossed brush strokes, grain, dusty foot), rammed earth (warped courses of six earths, aggregate, lift lines, differential erosion), honed limestone (600 mm slabs, lippage, C2 joints), teak (rings, grain), aged brass, verdigris copper, water, sand ripples. Full detail on the first hit, a cheap equivalent after the first bounce.
- **Post:** Intel Open Image Denoise (Apache-2.0) with albedo/normal guides, physically motivated glare, AgX filmic, locked white balance, sub-LSB dither.
- **Shader-trap checklist, proven:** `docs/proof/shader-traps.png`. The renderer's own fields are differentiated next to deliberately broken controls: (1) no value-noise lattice (gradient noise, quintic fade, rotated 1.97× octaves, domain warp); (2) no flat-top falloff (compact `(1−d²/r²)³` kernel for the live shader; the offline sun is physical); (3) no clamp creases (smootherstep/smax/smooth-abs everywhere a field reaches a normal).

### Sequences

| id | content | master frames | aspect masters | time |
|---|---|---|---|---|
| SEQ-B | street → gate → dahliz → courtyard | 97 (odd: step-2 tiers keep first and last) | landscape 1600×900 · portrait 720×1280 | 07:30 → 08:00 |
| SEQ-A | the courtyard's day | 121 | landscape 1600×900 · portrait 720×1280 | 08:00 → 18:50, warped |

Render settings for scrubbing: no motion blur; **seed per pixel, not per frame** (noise stays still under a fixed camera instead of boiling); exposure and white balance authored per path, never automatic; 16-bit float masters, dithered on export; first/last frames are the hand-off frames.

### Per-breakpoint delivery (WebP, image sequences; ladder always resident)

| breakpoint | orientation | hi-res tier | frames A / B | step | window | ladder resident | peak resident | ceiling |
|---|---|---|---|---|---|---|---|---|
| ≤ 479 (320–430 phones) | portrait | 720×1280 (3.69 MB decoded) | 61 / 49 | 2 | ±5 (11 × 3.69 = 40.5 MB) | active seq, the 61 frames the tier addresses, 144×256 = 9.0 MB; inactive seq every 8th = 1.2 MB | **50.7 MB** | 60 |
| 480–767 | portrait | 720×1280 | 61 / 49 | 2 | ±5 | as above | **50.7 MB** | 60 |
| 768–1199 (tablets, small laptops) | by orientation | 1280×720 or 720×1280 (3.69 MB) | 121 / 97 | 1 | ±8 (62.7 MB) | active 121 × 320×180 = 27.9 MB; inactive every 8th = 3.0 MB | **93.6 MB** | 150 |
| 1200–1799 | landscape | 1600×900 (5.76 MB) | 121 / 97 | 1 | ±7 (86.4 MB) | 27.9 + 3.0 MB | **117.3 MB** | 150 |
| ≥ 1800 | landscape | 1600×900, scaled in canvas | 121 / 97 | 1 | ±7 | 27.9 + 3.0 MB | **117.3 MB** | 150 |

Frame counts are master-aligned: `(frames − 1) × step + 1 = master`, so 61 × step 2 → 121 and 49 × step 2 → 97, and a breakpoint change mid-scroll lands on the same minute. Only the active sequence holds its full ladder and window; the other keeps every 8th ladder frame so scrolling back is never blank. Bitmaps are `close()`d on eviction. Decode is `createImageBitmap` off the main thread. WebP over AVIF, because decode speed on a deadline beats bytes. Every tier is validated with `check-sequence.mjs` (contiguity, step alignment, memory).

**Download per breakpoint (home page, measured).** Per-frame sizes come from encoding the Phase 1 masters to WebP (q72 hi-res, q50 ladder). Courtyard frames measure 40–43 KB at 1600×900 and 30–32 KB at 1280×720; street frames 13 KB and 9 KB; ladder frames 1–4 KB. Totals below assume courtyard-weight frames throughout (a deliberate over-estimate: B is mostly street and dark passage, which compress 3× better). Everything streams progressively as you scroll, ladder first, and nothing but the hero poster is on the critical path.

| breakpoint | SEQ-A | SEQ-B | ladders | C buffers | D heroes + masks | night + posters | **total** |
|---|---|---|---|---|---|---|---|
| ≤ 767 phone (portrait 720×1280, step 2) | 61 × 36 KB = 2.2 MB | 49 × 36 KB = 1.8 MB | 110 × 3 KB = 0.3 MB | 0.8 MB | 0.4 MB | 0.3 MB | **≈ 5.8 MB** |
| 768–1199 (1280×720) | 121 × 32 KB = 3.9 MB | 97 × 32 KB = 3.1 MB | 218 × 4 KB = 0.9 MB | 1.0 MB | 0.5 MB | 0.4 MB | **≈ 9.8 MB** |
| ≥ 1200 (1600×900) | 121 × 43 KB = 5.2 MB | 97 × 43 KB = 4.2 MB | 0.9 MB | 1.2 MB | 0.6 MB | 0.5 MB | **≈ 12.6 MB** |

First load (critical path, excluding images and fonts) stays under the 100 kB gzip budget; fonts ≈ 90 kB subset woff2.

### Stills and buffers
- 4 project heroes × 2 aspects + sun masks + SDFs; 2 detail renders per project page.
- C: G-buffer + ~12 indirect basis images per aspect (low-res, half-float).
- Night courtyard: base + 3 lantern light layers per aspect.
- Posters for reduced motion / no-JS: the key minutes (07:30, 08:00, 12:20:42, 17:00, dusk) as `<img>` with alt text.

### Render budget (this machine: 4 CPU cores, no GPU)
Measured wall-clock cost per path sample on this 4-core container: courtyard ≈ 2.7 µs, street ≈ 1.4 µs, desert ≈ 18 µs (dunes + two lattices). A 1600×900 courtyard frame at 64 spp took 253 s.

| job | frames × pixels × spp | wall-clock |
|---|---|---|
| SEQ-A landscape | 121 × 1600×900 × 48 | ≈ 6.3 h |
| SEQ-A portrait | 121 × 720×1280 × 48 | ≈ 4.0 h |
| SEQ-B landscape | 97 × 1600×900 × 48 | ≈ 3.7 h |
| SEQ-B portrait | 97 × 720×1280 × 48 | ≈ 2.4 h |
| stills, masks, C buffers, night layers | — | ≈ 4 h |
| **total** | | **≈ 20 h at 48 spp · ≈ 14 h at 32 spp** |

Everything renders in the background while Phase 3 is built, and frames are validated as they land. Before committing to 32 spp I scrub-test a 12-frame slice for denoiser flicker. If the budget still slips, frame counts drop to 97 (A) / 73 (B) before resolution does.

---

## 9. Phase 1 proof frames: honest assessment

See `docs/phase1/` (landscape 1600×900 and portrait 900×1600, JPEG). Each `.json` beside a frame records the exact sun it was lit by.

| frame | sun (from `.json`) | what it proves |
|---|---|---|
| `street_0730` / `street_0730_p` | 07:30 · 24.66° · 74.46° | the blazing east facade, hit almost head-on; the far wall's shadow band; the gate as the only dark in the frame |
| `court_0800` | 08:00 · 31.23° · 76.77° | the canopy's stars stretched ~1.6× across the west wall; floor still in shade |
| `court_zawal` / `court_zawal_p` | 12:20:43 (zawal is 12:20:42.5) · 88.23° · 180.06° | zawal: the stars fall straight down onto the floor; walls lose their shadows and glow with bounce light from the floor |
| `court_1700` | 17:00 · 27.15° · 284.65° | the pattern has crossed to the east wall; the west wall now shades the floor |
| `qudra_1340` | 13:40 · 71.79° · 269.74° (Al Qudra, 24.84°N) | a 40 m roof whose shadow is a field of small light triangles; lace infill inside deep structural ribs |

### 9.1 Do these look like real architecture? My frank read

**Yes, as high-end architectural visualisation. Not yet as photographs.** An architect will read these as buildable buildings in real sun. A trained eye will still call them renders, for specific and fixable reasons.

What reads as real:
- **The light.** It is physically computed rather than art-directed: solar geometry to the minute, penumbrae that widen with distance, warm bounce filling the shade, a sky that is correctly milky with dust. The three courtyard frames show the sweep (west wall → floor → east wall) that the whole site depends on.
- **Material response in grazing light:** rammed-earth courses and aggregate, limewash clouding, the lattice's real thickness (a 12 cm canopy passed no sun at 17:00, so it was thinned to 5 cm, as a real screen maker would).
- **Architectural discipline:** level verticals, arrises softly rounded, plinth shadow lines, recessed teak lintels, copper spouts.

What still reads as CG, and the Phase 2 fix for each:
1. **Too clean.** No dust on horizontals, no staining under spouts and sills, no hairline cracks, no sand blown into corners. → a geometry-driven weathering pass (exposure, crevice and run-off masks).
2. **No life.** No plants, fabric or objects of use. → the foliage primitive (the ghaf tree, a bougainvillea over the street wall, a potted olive), majlis cushions and a rug in the loggia, a brass tray.
3. **Some objects are primitives.** The brass "lantern" is a sphere, the jar is a sphere on a cylinder, and the neighbours across the street are plain massing. → lathed profiles, and neighbour facades with openings and parapets.
4. **The copper pool reads as a mottled box** (orange flecks through the water). → a uniform deep patina.
5. **The limestone floor is too uniform** (it reads as porcelain). → stronger slab-to-slab variation and sand in the joints.
6. **The street ends in empty haze.** → continue the street wall and add a far skyline.

**Verdict on the biggest risk:** the renderer's light transport is not the weak link. The gap to photographic is dressing and weathering, which is scene work, and the render plan budgets time for it. If you want to see one of these fixes before approving, the fastest high-signal one is the weathering pass on the courtyard.

---

## 10. Honesty, SEO, fallbacks (unchanged from the brief, restated as build rules)
- Footer: *ZAWAL is a fictional studio. This site is a design demonstration.* · *Made by Aarav Chopra*. Persistent corner mark on every page.
- `<meta name="robots" content="noindex, nofollow">` on every page; no structured data; no real clients, awards, developers, people or buildings.
- WebGL capability probe (silent, `failIfMajorPerformanceCaveat`, context released) → Canvas 2D tier → static frame in the DOM.
- Reduced motion: every sequence becomes its key stills with full captions; no pins; no Lenis.
- No JS: the prerendered page is a readable long-form article with the poster frames and all copy.
- First load < 100 kB gzip (HTML + CSS + entry JS). GSAP loads by dynamic import after first paint; no three.js.

## 11. Biggest risks
1. **Render realism** (the brief's biggest risk). See §9.1 for the frank read.
2. **Vegetation.** The Ghaf House needs a convincing old ghaf tree. Leaves are the hardest thing for a procedural renderer. Plan: a leaf-cell foliage primitive (3D-DDA through cells holding tiny leaflets, so dappled light is made of real sun-spots). If it fails my own review, the Ghaf House hero is framed from inside the house with the tree as a dappled shadow on limewash, which is honest and still beautiful.
3. **C next to A.** The live relight sits in the same frame as the path-traced day, so any quality drop is visible. The baked indirect basis exists to prevent that; the hand-off happens at dusk, where bounce light is weakest.
4. **Phone weight and memory.** About 6 MB of imagery on phones (measured, §8), streamed progressively; iOS Safari memory is respected per the ladder+window budget. Real-phone testing is still required (see the "only a real phone can prove" list at delivery).
5. **Render time** (≈ 14–20 h of wall-clock on this 4-core container). Mitigated by background rendering during the build, with the spp and frame-count fallbacks above.
