// Validates the frame bundles the film streams (public/seq/*.json):
// completeness (every frame of every tier present), per-frame size outliers
// (a broken or blank frame compresses far below its neighbours), and the
// decoded memory the player's window keeps resident on a phone.
//   node scripts/check-sequence.mjs [--allow-partial]
import { readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { fileURLToPath } from "node:url";

const dir = fileURLToPath(new URL("../public/seq/", import.meta.url));
const partial = process.argv.includes("--allow-partial");
const WINDOW = 5;           // decoded frames either side (player.js)
let bad = 0;
for (const f of readdirSync(dir).filter((x) => x.endsWith(".json")).sort()) {
  const idx = JSON.parse(readFileSync(join(dir, f), "utf8"));
  const [w, h] = idx.size;
  const n = idx.frames.length;
  const tiers = { hi: idx.hi, ladder: idx.ladder, ...(idx.depth ? { depth: idx.depth } : {}) };
  const notes = [];
  for (const [name, list] of Object.entries(tiers)) {
    const missing = list.map((x, i) => (x.n > 0 ? -1 : i)).filter((i) => i >= 0);
    if (list.length !== n) notes.push(`${name}: ${list.length} entries for ${n} frames`);
    if (missing.length) notes.push(`${name}: ${missing.length} missing (first ${missing.slice(0, 6).join(", ")})`);
    // outliers: a frame under 45 % of the median of its 9 neighbours
    const sizes = list.map((x) => x.n);
    for (let i = 0; i < n; i++) {
      if (!sizes[i]) continue;
      const nb = sizes.slice(Math.max(0, i - 4), i + 5).filter(Boolean).sort((a, b) => a - b);
      const med = nb[nb.length >> 1];
      if (sizes[i] < 0.45 * med) notes.push(`${name}: frame ${i} is ${sizes[i]} B against a local median of ${med} B`);
    }
  }
  // Resident decoded memory: (2*WINDOW+1) full frames (+ as many depth maps
  // at 1/4 size) + 6 ladder frames (1/8 size), RGBA.
  const full = w * h * 4, lad = (w / 8) * (h / 8) * 4, dep = idx.depth ? (w / 4) * (h / 4) * 4 : 0;
  const mem = (2 * WINDOW + 1) * (full + dep) + 6 * lad;
  const bytes = idx.hi.reduce((s, x) => s + x.n, 0) + idx.ladder.reduce((s, x) => s + x.n, 0) + (idx.depth || []).reduce((s, x) => s + x.n, 0);
  const incomplete = notes.some((x) => x.includes("missing"));
  const fail = notes.some((x) => !x.includes("missing")) || (incomplete && !partial);
  if (fail) bad++;
  console.log(`${fail ? "FAIL" : incomplete ? "PART" : "PASS"}  ${f.padEnd(10)} ${n} frames ${w}x${h}  download ${(bytes / 1048576).toFixed(1)} MB  resident ≈ ${(mem / 1048576).toFixed(0)} MB`);
  for (const x of notes) console.log(`      ${x}`);
}
if (bad) process.exitCode = 1;
