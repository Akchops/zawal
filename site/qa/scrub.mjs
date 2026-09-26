// Check 2: a slow scroll through a sequence on a phone, one screenshot per
// small step, to measure whether playback steps like a slideshow.
//   node qa/scrub.mjs <url> <outdir> <fromBeat> <toBeat> <pxPerStep> [WxH@dpr]
import { chromium } from "playwright";
import { mkdirSync, writeFileSync } from "node:fs";

const [,, url, out, fromS, toS, stepS, size = "390x844@2"] = process.argv;
const [wh, dprS] = size.split("@");
const [w, h] = wh.split("x").map(Number);
mkdirSync(out, { recursive: true });
const browser = await chromium.launch({ executablePath: "/opt/pw-browsers/chromium-1194/chrome-linux/chrome",
  args: ["--use-gl=angle", "--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--ignore-gpu-blocklist"] });
const page = await browser.newPage({ viewport: { width: w, height: h }, deviceScaleFactor: Number(dprS || 1), isMobile: true, hasTouch: true });
const errors = [];
page.on("pageerror", (e) => errors.push(e.message));
await page.goto(url, { waitUntil: "load" });
// Measure the picture only: captions, HUD and nav hidden (they change with scroll by design).
await page.addStyleTag({ content: ".cap,.hud,.nav,.fiction-mark,.grade{visibility:hidden!important}" });
const geo = await page.evaluate(() => {
  const film = document.querySelector("[data-film]");
  return { top: film.getBoundingClientRect().top + scrollY, bp: matchMedia("(max-width: 767px)").matches ? 420 : 700 };
});
const y0 = Math.round(geo.top + Number(fromS) * geo.bp), y1 = Math.round(geo.top + Number(toS) * geo.bp), step = Number(stepS);
// Arrive at the start, let the bundles land and the window decode.
await page.evaluate((y) => scrollTo(0, y), y0 - 60);
await page.waitForTimeout(6000);
for (let y = y0 - 60; y <= y0; y += 6) { await page.evaluate((y) => scrollTo(0, y), y); await page.waitForTimeout(60); }
const meta = [];
let k = 0;
for (let y = y0; y <= y1; y += step, k++) {
  await page.evaluate((y) => scrollTo(0, y), y);
  // two animation frames for the draw, then a settle for any decode
  await page.evaluate(() => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r))));
  await page.waitForTimeout(90);
  await page.screenshot({ path: `${out}/s${String(k).padStart(4, "0")}.png`, scale: "css" });
  meta.push({ k, y, beat: (y - geo.top) / geo.bp });
}
writeFileSync(`${out}/meta.json`, JSON.stringify({ url, size, step, bp: geo.bp, steps: meta, errors }, null, 1));
console.log(`${k} steps captured${errors.length ? `, page errors: ${errors.join("; ")}` : ""}`);
await browser.close();
