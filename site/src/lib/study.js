// The Shade Study drawing: the shadows a plot receives on 21 June, hour by
// hour, computed from the real sun (solar.js) and drawn as hairlines.
// One module for build time and run time: studySvg() returns markup, so a
// page without JavaScript still carries the finished drawing, and the
// scripts only animate what is already there (every outline has
// pathLength = 1, so drawing it is one dash offset from 1 to 0).
//
// A model (src/content/*.json): { w, h, casters: [...], highlight }, metres,
// x east, y north (plan up = north). A caster is a box footprint
// { box: [[x, y], ...], h }, a tree { circle: [cx, cy, r], h } (the crown
// casts from 55 % of its height up), or a raised roof { circle, h, roof: true }
// (its shadow is its own outline, displaced).
import { solarPosition, sunDir } from "./solar.js";

const DAY = { from: 6, to: 19 };

function hull(points) {
  const p = points.slice().sort((a, b) => a[0] - b[0] || a[1] - b[1]);
  const cross = (o, a, b) => (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]);
  const lower = [], upper = [];
  for (const q of p) { while (lower.length >= 2 && cross(lower[lower.length - 2], lower[lower.length - 1], q) <= 0) lower.pop(); lower.push(q); }
  for (let i = p.length - 1; i >= 0; i--) { const q = p[i]; while (upper.length >= 2 && cross(upper[upper.length - 2], upper[upper.length - 1], q) <= 0) upper.pop(); upper.push(q); }
  upper.pop(); lower.pop();
  return lower.concat(upper);
}

const circle = ([cx, cy, r], n = 32) =>
  Array.from({ length: n }, (_, i) => [cx + r * Math.cos((i / n) * 2 * Math.PI), cy + r * Math.sin((i / n) * 2 * Math.PI)]);

/** Shadow polygons (plan, metres) for local minute m on a date (default 21 June). */
export function shadowsAt(model, minutes, date = model.date || [2026, 6, 21]) {
  const { alt, az } = solarPosition(date[0], date[1], date[2], minutes, model.site || undefined);
  if (alt <= 2) return { alt, az, polys: [] };
  const d = sunDir(alt, az);               // +x east, +y up, +z south
  const k = 1 / Math.tan((alt * Math.PI) / 180);
  const hx = Math.hypot(d[0], d[2]) || 1;
  // Shadow displacement per metre of height, in plan (x east, y north).
  const sx = (-d[0] / hx) * k, sy = (d[2] / hx) * k;
  const polys = model.casters.map((c) => {
    const base = c.circle ? circle(c.circle) : c.box;
    const at = (f) => base.map(([x, y]) => [x + sx * c.h * f, y + sy * c.h * f]);
    if (c.roof) return at(1);
    if (c.circle) return hull(at(1).concat(at(0.55)));
    return hull(base.concat(at(1)));
  });
  return { alt, az, polys };
}

function inside(poly, x, y) {
  let c = false;
  for (let i = 0, j = poly.length - 1; i < poly.length; j = i++) {
    const [xi, yi] = poly[i], [xj, yj] = poly[j];
    if (yi > y !== yj > y && x < ((xj - xi) * (y - yi)) / (yj - yi) + xi) c = !c;
  }
  return c;
}

/** Shade hours per cell over the day (every 20 minutes, 06:00-19:00). */
export function shadeHours(model, step = 1) {
  const cols = Math.round(model.w / step), rows = Math.round(model.h / step);
  const hours = new Float32Array(cols * rows);
  const solid = new Uint8Array(cols * rows);
  for (let r = 0; r < rows; r++)
    for (let c = 0; c < cols; c++) {
      const x = (c + 0.5) * step, y = (r + 0.5) * step;
      if (model.casters.some((k) => !k.roof && !k.circle && inside(k.box, x, y))) solid[r * cols + c] = 1;
    }
  for (let m = DAY.from * 60; m <= DAY.to * 60; m += 20) {
    const { polys } = shadowsAt(model, m);
    if (!polys.length) continue;
    for (let r = 0; r < rows; r++)
      for (let c = 0; c < cols; c++) {
        const x = (c + 0.5) * step, y = (r + 0.5) * step;
        if (polys.some((p) => inside(p, x, y))) hours[r * cols + c] += 1 / 3;
      }
  }
  return { cols, rows, step, hours, solid };
}

