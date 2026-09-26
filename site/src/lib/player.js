// Frame-sequence player for the two pre-rendered sequences.
//
// Frames stream in as one bundle per sequence and tier (a concatenation of
// WebP files with a JSON index), so a phone downloads a handful of requests,
// frames become drawable the moment their bytes land, and the ladder tier
// (tiny frames) arrives first to cover any gap while scrubbing fast.
//
// Drawing is continuous, never a slideshow:
//  * "time" mode (A, fixed camera, the sun moves): neighbouring frames are
//    blended in linear light by the fractional frame position;
//  * "camera" mode (B, the walk): both neighbours are reprojected to the
//    camera *between* them using each frame's depth, then blended, so the
//    street slides past with real parallax instead of dissolving.
// Only a window of frames around the current one is decoded; the rest stay
// compressed. Tiers: WebGL -> Canvas 2D (plain blend) -> the static <img>.

import { FULLSCREEN_VS, coverUv, fullscreenTriangle, program, texture, webglTier } from "./gl.js";
import { camUniform, cameraFrom, lerpCam } from "./camera.js";

const SRGB = `
// Images are stored top row first; uv here has y up.
vec4 img(sampler2D s, vec2 uv) { return texture2D(s, vec2(uv.x, 1.0 - uv.y)); }
vec3 lin(vec3 c) { return c * (c * (c * 0.305306011 + 0.682171111) + 0.012522878); }
vec3 srgb(vec3 c) {
  vec3 s1 = sqrt(c), s2 = sqrt(s1), s3 = sqrt(s2);
  return clamp(0.585122381 * s1 + 0.783140355 * s2 - 0.368262736 * s3, 0.0, 1.0);
}`;

// Heat shimmer over sunlit ground (the one ambient motion): a small vertical
// displacement that grows toward the horizon line and with the brightness of
// what is there, scaled by uShimmer (0 once the camera leaves the street).
const SHIMMER = `
uniform float uShimmer, uTime;
vec2 shimmer(vec2 uv) {
  if (uShimmer <= 0.0) return uv;
  float band = smoothstep(0.02, 0.35, uv.y) * (1.0 - smoothstep(0.35, 0.5, uv.y));
  float w = sin(uv.y * 190.0 - uTime * 3.1 + sin(uv.x * 23.0 + uTime * 0.7) * 2.0)
          + 0.5 * sin(uv.y * 331.0 - uTime * 4.3 + uv.x * 41.0);
  return uv + vec2(0.0, w * 0.00055 * band * uShimmer);
}`;

const TIME_FS = `
precision mediump float;
varying vec2 vUv;
uniform sampler2D uA, uB;
uniform float uT;
uniform vec4 uCover;
${SRGB}
${SHIMMER}
void main() {
  vec2 uv = shimmer(vUv * uCover.xy + uCover.zw);
  // Blend in linear light: a shadow edge half way between two frames is half
  // lit, as it would be in a longer exposure, not muddied as in sRGB.
  vec3 c = mix(lin(img(uA, uv).rgb), lin(img(uB, uv).rgb), uT);
  gl_FragColor = vec4(srgb(c), 1.0);
}`;

