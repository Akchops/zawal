// Every page at the six QA widths: viewport strips at even scroll steps (what a
// reader sees; full-page captures distort 100svh layouts), horizontal
// overflow, console and network errors, cumulative layout shift, and the
// production basics (title, robots, landmarks, alt text, links, axe WCAG A/AA).
//   node qa/pages.mjs <baseUrl> <outDir> [--only 320,390] [--pages /studio/,/work/ghaf-house/] [--modes normal,rm,nojs] [--steps 10]
import { chromium } from "playwright";
import { mkdirSync, writeFileSync, readFileSync } from "node:fs";
import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
const AXE = readFileSync(require.resolve("axe-core"), "utf8");
const [,, base, out, ...rest] = process.argv;
const arg = (k, d) => (rest.includes(k) ? rest[rest.indexOf(k) + 1] : d);
const only = rest.includes("--only") ? arg("--only").split(",").map(Number) : null;
const modes = arg("--modes", "normal").split(",");
const maxSteps = Number(arg("--steps", "10"));
const PAGES = arg("--pages", "/,/work/ghaf-house/,/work/hotel-sikka/,/work/qudra-canopy/,/work/mushrif-reading-rooms/,/shade-study/,/studio/").split(",");
const VIEWPORTS = [
  { w: 320, h: 640, dpr: 2, mobile: true }, { w: 390, h: 844, dpr: 3, mobile: true }, { w: 430, h: 932, dpr: 3, mobile: true },
  { w: 768, h: 1024, dpr: 2, mobile: true }, { w: 1440, h: 900, dpr: 1, mobile: false }, { w: 1728, h: 1117, dpr: 2, mobile: false },
].filter((v) => !only || only.includes(v.w));

mkdirSync(out, { recursive: true });
const browser = await chromium.launch({ executablePath: "/opt/pw-browsers/chromium-1194/chrome-linux/chrome",
  args: ["--use-gl=angle", "--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--ignore-gpu-blocklist"] });
