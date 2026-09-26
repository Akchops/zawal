// The home film. One sticky stage; every scene is a function of the beat
// (scroll.js), so scrolling back replays everything exactly, and nothing
// here runs on a timer except the heat shimmer and the cold open.
//
//   0-4     the walk (SEQ-B, camera travel)           signature B
//   4-9.5   the day (SEQ-A, the sun moves)             signature A
//   9.5-10  hand-over: the same frame, relit live
//   10-12   you are the sun (relight, pointer/touch)   signature C
//   11.7-14 the Shade Study, drawn on sand; its gap opens as an aperture
//   14-22   four projects, morphing through their shadows  signature D
//   22-23.5 the carved screen closes into shade: services
//   23.4-25 the studio, uncovered by a shade line; the page darkens with sunset
//   25-26.5 night: the lanterns switch on one by one; contact
import { SequencePlayer } from "./player.js";
import { Compositor } from "./compositor.js";
import { SunStage, sunFromPointer, sunOnJune21, describeSun } from "./sun.js";
import { mountFilm, scene, everyFrame, smoothWheel, beatNow } from "./scroll.js";
import { solarPosition, clock } from "./solar.js";
import { studyDriver, markCentre } from "./study.js";
import { url } from "./paths.js";

const html = document.documentElement;
const film = document.querySelector("[data-film]");
const reduced = html.classList.contains("rm");


function tier() {
  const portrait = innerHeight > innerWidth * 1.05;
  if (portrait) return "p";
  return innerWidth * Math.min(devicePixelRatio || 1, 2) > 1400 ? "l" : "t";
}

/** Piecewise-linear map through [[x, y], ...] (x ascending). */
function pw(points, x) {
  if (x <= points[0][0]) return points[0][1];
  for (let i = 1; i < points.length; i++) {
    const [x1, y1] = points[i], [x0, y0] = points[i - 1];
    if (x <= x1) return y0 + ((y1 - y0) * (x - x0)) / (x1 - x0);
  }
  return points[points.length - 1][1];
}
const clamp01 = (x) => Math.max(0, Math.min(1, x));
const ramp = (b, a, z) => clamp01((b - a) / (z - a));
const ease = (x) => x * x * (3 - 2 * x);

// Shade-edge reveal: a hard, slanted edge that uncovers an element and later
// covers it again, travelling from the sun's side. a = trailing edge, b =
// leading edge, both 0..1 across the element; k = slant from sun altitude.
function shadeClip(el, a, b, k, fromLeft) {
  if (b >= 1 && a <= 0) { el.style.clipPath = ""; return; }
  const X = (t) => (fromLeft ? t : 1 - t) * 140 - 20;
  const s = fromLeft ? 1 : -1;
  const pa = X(a), pb = X(b);
  el.style.clipPath = `polygon(${pa + s * k}% -25%, ${pb + s * k}% -25%, ${pb - s * k}% 125%, ${pa - s * k}% 125%)`;
}

const getJSON = (p) => fetch(url(p)).then((r) => { if (!r.ok) throw new Error(p); return r.json(); });

