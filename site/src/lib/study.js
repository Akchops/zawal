// The Shade Study drawing: the shadows a plot receives on 21 June, hour by
// hour, computed from the real sun (solar.js), drawn as hairlines on sand.
// The part of the plot the shade never reaches is outlined last: that is
// where the building goes ("the gaps between them become the building").
import { solarPosition, sunDir } from "./solar.js";

// A fictional Jumeirah plot, metres, x east, y north (plan up = north).
export const SITE = { w: 34, h: 30 };
export const NEIGHBOURS = [
  { name: "block", pts: [[23, 20], [34, 20], [34, 30], [23, 30]], h: 16 },   // tall block, north-east
  { name: "house", pts: [[0, 22], [9, 22], [9, 30], [0, 30]], h: 7.5 },      // neighbour's house, north-west
  { name: "wall", pts: [[0, 0], [0.5, 0], [0.5, 22], [0, 22]], h: 3.2 },     // boundary wall, west
  { name: "ghaf", circle: [17, 11, 4.2], h: 7 },                             // the old ghaf on the plot
];

function hull(points) {
  const p = points.slice().sort((a, b) => a[0] - b[0] || a[1] - b[1]);
  const cross = (o, a, b) => (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]);
  const lower = [], upper = [];
  for (const q of p) { while (lower.length >= 2 && cross(lower[lower.length - 2], lower[lower.length - 1], q) <= 0) lower.pop(); lower.push(q); }
  for (let i = p.length - 1; i >= 0; i--) { const q = p[i]; while (upper.length >= 2 && cross(upper[upper.length - 2], upper[upper.length - 1], q) <= 0) upper.pop(); upper.push(q); }
  upper.pop(); lower.pop();
  return lower.concat(upper);
}

/** Shadow polygons (plan, metres) for local minute m on 21 June. */
export function shadowsAt(minutes) {
  const { alt, az } = solarPosition(2026, 6, 21, minutes);
  if (alt <= 2) return { alt, az, polys: [] };
  const d = sunDir(alt, az);               // +x east, +y up, +z south
  const k = 1 / Math.tan((alt * Math.PI) / 180);
  // Shadow displacement per metre of height, in plan (x east, y north).
  const sx = -d[0] / Math.hypot(d[0], d[2]) * k, sy = d[2] / Math.hypot(d[0], d[2]) * k;
  const polys = NEIGHBOURS.map((n) => {
    const base = n.circle
      ? Array.from({ length: 24 }, (_, i) => [n.circle[0] + n.circle[2] * Math.cos((i / 24) * 2 * Math.PI), n.circle[1] + n.circle[2] * Math.sin((i / 24) * 2 * Math.PI)])
      : n.pts;
    // A tree's crown casts from its crown height; a box from its footprint.
    const h = n.h, lift = n.circle ? 0.55 : 0;
    const top = base.map(([x, y]) => [x + sx * h, y + sy * h]);
    const bot = base.map(([x, y]) => [x + sx * h * lift, y + sy * h * lift]);
    return hull(n.circle ? top.concat(bot) : base.concat(top));
  });
  return { alt, az, polys };
}

/** Grid estimate of shade hours per cell over the day (06:00-19:00). */
export function shadeHours(step = 1) {
  const cols = Math.round(SITE.w / step), rows = Math.round(SITE.h / step);
  const hours = new Float32Array(cols * rows);
  const inside = (poly, x, y) => {
    let c = false;
    for (let i = 0, j = poly.length - 1; i < poly.length; j = i++) {
      const [xi, yi] = poly[i], [xj, yj] = poly[j];
      if (yi > y !== yj > y && x < ((xj - xi) * (y - yi)) / (yj - yi) + xi) c = !c;
    }
    return c;
  };
  for (let m = 360; m <= 1140; m += 20) {
    const { polys } = shadowsAt(m);
    for (let r = 0; r < rows; r++)
      for (let c = 0; c < cols; c++) {
        const x = (c + 0.5) * step, y = (r + 0.5) * step;
        if (polys.some((p) => inside(p, x, y))) hours[r * cols + c] += 1 / 3;
      }
  }
  return { cols, rows, hours };
}

const path = (poly, sc) => "M" + poly.map(([x, y]) => `${(x * sc).toFixed(1)},${((SITE.h - y) * sc).toFixed(1)}`).join("L") + "Z";

/** Builds the SVG: hour outlines (drawn by scroll), neighbours, and the gap. */
export function buildStudySvg(svg) {
  const sc = 20;
  svg.setAttribute("viewBox", `-60 -40 ${SITE.w * sc + 120} ${SITE.h * sc + 100}`);
  const ns = "http://www.w3.org/2000/svg";
  const el = (name, attrs, parent = svg) => { const e = document.createElementNS(ns, name); for (const k in attrs) e.setAttribute(k, attrs[k]); parent.appendChild(e); return e; };
  const defs = el("defs", {});
  const clip = el("clipPath", { id: "plot-clip" }, defs);
  el("rect", { x: 0, y: 0, width: SITE.w * sc, height: SITE.h * sc }, clip);
  el("rect", { x: 0, y: 0, width: SITE.w * sc, height: SITE.h * sc, class: "plot" });
  const hoursG = el("g", { class: "hours", "clip-path": "url(#plot-clip)" });
  const lines = [];
  for (let hh = 6; hh <= 19; hh++) {
    const { polys, alt } = shadowsAt(hh * 60);
    if (!polys.length) continue;
    const g = el("g", { class: "hour", "data-hour": hh }, hoursG);
    for (const p of polys) lines.push({ hour: hh, el: el("path", { d: path(p, sc), class: "shadow" }, g) });
    // label at the tip of the tall block's shadow
    const tip = polys[0].reduce((a, b) => (Math.hypot(b[0] - 28, b[1] - 25) > Math.hypot(a[0] - 28, a[1] - 25) ? b : a));
    const t = el("text", { x: (Math.max(0, Math.min(SITE.w, tip[0])) * sc).toFixed(0), y: ((SITE.h - Math.max(0, Math.min(SITE.h, tip[1]))) * sc).toFixed(0), class: "hour-label" }, g);
    t.textContent = String(hh).padStart(2, "0");
    g.dataset.alt = alt.toFixed(1);
  }
  for (const n of NEIGHBOURS) {
    if (n.circle) el("circle", { cx: n.circle[0] * sc, cy: (SITE.h - n.circle[1]) * sc, r: n.circle[2] * sc, class: "tree" });
    else el("path", { d: path(n.pts, sc), class: "neighbour" });
  }
  // The gap: cells shaded for fewer than 3 hours, as one outlined region.
  const { cols, rows, hours } = shadeHours(1);
  const gap = el("g", { class: "gap" });
  for (let r = 0; r < rows; r++)
    for (let c = 0; c < cols; c++)
      if (hours[r * cols + c] < 3 && c > 1 && r < 21) el("rect", { x: c * sc, y: (SITE.h - r - 1) * sc, width: sc + 0.5, height: sc + 0.5 }, gap);
  const north = el("g", { class: "north", transform: `translate(${SITE.w * sc + 30},20)` });
  el("path", { d: "M0,22 L0,-6 M-5,2 L0,-8 L5,2", class: "north-arrow" }, north);
  const nt = el("text", { x: 0, y: 40, class: "hour-label", "text-anchor": "middle" }, north);
  nt.textContent = "N";
  return { lines, gap };
}