const log = [];
for (const path of PAGES) {
  const slug = path === "/" ? "home" : path.replace(/^\/|\/$/g, "").replace(/\//g, "-");
  for (const v of VIEWPORTS) {
    for (const mode of modes) {
      const ctx = await browser.newContext({
        viewport: { width: v.w, height: v.h }, deviceScaleFactor: Math.min(v.dpr, 2), isMobile: v.mobile, hasTouch: v.mobile,
        reducedMotion: mode === "rm" ? "reduce" : "no-preference", javaScriptEnabled: mode !== "nojs",
      });
      // Layout shift that no input caused, summed the way CLS counts it (session windows ignored: a strict upper bound).
      await ctx.addInitScript(() => {
        window.__cls = 0; window.__shifts = [];
        try {
          new PerformanceObserver((l) => { for (const e of l.getEntries()) if (!e.hadRecentInput) {
            window.__cls += e.value;
            window.__shifts.push({ v: +e.value.toFixed(4), t: Math.round(e.startTime), src: (e.sources || []).map((s) => s.node && (s.node.id || s.node.className || s.node.nodeName)).slice(0, 3) });
          } }).observe({ type: "layout-shift", buffered: true });
        } catch {}
      });
      const page = await ctx.newPage();
      const entry = { page: path, viewport: `${v.w}x${v.h}`, mode, console: [], network: [], shots: [] };
      page.on("console", (m) => { if (["error", "warning"].includes(m.type())) entry.console.push(`${m.type()}: ${m.text()}`); });
      page.on("pageerror", (e) => entry.console.push(`pageerror: ${e.message}`));
      page.on("requestfailed", (r) => { if (!/ERR_ABORTED/.test(r.failure()?.errorText || "")) entry.network.push(`failed: ${r.url()} ${r.failure()?.errorText}`); });
      page.on("response", (r) => { if (r.status() >= 400) entry.network.push(`${r.status()}: ${r.url()}`); });
      const q = mode === "normal" && path === "/" ? "?forcegl&noguard" : "";
      await page.goto(base + path + q, { waitUntil: "load" });
      await page.waitForTimeout(1800);
      entry.cls_load = await page.evaluate(() => +window.__cls?.toFixed(4)).catch(() => null);
      const H = await page.evaluate(() => document.documentElement.scrollHeight);
      entry.height = H;
      const n = Math.min(maxSteps, Math.max(2, Math.ceil(H / v.h)));
      for (let k = 0; k < n; k++) {
        await page.evaluate((y) => scrollTo(0, y), Math.round(((H - v.h) * k) / (n - 1)));
        await page.waitForTimeout(700);
        const name = `${slug}-${v.w}-${mode}-${k}.png`;
        await page.screenshot({ path: `${out}/${name}`, scale: "css" });
        entry.shots.push(name);
      }
      entry.overflow = await page.evaluate(() => document.documentElement.scrollWidth - innerWidth);
      entry.cls = await page.evaluate(() => +window.__cls?.toFixed(4)).catch(() => null);
      entry.shifts = await page.evaluate(() => (window.__shifts || []).slice(0, 8)).catch(() => []);
      // Production basics, read from the rendered DOM.
      entry.dom = await page.evaluate(() => {
        const q = (s) => document.querySelector(s);
        const vis = (el) => { const r = el.getBoundingClientRect(); const cs = getComputedStyle(el); return r.width > 0 && r.height > 0 && cs.visibility !== "hidden" && cs.display !== "none"; };
        return {
          title: document.title, lang: document.documentElement.lang,
          description: q('meta[name="description"]')?.content || null, robots: q('meta[name="robots"]')?.content || null,
          ldjson: document.querySelectorAll('script[type="application/ld+json"]').length,
          landmarks: { header: document.querySelectorAll("header").length, nav: document.querySelectorAll("nav").length, main: document.querySelectorAll("main").length, footer: document.querySelectorAll("footer").length },
          h1: [...document.querySelectorAll("h1")].map((h) => h.textContent.trim().slice(0, 60)),
          imgNoAlt: [...document.querySelectorAll("img:not([alt])")].map((i) => i.currentSrc || i.src),
          links: [...new Set([...document.querySelectorAll("a[href]")].map((a) => a.getAttribute("href")))],
          honesty: document.body.innerText.includes("ZAWAL is a fictional studio. This site is a design demonstration."),
          credit: document.body.innerText.includes("Made by Aarav Chopra"),
          mark: [...document.querySelectorAll(".fiction-mark, [data-fiction-mark]")].filter(vis).length,
          smallTargets: [...document.querySelectorAll("a[href], button, input, select")].filter(vis).filter((el) => { const r = el.getBoundingClientRect(); return (r.width < 24 || r.height < 24) && !el.closest("p, li, dd, figcaption"); }).map((el) => `${el.tagName.toLowerCase()} "${(el.textContent || el.getAttribute("aria-label") || "").trim().slice(0, 30)}" ${Math.round(el.getBoundingClientRect().width)}x${Math.round(el.getBoundingClientRect().height)}`).slice(0, 12),
        };
      }).catch((e) => ({ error: e.message }));
      if (mode !== "nojs") {
        await page.evaluate(() => scrollTo(0, 0));
        await page.addScriptTag({ content: AXE }).catch(() => {});
        entry.axe = await page.evaluate(async () => {
          if (!window.axe) return null;
          const r = await window.axe.run(document, { runOnly: { type: "tag", values: ["wcag2a", "wcag2aa", "wcag21a", "wcag21aa"] }, resultTypes: ["violations"] });
          return r.violations.map((x) => ({ id: x.id, impact: x.impact, n: x.nodes.length, first: x.nodes.slice(0, 3).map((nd) => nd.target.join(" ") + " :: " + (nd.failureSummary || "").split("\n").slice(1, 2).join(" ")) }));
        }).catch((e) => [{ id: "axe-error", impact: "?", n: 0, first: [e.message] }]);
      }
      log.push(entry);
      const ax = entry.axe ? entry.axe.map((x) => `${x.id}(${x.n})`).join(" ") : "-";
      console.log(`${slug.padEnd(28)} ${entry.viewport.padEnd(9)} ${mode.padEnd(6)} console ${entry.console.length} network ${entry.network.length} overflow ${entry.overflow} cls ${entry.cls} axe ${ax}`);
      await ctx.close();
    }
  }
}
writeFileSync(`${out}/log.json`, JSON.stringify(log, null, 1));
await browser.close();