async function start() {
  const beats = +film.dataset.beats;
  mountFilm(film, beats);
  const stage = film.querySelector("[data-stage]");
  const cvB = stage.querySelector('[data-seq="b"]');
  const cvA = stage.querySelector('[data-seq="a"]');
  const cvSun = stage.querySelector("[data-sun]");
  const cvComp = stage.querySelector("[data-comp]");
  const first = stage.querySelector(".first");
  const studyL = stage.querySelector('[data-layer="study"]');
  const studioL = stage.querySelector('[data-layer="studio"]');
  const dusk = stage.querySelector("[data-dusk]");
  const hudEl = stage.querySelector("[data-hud]");
  const t = tier();
  const aspect = t === "p" ? "p" : "l";
  const dpr = devicePixelRatio || 1;
  const restB = t === "p"
    ? [{ index: 0, url: `/img/hero/b000-p${dpr > 2.2 ? 1440 : dpr > 1.4 ? 1080 : 720}.webp` }]
    : [{ index: 0, url: `/img/hero/b000-l${innerWidth * dpr > 1900 ? 2560 : 1600}.webp` }];

  // ---------------- HUD ----------------
  const hud = {
    date: stage.querySelector("[data-hud-date]"), time: stage.querySelector("[data-hud-time]"),
    alt: stage.querySelector("[data-hud-alt]"), az: stage.querySelector("[data-hud-az]"), shadow: stage.querySelector("[data-hud-shadow]"),
  };
  let sunAlt = 24.66;
  const hudLast = {};
  const setText = (k, v) => { if (hudLast[k] !== v) { hudLast[k] = v; hud[k].textContent = v; } };
  const hudSun = (date, time, alt, az) => {
    sunAlt = alt;
    setText("date", date);
    setText("time", time);
    setText("alt", `${alt.toFixed(alt < 10 && alt > -10 ? 2 : 1)}°`);
    setText("az", `${az.toFixed(1)}°`);
    setText("shadow", alt > 0.5 ? `shadow ${(1 / Math.tan((alt * Math.PI) / 180)).toFixed(2)} × h` : "sun down");
  };
  const hudMinutes = (m) => { const s = solarPosition(2026, 6, 21, m); hudSun("21·06", clock(m, true), s.alt, s.az); };
  const hudOff = (off) => hudEl.classList.toggle("off", off);

  // ---------------- walk and day ----------------
  const [ib, ia] = await Promise.all([getJSON(`/seq/b-${t}.json`), getJSON(`/seq/a-${t}.json`)]);
  const mk = (canvas, idx, base, mode, rest) => {
    const p = new SequencePlayer({ canvas, base, mode, rest, aspect: idx.size[0] / idx.size[1], frames: idx.frames.map((f) => ({ cam: f.cam, minutes: f.m })) });
    p.index = idx;
    return p;
  };
  const pb = mk(cvB, ib, `/seq/b-${t}`, "camera", restB);
  const pa = mk(cvA, ia, `/seq/a-${t}`, "time", []);
  const zawal = ia.zawal ?? 48;
  const nA = ia.frames.length, nB = ib.frames.length;
  const mapB = [[0, 0], [1.5, 36], [2.5, 60], [3.5, 84], [4, nB - 1]];
  const mapA = [[4, 0], [6, zawal], [7.5, zawal + 6], [9.5, nA - 1]];
  const minutesAt = (idx, f) => {
    const i0 = Math.floor(f), i1 = Math.min(i0 + 1, idx.frames.length - 1);
    return idx.frames[i0].m + (idx.frames[i1].m - idx.frames[i0].m) * (f - i0);
  };

  // ---------------- C: the live sun ----------------
  const sunStage = new SunStage({ canvas: cvSun, base: `/sun/sun-${aspect}.json` });
  let sunOK = null;                         // null = not tried, true/false
  const sunWhen = stage.querySelector("[data-sun-when]");
  const touch = matchMedia("(pointer: coarse)").matches;
  if (touch) {
    stage.querySelector("[data-sun-hint-pointer]").hidden = true;
    stage.querySelector("[data-sun-hint-touch]").hidden = false;
  }
  let user = null, userBeat = -1, sunNow = null, blendFrom = null, blendT = 1;
  const inC = (b) => b >= 9.9 && b < 12.1;
  const placeSun = (s) => { user = s; userBeat = beatNow(); blendT = 1; };
  // Idle, the sun rewinds across the sky with the scroll: from the hand-over
  // (the day's last frame) back to just after sunrise.
  const xHand = (ia.frames[nA - 1].m - 332) / (1149 - 332);
  const idleSun = (b) => { const r = ramp(b, 9.9, 11.9); return sunOnJune21(xHand * (1 - r) + 0.02 * r); };
  const applySun = (s) => {
    sunNow = s;
    // A sun on the 21 June path is named by its own clock; any other sun by
    // the day and time in Dubai that has it (or none).
    const d = s.minutes != null ? { never: false, when: `21 June, ${clock(s.minutes)}` } : describeSun(s.alt, s.az);
    const text = d.never ? "No day in Dubai has this sun." : `Your sun: ${d.when}`;
    if (sunWhen.textContent !== text) sunWhen.textContent = text;
    hudSun("YOUR SUN", d.never ? "—" : d.when.split(", ").pop(), s.alt, s.az);
    if (sunOK) sunStage.setSun(s.alt, s.az);
    else if (sunOK === false && s.minutes != null) {
      // Canvas 2D / no relight: the day's own frames, nearest to that minute.
      let f = 0;
      for (let i = 0; i < nA; i++) if (ia.frames[i].m <= s.minutes) f = i;
      const m0 = ia.frames[f].m, m1 = ia.frames[Math.min(f + 1, nA - 1)].m;
      pa.set(f + (m1 > m0 ? clamp01((s.minutes - m0) / (m1 - m0)) : 0));
    }
  };
  const onPointer = (e) => {
    const b = beatNow();
    if (!inC(b)) return;
    const r = stage.getBoundingClientRect();
    const x = clamp01((e.clientX - r.left) / r.width), y = clamp01((e.clientY - r.top) / r.height);
    if (e.pointerType === "mouse") placeSun(sunFromPointer(x, y));
    else if (e.type === "pointermove" && dragging) placeSun(sunOnJune21(x));
    else if (e.type === "pointerup" && !moved) placeSun(sunFromPointer(x, y));
  };
  let dragging = false, moved = false, downX = 0, downY = 0;
  stage.style.touchAction = "pan-y";
  stage.addEventListener("pointermove", (e) => {
    if (e.pointerType !== "mouse" && dragging) {
      if (Math.abs(e.clientX - downX) > 8 && Math.abs(e.clientX - downX) > Math.abs(e.clientY - downY)) moved = true;
      if (!moved) return;
    }
    onPointer(e);
  }, { passive: true });
  stage.addEventListener("pointerdown", (e) => { if (e.pointerType !== "mouse") { dragging = true; moved = false; downX = e.clientX; downY = e.clientY; } }, { passive: true });
  stage.addEventListener("pointerup", (e) => { if (e.pointerType !== "mouse") { onPointer(e); dragging = false; } }, { passive: true });
  stage.addEventListener("pointercancel", () => { dragging = false; }, { passive: true });
  // Tilt: an opt-in sundial (iOS asks permission on the tap).
  const tiltBtn = stage.querySelector("[data-sun-tilt]");
  if (touch && "DeviceOrientationEvent" in window) {
    tiltBtn.hidden = false;
    tiltBtn.addEventListener("click", async () => {
      try {
        const need = DeviceOrientationEvent.requestPermission;
        if (need && (await need.call(DeviceOrientationEvent)) !== "granted") return;
        tiltBtn.textContent = "Tilt: on";
        addEventListener("deviceorientation", (e) => {
          if (!inC(beatNow()) || e.beta == null) return;
          const alt = Math.max(2, Math.min(89, 90 - Math.abs(e.beta)));
          const az = 180 + Math.max(-110, Math.min(110, (e.gamma || 0) * 2.2));
          placeSun({ alt, az });
        });
      } catch { /* no sensor: the button stays inert */ }
    });
  }

  // ---------------- the Study ----------------
  const studySvgEl = studyL.querySelector("svg");
  const drive = studySvgEl ? studyDriver(studySvgEl) : () => {};
  const plot = studyL.querySelector("[data-study]");
  const studyText = studyL.querySelector(".study-text");
  let apertureOrigin = null;
  const measureAperture = () => {
    const c = markCentre(studySvgEl);
    if (!c) return;
    // transform-origin is in the plot element's own box; the scale is the one
    // at which the footprint's rectangle just covers the whole stage.
    const vb = studySvgEl.viewBox.baseVal, r = studySvgEl.getBoundingClientRect(), P = plot.getBoundingClientRect(), S = stage.getBoundingClientRect();
    const k = Math.min(r.width / vb.width, r.height / vb.height);
    const x0 = r.left + (r.width - vb.width * k) / 2, y0 = r.top + (r.height - vb.height * k) / 2;
    const cx = x0 + (c.x - vb.x) * k, cy = y0 + (c.y - vb.y) * k, w = c.w * k, h = c.h * k;
    const kMax = 1.04 * Math.max(2 * Math.max(cx - S.left, S.right - cx) / w, 2 * Math.max(cy - S.top, S.bottom - cy) / h);
    apertureOrigin = { x: cx - P.left, y: cy - P.top, kMax };
  };

  // ---------------- D, method, night: the compositor ----------------
  const comp = new Compositor(cvComp);
  const slugs = [...stage.querySelectorAll("[data-project]")].map((el) => el.dataset.project);
  const projectsMeta = slugs.map((slug) => {
    const el = stage.querySelector(`[data-project="${slug}"]`);
    return { slug, from: +el.dataset.from, minutes: 0, el };
  });
  const heroW = () => {
    const w = cvComp.getBoundingClientRect().width * Math.min(dpr, 1.5);
    return aspect === "p" ? (w > 760 ? 1080 : 720) : (w > 1400 ? 1920 : 1280);
  };
  const hero = (i) => `/img/projects/${slugs[i]}-${aspect}${heroW()}.webp`;
  const mask = (i) => `/img/projects/${slugs[i]}-${aspect}-sun.webp`;
  const night = (k) => `/night/night-${aspect}-${k}.webp`;
  let compLoading = false;
  const preloadComp = () => {
    if (compLoading) return;
    compLoading = true;
    slugs.forEach((_, i) => { comp.load(hero(i)); comp.load(mask(i), true); });
    [0, 1, 2, 3].forEach((k) => comp.load(night(k)));
  };
  const projectHud = [
    { date: "GHAF HOUSE", m: 970 }, { date: "HOTEL SIKKA", m: 670 },
    { date: "QUDRA CANOPY", m: 820, site: { lat: 24.84, lon: 55.37, tz: 4 } }, { date: "MUSHRIF", m: 1085 },
  ];

  // ---------------- scenes ----------------
  scene(0, 4, (p, b) => {
    const f = pw(mapB, b);
    pb.set(f);
    pb.shimmer = 1 - clamp01((f - 26) / 20);
    if (b < 4) hudMinutes(minutesAt(ib, f));
  });
  scene(4, 10, (p, b) => {
    const f = pw(mapA, b);
    if (!(inC(b) && sunOK === false)) pa.set(f);
    if (b < 9.9) hudMinutes(minutesAt(ia, f));
  });
  scene(9.5, 12.4, (p, b, inside) => {
    // Hand-over: the relight fades up over the day's identical last frame.
    if (b >= 9.5 && sunOK === null) {
      sunOK = false;
      sunStage.start().then((ok) => { sunOK = ok; if (ok) { sunStage.resize(); applySun(sunNow || idleSun(beatNow())); } }).catch(() => { sunOK = false; });
    }
    cvSun.style.opacity = String(ease(ramp(b, 9.55, 10.0)));
    if (!inside) return;
    if (b >= 9.9) {
      const idle = idleSun(b);
      if (user && Math.abs(b - userBeat) > 0.25) { blendFrom = user; blendT = 0; user = null; }
      if (user) applySun(user);
      else if (blendFrom && blendT < 1) {
        blendT = Math.min(1, blendT + 0.08);
        const k = ease(blendT);
        applySun({ alt: blendFrom.alt + (idle.alt - blendFrom.alt) * k, az: blendFrom.az + (idle.az - blendFrom.az) * k, minutes: idle.minutes });
      } else applySun(idle);
    }
  });
  scene(11.6, 14.4, (p, b) => {
    // In through the canopy's shadow, drawn hour by hour, out through its gap.
    const inM = ramp(b, 11.7, 12.2);
    const m = `${(100 - 100 * inM).toFixed(2)}% 0`;
    if (inM < 1) {
      studyL.style.webkitMaskImage = studyL.style.maskImage = `url("${url("/img/eclipse-mask.svg")}")`;
      studyL.style.webkitMaskSize = studyL.style.maskSize = "auto 100%";
      studyL.style.webkitMaskRepeat = studyL.style.maskRepeat = "no-repeat";
      studyL.style.webkitMaskPosition = studyL.style.maskPosition = m;
    } else studyL.style.webkitMaskImage = studyL.style.maskImage = "none";
    drive(ramp(b, 12.05, 13.55));
    hudSun("21·06", `${String(6 + Math.min(13, Math.floor(ramp(b, 12.05, 13.3) * 13.99))).padStart(2, "0")}:00`, 60, 180);
    setText("alt", "06:00 → 19:00"); setText("az", "JUMEIRAH"); setText("shadow", "the Shade Study");
    const ap = ramp(b, 13.6, 14.3);
    if (ap > 0 && !apertureOrigin) measureAperture();
    if (apertureOrigin) {
      const k = Math.exp(Math.log(apertureOrigin.kMax) * ease(ap));   // even growth, in log scale
      plot.style.transformOrigin = `${apertureOrigin.x}px ${apertureOrigin.y}px`;
      plot.style.transform = ap > 0 ? `scale(${k})` : "";
    }
    // The drawing lifts off the sheet; only the footprint's hole remains, and grows.
    const ink = studySvgEl?.querySelector(".ink");
    if (ink) ink.style.opacity = String(1 - ramp(b, 13.58, 13.78));
    studyText.style.opacity = String(1 - ramp(b, 13.55, 13.8));
  });
  scene(13.4, 26.6, (p, b, inside) => {
    if (!inside) return;
    // D: each project holds, then morphs into the next through its shadow.
    if (b < 22) {
      const i = Math.max(0, Math.min(3, Math.floor((b - 14) / 2)));
      const u = b - (14 + 2 * i);
      if (i < 3 && u > 1.3) comp.set({ mode: "morph", a: hero(i), b: hero(i + 1), ma: mask(i), mb: mask(i + 1), t: clamp01((u - 1.3) / 0.7) });
      else comp.set({ mode: "morph", a: hero(i), b: hero(i), ma: mask(i), mb: mask(i), t: 0 });
      const ph = projectHud[i];
      const s = solarPosition(2026, 6, 21, ph.m, ph.site);
      hudSun(ph.date, clock(ph.m), s.alt, s.az);
      return;
    }
    if (b < 25.3) {
      comp.set({ mode: "close", a: hero(3), t: ramp(b, 22.0, 22.95) });
      return;
    }
    // Night: the dark courtyard emerges, then the lanterns, one by one.
    const k = ramp(b, 25.6, 26.3) * 3, i = Math.min(2, Math.floor(k));
    comp.set({ mode: "fade", a: night(i), b: night(i + 1), t: k - i, gain: ease(ramp(b, 25.3, 25.62)) });
    const s = solarPosition(2026, 6, 21, 1230);
    hudSun("21·06", "20:30:00", s.alt, s.az);
  });
  scene(23.3, 25.5, (p, b) => {
    // The studio is uncovered by a shade line at the sun's angle.
    const e = ramp(b, 23.4, 23.9);
    const k = 14;
    studioL.style.clipPath = e >= 1 ? "" : `polygon(-30% 0, ${e * 160 - 30 + k}% 0, ${e * 160 - 30 - k}% 100%, -30% 100%)`;
    // Sunset: the sheet darkens to the shade colour.
    const d = ramp(b, 24.75, 25.3);
    dusk.style.visibility = d > 0 && b < 25.32 ? "visible" : "hidden";
    dusk.style.opacity = String(ease(d));
    dusk.style.background = `linear-gradient(180deg, rgba(15,20,24,${Math.min(1, d * 1.6)}) 0%, rgba(${Math.round(15 + 90 * (1 - d))},${Math.round(20 + 40 * (1 - d))},${Math.round(24 + 30 * (1 - d))},1) 100%)`;
  });

  // Which layers are on: the stage shows at most two at a time.
  const show = (el, on) => { if (el.hidden === on) el.hidden = !on; };
  scene(0, beats, (p, b) => {
    show(cvB, b < 4);
    show(cvA, (b >= 4 && b < 10.2) || (inC(b) && sunOK === false));
    show(cvSun, b >= 9.5 && b < 12.4 && sunOK !== false);
    show(studyL, b >= 11.6 && b < 14.35);
    show(cvComp, b >= 13.4);
    show(studioL, b >= 23.35 && b < 25.35);
    if (b > 8.5) preloadComp();
    hudOff(b > 22 && b < 25.3);
  });

  // ---------------- captions ----------------
  for (const shot of stage.querySelectorAll("[data-shot]")) {
    const from = +shot.dataset.from, to = +shot.dataset.to;
    const lines = [...shot.querySelectorAll("[data-reveal], [data-hint]")];
    const fromLeft = +(shot.dataset.sun || 90) < 180;   // morning sun from the east = screen left here
    const span = to - from;
    const firstShot = from <= 0.001;
    scene(from - 0.05, to + 0.05, (p, b) => {
      const on = b > from - 0.02 && b < to + 0.02;
      shot.classList.toggle("on", on);
      const k = Math.max(4, Math.min(22, 10 / Math.tan((Math.max(sunAlt, 5) * Math.PI) / 180)));
      lines.forEach((el, i) => {
        const inStart = from + span * (0.02 + 0.05 * i), inEnd = inStart + Math.min(0.5, span * 0.2);
        const outStart = to - span * (0.2 - 0.02 * i), outEnd = outStart + span * 0.14;
        const bIn = clamp01((b - inStart) / (inEnd - inStart));
        const aOut = clamp01((b - outStart) / (outEnd - outStart));
        shadeClip(el, aOut, firstShot ? 1 : bIn, k, fromLeft);
      });
    });
  }

  // ---------------- drawing ----------------
  everyFrame((time, b) => {
    if (b < 4) pb.draw(time);
    else if (b < 10.2 || (inC(b) && sunOK === false)) pa.draw(time);
    if (b >= 9.5 && b < 12.4 && sunOK) sunStage.draw();
    if (b >= 13.4) comp.draw();
    let again = b < 4 && pb.shimmer > 0;
    if (b >= 13.4 && comp.dirty) again = true;          // images still arriving
    if (b >= 9.9 && b < 12.4 && blendT < 1) again = true;
    return again;
  });
  addEventListener("resize", () => {
    pb.resize(); pa.resize(); pb.dirty = pa.dirty = true;
    sunStage.dirty = true; comp.dirty = true; apertureOrigin = null;
  });

  // The street's sharp still replaces the first-paint picture as soon as it
  // is drawn; the walk's bundles stream behind it, the day's a moment later.
  pb.onFirstDraw(() => { first.style.visibility = "hidden"; });
  pb.start().catch(() => {});
  setTimeout(() => pa.start().catch(() => {}), 1500);
  requestAnimationFrame(() => setTimeout(smoothWheel, 0));
}

