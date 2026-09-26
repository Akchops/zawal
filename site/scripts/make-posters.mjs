// Stills for the static film (no JavaScript, or reduced motion): the frames
// each shot rests on, copied from the rendered sequences (already WebP).
//   public/img/posters/<frame>-p.webp  (portrait 720)   <frame>-l.webp  (landscape 1600)
import { copyFileSync, existsSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const site = join(dirname(fileURLToPath(import.meta.url)), "..");
const out = join(site, "public", "img", "posters");
mkdirSync(out, { recursive: true });
const FRAMES = ["b000", "b036", "b060", "b076", "a000", "a048", "a100", "a120"];
let n = 0, missing = [];
for (const f of FRAMES) {
  const seq = f[0];
  for (const [v, dir] of [["p", "p/720"], ["l", "l/1600"]]) {
    const src = join(site, "frames", seq, dir, `${f}.webp`);
    if (existsSync(src)) { copyFileSync(src, join(out, `${f}-${v}.webp`)); n++; } else missing.push(`${f}-${v}`);
  }
}
console.log(`posters: ${n} copied${missing.length ? `, not rendered yet: ${missing.join(" ")}` : ""}`);
