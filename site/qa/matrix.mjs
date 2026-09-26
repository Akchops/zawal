// QA evidence matrix for the home film: six viewports x nine scroll depths,
// plus the reduced-motion, no-WebGL and no-JS paths, with console and
// network logged for every run. Writes PNGs, one contact sheet per run
// (via qa/sheet.py) and log.json.
//   node qa/matrix.mjs <baseUrl> <outDir> [--only 390] [--modes normal,rm,nogl,nojs]
import { chromium } from "playwright";
import { mkdirSync, writeFileSync } from "node:fs";

const [,, base, out, ...rest] = process.argv;
const only = rest.includes("--only") ? rest[rest.indexOf("--only") + 1].split(",").map(Number) : null;
const modes = rest.includes("--modes") ? rest[rest.indexOf("--modes") + 1].split(",") : ["normal", "rm", "nogl", "nojs"];
const VIEWPORTS = [
  { w: 320, h: 640, dpr: 2, mobile: true }, { w: 390, h: 844, dpr: 3, mobile: true }, { w: 430, h: 932, dpr: 3, mobile: true },
  { w: 768, h: 1024, dpr: 2, mobile: true }, { w: 1440, h: 900, dpr: 1, mobile: false }, { w: 1728, h: 1117, dpr: 2, mobile: false },
].filter((v) => !only || only.includes(v.w));
// Nine depths, one inside each part of the film: street, passage, morning,
// zawal, the live sun, the Study, a project, a morph, night.
const DEPTHS = [0, 2.9, 5.0, 6.8, 10.9, 13.2, 14.6, 15.65, 26.2];
mkdirSync(out, { recursive: true });
const browser = await chromium.launch({ executablePath: "/opt/pw-browsers/chromium-1194/chrome-linux/chrome",
  args: ["--use-gl=angle", "--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--ignore-gpu-blocklist"] });
const log = [];
for (const v of VIEWPORTS) {
  for (const mode of modes) {
    const ctx = await browser.newContext({
      viewport: { width: v.w, height: v.h }, deviceScaleFactor: Math.min(v.dpr, 2), isMobile: v.mobile, hasTouch: v.mobile,
      reducedMotion: mode === "rm" ? "reduce" : "no-preference", javaScriptEnabled: mode !== "nojs",
    });
    const page = await ctx.newPage();
    const entry = { viewport: `${v.w}x${v.h}`, mode, console: [], network: [], shots: [] };
    page.on("console", (m) => { if (["error", "warning"].includes(m.type())) entry.console.push(`${m.type()}: ${m.text()}`); });
    page.on("pageerror", (e) => entry.console.push(`pageerror: ${e.message}`));
    page.on("requestfailed", (r) => entry.network.push(`failed: ${r.url()} ${r.failure()?.errorText}`));
    page.on("response", (r) => { if (r.status() >= 400) entry.network.push(`${r.status()}: ${r.url()}`); });
    // SwiftShader is software GL: the normal and reduced-motion runs accept it
    // (?forcegl) so WebGL paths are exercised; the no-WebGL run forces Canvas 2D.
    const q = mode === "nogl" ? "?nogl" : mode === "nojs" ? "" : "?forcegl";
    await page.goto(base + "/" + q, { waitUntil: "load" });
    await page.waitForTimeout(2500);
    const film = mode === "normal" || mode === "nogl";
    const depths = film ? DEPTHS : [0, 0.12, 0.25, 0.37, 0.5, 0.62, 0.75, 0.87, 0.99];   // fractions of the article
    for (const d of depths) {
      await page.evaluate(([d, film]) => {
        if (film) {
          const f = document.querySelector("[data-film]");
          const bp = matchMedia("(max-width: 767px)").matches ? 420 : 700;
          scrollTo(0, f.getBoundingClientRect().top + scrollY + d * bp);
        } else scrollTo(0, (document.documentElement.scrollHeight - innerHeight) * d);
      }, [d, film]);
      await page.waitForTimeout(film ? 1600 : 500);
      const name = `${v.w}-${mode}-${String(d).replace(".", "_")}.png`;
      await page.screenshot({ path: `${out}/${name}`, scale: "css" });
      entry.shots.push(name);
    }
    log.push(entry);
    console.log(`${entry.viewport.padEnd(9)} ${mode.padEnd(6)} console ${entry.console.length}  network ${entry.network.length}`);
    await ctx.close();
  }
}
writeFileSync(`${out}/log.json`, JSON.stringify(log, null, 1));
await browser.close();
