// The four signatures as evidence strips: each one captured at consecutive
// scroll positions (A, B, D) or under real input (C), at one viewport.
//   node qa/signatures.mjs <baseUrl> <outDir> <W>x<H>[@dpr] [--gl]
// --gl adds ?forcegl&noguard (software GL in this container counts as "no GPU"
// otherwise, and the site would correctly serve its Canvas 2D tier).
import { chromium } from "playwright";
import { mkdirSync, writeFileSync } from "node:fs";

const [,, base, out, size, ...rest] = process.argv;
const [wh, dprS] = size.split("@");
const [w, h] = wh.split("x").map(Number);
const mobile = w < 768;
mkdirSync(out, { recursive: true });
const browser = await chromium.launch({ executablePath: "/opt/pw-browsers/chromium-1194/chrome-linux/chrome",
  args: ["--use-gl=angle", "--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--ignore-gpu-blocklist"] });
const page = await browser.newPage({ viewport: { width: w, height: h }, deviceScaleFactor: Number(dprS || 1), isMobile: mobile, hasTouch: mobile });
const logs = [];
page.on("console", (m) => { if (["error", "warning"].includes(m.type())) logs.push(`${m.type()}: ${m.text()}`); });
page.on("pageerror", (e) => logs.push(`pageerror: ${e.message}`));
page.on("requestfailed", (r) => { if (!/ERR_ABORTED/.test(r.failure()?.errorText || "")) logs.push(`failed: ${r.url()}`); });
page.on("response", (r) => { if (r.status() >= 400) logs.push(`http ${r.status()}: ${r.url()}`); });
await page.goto(base + "/" + (rest.includes("--gl") ? "?forcegl&noguard" : ""), { waitUntil: "load" });
await page.waitForTimeout(3000);

const toBeat = async (b, wait = 1800) => {
  await page.evaluate((b) => {
    const f = document.querySelector("[data-film]");
    const bp = matchMedia("(max-width: 767px)").matches ? 420 : 700;
    scrollTo(0, f.getBoundingClientRect().top + scrollY + b * bp);
  }, b);
  await page.waitForTimeout(wait);
};
const shot = async (name) => { await page.screenshot({ path: `${out}/${name}.png` }); return name; };
const readout = () => page.evaluate(() => ({
  hud: ["date", "time", "alt", "az"].map((k) => document.querySelector(`[data-hud-${k}]`)?.textContent.trim()).join(" | "),
  sunWhen: document.querySelector("[data-sun-when]")?.textContent,
}));
const record = {};

// A · a day in one scroll: morning, zawal, afternoon, dusk.
record.A = [];
for (const [b, n] of [[5.0, "A1-morning"], [6.8, "A2-zawal"], [8.6, "A3-afternoon"], [9.6, "A4-dusk"]]) {
  await toBeat(b); record.A.push({ beat: b, shot: await shot(n), ...(await readout()) });
}
// B · street to courtyard: the walk, frame by frame of camera travel.
record.B = [];
for (const [b, n] of [[0.0, "B1-street"], [1.3, "B2-lane"], [2.3, "B3-gate"], [3.1, "B4-passage"], [3.8, "B5-courtyard"]]) {
  await toBeat(b); record.B.push({ beat: b, shot: await shot(n), ...(await readout()) });
}
// C · you are the sun: the same beat, two different suns chosen by input.
record.C = [];
await toBeat(11.2, 3500);
record.C.push({ input: "none (idle sun)", shot: await shot("C1-idle"), ...(await readout()) });
const inputs = [["C2-low-east", 0.12, 0.72], ["C3-high-south", 0.5, 0.18], ["C4-low-west", 0.9, 0.7]];
for (const [n, x, y] of inputs) {
  if (mobile) await page.touchscreen.tap(Math.round(w * x), Math.round(h * y));
  else await page.mouse.move(Math.round(w * x), Math.round(h * y), { steps: 6 });
  await page.waitForTimeout(1600);
  record.C.push({ input: `${mobile ? "tap" : "pointer"} at ${x},${y}`, shot: await shot(n), ...(await readout()) });
}
await page.keyboard.press("ArrowRight"); await page.keyboard.press("ArrowRight"); await page.keyboard.press("ArrowRight");
await page.waitForTimeout(1200);
record.C.push({ input: "keyboard: → ×3", shot: await shot("C5-keys"), ...(await readout()) });
// D · projects morph through the shadow: Ghaf House into Hotel Sikka.
record.D = [];
for (const [b, n] of [[14.9, "D1-ghaf"], [15.42, "D2-patches-spread"], [15.6, "D3-dapple-to-slats"], [15.78, "D4-slats"], [15.95, "D5-sikka-patches"], [16.4, "D6-sikka"]]) {
  await toBeat(b); record.D.push({ beat: b, shot: await shot(n), ...(await readout()) });
}
// The close into shade, the method.
for (const [b, n] of [[22.3, "D7-closing"], [22.9, "D8-method"]]) { await toBeat(b); record.D.push({ beat: b, shot: await shot(n) }); }
record.console = logs;
writeFileSync(`${out}/signatures.json`, JSON.stringify(record, null, 1));
console.log(logs.length ? logs.join("\n") : "console clean");
await browser.close();
