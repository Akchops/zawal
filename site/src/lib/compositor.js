// One canvas for everything after the day, three jobs, one shader:
//
//  * morph (signature D): project A's sunlit patches (its sun-visibility
//    mask, as a signed distance field) turn into A's pattern of light filling
//    the screen (the ghaf's dapple, the palm ribs' lines, the canopy's
//    triangles, the carved stars); that pattern morphs into project B's; B's
//    pattern then gathers back into B's own sunlit patches as B appears. The
//    shade goes dark while the light carries the change. Continuous,
//    reversible, never a cross-fade of pixels.
//  * close: the carved screen appears over the last image and its holes close
//    one by one into solid shade.
//  * fade: two frames blended in linear light (the night lanterns), times a
//    gain (the courtyard emerging from the dark).
//
// Tiers: WebGL, else the same rules on the CPU at 1/4 size with Canvas 2D.
import { FULLSCREEN_VS, coverUv, fullscreenTriangle, program, texture, webglTier } from "./gl.js";
import { url } from "./paths.js";

const FS = `
precision highp float;
varying vec2 vUv;             // 0..1 across the canvas, y up
uniform sampler2D uA, uB;     // the outgoing and incoming images (sRGB)
uniform sampler2D uMA, uMB;   // their sun masks: signed distance to the lit edge, 1/4 size
uniform vec4 uCover;          // cover-crop of the image into the canvas (xy scale, zw offset)
uniform float uMode;          // 0 morph (D), 1 close the screen (method), 2 fade (night)
uniform float uT;             // progress 0..1 of the current mode
uniform float uGain;          // fade mode: 0 = all ink, 1 = the image (the courtyard emerging)
uniform float uCell;          // close mode: lattice cell size, as a fraction of the canvas height
uniform float uAspect;        // canvas width / height
uniform float uPatA, uPatB;   // morph: the two projects' patterns, 0 dapple, 1 slats, 2 triangles, 3 stars
uniform float uPeriods;       // morph: pattern periods across the screen height
uniform float uMaskPx;        // morph: one mask pixel, in screen heights
uniform float uPx;            // one canvas pixel, in screen heights (for antialiasing)
uniform vec3 uInk;            // the page's shade colour (#0f1418) in linear light

// Images are stored top row first; uv has y up.
vec4 img(sampler2D s, vec2 uv) { return texture2D(s, vec2(uv.x, 1.0 - uv.y)); }
// sRGB <-> linear, the same smooth fits as player.js (no crease at the curve's joint).
vec3 lin(vec3 c) { return c * (c * (c * 0.305306011 + 0.682171111) + 0.012522878); }
vec3 srgb(vec3 c) {
  vec3 s1 = sqrt(c), s2 = sqrt(s1), s3 = sqrt(s2);
  return clamp(0.585122381 * s1 + 0.783140355 * s2 - 0.368262736 * s3, 0.0, 1.0);
}
// Mask texel -> signed distance in mask pixels: stored as 128 + 4 d, so
// (v * 255 - 128) / 4. Negative = in sun, positive = in shade.
float sdfAt(sampler2D m, vec2 uv) { return (img(m, uv).r * 255.0 - 128.0) * 0.25; }

// Random numbers per cell (fract-sin: one value per input, so no lattice pattern arises).
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
vec2 hash2(vec2 p) { return fract(sin(vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)))) * 43758.5453); }

// ---- the four patterns of light: signed distance in pattern units (period 1), < 0 = lit ----
float sdBox2(vec2 p, vec2 b) { vec2 d = abs(p) - b; return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0); }
// The carved screen's star (renderer/src/patterns.h PAT_STAR8), c centred in its cell.
float starSD(vec2 c) {
  const float A = 0.31, Bc = 0.11, C = 0.012, K = 0.70710678;   // star size, corner diamond, rounding, cos 45
  float r1 = sdBox2(c, vec2(A - C)) - C;                                            // square
  float r2 = sdBox2(vec2(K * (c.x + c.y), K * (c.y - c.x)), vec2(A - C)) - C;        // the square at 45 deg
  vec2 k = c - vec2(c.x > 0.0 ? 0.5 : -0.5, c.y > 0.0 ? 0.5 : -0.5);                // nearest cell corner
  float corner = sdBox2(vec2(K * (k.x + k.y), K * (k.y - k.x)), vec2(Bc)) - 0.25 * C; // corner diamond
  return min(min(r1, r2), corner);
}
// Ghaf House: sun-flecks through a feathery crown. One fleck per cell,
// jittered (0.7 of a cell), radius 0.16..0.38, a little flattened (1.25) as
// the sun's disc lands at an angle. The 3x3 search finds the nearest fleck.
float patDapple(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  float d = 1e9;
  for (int y = -1; y <= 1; y++)
    for (int x = -1; x <= 1; x++) {
      vec2 g = vec2(float(x), float(y));
      vec2 h = hash2(i + g);
      vec2 c = g + 0.5 + (h - 0.5) * 0.7;
      float r = 0.16 + 0.22 * h.x;
      d = min(d, length((f - c) * vec2(1.0, 1.25)) - r);
    }
  return d;
}
// Hotel Sikka: the palm ribs' lines. Lit bands 0.17 wide every 0.42 (the gaps
// between ribs), cut by a dark batten 0.14 wide every 2.6.
float patSlats(vec2 p) {
  float band = abs(fract(p.y / 0.42) - 0.5) * 0.42 - 0.085;     // distance to the band centre - half-width
  float batten = 0.07 - abs(fract(p.x / 2.6) - 0.5) * 2.6;      // > 0 under a batten
  return max(band, batten);                                     // lit: in a band AND not under a batten
}
// Qudra Canopy: small triangles of light. Skew into a triangular lattice of
// side 1 (this takes a unit equilateral grid to the unit square), find the
// barycentric coordinates in the triangle the point falls in; the smallest
// is 0 on an edge and 1/3 at the centre. Lit where it is more than 0.11 in
// from every edge; x 0.866 (the triangle's height) turns it into distance.
float patTriangles(vec2 p) {
  vec2 q = vec2(p.x - p.y * 0.57735, p.y * 1.1547);            // 1/sqrt(3), 2/sqrt(3)
  vec2 f = fract(q);
  float s = f.x + f.y;
  vec3 b = s < 1.0 ? vec3(f.x, f.y, 1.0 - s) : vec3(1.0 - f.x, 1.0 - f.y, s - 1.0);
  return (0.11 - min(b.x, min(b.y, b.z))) * 0.866;
}
// Mushrif: the carved stars.
float patStars(vec2 p) { return starSD(fract(p) - 0.5); }
float pattern(float id, vec2 p) {
  if (id < 0.5) return patDapple(p);
  if (id < 1.5) return patSlats(p);
  if (id < 2.5) return patTriangles(p);
  return patStars(p);
}

void main() {
  vec2 uv = vUv * uCover.xy + uCover.zw;          // canvas -> image (cover-crop)
  vec3 A = lin(img(uA, uv).rgb);
  vec3 col;
  if (uMode < 0.5) {
    // ---- D: morph through the shadow ----
    vec3 B = lin(img(uB, uv).rgb);
    float t = uT;
    // Every field below is in screen heights, so shapes from the masks and
    // from the patterns can be blended edge to edge.
    float fAm = sdfAt(uMA, uv) * uMaskPx;          // A's own sunlit patches
    float fBm = sdfAt(uMB, uv) * uMaskPx;          // B's
    vec2 pp = vec2((vUv.x - 0.5) * uAspect, vUv.y - 0.5) * uPeriods;   // pattern space, centred on the screen
    float fAp = pattern(uPatA, pp) / uPeriods;     // A's pattern, full screen
    float fBp = pattern(uPatB, pp) / uPeriods;     // B's
    // Three overlapping stages: A's patches become A's pattern (w1), that
    // pattern becomes B's (w2), B's pattern gathers into B's patches (w3).
    // Linear blends of distance fields move every edge continuously.
    float w1 = smoothstep(0.12, 0.34, t);
    float w2 = smoothstep(0.38, 0.62, t);
    float w3 = smoothstep(0.66, 0.88, t);
    float f = mix(mix(mix(fAm, fAp, w1), fBp, w2), fBm, w3);
    // Edge width: 2 canvas pixels either side, a little softer than a vector
    // edge, as sunlight's own penumbra is.
    float lit = 1.0 - smoothstep(-2.0 * uPx, 2.0 * uPx, f);
    // The shade goes dark first (0 -> 0.22) and comes back last (0.78 -> 1).
    float kd = smoothstep(0.0, 0.22, t) * (1.0 - smoothstep(0.78, 1.0, t));
    // Inside the light: the images' sunlit colour at either end; while the
    // light is pure pattern (w1 up, w3 not yet) it is sunlight itself, warm,
    // a touch brighter toward the top of the screen (light from above).
    float pure = w1 * (1.0 - w3);
    vec3 sunC = vec3(1.0, 0.84, 0.62) * (0.78 + 0.3 * vUv.y);
    vec3 litC = mix(mix(A, B, w2), sunC, 0.82 * pure);
    // Outside it: the image's shade (A until the middle, B after), darkened to ink by kd.
    vec3 shade = mix(t < 0.5 ? A : B, uInk, kd);
    col = mix(shade, litC, lit);
  } else if (uMode < 1.5) {
    // ---- Method: the carved screen appears, then its holes close one by one ----
    vec2 p = vec2(vUv.x * uAspect, vUv.y) / uCell;  // one lattice cell per uCell of the height
    vec2 cell = floor(p);
    vec2 c = p - cell - 0.5;                        // position in the cell, centred
    // The screen arrives (0 -> 0.14): every hole shrinks from its whole cell
    // (threshold 0.75) to its star (0.06).
    float thrG = mix(0.75, 0.06, smoothstep(0.0, 0.14, uT));
    // Each star then closes at its own moment tau (0.16..0.74) over 0.2 of
    // progress, down to -0.33, below the star's deepest point: nothing left.
    float tau = 0.16 + 0.58 * hash(cell);
    float thrC = mix(0.06, -0.33, smoothstep(tau, tau + 0.2, uT));
    float thr = min(thrG, thrC);
    float hole = 1.0 - smoothstep(thr - 0.015, thr + 0.015, starSD(c));   // antialiased edge
    col = mix(uInk, A, hole);                       // outside the holes: solid shade
  } else {
    // ---- Night: two states blended in linear light (a lantern brightening),
    // the whole courtyard emerging from the page's shade colour by uGain ----
    vec3 B = lin(img(uB, uv).rgb);
    col = mix(A, B, uT) * uGain + uInk * (1.0 - uGain);
  }
  // Back to sRGB, dithered by +-half an 8-bit step so dark gradients do not band.
  gl_FragColor = vec4(srgb(col) + (hash(gl_FragCoord.xy) - 0.5) / 255.0, 1.0);
}`;

