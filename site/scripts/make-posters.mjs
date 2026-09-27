// Stills for the static film (no JavaScript, or reduced motion): the frames
// each shot rests on, copied from the rendered sequences (already WebP).
//   public/img/posters/<frame>-p.webp  (portrait 720)   <frame>-l.webp  (landscape 1600)
// Plus light first-paint versions of the Shade Study instrument's still
// (<frame>-p-lite.webp 540 wide, <frame>-l-lite.webp 960 wide), which that
// page shows inside the first-load budget before the live stage takes over.
import { copyFileSync, existsSync, mkdirSync } from "node:fs";
import { execFileSync } from "node:child_process";
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
const LITE = { a048: [["p", 540, 66], ["l", 960, 60]] };
const shrink = `import sys
from PIL import Image
src, dst, w, q = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
im = Image.open(src).convert("RGB")
im.resize((w, round(im.height * w / im.width)), Image.LANCZOS).save(dst, "WEBP", quality=q, method=6)`;
for (const [f, tiers] of Object.entries(LITE))
  for (const [v, w, q] of tiers) {
    const src = join(out, `${f}-${v}.webp`);
    if (existsSync(src)) { execFileSync("python3", ["-c", shrink, src, join(out, `${f}-${v}-lite.webp`), String(w), String(q)]); n++; }
    else missing.push(`${f}-${v}-lite`);
  }
console.log(`posters: ${n} copied${missing.length ? `, not rendered yet: ${missing.join(" ")}` : ""}`);
