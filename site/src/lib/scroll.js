// The home page's clock. One tall section, one sticky stage; scroll position
// inside the section is measured in beats (700 px desktop, 420 px phones, as
// DIRECTION.md §4 fixes), and every scene reads the same beat value. Scrubbed
// motion is linear in scroll (scroll = time); nothing here eases.
//
// Lenis smooths discrete wheel steps on desktop only: frame sequences judder
// on raw wheel ticks. Touch keeps native momentum; reduced motion keeps
// native everything. The page works identically with Lenis absent.

const scenes = [];
let film = null, beatPx = 700, beats = 1, current = -1, raf = 0, lenis = null;

export function beatLength() {
  return matchMedia("(max-width: 767px)").matches ? 420 : 700;
}

export function mountFilm(el, totalBeats) {
  film = el;
  beats = totalBeats;
  const layout = () => {
    beatPx = beatLength();
    // The stage stays pinned for all beats; the section is one viewport taller.
    film.style.height = `${beats * beatPx + innerHeight}px`;
    current = -1;
    schedule();
  };
  layout();
  addEventListener("resize", layout, { passive: true });
  addEventListener("scroll", schedule, { passive: true });
  return layout;
}

/** A scene: [from, to] in beats, and a function of local progress. */
export function scene(from, to, update) {
  scenes.push({ from, to, update, active: false });
}

export function beatNow() {
  if (!film) return 0;
  const top = film.getBoundingClientRect().top + scrollY;
  return Math.max(0, Math.min(beats, (scrollY - top) / beatPx));
}

/** Scroll position (px) of a beat, for links and keyboard jumps. */
export function beatToY(b) {
  const top = film.getBoundingClientRect().top + scrollY;
  return top + b * beatPx;
}

function schedule() {
  if (!raf) raf = requestAnimationFrame(tick);
}

const drawers = new Set();
export function everyFrame(fn) { drawers.add(fn); schedule(); }
/** Something became drawable without a scroll (a frame decoded, an image
 *  arrived): run one more tick so it is shown now, not on the next scroll. */
export function wake() { schedule(); }

function tick(t) {
  raf = 0;
  const b = beatNow();
  if (b !== current) {
    current = b;
    for (const s of scenes) {
      const inside = b >= s.from - 0.001 && b <= s.to + 0.001;
      if (inside || s.active) {
        const p = Math.max(0, Math.min(1, (b - s.from) / (s.to - s.from)));
        s.update(p, b, inside);
        s.active = inside;
      }
    }
  }
  let again = false;
  for (const fn of drawers) again = fn(t / 1000, b) || again;
  if (again) schedule();
}

/** Lenis on desktop wheel only, loaded after first paint, never under reduced motion. */
export async function smoothWheel() {
  if (matchMedia("(prefers-reduced-motion: reduce)").matches) return;
  if (!matchMedia("(hover: hover) and (pointer: fine)").matches) return;
  try {
    const { default: Lenis } = await import("lenis");
    lenis = new Lenis({ duration: 1.05, smoothWheel: true, syncTouch: false, touchMultiplier: 1 });
    const loop = (time) => { lenis.raf(time); requestAnimationFrame(loop); };
    requestAnimationFrame(loop);
    lenis.on("scroll", schedule);
  } catch {
    /* native scroll is the designed fallback */
  }
}

export function scrollToY(y) {
  if (lenis) lenis.scrollTo(y, { duration: 1.2 });
  else scrollTo({ top: y, behavior: matchMedia("(prefers-reduced-motion: reduce)").matches ? "auto" : "smooth" });
}