const INK = [0.0056, 0.0070, 0.0086];     // --shade #0f1418 in linear light
export const PATTERNS = { dapple: 0, slats: 1, triangles: 2, carved: 3, stars: 3 };

// ---- the same patterns for the Canvas 2D tier (pattern units, < 0 = lit) ----
const fract = (x) => x - Math.floor(x);
const hash1 = (x, y) => fract(Math.sin(x * 127.1 + y * 311.7) * 43758.5453);
const hash2b = (x, y) => fract(Math.sin(x * 269.5 + y * 183.3) * 43758.5453);
const sdBox = (px, py, bx, by) => {
  const dx = Math.abs(px) - bx, dy = Math.abs(py) - by;
  return Math.hypot(Math.max(dx, 0), Math.max(dy, 0)) + Math.min(Math.max(dx, dy), 0);
};
function starSD(cx, cy) {
  const A = 0.31, Bc = 0.11, C = 0.012, K = Math.SQRT1_2;
  const r1 = sdBox(cx, cy, A - C, A - C) - C;
  const r2 = sdBox(K * (cx + cy), K * (cy - cx), A - C, A - C) - C;
  const kx = cx - (cx > 0 ? 0.5 : -0.5), ky = cy - (cy > 0 ? 0.5 : -0.5);
  const corner = sdBox(K * (kx + ky), K * (ky - kx), Bc, Bc) - 0.25 * C;
  return Math.min(r1, r2, corner);
}
function pattern2d(id, x, y) {
  if (id === 0) {
    const ix = Math.floor(x), iy = Math.floor(y), fx = x - ix, fy = y - iy;
    let d = 1e9;
    for (let j = -1; j <= 1; j++)
      for (let i = -1; i <= 1; i++) {
        const hx = hash1(ix + i, iy + j), hy = hash2b(ix + i, iy + j);
        const cx = i + 0.5 + (hx - 0.5) * 0.7, cy = j + 0.5 + (hy - 0.5) * 0.7;
        d = Math.min(d, Math.hypot(fx - cx, (fy - cy) * 1.25) - (0.16 + 0.22 * hx));
      }
    return d;
  }
  if (id === 1) return Math.max(Math.abs(fract(y / 0.42) - 0.5) * 0.42 - 0.085, 0.07 - Math.abs(fract(x / 2.6) - 0.5) * 2.6);
  if (id === 2) {
    const qx = fract(x - y * 0.57735), qy = fract(y * 1.1547), s = qx + qy;
    const m = s < 1 ? Math.min(qx, qy, 1 - s) : Math.min(1 - qx, 1 - qy, s - 1);
    return (0.11 - m) * 0.866;
  }
  return starSD(fract(x) - 0.5, fract(y) - 0.5);
}
const sm = (a, b, x) => { const u = Math.max(0, Math.min(1, (x - a) / (b - a))); return u * u * (3 - 2 * u); };

