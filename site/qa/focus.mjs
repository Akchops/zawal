// Keyboard pass: Tab through a page and, for every stop, check the focused
// element is on screen, not covered by something else, and drawn with a
// visible focus indicator. Writes one crop per stop and prints a table.
//   node qa/focus.mjs <url> <outDir> <W>x<H> [stops=24]
import { chromium } from "playwright";
import { mkdirSync } from "node:fs";

const [,, url, out, size, stopsS = "24"] = process.argv;
const [w, h] = size.split("x").map(Number);
mkdirSync(out, { recursive: true });
const browser = await chromium.launch({ executablePath: "/opt/pw-browsers/chromium-1194/chrome-linux/chrome",
  args: ["--use-gl=angle", "--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--ignore-gpu-blocklist"] });
const page = await browser.newPage({ viewport: { width: w, height: h }, isMobile: w < 768, hasTouch: w < 768 });
await page.goto(url, { waitUntil: "load" });
await page.waitForTimeout(1500);
let bad = 0;
for (let k = 0; k < Number(stopsS); k++) {
  await page.keyboard.press("Tab");
  await page.waitForTimeout(250);
  const r = await page.evaluate(() => {
    const el = document.activeElement;
    if (!el || el === document.body) return null;
    const b = el.getBoundingClientRect();
    const cs = getComputedStyle(el);
    const cx = Math.min(innerWidth - 1, Math.max(0, b.left + b.width / 2)), cy = Math.min(innerHeight - 1, Math.max(0, b.top + b.height / 2));
    const top = document.elementFromPoint(cx, cy);
    return {
      label: `${el.tagName.toLowerCase()}${el.id ? "#" + el.id : ""} "${(el.getAttribute("aria-label") || el.textContent || el.value || "").trim().replace(/\s+/g, " ").slice(0, 32)}"`,
      rect: [Math.round(b.left), Math.round(b.top), Math.round(b.width), Math.round(b.height)],
      onScreen: b.bottom > 0 && b.top < innerHeight && b.right > 0 && b.left < innerWidth && b.width > 0,
      covered: !!top && top !== el && !el.contains(top) && !top.contains(el),
      coveredBy: top && top !== el && !el.contains(top) ? `${top.tagName.toLowerCase()}.${String(top.className).split(" ")[0]}` : "",
      ring: cs.outlineStyle !== "none" && parseFloat(cs.outlineWidth) > 0 ? `outline ${cs.outlineWidth} ${cs.outlineColor}` : (cs.boxShadow !== "none" ? "box-shadow" : "NONE"),
    };
  });
  if (!r) { console.log(`${String(k + 1).padStart(2)} (body)`); continue; }
  const problems = [!r.onScreen && "off-screen", r.covered && `covered by ${r.coveredBy}`, r.ring === "NONE" && "no ring"].filter(Boolean);
  if (problems.length) bad++;
  console.log(`${String(k + 1).padStart(2)} ${r.label.padEnd(44)} ${r.ring.padEnd(34)} ${problems.join(", ") || "ok"}`);
  if (r.onScreen) {
    const [x, y, bw, bh] = r.rect;
    const clip = { x: Math.max(0, x - 24), y: Math.max(0, y - 24), width: Math.min(w - Math.max(0, x - 24), bw + 48), height: Math.min(h - Math.max(0, y - 24), bh + 48) };
    if (clip.width > 4 && clip.height > 4) await page.screenshot({ path: `${out}/stop-${String(k + 1).padStart(2, "0")}.png`, clip });
  }
}
console.log(bad ? `${bad} stop(s) with problems` : "every stop visible with a focus ring");
await browser.close();
