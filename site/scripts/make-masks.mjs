// Page-transition masks, one per project pattern (plus the courtyard star):
// a 4000 x 1000 strip, [solid | a band where the pattern's shapes shrink to
// nothing | empty], slid across the page by the view transition. The old page
// leaves through the pattern's shadow; the new one arrives through it.
import { writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const out = join(dirname(fileURLToPath(import.meta.url)), "..", "public", "img");
const X0 = 1450, X1 = 2550;
const r1 = (v) => Math.round(v * 10) / 10;
const hash = (i, j) => { const s = Math.sin(i * 127.1 + j * 311.7) * 43758.5453; return s - Math.floor(s); };
const wrap = (body) => `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 4000 1000" width="4000" height="1000"><rect width="${X0}" height="1000"/>${body}</svg>`;
const size = (x) => Math.max(0, 1 - (x - X0) / (X1 - X0));

// Dapple (Ghaf House): leaf-shade blobs, jittered, shrinking.
{
  let b = "";
  for (let i = 0; i < 26; i++) for (let j = 0; j < 12; j++) {
    const x = X0 - 40 + i * 46 + (hash(i, j) - 0.5) * 30, y = j * 90 + (hash(j, i) - 0.5) * 40;
    const r = 58 * size(x) * (0.75 + 0.5 * hash(i + 9, j));
    if (r > 1) b += `<ellipse cx="${r1(x)}" cy="${r1(y)}" rx="${r1(r)}" ry="${r1(r * 0.8)}"/>`;
  }
  writeFileSync(join(out, "vt-dapple.svg"), wrap(`<g>${b}</g>`));
}
// Slats (Hotel Sikka): palm ribs thinning to lines, then gone.
{
  let b = "";
  for (let i = 0; i < 30; i++) {
    const x = X0 + i * 38, w = 38 * size(x) * (0.85 + 0.3 * hash(i, 3));
    if (w > 0.8) b += `<rect x="${r1(x - w / 2)}" y="0" width="${r1(w)}" height="1000"/>`;
  }
  for (let j = 0; j < 4; j++) b += `<rect x="${X0}" y="${120 + j * 250}" width="${r1((X1 - X0) * 0.35)}" height="14"/>`;
  writeFileSync(join(out, "vt-slats.svg"), wrap(`<g>${b}</g>`));
}
// Triangles (Qudra Canopy): the canopy's small triangles of light, inverted.
{
  let b = "";
  const P = 92, H = P * 0.866;
  for (let i = -1; i < 26; i++) for (let j = 0; j < 14; j++) {
    const up = (i + j) % 2 === 0, cx = X0 + i * P / 2, cy = j * H;
    const s = size(cx) * 1.08;
    if (s <= 0.02) continue;
    const hs = (P / 2) * s, hh = H * s, yc = cy + H / 2;
    const pts = up ? [[cx, yc - hh / 2], [cx + hs, yc + hh / 2], [cx - hs, yc + hh / 2]] : [[cx - hs, yc - hh / 2], [cx + hs, yc - hh / 2], [cx, yc + hh / 2]];
    b += `<polygon points="${pts.map((p) => p.map(r1).join(",")).join(" ")}"/>`;
  }
  writeFileSync(join(out, "vt-triangles.svg"), wrap(`<g>${b}</g>`));
}
console.log("masks written");