export class Compositor {
  constructor(canvas) {
    this.canvas = canvas;
    this.images = new Map();       // url -> ImageBitmap | Promise | "failed"
    this.tex = new Map();
    this.tier = webglTier();
    this.state = null;
    this.dirty = true;
  }

  init() {
    if (this.gl || this.ctx) return;
    if (this.tier === "webgl") {
      const gl = this.canvas.getContext("webgl", { antialias: false, alpha: false });
      if (gl) {
        this.gl = gl;
        fullscreenTriangle(gl);
        this.prog = program(gl, FULLSCREEN_VS, FS);
        this.canvas.addEventListener("webglcontextlost", (e) => { e.preventDefault(); this.gl = null; this.tex.clear(); this.tier = "2d"; this.dirty = true; this.wake?.(); });
        return;
      }
      this.tier = "2d";
    }
    this.ctx = this.canvas.getContext("2d");
  }

  /** Starts loading an image; resolves to the bitmap. Masks keep raw bytes. */
  load(src, raw = false) {
    let v = this.images.get(src);
    if (!v) {
      v = fetch(url(src)).then((r) => { if (!r.ok) throw new Error(src); return r.blob(); })
        .then((b) => createImageBitmap(b, raw ? { colorSpaceConversion: "none", premultiplyAlpha: "none" } : {}))
        .then((bm) => { this.images.set(src, bm); this.dirty = true; this.wake?.(); return bm; })
        .catch(() => { this.images.set(src, "failed"); this.dirty = true; this.wake?.(); return null; });
      this.images.set(src, v);
    }
    return v;
  }
  has(src) { const v = this.images.get(src); return !!v && v !== "failed" && !(v instanceof Promise); }
  failed(src) { return this.images.get(src) === "failed"; }
  bitmap(src) { return this.has(src) ? this.images.get(src) : null; }

