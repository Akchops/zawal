import { chromium } from "playwright";
const b = await chromium.launch({ executablePath: "/opt/pw-browsers/chromium-1194/chrome-linux/chrome" });
const p = await b.newPage({ viewport: { width: 390, height: 844 }, deviceScaleFactor: 2, isMobile: true, hasTouch: true });
await p.goto("http://localhost:4400/", { waitUntil: "load" });
await p.waitForTimeout(1500);
await p.addStyleTag({ content: ".screen{background:repeating-linear-gradient(45deg,#c33 0 10px,#fff 10px 20px)!important}.screen>*{display:none!important}" });
await p.evaluate(() => { const f = document.querySelector("[data-film]"); scrollTo(0, f.getBoundingClientRect().top + scrollY + 12.6 * 420); });
await p.waitForTimeout(1200);
await p.screenshot({ path: "/tmp/claude-0/-home-user-zawal/f0ae0003-c079-55c1-9d73-e8d3cc1745b7/scratchpad/qa/dbg.png" });
const info = await p.evaluate(() => {
  const svg = document.querySelector("[data-study] svg"); const r = svg.getBoundingClientRect();
  const L = document.querySelector('[data-layer="study"]').getBoundingClientRect();
  return { svg: [r.x, r.y, r.width, r.height], layer: [L.x, L.y, L.width, L.height], masks: document.querySelectorAll("mask").length };
});
console.log(JSON.stringify(info));
await b.close();