const f1 = (v) => (Math.round(v * 10) / 10).toString();

/** Largest rectangle (plan metres) of cells that pass the model's highlight test. */
export function highlightRect(model) {
  const { cols, rows, step, hours, solid } = shadeHours(model, model.step || 1);
  const hl = model.highlight || { below: 1.5 };
  const ok = (c, r) => {
    if (c < 1 || r < 1 || c >= cols - 1 || r >= rows - 1) return false;
    const i = r * cols + c;
    return !solid[i] && (hl.below != null ? hours[i] < hl.below : hours[i] >= hl.above);
  };
  const h = new Int32Array(cols);
  let best = null, bestArea = 0;
  for (let r = 0; r < rows; r++) {
    for (let c = 0; c < cols; c++) h[c] = ok(c, r) ? h[c] + 1 : 0;
    const st = [];
    for (let c = 0; c <= cols; c++) {
      const hc = c < cols ? h[c] : 0;
      let s0 = c;
      while (st.length && st[st.length - 1][1] >= hc) {
        const [a, hh] = st.pop();
        const area = hh * (c - a);
        if (area > bestArea) { bestArea = area; best = { x: a * step, y: (r - hh + 1) * step, w: (c - a) * step, h: hh * step }; }
        s0 = a;
      }
      st.push([s0, hc]);
    }
  }
  return best;
}

/**
 * The drawing as SVG markup. Classes: .plot, .hour > .shadow (+ .hour-label),
 * .caster, .tree, .mark (the highlighted region), .north.
 */