// Reduced motion: the film is an article; the one thing kept live is the
// sun, because it moves only when the reader moves it.
async function startReduced() {
  const host = film.querySelector("[data-sun-inline]");
  if (!host) return;
  const canvas = document.createElement("canvas");
  canvas.setAttribute("aria-label", "The courtyard, relit for the sun you choose");
  canvas.setAttribute("role", "img");
  host.appendChild(canvas);
  const portrait = innerHeight > innerWidth * 1.05;
  const stage = new SunStage({ canvas, base: `/sun/sun-${portrait ? "p" : "l"}.json` });
  const when = film.querySelector("[data-sun-when]");
  const ok = await stage.start().catch(() => false);
  if (!ok) { host.remove(); return; }
  const apply = (s) => {
    stage.setSun(s.alt, s.az);
    const d = describeSun(s.alt, s.az);
    when.textContent = d.never ? "No day in Dubai has this sun." : `Your sun: ${d.when}`;
    stage.draw();
  };
  const at = (e) => {
    const r = canvas.getBoundingClientRect();
    return [clamp01((e.clientX - r.left) / r.width), clamp01((e.clientY - r.top) / r.height)];
  };
  canvas.addEventListener("pointermove", (e) => { if (e.pointerType === "mouse") apply(sunFromPointer(...at(e))); });
  canvas.addEventListener("pointerup", (e) => { if (e.pointerType !== "mouse") apply(sunFromPointer(...at(e))); });
  apply(stage.sun);
  addEventListener("resize", () => { stage.dirty = true; stage.draw(); });
}

// Last, so every helper above is initialised before the film starts.
if (film && !reduced) start().catch((e) => console.warn("film", e));
else if (film && reduced) startReduced();