const CAMERA_FS = `
precision highp float;
varying vec2 vUv;
uniform sampler2D uA, uB, uDA, uDB;
uniform float uT;
uniform vec4 uCover;
uniform vec4 uCamT[4], uCamA[4], uCamB[4];   // pos,tanV | fwd,aspect | right,shift | up,0
uniform vec2 uDepth;                          // ln(dmin), ln(dmax / dmin)
${SRGB}
${SHIMMER}

// The renderer's camera: image uv (y up) -> world ray direction, and back.
vec3 rayDir(vec4 c0, vec4 c1, vec4 c2, vec4 c3, vec2 uv) {
  float sx = (uv.x * 2.0 - 1.0) * c0.w * c1.w;
  float sy = (uv.y * 2.0 - 1.0 + c2.w) * c0.w;
  return normalize(c1.xyz + c2.xyz * sx + c3.xyz * sy);
}
vec2 project(vec4 c0, vec4 c1, vec4 c2, vec4 c3, vec3 p) {
  vec3 v = p - c0.xyz;
  float z = max(dot(v, c1.xyz), 1e-3);
  return vec2((dot(v, c2.xyz) / z / (c0.w * c1.w) + 1.0) * 0.5,
              (dot(v, c3.xyz) / z / c0.w - c2.w + 1.0) * 0.5);
}
float depthAt(sampler2D d, vec2 uv) { return exp(uDepth.x + img(d, uv).r * uDepth.y); }

// Where does frame C see the surface that the in-between camera sees along
// (o, dir)? Fixed-point iteration on the distance along the ray, started from
// frame C's own depth at the same screen position (the frames are close).
// Returns linear colour and a confidence weight (0 where C cannot see it:
// outside its frame, or a disocclusion).
vec4 fetchFrom(sampler2D col, sampler2D dep, vec4 c0, vec4 c1, vec4 c2, vec4 c3, vec3 o, vec3 dir, vec2 uv0) {
  vec2 uv = uv0;
  float z = depthAt(dep, uv0);
  for (int k = 0; k < 4; k++) {
    uv = project(c0, c1, c2, c3, o + dir * z);
    vec3 q = c0.xyz + rayDir(c0, c1, c2, c3, uv) * depthAt(dep, uv);
    z = max(dot(q - o, dir), 0.05);
  }
  uv = project(c0, c1, c2, c3, o + dir * z);
  vec3 q = c0.xyz + rayDir(c0, c1, c2, c3, uv) * depthAt(dep, uv);
  float err = length(q - (o + dir * z)) / max(z, 0.5);
  float inside = step(0.0, uv.x) * step(uv.x, 1.0) * step(0.0, uv.y) * step(uv.y, 1.0);
  return vec4(lin(img(col, uv).rgb), inside * exp(-err * 60.0));
}

void main() {
  vec2 uv = shimmer(vUv * uCover.xy + uCover.zw);
  vec3 o = uCamT[0].xyz;
  vec3 dir = rayDir(uCamT[0], uCamT[1], uCamT[2], uCamT[3], uv);
  vec4 a = fetchFrom(uA, uDA, uCamA[0], uCamA[1], uCamA[2], uCamA[3], o, dir, uv);
  vec4 b = fetchFrom(uB, uDB, uCamB[0], uCamB[1], uCamB[2], uCamB[3], o, dir, uv);
  float wa = a.w * (1.0 - uT) + 1e-4 * (1.0 - uT), wb = b.w * uT + 1e-4 * uT;
  vec3 c = (a.rgb * wa + b.rgb * wb) / (wa + wb);
  gl_FragColor = vec4(srgb(c), 1.0);
}`;

const LOG_DMIN = Math.log(0.25), LOG_RANGE = Math.log(400 / 0.25);

async function streamBundle(url, index, onFrame, signal) {
  const res = await fetch(url, { signal });
  if (!res.ok || !res.body) throw new Error(`bundle ${url}: ${res.status}`);
  const total = index.reduce((s, f) => Math.max(s, f.o + f.n), 0);
  const buf = new Uint8Array(total);
  const reader = res.body.getReader();
  let got = 0, next = 0;
  const order = index.map((f, i) => i).sort((a, b) => index[a].o - index[b].o);
  for (;;) {
    const { done, value } = await reader.read();
    if (done) break;
    buf.set(value, got);
    got += value.length;
    while (next < order.length && index[order[next]].o + index[order[next]].n <= got) {
      const f = index[order[next]];
      onFrame(order[next], new Blob([buf.subarray(f.o, f.o + f.n)], { type: f.t || "image/webp" }));
      next++;
    }
  }
}

export class SequencePlayer {
  /**
   * @param {object} o
   * @param {HTMLCanvasElement} o.canvas
   * @param {object[]} o.frames   the sequence's frames (sequences/seq_*.json)
   * @param {string} o.base       URL prefix of the bundles, e.g. "/seq/a-p"
   * @param {"time"|"camera"} o.mode
   * @param {{index:number,url:string}[]} [o.rest]  hi-res stills for frames the scroll rests on
   * @param {number} [o.window]   decoded frames kept either side of the current one
   */
  constructor({ canvas, frames, base, mode, rest = [], window = 5, aspect }) {
    this.canvas = canvas;
    this.frames = frames;
    this.base = base;
    this.mode = mode;
    this.rest = rest;
    this.win = window;
    this.aspect = aspect;
    this.n = frames.length;
    this.hi = new Array(this.n);          // Blob per frame
    this.lo = new Array(this.n);          // ladder Blob per frame
    this.dep = new Array(this.n);         // depth Blob per frame (camera mode)
    this.bmp = new Map();                 // "hi:3" -> ImageBitmap
    this.pending = new Map();
    this.tex = new Map();                 // "hi:3" -> WebGLTexture (small LRU)
    this.restBmp = new Map();
    this.f = 0;
    this.shimmer = 0;
    this.abort = new AbortController();
    this.tier = webglTier();
    this.dirty = true;
    this.listeners = [];
  }