  resize() {
    const r = this.canvas.getBoundingClientRect();
    const cap = this.tier === "webgl" ? (this.maxDpr || 1.5) : 1;
    const dpr = Math.min(devicePixelRatio || 1, cap);
    const w = Math.max(1, Math.round(r.width * dpr)), h = Math.max(1, Math.round(r.height * dpr));
    if (this.canvas.width !== w || this.canvas.height !== h) { this.canvas.width = w; this.canvas.height = h; this.dirty = true; }
  }

  /** What to draw: {mode: "morph"|"close"|"fade", a, b, ma, mb, pa, pb, t, gain}. */
  set(state) {
    const s = this.state;
    if (s && s.mode === state.mode && s.a === state.a && s.b === state.b && Math.abs(s.t - state.t) < 1e-4 && (s.gain ?? 1) === (state.gain ?? 1)) return;
    this.state = state;
    this.dirty = true;
  }

  glTex(src) {
    let t = this.tex.get(src);
    if (!t) {
      const bm = this.bitmap(src);
      if (!bm) return null;
      t = texture(this.gl, bm, { linear: true });
      this.tex.set(src, t);
    }
    return t;
  }

  draw() {
    if (!this.dirty || !this.state) return;
    this.init();
    this.resize();
    const s = this.state;
    const need = [s.a, s.b, s.ma, s.mb].filter(Boolean);
    if (need.some((k) => this.failed(k))) { this.dirty = false; return; }   // missing asset: stay as is
    if (!need.every((k) => this.has(k))) { need.forEach((k) => this.load(k, k === s.ma || k === s.mb)); return; }
    this.dirty = false;
    if (this.gl) return this.drawGL(s);
    this.draw2D(s);
  }

