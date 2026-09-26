// First-load budget: everything a fresh visit fetches before the load event,
// measured in a real browser against the built site, text counted gzipped.
// Budget: < 100 kB per page and device class. Anything fetched later (the
// film's frames, the film script, relight buffers) is listed separately.
//   node scripts/check-budget.mjs [--pages /,/studio/] [--json out.json]
import { createServer } from "node:http";
import { readFile } from "node:fs/promises";
import { existsSync, statSync, writeFileSync } from "node:fs";
import { extname, join } from "node:path";
import { gzipSync } from "node:zlib";
import { fileURLToPath } from "node:url";
import { chromium } from "playwright";

const dist = fileURLToPath(new URL("../dist/", import.meta.url));
const BUDGET = 100 * 1024;
const args = process.argv.slice(2);
const pages = (args.includes("--pages") ? args[args.indexOf("--pages") + 1] : "/,/work/ghaf-house/,/shade-study/,/studio/").split(",");
const TYPES = { ".html": "text/html", ".js": "text/javascript", ".css": "text/css", ".svg": "image/svg+xml", ".json": "application/json",
  ".webp": "image/webp", ".woff2": "font/woff2", ".bin": "application/octet-stream", ".png": "image/png", ".txt": "text/plain" };
const server = createServer(async (req, res) => {
  let p = decodeURIComponent(new URL(req.url, "http://x").pathname);
  if (p.endsWith("/")) p += "index.html";
  const f = join(dist, p);
  if (!f.startsWith(dist) || !existsSync(f) || statSync(f).isDirectory()) { res.writeHead(404); res.end(); return; }
  res.writeHead(200, { "content-type": TYPES[extname(f)] || "application/octet-stream" });
  res.end(await readFile(f));
});
await new Promise((r) => server.listen(0, r));
const base = `http://localhost:${server.address().port}`;
const textual = (url) => /\.(html|js|css|svg|json|txt)(\?|$)/.test(url) || url.endsWith("/");

const devices = [
  { name: "phone 390@3", viewport: { width: 390, height: 844 }, deviceScaleFactor: 3, isMobile: true, hasTouch: true },
  { name: "desktop 1440@1", viewport: { width: 1440, height: 900 }, deviceScaleFactor: 1 },
  { name: "desktop 1728@2", viewport: { width: 1728, height: 1117 }, deviceScaleFactor: 2 },
];
const browser = await chromium.launch({ executablePath: "/opt/pw-browsers/chromium-1194/chrome-linux/chrome" });
let fail = false;
const report = [];
for (const d of devices) {
  for (const path of pages) {
    const ctx = await browser.newContext(d);
    const page = await ctx.newPage();
    const early = [], late = [];
    let loaded = false;
    page.on("load", () => { loaded = true; });
    page.on("response", async (r) => {
      const list = loaded ? late : early;
      try {
        const body = await r.body();
        const n = textual(r.url()) ? gzipSync(body).length : body.length;
        list.push({ url: r.url().replace(base, ""), bytes: n });
      } catch { /* redirects, aborted */ }
    });
    await page.goto(base + path, { waitUntil: "load" });
    await page.waitForTimeout(400);
    const total = early.reduce((s, x) => s + x.bytes, 0);
    const ok = total < BUDGET;
    if (!ok) fail = true;
    report.push({ device: d.name, page: path, firstLoadKB: +(total / 1024).toFixed(1), ok, requests: early });
    console.log(`${ok ? "PASS" : "FAIL"}  ${d.name.padEnd(15)} ${path.padEnd(22)} ${(total / 1024).toFixed(1).padStart(6)} kB first load` +
      `  (${early.length} requests: ${early.map((x) => `${x.url.split("/").pop() || "index"} ${(x.bytes / 1024).toFixed(1)}`).join(", ")})`);
    await ctx.close();
  }
}
await browser.close();
server.close();
if (args.includes("--json")) writeFileSync(args[args.indexOf("--json") + 1], JSON.stringify(report, null, 1));
if (fail) { console.error("first-load budget exceeded"); process.exitCode = 1; }