  async start() {
    const idx = await fetch(`${this.base}.json`, { signal: this.abort.signal }).then((r) => r.json());
    this.index = idx;
    if (this.tier === "webgl") this.initGL();
    else this.ctx = this.canvas.getContext("2d");
    this.resize();
    for (const r of this.rest) {
      fetch(r.url, { signal: this.abort.signal }).then((res) => res.blob())
        .then((b) => createImageBitmap(b)).then((bm) => { this.restBmp.set(r.index, bm); this.dirty = true; })
        .catch(() => {});
    }
    // Ladder first (a few hundred kB for the whole sequence), then depth, then full frames.
    const got = (arr) => (i, blob) => { arr[i] = blob; this.dirty = true; this.ensureWindow(); this.emit(); };
    await streamBundle(`${this.base}-ladder.bin`, idx.ladder, got(this.lo), this.abort.signal).catch(() => {});
    if (this.mode === "camera" && idx.depth) await streamBundle(`${this.base}-depth.bin`, idx.depth, got(this.dep), this.abort.signal).catch(() => {});
    await streamBundle(`${this.base}-hi.bin`, idx.hi, got(this.hi), this.abort.signal).catch(() => {});
  }

  on(fn) { this.listeners.push(fn); }
  emit() { for (const fn of this.listeners) fn(this); }
  loaded() { return this.hi.filter(Boolean).length / this.n; }

  /** Fractional frame position, 0 .. n-1. */
  set(f) {
    f = Math.max(0, Math.min(this.n - 1, f));
    if (f === this.f) return;
    this.f = f;
    this.dirty = true;
    this.ensureWindow();
  }

  ensureWindow() {
    const c = Math.round(this.f);
    const want = new Set();
    for (let k = -this.win; k <= this.win; k++) {
      const i = c + k;
      if (i < 0 || i >= this.n) continue;
      want.add(`hi:${i}`);
      if (this.mode === "camera") want.add(`dep:${i}`);
    }
    for (let k = -2; k <= 3; k++) { const i = c + k; if (i >= 0 && i < this.n) want.add(`lo:${i}`); }
    for (const key of want) {
      if (this.bmp.has(key) || this.pending.has(key)) continue;
      const [kind, s] = key.split(":");
      const i = +s;
      const blob = kind === "hi" ? this.hi[i] : kind === "lo" ? this.lo[i] : this.dep[i];
      if (!blob) continue;
      const opts = kind === "dep" ? { colorSpaceConversion: "none", premultiplyAlpha: "none" } : {};
      const p = createImageBitmap(blob, opts).then((bm) => {
        this.pending.delete(key);
        if (!want.has(key) && !this.isWanted(key)) { bm.close(); return; }
        this.bmp.set(key, bm);
        this.dirty = true;
      }).catch(() => this.pending.delete(key));
      this.pending.set(key, p);
    }
    for (const [key, bm] of this.bmp) {
      if (!this.isWanted(key)) {
        bm.close();
        this.bmp.delete(key);
        const t = this.tex.get(key);
        if (t) { this.gl.deleteTexture(t); this.tex.delete(key); }
      }
    }
  }

  isWanted(key) {
    const [kind, s] = key.split(":");
    const d = Math.abs(+s - Math.round(this.f));
    return kind === "lo" ? d <= 3 : d <= this.win + 1;
  }

  /** Best bitmap for frame i: full frame, else its ladder frame. */
  source(i) {
    return this.bmp.get(`hi:${i}`) ? `hi:${i}` : this.bmp.get(`lo:${i}`) ? `lo:${i}` : null;
  }

  initGL() {
    const gl = this.canvas.getContext("webgl", { antialias: false, alpha: false, premultipliedAlpha: false, preserveDrawingBuffer: false });
    if (!gl) { this.tier = "2d"; return; }
    this.gl = gl;
    fullscreenTriangle(gl);
    this.progTime = program(gl, FULLSCREEN_VS, TIME_FS);
    if (this.mode === "camera") {
      try { this.progCam = program(gl, FULLSCREEN_VS, CAMERA_FS); } catch { this.progCam = null; }
    }
    this.canvas.addEventListener("webglcontextlost", (e) => { e.preventDefault(); this.tier = "2d"; this.gl = null; this.ctx = null; });
  }

  resize() {
    const r = this.canvas.getBoundingClientRect();
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    const w = Math.max(1, Math.round(r.width * dpr)), h = Math.max(1, Math.round(r.height * dpr));
    if (this.canvas.width !== w || this.canvas.height !== h) {
      this.canvas.width = w;
      this.canvas.height = h;
      this.dirty = true;
    }
    this.viewAspect = w / h;
  }

  glTex(key) {
    let t = this.tex.get(key);
    if (!t) {
      const bm = this.bmp.get(key);
      if (!bm) return null;
      t = texture(this.gl, bm, { linear: true });
      this.tex.set(key, t);
    }
    return t;
  }