  // Pattern scale: periods across the screen height (bolder on phones).
  periods() { return this.canvas.width / this.canvas.height < 1 ? 8.5 : 6.5; }

  drawGL(s) {
    const gl = this.gl, { p, uni } = this.prog;
    const A = this.bitmap(s.a);
    gl.viewport(0, 0, this.canvas.width, this.canvas.height);
    gl.useProgram(p);
    const bind = (u, src, unit) => { gl.activeTexture(gl.TEXTURE0 + unit); gl.bindTexture(gl.TEXTURE_2D, this.glTex(src)); gl.uniform1i(u, unit); };
    bind(uni.uA, s.a, 0);
    bind(uni.uB, s.b || s.a, 1);
    bind(uni.uMA, s.ma || s.a, 2);
    bind(uni.uMB, s.mb || s.ma || s.a, 3);
    const mode = s.mode === "morph" ? 0 : s.mode === "close" ? 1 : 2;
    const view = this.canvas.width / this.canvas.height;
    const cover = coverUv(A.width / A.height, view);
    const M = s.ma ? this.bitmap(s.ma) : null;
    gl.uniform1f(uni.uMode, mode);
    gl.uniform1f(uni.uT, s.t);
    gl.uniform1f(uni.uGain, s.gain ?? 1);
    gl.uniform1f(uni.uAspect, view);
    gl.uniform1f(uni.uCell, view < 1 ? 0.075 : 0.105);
    gl.uniform1f(uni.uPatA, s.pa ?? 3);
    gl.uniform1f(uni.uPatB, s.pb ?? 3);
    gl.uniform1f(uni.uPeriods, this.periods());
    gl.uniform1f(uni.uMaskPx, M ? 1 / (M.height * cover[1]) : 0.01);
    gl.uniform1f(uni.uPx, 1 / this.canvas.height);
    gl.uniform3f(uni.uInk, INK[0], INK[1], INK[2]);
    gl.uniform4fv(uni.uCover, cover);
    gl.drawArrays(gl.TRIANGLES, 0, 3);
  }