export function studySvg(model, { sc = 20, id = "study", title = "", ground = false } = {}) {
  const W = model.w * sc, H = model.h * sc;
  const P = ([x, y]) => `${f1(x * sc)},${f1((model.h - y) * sc)}`;
  const path = (poly) => "M" + poly.map(P).join("L") + "Z";
  const out = [];
  out.push(`<svg class="study-svg" viewBox="-60 -40 ${W + 130} ${H + 90}" role="img" aria-labelledby="${id}-t" xmlns="http://www.w3.org/2000/svg">`);
  out.push(`<title id="${id}-t">${title || "Shadows on the site on 21 June, hour by hour, from the real sun."}</title>`);
  out.push(`<defs><clipPath id="${id}-clip"><rect x="0" y="0" width="${W}" height="${H}"/></clipPath></defs>`);
  // The highlight: the largest rectangle, set back 1 m from the boundary,
  // inside the part of the plot the shadows leave alone (or, for a roof that
  // is its own shade, inside the part they cover). A building's footprint.
  const R = highlightRect(model);
  const rx = R ? R.x * sc : 0, ry = R ? (model.h - R.y - R.h) * sc : 0, rw = R ? R.w * sc : 0, rh = R ? R.h * sc : 0;
  const d = R ? `M${f1(rx)},${f1(ry)}H${f1(rx + rw)}V${f1(ry + rh)}H${f1(rx)}Z` : "";
  // The sheet itself with that rectangle cut out of it (one even-odd path):
  // on the home page the hole becomes the aperture the first project is
  // seen through.
  if (ground) {
    out.push(`<path class="ground" fill-rule="evenodd" d="M-6000,-6000H6000V6000H-6000Z${d}"/>`);
    out.push(`<path class="plot-fill" fill-rule="evenodd" d="M0,0H${W}V${H}H0Z${d}"/>`);
    // Covers the hole until the aperture opens, so the drawing does not give it away.
    out.push(`<path class="hole-cover" d="${d || "M0 0"}"/>`);
  }
  out.push(`<g class="ink">`);
  out.push(`<rect class="plot" x="0" y="0" width="${W}" height="${H}"/>`);
  out.push(`<path class="mark" d="${d || "M0 0"}"${R ? ` data-x="${f1(rx)}" data-y="${f1(ry)}" data-w="${f1(rw)}" data-h="${f1(rh)}"` : ""}/>`);
  out.push(`<g class="hours" clip-path="url(#${id}-clip)">`);
  for (let hh = DAY.from; hh <= DAY.to; hh++) {
    const { polys, alt } = shadowsAt(model, hh * 60);
    if (!polys.length) continue;
    out.push(`<g class="hour" data-hour="${hh}" data-alt="${alt.toFixed(1)}">`);
    for (const p of polys) out.push(`<path class="shadow" pathLength="1" d="${path(p)}"/>`);
    // Label at the far tip of the longest shadow, kept inside the plot.
    let tip = null, best = -1;
    for (const p of polys)
      for (const q of p) {
        const cx = model.w / 2, cy = model.h / 2, dd = Math.hypot(q[0] - cx, q[1] - cy);
        if (dd > best) { best = dd; tip = q; }
      }
    const tx = Math.max(0.6, Math.min(model.w - 0.6, tip[0])), ty = Math.max(0.6, Math.min(model.h - 0.6, tip[1]));
    out.push(`<text class="hour-label" x="${f1(tx * sc)}" y="${f1((model.h - ty) * sc)}">${String(hh).padStart(2, "0")}</text>`);
    out.push(`</g>`);
  }
  out.push(`</g>`);
  for (const c of model.casters) {
    if (c.circle) out.push(`<circle class="${c.roof ? "roof" : "tree"}" cx="${f1(c.circle[0] * sc)}" cy="${f1((model.h - c.circle[1]) * sc)}" r="${f1(c.circle[2] * sc)}"/>`);
    else out.push(`<path class="caster" d="${path(c.box)}"/>`);
  }
  out.push(`<g class="north" transform="translate(${W + 34},18)"><path d="M0,24 L0,-6 M-5,2 L0,-8 L5,2"/><text x="0" y="42" text-anchor="middle">N</text></g>`);
  out.push(`<g class="scale" transform="translate(0,${H + 26})"><path d="M0,0 H${10 * sc} M0,-4 V4 M${10 * sc},-4 V4"/><text x="${5 * sc}" y="-8" text-anchor="middle">10 m</text></g>`);
  out.push(`</g></svg>`);
  return out.join("");
}

/** Centre (SVG user units) of the highlighted region, from its path. */
export function markCentre(svg) {
  const m = svg.querySelector(".mark");
  if (!m || !m.dataset.w) return null;
  const x = +m.dataset.x, y = +m.dataset.y, w = +m.dataset.w, h = +m.dataset.h;
  return { x: x + w / 2, y: y + h / 2, w, h };
}

/**
 * Drives a drawn study by progress 0..1: hour outlines draw in order
 * (06 -> 19), then the highlighted region fills. Returns the updater.
 */
export function studyDriver(svg) {
  const hours = [...svg.querySelectorAll(".hour")];
  const mark = svg.querySelector(".mark");
  const n = hours.length;
  hours.forEach((g) => g.querySelectorAll(".shadow").forEach((p) => { p.style.strokeDasharray = "1 1"; }));
  return (p) => {
    const draw = Math.min(1, p / 0.78);
    hours.forEach((g, i) => {
      const u = Math.max(0, Math.min(1, draw * n - i));
      g.querySelectorAll(".shadow").forEach((s) => { s.style.strokeDashoffset = String(1 - u); });
      g.style.opacity = u > 0 ? "1" : "0";
      g.classList.toggle("now", u > 0 && u < 1);
    });
    if (mark) mark.style.opacity = String(Math.max(0, Math.min(1, (p - 0.8) / 0.15)));
  };
}
