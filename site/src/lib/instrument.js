// The Shade Study instrument: pick a date and a minute and the courtyard is
// relit live (sun.js); drag across it and the date and time follow (the
// inverse solar search). The plot below is recomputed for the chosen day.
// Without WebGL the numbers and the plot still work; the picture stays still.
import { SunStage, sunFromPointer } from "./sun.js";
import { solarPosition, whenIsTheSun, clock } from "./solar.js";
import { studySvg } from "./study.js";
import study from "../content/study.json";

const root = document.querySelector("[data-instrument]");
if (root) init();

function init() {
  const $ = (s) => root.querySelector(s);
  const stageEl = $("[data-inst-stage]"), canvas = $("[data-inst-canvas]"), still = root.querySelector(".inst-still");
  const month = $("[data-inst-month]"), day = $("[data-inst-day]"), time = $("[data-inst-time]"), out = $("[data-inst-clock]");
  const read = $("[data-inst-read]"), when = $("[data-inst-when]"), plot = $("[data-inst-plot]"), plotDate = $("[data-inst-plot-date]");
  const MONTHS = [...month.options].map((o) => o.textContent);
  const Y = 2026;
  const daysIn = (m) => new Date(Date.UTC(Y, m, 0)).getUTCDate();
  const riseSet = (m, d) => {
    const alt = (t) => solarPosition(Y, m, d, t).alt;
    const cross = (a, b, up) => { for (let k = 0; k < 30; k++) { const c = (a + b) / 2; (alt(c) > 0) === up ? (b = c) : (a = c); } return (a + b) / 2; };
    return [cross(240, 720, true), cross(720, 1320, false)];
  };

  const portrait = stageEl.clientHeight > stageEl.clientWidth;
  const stage = new SunStage({ canvas, base: `/sun/sun-${portrait ? "p" : "l"}.json` });
  let live = false;
  // No live stage (no WebGL, or it failed): the still stays, so swap its light
  // first-paint version for the sharp one.
  const sharpen = () => { for (const el of still.querySelectorAll("[data-srcset]")) el.srcset = el.dataset.srcset; };
  stage.start().then((ok) => {
    if (!ok) return sharpen();
    live = true;
    canvas.hidden = false;
    still.style.visibility = "hidden";
    update();
  }).catch(sharpen);

  let plotKey = "";
  const drawPlot = (m, d, minutes) => {
    const key = `${m}-${d}`;
    if (key !== plotKey) {
      plotKey = key;
      plot.innerHTML = studySvg({ ...study.home, date: [Y, m, d] }, { id: "ss-study", title: `The plot on ${d} ${MONTHS[m - 1]}: its shadows hour by hour.` });
      plotDate.textContent = `${d} ${MONTHS[m - 1]}`;
    }
    const hour = Math.round(minutes / 60);
    for (const g of plot.querySelectorAll(".hour")) g.classList.toggle("now", +g.dataset.hour === hour);
  };

  function update(label) {
    const m = +month.value;
    if (+day.value > daysIn(m)) day.value = String(daysIn(m));
    const d = +day.value;
    const [rise, set] = riseSet(m, d);
    time.min = String(Math.ceil(rise + 2));
    time.max = String(Math.floor(set - 2));
    const minutes = Math.max(+time.min, Math.min(+time.max, +time.value));
    time.value = String(minutes);
    out.textContent = clock(minutes);
    const s = solarPosition(Y, m, d, minutes);
    read.textContent = `${d} ${MONTHS[m - 1].slice(0, 3)} · ${clock(minutes)} · alt ${s.alt.toFixed(1)}° · az ${s.az.toFixed(1)}°`;
    when.textContent = label || `Shadows are ${(1 / Math.tan((Math.max(s.alt, 0.5) * Math.PI) / 180)).toFixed(2)} m long for every metre of height.`;
    if (live) { stage.setSun(s.alt, s.az); stage.draw(); }
    drawPlot(m, d, minutes);
  }

  month.addEventListener("change", () => update());
  day.addEventListener("change", () => update());
  time.addEventListener("input", () => update());

  // Dragging the sun: the pointer is a sun; the controls follow when some
  // day in Dubai has it.
  let dragging = false, last = 0;
  const fromPointer = (e) => {
    const r = stageEl.getBoundingClientRect();
    const x = Math.max(0, Math.min(1, (e.clientX - r.left) / r.width)), y = Math.max(0, Math.min(1, (e.clientY - r.top) / r.height));
    const s = sunFromPointer(x, y);
    const now = performance.now();
    if (now - last < 40) return;
    last = now;
    const w = whenIsTheSun(s.alt, s.az);
    if (w) {
      month.value = String(w.month); day.value = String(w.day); time.value = String(Math.round(w.minutes));
      update(`Your sun: ${w.label}.`);
    } else {
      if (live) { stage.setSun(s.alt, s.az); stage.draw(); }
      read.textContent = `alt ${s.alt.toFixed(1)}° · az ${s.az.toFixed(1)}°`;
      when.textContent = "No day in Dubai has this sun.";
    }
  };
  stageEl.addEventListener("pointerdown", (e) => { dragging = true; if (e.pointerType === "mouse") fromPointer(e); });
  stageEl.addEventListener("pointermove", (e) => { if (e.pointerType === "mouse" ? dragging : dragging && Math.abs(e.movementX) > Math.abs(e.movementY)) fromPointer(e); }, { passive: true });
  addEventListener("pointerup", (e) => { if (dragging && e.pointerType !== "mouse") fromPointer(e); dragging = false; });
  addEventListener("resize", () => { if (live) { stage.dirty = true; stage.draw(); } });
  update();
}
