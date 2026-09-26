// Project pages: the Shade Study draws itself hour by hour as it crosses the
// viewport (scroll-scrubbed, so scrolling back undraws it). Without motion it
// is simply there, finished, as it is without JavaScript.
import { studyDriver } from "./study.js";

// The hero's sharp set (this module is imported after the load event).
for (const pic of document.querySelectorAll("[data-sharpen]"))
  for (const el of pic.querySelectorAll("[data-srcset]")) el.srcset = el.dataset.srcset;

const reduced = matchMedia("(prefers-reduced-motion: reduce)").matches;
for (const host of document.querySelectorAll("[data-study-draw]")) {
  const svg = host.querySelector("svg");
  if (!svg || reduced) continue;
  const drive = studyDriver(svg);
  let raf = 0;
  const update = () => {
    raf = 0;
    const r = host.getBoundingClientRect();
    const p = (innerHeight * 0.85 - r.top) / (r.height * 0.75 + innerHeight * 0.2);
    drive(Math.max(0, Math.min(1, p)));
  };
  update();
  addEventListener("scroll", () => { if (!raf) raf = requestAnimationFrame(update); }, { passive: true });
  addEventListener("resize", update, { passive: true });
}
