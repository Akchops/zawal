// A page as a strip of viewport screenshots at even scroll steps (full-page
// captures distort 100svh layouts; this is what a reader actually sees).
//   node qa/strip.mjs <url> <out.png-prefix> <W>x<H>[@dpr] [steps=6] [--nojs] [--rm]
import { chromium } from "playwright";
const [,, url, out, size, stepsS = "6", ...rest] = process.argv;
const [wh, dprS] = size.split("@");
const [w, h] = wh.split("x").map(Number);
const browser = await chromium.launch({ executablePath: "/opt/pw-browsers/chromium-1194/chrome-linux/chrome", args: ["--use-gl=angle", "--use-angle=swiftshader", "--enable-unsafe-swiftshader"] });
const ctx = await browser.newContext({ viewport: { width: w, height: h }, deviceScaleFactor: Number(dprS || 1), isMobile: w < 768, hasTouch: w < 768,
  reducedMotion: rest.includes("--rm") ? "reduce" : "no-preference", javaScriptEnabled: !rest.includes("--nojs") });
const page = await ctx.newPage();
const logs = [];
page.on("console", (m) => { if (["error", "warning"].includes(m.type())) logs.push(`${m.type()}: ${m.text()}`); });
page.on("pageerror", (e) => logs.push(`pageerror: ${e.message}`));
page.on("response", (r) => { if (r.status() >= 400) logs.push(`http ${r.status()}: ${r.url()}`); });
await page.goto(url, { waitUntil: "load" });
await page.waitForTimeout(1200);
const H = await page.evaluate(() => document.documentElement.scrollHeight - innerHeight);
const n = Number(stepsS);
// overflow check: anything wider than the viewport?
const overflow = await page.evaluate(() => document.documentElement.scrollWidth - innerWidth);
for (let k = 0; k < n; k++) {
  await page.evaluate((y) => scrollTo(0, y), Math.round((H * k) / (n - 1)));
  await page.waitForTimeout(500);
  await page.screenshot({ path: `${out}-${k}.png`, scale: "css" });
}
console.log(`height ${H + h}px, horizontal overflow ${overflow}px`);
console.log(logs.length ? logs.join("\n") : "console clean");
await browser.close();
