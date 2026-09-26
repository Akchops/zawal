// One canvas for everything after the day, three jobs, one shader:
//
//  * morph (signature D): project A's lit patches erode along their own
//    shadow geometry until only its pattern of light remains; that pattern's
//    signed distance field morphs into project B's; B's lit patches then
//    bloom back to the full image and its shade returns. Masks are the
//    renderer's sun-visibility AOV at 1/4 size: R = signed distance to the
//    lit edge (128 = edge, 4 steps per mask pixel, lower = in sun), G = sun
//    visibility. Continuous, reversible, never a cross-fade.
//  * close: the carved screen's holes close one by one into solid shade.
//  * fade: two frames blended in linear light (the night lanterns), times a
//    gain (the courtyard emerging from the dark).
//
// Tiers: WebGL, else the same rules on the CPU at 1/4 size with Canvas 2D.
import { FULLSCREEN_VS, coverUv, fullscreenTriangle, program, texture, webglTier } from "./gl.js";
import { url } from "./paths.js";

const FS = `
precision highp float;
varying vec2 vUv;
uniform sampler2D uA, uB, uMA, uMB;
uniform vec4 uCover;
uniform float uMode, uT, uGain, uCell, uAspect;
uniform vec3 uInk;
uniform vec2 uMask;          // mask size in texels

vec4 img(sampler2D s, vec2 uv) { return texture2D(s, vec2(uv.x, 1.0 - uv.y)); }
vec3 lin(vec3 c) { return c * (c * (c * 0.305306011 + 0.682171111) + 0.012522878); }
vec3 srgb(vec3 c) {
  vec3 s1 = sqrt(c), s2 = sqrt(s1), s3 = sqrt(s2);
  return clamp(0.585122381 * s1 + 0.783140355 * s2 - 0.368262736 * s3, 0.0, 1.0);
}
float sdfAt(sampler2D m, vec2 uv) { return (img(m, uv).r * 255.0 - 128.0) * 0.25; }   // mask px, < 0 lit

// The carved screen's star (renderer/src/patterns.h PAT_STAR8), unit period.
float sdBox2(vec2 p, vec2 b) { vec2 d = abs(p) - b; return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0); }
float starSD(vec2 c) {
  const float A = 0.31, Bc = 0.11, C = 0.012, K = 0.70710678;
  float r1 = sdBox2(c, vec2(A - C)) - C;
  float r2 = sdBox2(vec2(K * (c.x + c.y), K * (c.y - c.x)), vec2(A - C)) - C;
  vec2 k = c - vec2(c.x > 0.0 ? 0.5 : -0.5, c.y > 0.0 ? 0.5 : -0.5);
  float corner = sdBox2(vec2(K * (k.x + k.y), K * (k.y - k.x)), vec2(Bc)) - 0.25 * C;
  return min(min(r1, r2), corner);
}
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }

void main() {
  vec2 uv = vUv * uCover.xy + uCover.zw;
  vec3 A = lin(img(uA, uv).rgb);
  vec3 col;
  if (uMode < 0.5) {
    vec3 B = lin(img(uB, uv).rgb);
    float t = uT;
    float kd = smoothstep(0.0, 0.2, t) * (1.0 - smoothstep(0.8, 1.0, t));   // the shade goes dark, then returns
    float eA = 7.0 * smoothstep(0.08, 0.4, t);                              // A's light erodes
    float eB = 7.0 * (1.0 - smoothstep(0.6, 0.92, t));                      // B's light blooms
    float s = smoothstep(0.38, 0.62, t);                                    // pattern morph
    float f = mix(sdfAt(uMA, uv) + eA, sdfAt(uMB, uv) + eB, s);
    float aa = 0.6;
    float lit = 1.0 - smoothstep(-aa, aa, f);
    vec3 sunC = vec3(1.0, 0.86, 0.66);
    vec3 litC = mix(mix(A, B, s), sunC * 0.9, 0.55 * sin(3.14159 * s));
    vec3 shade = mix(t < 0.5 ? A : B, uInk, kd);
    col = mix(shade, litC, lit);
  } else if (uMode < 1.5) {
    // Screen-space lattice, one cell per uCell of the short side.
    vec2 p = vec2(vUv.x * uAspect, vUv.y) / uCell;
    vec2 cell = floor(p);
    vec2 c = p - cell - 0.5;
    float tau = 0.08 + 0.62 * hash(cell);                  // when this hole closes
    float open = 1.0 - smoothstep(tau, tau + 0.22, uT);
    float thr = mix(-0.32, 0.75, open);
    float hole = 1.0 - smoothstep(thr - 0.02, thr + 0.02, starSD(c));
    col = mix(uInk, A, hole);
  } else {
    vec3 B = lin(img(uB, uv).rgb);
    col = mix(A, B, uT) * uGain + uInk * (1.0 - uGain);
  }
  gl_FragColor = vec4(srgb(col), 1.0);
}`;

const INK = [0.0056, 0.0070, 0.0086];     // --shade #0f1418 in linear light