  // Canvas 2D: the same rules, evaluated on a 1/4-size grid on the CPU.
  draw2D(s) {
    const ctx = this.ctx, cw = this.canvas.width, ch = this.canvas.height;
    const A = this.bitmap(s.a), B = s.b ? this.bitmap(s.b) : null;
    const cover = (bm) => {
      const k = Math.max(cw / bm.width, ch / bm.height);
      const w = bm.width * k, h = bm.height * k;
      return [(cw - w) / 2, (ch - h) / 2, w, h];
    };
    ctx.globalAlpha = 1;
    ctx.globalCompositeOperation = "source-over";
    if (s.mode === "fade") {
      ctx.drawImage(A, ...cover(A));
      if (B && s.t > 0.004) { ctx.globalAlpha = s.t; ctx.drawImage(B, ...cover(B)); ctx.globalAlpha = 1; }
      const g = s.gain ?? 1;
      if (g < 1) { ctx.fillStyle = `rgba(15,20,24,${1 - g})`; ctx.fillRect(0, 0, cw, ch); }
      return;
    }
    if (s.mode === "close") {
      // The screen's holes as shrinking squares of shade (the star's outline
      // is left to the WebGL tier; the order and rhythm are the same).
      ctx.drawImage(A, ...cover(A));
      const cell = Math.round(ch * (cw / ch < 1 ? 0.075 : 0.105));
      ctx.fillStyle = "#0f1418";
      for (let y = 0; y < ch; y += cell)
        for (let x = 0; x < cw; x += cell) {
          const tau = 0.16 + 0.58 * hash1(Math.floor(x / cell), Math.floor(y / cell));
          const shut = sm(tau, tau + 0.2, s.t) * 0.85 + 0.15 * sm(0, 0.14, s.t);
          if (shut <= 0) continue;
          const b = (cell * shut) / 2 + 0.5;
          ctx.fillRect(x, y, cell, b); ctx.fillRect(x, y + cell - b, cell, b);
          ctx.fillRect(x, y, b, cell); ctx.fillRect(x + cell - b, y, b, cell);
        }
      return;
    }
    // morph: the lit field on a 1/4-size grid, then composited.
    const MA = this.bitmap(s.ma), MB = this.bitmap(s.mb);
    const read = (bm) => {
      const c = document.createElement("canvas"); c.width = bm.width; c.height = bm.height;
      const x = c.getContext("2d", { willReadFrequently: true });
      x.drawImage(bm, 0, 0);
      return { d: x.getImageData(0, 0, bm.width, bm.height).data, w: bm.width, h: bm.height };
    };
    this.cacheA = this.cacheA?.src === s.ma ? this.cacheA : { src: s.ma, ...read(MA) };
    this.cacheB = this.cacheB?.src === s.mb ? this.cacheB : { src: s.mb, ...read(MB) };
    const t = s.t;
    const w1 = sm(0.12, 0.34, t), w2 = sm(0.38, 0.62, t), w3 = sm(0.66, 0.88, t), kd = sm(0, 0.22, t) * (1 - sm(0.78, 1, t));
    const gw = Math.max(1, Math.round(cw / 4)), gh = Math.max(1, Math.round(ch / 4));
    if (!this.grid || this.grid.width !== gw || this.grid.height !== gh) { this.grid = document.createElement("canvas"); this.grid.width = gw; this.grid.height = gh; }
    const gx = this.grid.getContext("2d");
    const out = gx.createImageData(gw, gh), o = out.data;
    const view = cw / ch, P = this.periods();
    const cov = coverUv(MA.width / MA.height, view);
    const maskPx = 1 / (MA.height * cov[1]);
    const sdf = (m, u, v) => {
      const x = Math.min(m.w - 1, Math.max(0, Math.floor(u * m.w))), y = Math.min(m.h - 1, Math.max(0, Math.floor((1 - v) * m.h)));
      return (m.d[(y * m.w + x) * 4] - 128) * 0.25 * maskPx;
    };
    const aa = 2 / gh;
    for (let j = 0; j < gh; j++)
      for (let i = 0; i < gw; i++) {
        const sx = (i + 0.5) / gw, sy = 1 - (j + 0.5) / gh;            // canvas uv, y up
        const u = sx * cov[0] + cov[2], v = sy * cov[1] + cov[3];      // image uv
        const px = (sx - 0.5) * view * P, py = (sy - 0.5) * P;
        const fAm = sdf(this.cacheA, u, v), fBm = sdf(this.cacheB, u, v);
        const fAp = w1 > 0 ? pattern2d(s.pa ?? 3, px, py) / P : 0, fBp = w2 > 0 || w3 > 0 ? pattern2d(s.pb ?? 3, px, py) / P : 0;
        const f = (((fAm * (1 - w1) + fAp * w1) * (1 - w2) + fBp * w2) * (1 - w3)) + fBm * w3;
        o[(j * gw + i) * 4 + 3] = (1 - sm(-aa, aa, f)) * 255;
      }
    gx.putImageData(out, 0, 0);
    // shade
    const base = t < 0.5 ? A : B;
    ctx.drawImage(base, ...cover(base));
    if (kd > 0) { ctx.fillStyle = `rgba(15,20,24,${kd})`; ctx.fillRect(0, 0, cw, ch); }
    // light: the images (A -> B) washed toward sunlight while it is pure pattern, cut by the field
    if (!this.lcv) this.lcv = document.createElement("canvas");
    const lc = this.lcv; lc.width = cw; lc.height = ch;
    const lx = lc.getContext("2d");
    lx.drawImage(A, ...cover(A));
    if (w2 > 0) { lx.globalAlpha = w2; lx.drawImage(B, ...cover(B)); lx.globalAlpha = 1; }
    const pure = w1 * (1 - w3);
    if (pure > 0) { lx.fillStyle = `rgba(255,236,208,${0.82 * pure})`; lx.fillRect(0, 0, cw, ch); }
    lx.globalCompositeOperation = "destination-in";
    lx.imageSmoothingEnabled = true;
    lx.drawImage(this.grid, 0, 0, cw, ch);
    ctx.drawImage(lc, 0, 0);
  }
}
