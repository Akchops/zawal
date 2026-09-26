// The home film: the walk (B) and the day (A) on one pinned stage, then the
// scenes that follow. Everything is a function of the beat (scroll.js).
import { SequencePlayer } from "./player.js";
import { mountFilm, scene, everyFrame, smoothWheel, beatNow } from "./scroll.js";
import { solarPosition, clock } from "./solar.js";

const html = document.documentElement;
const film = document.querySelector("[data-film]");
const motion = html.classList.contains("js") && !html.classList.contains("rm");

if (film && motion) start();

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

// Shade-edge reveal: a hard, slanted edge that uncovers an element and later
// covers it again, travelling from the sun's side. a = trailing edge, b =
// leading edge, both 0..1 across the element; k = slant from sun altitude.
function shadeClip(el, a, b, k, fromLeft) {
  const X = (t) => (fromLeft ? t : 1 - t) * 140 - 20;
  const s = fromLeft ? 1 : -1;
  const pa = X(a), pb = X(b);
  el.style.clipPath = `polygon(${pa + s * k}% -25%, ${pb + s * k}% -25%, ${pb - s * k}% 125%, ${pa - s * k}% 125%)`;
}

async function start() {
  const beats = +film.dataset.beats;
  mountFilm(film, beats);
  const stage = film.querySelector("[data-stage]");
  const cvB = stage.querySelector('[data-seq="b"]');
  const cvA = stage.querySelector('[data-seq="a"]');
  const first = stage.querySelector(".first");
  const t = tier();
  const restB = t === "p"
    ? [{ index: 0, url: `/img/hero/b000-p${devicePixelRatio > 2.2 ? 1440 : devicePixelRatio > 1.4 ? 1080 : 720}.webp` }]
    : [{ index: 0, url: `/img/hero/b000-l${innerWidth * devicePixelRatio > 1900 ? 2560 : 1600}.webp` }];

  // Frame metadata comes with the bundle index; fetch both indexes first.
  const [ib, ia] = await Promise.all([fetch(`/seq/b-${t}.json`).then((r) => r.json()), fetch(`/seq/a-${t}.json`).then((r) => r.json())]);
  const mk = (canvas, idx, base, mode, rest) => new SequencePlayer({
    canvas, base, mode, rest, aspect: idx.size[0] / idx.size[1],
    frames: idx.frames.map((f) => ({ cam: f.cam, minutes: f.m })),
  });
  const pb = mk(cvB, ib, `/seq/b-${t}`, "camera", restB);
  const pa = mk(cvA, ia, `/seq/a-${t}`, "time", []);
  const zawal = ia.zawal ?? 48;
  const nA = ia.frames.length, nB = ib.frames.length;
  // Beat -> frame. Walk: street 0-1.5, gate -1.5-2.5, passage 2.5-3.5,
  // emergence 3.5-4. Day: morning to zawal 4-6, the hold 6-7.5 (six equal
  // frames), afternoon to dusk 7.5-9.5, hand-over 9.5-10.
  const mapB = [[0, 0], [1.5, 36], [2.5, 60], [3.5, 84], [4, nB - 1]];
  const mapA = [[4, 0], [6, zawal], [7.5, zawal + 6], [9.5, nA - 1], [10, nA - 1]];

  const hud = {
    time: stage.querySelector("[data-hud-time]"), alt: stage.querySelector("[data-hud-alt]"),
    az: stage.querySelector("[data-hud-az]"), shadow: stage.querySelector("[data-hud-shadow]"),
  };
  let sun = { alt: 24.66, az: 74.46 };
  const setHud = (minutes) => {
    sun = solarPosition(2026, 6, 21, minutes);
    hud.time.textContent = clock(minutes, true);
    hud.alt.textContent = `${sun.alt.toFixed(2)}°`;
    hud.az.textContent = `${sun.az.toFixed(1)}°`;
    hud.shadow.textContent = sun.alt > 0.5 ? `shadow ${(1 / Math.tan((sun.alt * Math.PI) / 180)).toFixed(2)} × h` : "sun down";
  };
  const minutesAt = (idx, f) => {
    const i0 = Math.floor(f), i1 = Math.min(i0 + 1, idx.frames.length - 1);
    return idx.frames[i0].m + (idx.frames[i1].m - idx.frames[i0].m) * (f - i0);
  };

  let showA = false;
  scene(0, 4, (p, b) => {
    const f = pw(mapB, b);
    pb.set(f);
    pb.shimmer = 1 - Math.min(1, Math.max(0, (f - 26) / 20));
    if (b < 4) setHud(minutesAt(ib, f));
  });
  scene(4, 10, (p, b) => {
    const f = pw(mapA, b);
    pa.set(f);
    setHud(minutesAt(ia, f));
  });
  // Which canvas is on: B's last frame is A's first, so the swap is invisible.
  scene(0, beats, (p, b) => {
    const wantA = b >= 4;
    if (wantA !== showA) {
      showA = wantA;
      cvA.hidden = !wantA;
      cvB.hidden = wantA;
    }
  });

  // Captions: each shot's lines are uncovered by the shade edge over the
  // first part of its range and covered again at the end.
  for (const shot of stage.querySelectorAll("[data-shot]")) {
    const from = +shot.dataset.from, to = +shot.dataset.to;
    const lines = [...shot.querySelectorAll("[data-reveal], [data-hint]")];
    const fromLeft = +(shot.dataset.sun || 90) < 180;   // morning sun from the east = screen left here
    const span = to - from;
    scene(from - 0.05, to + 0.05, (p, b) => {
      const on = b > from - 0.02 && b < to + 0.02;
      shot.classList.toggle("on", on);
      const k = Math.max(4, Math.min(22, 10 / Math.tan((Math.max(sun.alt, 5) * Math.PI) / 180)));
      lines.forEach((el, i) => {
        const inStart = from + span * (0.02 + 0.06 * i), inEnd = inStart + span * 0.2;
        const outStart = to - span * (0.2 - 0.03 * i), outEnd = outStart + span * 0.14;
        const bIn = Math.max(0, Math.min(1, (b - inStart) / (inEnd - inStart)));
        const aOut = Math.max(0, Math.min(1, (b - outStart) / (outEnd - outStart)));
        const first = from <= 0.001;                      // the cold open starts uncovered
        shadeClip(el, aOut, first ? 1 : bIn, k, fromLeft);
      });
    });
  }

  // Draw loop: players redraw when their frame changes; the shimmer animates.
  everyFrame((time, b) => {
    const active = b < 4 ? pb : pa;
    active.draw(time);
    return active === pb && pb.shimmer > 0;
  });
  addEventListener("resize", () => { pb.resize(); pa.resize(); pb.dirty = pa.dirty = true; });

  // The cold open's wordmark is uncovered by a shade edge at the scene's sun
  // angle on arrival (0.9 s), the one time-based reveal on the page.
  const cold = stage.querySelector(".shot-cold");
  cold.classList.add("on");
  const coldLines = [...cold.querySelectorAll("[data-reveal]")];
  const t0 = performance.now();
  const intro = (now) => {
    const u = Math.min(1, (now - t0) / 900);
    coldLines.forEach((el, i) => shadeClip(el, 0, Math.max(0, Math.min(1, u * 1.4 - i * 0.2)), 14, true));
    if (u < 1 && beatNow() < 0.05) requestAnimationFrame(intro);
  };
  requestAnimationFrame(intro);

  pb.onFirstDraw(() => { first.style.visibility = "hidden"; });
  pb.start().catch(() => {});
  // The day's bundles stream after the walk's have started.
  setTimeout(() => pa.start().catch(() => {}), 1200);
  requestAnimationFrame(() => setTimeout(smoothWheel, 0));
}