export class Compositor {
  constructor(canvas) {
    this.canvas = canvas;
    this.images = new Map();       // url -> ImageBitmap | Promise
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
        this.canvas.addEventListener("webglcontextlost", (e) => { e.preventDefault(); this.gl = null; this.tex.clear(); this.tier = "2d"; });
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
      v = fetch(url(src)).then((r) => r.blob())
        .then((b) => createImageBitmap(b, raw ? { colorSpaceConversion: "none", premultiplyAlpha: "none" } : {}))
        .then((bm) => { this.images.set(src, bm); this.dirty = true; return bm; })
        .catch(() => { this.images.set(src, "failed"); this.dirty = true; return null; });
      this.images.set(src, v);
    }
    return v;
  }
  has(src) { const v = this.images.get(src); return !!v && v !== "failed" && !(v instanceof Promise); }
  failed(src) { return this.images.get(src) === "failed"; }
  bitmap(src) { return this.has(src) ? this.images.get(src) : null; }

  resize() {
    const r = this.canvas.getBoundingClientRect();
    const cap = this.tier === "webgl" ? 1.5 : 1;
    const dpr = Math.min(devicePixelRatio || 1, cap);
    const w = Math.max(1, Math.round(r.width * dpr)), h = Math.max(1, Math.round(r.height * dpr));
    if (this.canvas.width !== w || this.canvas.height !== h) { this.canvas.width = w; this.canvas.height = h; this.dirty = true; }
  }

  /** What to draw: {mode: "morph"|"close"|"fade", a, b, ma, mb, t, gain}. */
  set(state) {
    const s = this.state;
    if (s && s.mode === state.mode && s.a === state.a && s.b === state.b && Math.abs(s.t - state.t) < 1e-4 && (s.gain ?? 1) === (state.gain ?? 1)) return;
    this.state = state;
    this.dirty = true;
  }

  glTex(src, raw) {
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
    gl.uniform1f(uni.uMode, mode);
    gl.uniform1f(uni.uT, s.t);
    gl.uniform1f(uni.uGain, s.gain ?? 1);
    gl.uniform1f(uni.uAspect, view);
    gl.uniform1f(uni.uCell, view < 1 ? 0.075 : 0.105);
    gl.uniform3f(uni.uInk, INK[0], INK[1], INK[2]);
    gl.uniform4fv(uni.uCover, coverUv(A.width / A.height, view));
    gl.drawArrays(gl.TRIANGLES, 0, 3);
  }

  // Canvas 2D: the same rules, evaluated on a 1/4-size mask on the CPU.
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
      ctx.drawImage(A, ...cover(A));
      const cell = Math.round(Math.min(cw, ch) * 0.105);
      ctx.fillStyle = "#0f1418";
      for (let y = 0; y < ch; y += cell)
        for (let x = 0; x < cw; x += cell) {
          const h = (Math.sin((x / cell) * 127.1 + (y / cell) * 311.7) * 43758.5453) % 1;
          const tau = 0.08 + 0.62 * Math.abs(h);
          const shut = Math.max(0, Math.min(1, (s.t - tau) / 0.22));
          if (shut <= 0) continue;
          // the hole shrinks to its centre: ink fills the cell's border band
          const b = (cell * shut) / 2 + 0.5;
          ctx.fillRect(x, y, cell, b); ctx.fillRect(x, y + cell - b, cell, b);
          ctx.fillRect(x, y, b, cell); ctx.fillRect(x + cell - b, y, b, cell);
        }
      return;
    }
    // morph
    const MA = this.bitmap(s.ma), MB = this.bitmap(s.mb);
    const mw = MA.width, mh = MA.height;
    if (!this.mcv) { this.mcv = document.createElement("canvas"); this.rd = document.createElement("canvas"); }
    const read = (bm) => {
      const c = this.rd; c.width = mw; c.height = mh;
      const x = c.getContext("2d", { willReadFrequently: true });
      x.drawImage(bm, 0, 0);
      return x.getImageData(0, 0, mw, mh).data;
    };
    this.cacheA = this.cacheA?.src === s.ma ? this.cacheA : { src: s.ma, d: read(MA) };
    this.cacheB = this.cacheB?.src === s.mb ? this.cacheB : { src: s.mb, d: read(MB) };
    const t = s.t, sm = (a, b, x) => { const u = Math.max(0, Math.min(1, (x - a) / (b - a))); return u * u * (3 - 2 * u); };
    const kd = sm(0, 0.2, t) * (1 - sm(0.8, 1, t)), eA = 7 * sm(0.08, 0.4, t), eB = 7 * (1 - sm(0.6, 0.92, t)), m = sm(0.38, 0.62, t);
    const mc = this.mcv; mc.width = mw; mc.height = mh;
    const mx = mc.getContext("2d");
    const out = mx.createImageData(mw, mh);
    const da = this.cacheA.d, db = this.cacheB.d, o = out.data;
    for (let i = 0; i < mw * mh; i++) {
      const f = ((da[i * 4] - 128) / 4 + eA) * (1 - m) + ((db[i * 4] - 128) / 4 + eB) * m;
      const lit = 1 - sm(-0.6, 0.6, f);
      o[i * 4 + 3] = lit * 255;
    }
    mx.putImageData(out, 0, 0);
    const base = t < 0.5 ? A : B;
    ctx.drawImage(base, ...cover(base));
    if (kd > 0) { ctx.fillStyle = `rgba(15,20,24,${kd})`; ctx.fillRect(0, 0, cw, ch); }
    // lit layer: the image(s), cut by the mask
    if (!this.lcv) this.lcv = document.createElement("canvas");
    const lc = this.lcv; lc.width = cw; lc.height = ch;
    const lx = lc.getContext("2d");
    lx.drawImage(A, ...cover(A));
    if (m > 0) { lx.globalAlpha = m; lx.drawImage(B, ...cover(B)); lx.globalAlpha = 1; }
    lx.globalCompositeOperation = "destination-in";
    lx.imageSmoothingEnabled = true;
    lx.drawImage(mc, ...cover(MA));
    ctx.drawImage(lc, 0, 0);
  }
}
