// Packs the rendered sequence frames (site/frames/<seq>/<l|p>/<tier>/*.webp,
// written by renderer/tools/render_seq.py) into streamable bundles:
//
//   public/seq/<seq>-<base>-hi.bin      full frames, in frame order
//   public/seq/<seq>-<base>-ladder.bin  tiny frames for instant coverage
//   public/seq/<seq>-<base>-depth.bin   depth maps (the walk only)
//   public/seq/<seq>-<base>.json        offsets + per-frame minute, sun, camera
//
// bases: p (portrait 720x1280), l (landscape 1600x900), t (landscape 1280x720).
// Works with a partly rendered sequence: missing frames get n = 0 and the
// player skips them, so the site can be built while frames are still landing.
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const site = join(here, "..");
const frames = join(site, "frames");
const out = join(site, "public", "seq");
const seqDir = join(site, "..", "renderer", "sequences");
mkdirSync(out, { recursive: true });

const BASES = {
  p: { variant: "portrait", short: "p", hi: "720", ladder: "ladder" },
  l: { variant: "landscape", short: "l", hi: "1600", ladder: "ladder" },
  t: { variant: "landscape", short: "l", hi: "1280", ladder: "ladder" },
};

function pack(seq, files, dir) {
  const parts = [];
  const index = [];
  let o = 0;
  for (const f of files) {
    const p = join(dir, `${f}.webp`);
    if (!existsSync(p)) { index.push({ o, n: 0 }); continue; }
    const b = readFileSync(p);
    parts.push(b);
    index.push({ o, n: b.length });
    o += b.length;
  }
  return { buf: Buffer.concat(parts), index, bytes: o };
}

const report = [];
for (const seq of ["a", "b"]) {
  const specPath = join(seqDir, `seq_${seq}.json`);
  if (!existsSync(specPath)) continue;
  const spec = JSON.parse(readFileSync(specPath, "utf8"));
  for (const [base, cfg] of Object.entries(BASES)) {
    const list = spec[cfg.variant].frames;
    const names = list.map((f) => f.file);
    const root = join(frames, seq, cfg.short);
    if (!existsSync(join(root, cfg.hi))) continue;
    const hi = pack(seq, names, join(root, cfg.hi));
    const ladder = pack(seq, names, join(root, cfg.ladder));
    const depth = seq === "b" ? pack(seq, names, join(root, "depth")) : null;
    const meta = list.map((f) => ({ m: f.minutes, alt: f.alt, az: f.az, cam: f.cam }));
    const idx = { seq, base, size: spec[cfg.variant].size, hi: hi.index, ladder: ladder.index, depth: depth?.index, frames: meta,
      zawal: spec.zawal_index ?? null };
    writeFileSync(join(out, `${seq}-${base}-hi.bin`), hi.buf);
    writeFileSync(join(out, `${seq}-${base}-ladder.bin`), ladder.buf);
    if (depth) writeFileSync(join(out, `${seq}-${base}-depth.bin`), depth.buf);
    writeFileSync(join(out, `${seq}-${base}.json`), JSON.stringify(idx));
    const have = hi.index.filter((x) => x.n > 0).length;
    report.push(`${seq}-${base}: ${have}/${names.length} frames, hi ${(hi.bytes / 1024).toFixed(0)} kB, ladder ${(ladder.bytes / 1024).toFixed(0)} kB` +
      (depth ? `, depth ${(depth.bytes / 1024).toFixed(0)} kB` : ""));
  }
}
console.log(report.join("\n") || "no frames yet");
