// Screenshots of the home film at given beats.
//   node qa/shots.mjs <url> <out-prefix> <width>x<height>[@dpr] <beat,beat,...> [--wait ms]
import { chromium } from "playwright";

const [,, url, prefix, size, beatList, ...rest] = process.argv;
const [wh, dprS] = size.split("@");
const [w, h] = wh.split("x").map(Number);
const dpr = Number(dprS || 1);
const wait = Number(rest[rest.indexOf("--wait") + 1] || 1200);
const beats = beatList.split(",").map(Number);
const browser = await chromium.launch({ executablePath: "/opt/pw-browsers/chromium-1194/chrome-linux/chrome", args: ["--use-gl=angle", "--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--ignore-gpu-blocklist"] });
const page = await browser.newPage({ viewport: { width: w, height: h }, deviceScaleFactor: dpr, isMobile: w < 768, hasTouch: w < 768 });
const logs = [];
page.on("console", (m) => { if (m.type() === "error" || m.type() === "warning") logs.push(`${m.type()}: ${m.text()}`); });
page.on("pageerror", (e) => logs.push(`pageerror: ${e.message}`));
page.on("requestfailed", (r) => logs.push(`requestfailed: ${r.url()} ${r.failure()?.errorText}`));
page.on("response", (r) => { if (r.status() >= 400) logs.push(`http ${r.status()}: ${r.url()}`); });
await page.goto(url, { waitUntil: "load" });
await page.waitForTimeout(wait);
for (const b of beats) {
  await page.evaluate((b) => {
    const film = document.querySelector("[data-film]");
    const bp = matchMedia("(max-width: 767px)").matches ? 420 : 700;
    const top = film.getBoundingClientRect().top + scrollY;
    scrollTo(0, top + b * bp);
  }, b);
  await page.waitForTimeout(wait);
  await page.screenshot({ path: `${prefix}-${String(b).replace(".", "_")}.png` });
}
console.log(logs.length ? logs.join("\n") : "console clean");
await browser.close();
