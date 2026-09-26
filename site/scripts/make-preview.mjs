// The private preview: the built site (dist/) rewritten so every URL is
// relative (it is served from an unknown folder, not a domain root) and page
// links name their index.html (the host does not map folders to it).
//   node scripts/make-preview.mjs [outDir=preview]
import { cpSync, readdirSync, readFileSync, rmSync, statSync, writeFileSync } from "node:fs";
import { dirname, join, relative, sep } from "node:path";
import { fileURLToPath } from "node:url";

const site = join(dirname(fileURLToPath(import.meta.url)), "..");
const dist = join(site, "dist");
const out = join(site, process.argv[2] || "preview");
rmSync(out, { recursive: true, force: true });
cpSync(dist, out, { recursive: true });

const walk = (d) => readdirSync(d).flatMap((f) => { const p = join(d, f); return statSync(p).isDirectory() ? walk(p) : [p]; });
const files = walk(out);
const pages = new Set(files.filter((f) => f.endsWith(".html")).map((f) => "/" + relative(out, f).split(sep).join("/")));

function rel(fromFile, url) {
  // url: site-absolute ("/work/x/#y"); returns a path relative to fromFile's folder
  const m = url.match(/^([^?#]*)([?#].*)?$/);
  let path = m[1], tail = m[2] || "";
  if (path.endsWith("/")) path += "index.html";
  if (!path.includes(".") && pages.has(path + "/index.html")) path += "/index.html";
  const fromDir = dirname("/" + relative(out, fromFile).split(sep).join("/"));
  let r = relative(fromDir, path).split(sep).join("/");
  if (!r) r = "./";
  return r + tail;
}
const isSite = (u) => u.startsWith("/") && !u.startsWith("//");

let changed = 0;
for (const f of files) {
  if (f.endsWith(".html")) {
    let s = readFileSync(f, "utf8");
    // attributes holding one URL
    s = s.replace(/\b(href|src|poster|action|data-src)="([^"]*)"/g, (all, a, u) => (isSite(u) ? `${a}="${rel(f, u)}"` : all));
    // attributes holding URL lists
    s = s.replace(/\b(srcset|imagesrcset)="([^"]*)"/g, (all, a, v) =>
      `${a}="${v.split(",").map((part) => { const t = part.trim().split(/\s+/); if (isSite(t[0])) t[0] = rel(f, t[0]); return t.join(" "); }).join(", ")}"`);
    // CSS url(/...) in inline styles
    s = s.replace(/url\((["']?)(\/[^"')]+)\1\)/g, (all, q, u) => (isSite(u) ? `url(${q}${rel(f, u)}${q})` : all));
    writeFileSync(f, s);
    changed++;
  } else if (f.endsWith(".js")) {
    let s = readFileSync(f, "utf8");
    // Vite's preload helper builds "/_z/..." URLs; make them relative to the chunk.
    const t = s.replace(/function\((\w)\)\{return"\/"\+\1\}/g, 'function($1){return new URL("../"+$1,import.meta.url).href}');
    if (t !== s) { writeFileSync(f, t); changed++; }
  } else if (f.endsWith(".css")) {
    let s = readFileSync(f, "utf8");
    s = s.replace(/url\((["']?)(\/[^"')]+)\1\)/g, (all, q, u) => `url(${q}${rel(f, u)}${q})`);
    writeFileSync(f, s);
    changed++;
  }
}
console.log(`preview: ${files.length} files, ${changed} rewritten -> ${relative(site, out)}/`);