  /** Draws if anything changed. Call once per animation frame. */
  draw(time = 0) {
    if (!this.dirty && this.shimmer <= 0) return;
    const i0 = Math.floor(this.f), i1 = Math.min(i0 + 1, this.n - 1);
    let t = this.f - i0;
    // Resting on a frame that has a hi-res still: show the still, sharpest.
    const rest = this.restBmp.get(Math.round(this.f));
    if (rest && Math.abs(this.f - Math.round(this.f)) < 0.02) return this.drawStill(rest, time);
    const ka = this.source(i0), kb = this.source(i1);
    if (!ka && !kb) return;
    const A = ka || kb, B = kb || ka;
    if (!ka) t = 1;
    if (!kb) t = 0;
    this.dirty = false;
    if (this.gl) return this.drawGL(A, B, t, i0, i1, time);
    this.draw2D(A, B, t);
  }

  drawStill(bm, time) {
    this.dirty = false;
    if (this.gl) {
      const key = "rest";
      let tex = this.tex.get(key);
      if (!tex || this.restShown !== bm) {
        if (tex) this.gl.deleteTexture(tex);
        tex = texture(this.gl, bm);
        this.tex.set(key, tex);
        this.restShown = bm;
      }
      return this.drawTimeGL(tex, tex, 0, bm.width / bm.height, time);
    }
    this.draw2DBitmap(bm, 1);
  }

  drawGL(ka, kb, t, i0, i1, time) {
    const gl = this.gl;
    const ta = this.glTex(ka), tb = this.glTex(kb);
    const bm = this.bmp.get(ka);
    const imgAspect = bm.width / bm.height;
    const da = this.mode === "camera" && this.progCam && ka.startsWith("hi") && kb.startsWith("hi") ? this.glTex(`dep:${i0}`) : null;
    const db = da ? this.glTex(`dep:${i1}`) : null;
    if (da && db && t > 0 && t < 1) {
      const { p, uni } = this.progCam;
      gl.viewport(0, 0, this.canvas.width, this.canvas.height);
      gl.useProgram(p);
      const fa = this.frames[i0].cam, fb = this.frames[i1].cam;
      const ct = cameraFrom(lerpCam(fa, fb, t), imgAspect);
      gl.uniform4fv(uni.uCamT, camUniform(ct));
      gl.uniform4fv(uni.uCamA, camUniform(cameraFrom(fa, imgAspect)));
      gl.uniform4fv(uni.uCamB, camUniform(cameraFrom(fb, imgAspect)));
      gl.uniform2f(uni.uDepth, LOG_DMIN, LOG_RANGE);
      gl.uniform1f(uni.uT, t);
      gl.uniform4fv(uni.uCover, coverUv(imgAspect, this.viewAspect));
      gl.uniform1f(uni.uShimmer, this.shimmer);
      gl.uniform1f(uni.uTime, time);
      const bind = (u, tx, unit) => { gl.activeTexture(gl.TEXTURE0 + unit); gl.bindTexture(gl.TEXTURE_2D, tx); gl.uniform1i(u, unit); };
      bind(uni.uA, ta, 0); bind(uni.uB, tb, 1); bind(uni.uDA, da, 2); bind(uni.uDB, db, 3);
      gl.drawArrays(gl.TRIANGLES, 0, 3);
      return;
    }
    this.drawTimeGL(ta, tb, t, imgAspect, time);
  }

  drawTimeGL(ta, tb, t, imgAspect, time) {
    const gl = this.gl;
    const { p, uni } = this.progTime;
    gl.viewport(0, 0, this.canvas.width, this.canvas.height);
    gl.useProgram(p);
    gl.uniform1f(uni.uT, t);
    gl.uniform4fv(uni.uCover, coverUv(imgAspect, this.viewAspect));
    gl.uniform1f(uni.uShimmer, this.shimmer);
    gl.uniform1f(uni.uTime, time);
    gl.activeTexture(gl.TEXTURE0); gl.bindTexture(gl.TEXTURE_2D, ta); gl.uniform1i(uni.uA, 0);
    gl.activeTexture(gl.TEXTURE1); gl.bindTexture(gl.TEXTURE_2D, tb); gl.uniform1i(uni.uB, 1);
    gl.drawArrays(gl.TRIANGLES, 0, 3);
  }

  draw2D(ka, kb, t) {
    this.draw2DBitmap(this.bmp.get(ka), 1);
    if (t > 0.004 && kb !== ka) this.draw2DBitmap(this.bmp.get(kb), t);
  }

  draw2DBitmap(bm, alpha) {
    const ctx = this.ctx || (this.ctx = this.canvas.getContext("2d"));
    if (!ctx || !bm) return;
    const cw = this.canvas.width, ch = this.canvas.height;
    const s = Math.max(cw / bm.width, ch / bm.height);
    const w = bm.width * s, h = bm.height * s;
    ctx.globalAlpha = alpha;
    ctx.drawImage(bm, (cw - w) / 2, (ch - h) / 2, w, h);
    ctx.globalAlpha = 1;
  }

  destroy() {
    this.abort.abort();
    for (const bm of this.bmp.values()) bm.close();
    this.bmp.clear();
  }
}
