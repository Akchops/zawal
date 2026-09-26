// Full-page screenshot of one URL at one size, with console/network errors.
//   node qa/page.mjs <url> <out.png> <width>x<height>[@dpr] [--full] [--scroll y]
import { chromium } from "playwright";
const [,, url, out, size, ...rest] = process.argv;
const [wh, dprS] = size.split("@");
const [w, h] = wh.split("x").map(Number);
const browser = await chromium.launch({ executablePath: "/opt/pw-browsers/chromium-1194/chrome-linux/chrome", args: ["--use-gl=angle", "--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--ignore-gpu-blocklist"] });
const ctx = await browser.newContext({ viewport: { width: w, height: h }, deviceScaleFactor: Number(dprS || 1), isMobile: w < 768, hasTouch: w < 768,
  reducedMotion: rest.includes("--rm") ? "reduce" : "no-preference", javaScriptEnabled: !rest.includes("--nojs") });
const page = await ctx.newPage();
const logs = [];
page.on("console", (m) => { if (["error", "warning"].includes(m.type())) logs.push(`${m.type()}: ${m.text()}`); });
page.on("pageerror", (e) => logs.push(`pageerror: ${e.message}`));
page.on("response", (r) => { if (r.status() >= 400) logs.push(`http ${r.status()}: ${r.url()}`); });
await page.goto(url, { waitUntil: "load" });
await page.waitForTimeout(1200);
const sy = rest.indexOf("--scroll");
if (sy >= 0) { await page.evaluate((y) => scrollTo(0, y), Number(rest[sy + 1])); await page.waitForTimeout(900); }
if (rest.includes("--full")) {
  // step through the page so scroll-driven things run, then capture it whole
  const H = await page.evaluate(() => document.documentElement.scrollHeight);
  for (let y = 0; y < H; y += h * 0.8) { await page.evaluate((y) => scrollTo(0, y), y); await page.waitForTimeout(120); }
  await page.evaluate(() => scrollTo(0, 0)); await page.waitForTimeout(300);
}
await page.screenshot({ path: out, fullPage: rest.includes("--full") });
console.log(logs.length ? logs.join("\n") : "console clean");
await browser.close();
