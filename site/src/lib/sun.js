// Signature C: "You are the sun". The courtyard relit live for any sun.
//
// Per pixel: position from the baked depth, normal and albedo from the
// G-buffer; direct sunlight computed here with analytic shadows (walls, wind
// tower, canopy beams, the canopy's star lattice with a physical penumbra,
// the large objects); everything else (sky light, bounce light) interpolated
// from 12 path-traced basis images baked for sun directions around the sky.
// Tone mapping repeats tools/post.py (exposure, AgX + punch, grade, vignette),
// so at the hand-over sun the image equals the day's last frame.
import { FULLSCREEN_VS, coverUv, fullscreenTriangle, program, texture, webglTier } from "./gl.js";
import { cameraFrom, camUniform } from "./camera.js";
import { solarPosition, sunDir, whenIsTheSun, clock } from "./solar.js";

const FS = `
precision highp float;
varying vec2 vUv;
uniform sampler2D uDepth, uNormal, uAlbedo, uB0, uB1, uB2;
uniform vec3 uW;            // weights of the three nearest basis images
uniform vec3 uScale;        // their linear scales
uniform vec4 uCam[4];       // pos,tanV | fwd,aspect | right,shift | up,0
uniform vec4 uCover;
uniform vec3 uSun;          // unit vector to the sun
uniform vec3 uSunE;         // direct-sun irradiance (transmittance) RGB
uniform float uDepthMax, uExposure, uVignette, uTime;
uniform vec2 uAspect;       // canvas aspect, for the vignette

vec4 img(sampler2D s, vec2 uv) { return texture2D(s, vec2(uv.x, 1.0 - uv.y)); }
vec3 srgbToLin(vec3 c) { return c * (c * (c * 0.305306011 + 0.682171111) + 0.012522878); }

// ---- the canopy's pattern: renderer/src/patterns.h PAT_STAR8, line for line ----
float sdBox2(vec2 p, vec2 b) {
  vec2 d = abs(p) - b;
  return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}
// courtyardStar(): period 0.36, a 0.325, b 0.09, c 0.007, origin (0.18, 0.18); u = z, v = x.
float starSD(vec2 uv) {
  const float P = 0.36, A = 0.325, Bc = 0.09, C = 0.007, K = 0.70710678;
  vec2 q = uv - vec2(0.18);
  vec2 c = q - P * floor(q / P) - 0.5 * P;
  float s = A * P;
  float r1 = sdBox2(c, vec2(s - C)) - C;
  float r2 = sdBox2(vec2(K * (c.x + c.y), K * (c.y - c.x)), vec2(s - C)) - C;
  float star = min(r1, r2);
  vec2 k = c - vec2(c.x > 0.0 ? 0.5 * P : -0.5 * P, c.y > 0.0 ? 0.5 * P : -0.5 * P);
  float corner = sdBox2(vec2(K * (k.x + k.y), K * (k.y - k.x)), vec2(Bc * P)) - 0.25 * C;
  return min(star, corner);                       // < 0 inside a hole
}

// Ray-box: does the segment from p along d hit the box before tmax?
float hitBox(vec3 p, vec3 d, vec3 lo, vec3 hi) {
  vec3 inv = 1.0 / d;
  vec3 t0 = (lo - p) * inv, t1 = (hi - p) * inv;
  vec3 tn = min(t0, t1), tf = max(t0, t1);
  float a = max(max(tn.x, tn.y), tn.z), b = min(min(tf.x, tf.y), tf.z);
  return (b > max(a, 0.0005)) ? 0.0 : 1.0;       // 0 = blocked
}
float hitSphere(vec3 p, vec3 d, vec3 c, float r) {
  vec3 oc = p - c;
  float b = dot(oc, d), h = b * b - dot(oc, oc) + r * r;
  return (h > 0.0 && -b - sqrt(h) > 0.0005) ? 0.0 : 1.0;
}

// Visibility of the sun disc from p (0..1).
float sunVis(vec3 p, vec3 n, vec3 L) {
  if (L.y <= 0.0) return 0.0;
  vec3 o = p + n * 0.004;
  float v = 1.0;
  // Enclosure: side walls with their coping, the north wing and its wind
  // tower, the loggia's upper mass to the south.
  v *= hitBox(o, L, vec3(-0.5, 0.0, -0.5), vec3(0.0, 7.47, 13.7));
  v *= hitBox(o, L, vec3(7.2, 0.0, -0.5), vec3(7.7, 7.47, 13.7));
  v *= hitBox(o, L, vec3(-4.5, 0.0, -4.5), vec3(11.7, 7.0, 0.0));
  v *= hitBox(o, L, vec3(6.0, 7.0, -3.95), vec3(9.0, 12.9, -0.95));
  v *= hitBox(o, L, vec3(-0.5, 3.5, 13.65), vec3(7.7, 7.0, 17.1));
  // Teak beams under the lattice.
  for (int k = 0; k < 4; k++) {
    float z = 3.07 + float(k) * 2.2867;
    v *= hitBox(o, L, vec3(-0.2, 4.9, z - 0.07), vec3(7.4, 5.2, z + 0.07));
  }
  v *= hitBox(o, L, vec3(0.0, 5.15, 2.96), vec3(7.2, 5.29, 3.04));
  v *= hitBox(o, L, vec3(0.0, 5.15, 9.96), vec3(7.2, 5.29, 10.04));
  // Large objects: planters, olive crowns (half the light gets through),
  // the water jar, the hanging lantern (pierced), the bench.
  v *= hitBox(o, L, vec3(2.1, 0.0, 12.2), vec3(2.88, 0.81, 12.96));
  v *= hitBox(o, L, vec3(4.32, 0.0, 12.2), vec3(5.1, 0.81, 12.96));
  v *= mix(0.45, 1.0, hitSphere(o, L, vec3(2.49, 2.2, 12.58), 0.78));
  v *= mix(0.45, 1.0, hitSphere(o, L, vec3(4.71, 2.2, 12.58), 0.78));
  v *= hitSphere(o, L, vec3(6.8, 0.64, 11.95), 0.42);
  v *= mix(0.55, 1.0, hitSphere(o, L, vec3(3.6, 3.5, 8.8), 0.23));
  v *= hitBox(o, L, vec3(0.02, 0.38, 4.4), vec3(0.52, 0.45, 6.6));
  if (v <= 0.0) return 0.0;
  // The lattice: 5 cm slab at 5.20-5.25 m over x 0..7.2, z 3..10. The pattern
  // is tested where the ray enters and leaves the slab (a thick screen cuts
  // low sun), softened by the sun's own disc: penumbra = travel * tan(0.27 deg).
  if (o.y < 5.2) {
    float t0 = (5.2 - o.y) / L.y, t1 = (5.25 - o.y) / L.y;
    vec3 a = o + L * t0, b = o + L * t1;
    if (a.x > 0.0 && a.x < 7.2 && a.z > 3.0 && a.z < 10.0) {
      float w = max(t0 * 0.0047, 0.0015);
      float ha = starSD(vec2(a.z, a.x)), hb = starSD(vec2(b.z, b.x));
      v *= smoothstep(-w, w, -max(ha, hb));
    }
  }
  return v;
}

// ---- tone mapping: tools/post.py ----
const mat3 AGX_IN = mat3(0.842479062253094, 0.0784335999999992, 0.0792237451477643,
                         0.0423282422610123, 0.878468636469772, 0.0791661274605434,
                         0.0423756549057051, 0.0784336, 0.879142973793104);
const mat3 AGX_OUT = mat3(1.19687900512017, -0.0980208811401368, -0.0990297440797205,
                          -0.0528968517574562, 1.15190312990417, -0.0989611768448433,
                          -0.0529716355144438, -0.0980434501171241, 1.15107367264116);
vec3 agx(vec3 x) {
  const float mn = -12.47393, mx = 4.026069;
  vec3 v = max(x, vec3(1e-10)) * AGX_IN;
  v = clamp(log2(v), mn, mx);
  v = (v - mn) / (mx - mn);
  vec3 v2 = v * v, v4 = v2 * v2;
  v = 15.5 * v4 * v2 - 40.14 * v4 * v + 31.96 * v4 - 6.868 * v2 * v + 0.4298 * v2 + 0.1191 * v - 0.00232;
  float l = dot(v, vec3(0.2126, 0.7152, 0.0722));
  v = l + 1.08 * (v - l);
  v = max(v * AGX_OUT, 0.0);
  vec3 lin = pow(v, vec3(2.2));
  vec3 lo = lin * 12.92, hi = 1.055 * pow(lin, vec3(1.0 / 2.4)) - 0.055;
  return clamp(mix(hi, lo, step(lin, vec3(0.0031308))), 0.0, 1.0);
}

vec3 rayDir(vec2 uv) {
  float sx = (uv.x * 2.0 - 1.0) * uCam[0].w * uCam[1].w;
  float sy = (uv.y * 2.0 - 1.0 + uCam[2].w) * uCam[0].w;
  return normalize(uCam[1].xyz + uCam[2].xyz * sx + uCam[3].xyz * sy);
}

void main() {
  vec2 uv = vUv * uCover.xy + uCover.zw;
  vec3 dq = img(uDepth, uv).rgb;
  float depth = (dq.r * 65280.0 + dq.g * 255.0) / 65535.0 * uDepthMax;
  vec3 ind = uW.x * uScale.x * pow(img(uB0, uv).rgb, vec3(2.2))
           + uW.y * uScale.y * pow(img(uB1, uv).rgb, vec3(2.2))
           + uW.z * uScale.z * pow(img(uB2, uv).rgb, vec3(2.2));
  vec3 col = ind;
  if (depth < uDepthMax * 0.999) {
    vec3 p = uCam[0].xyz + rayDir(uv) * depth;
    vec3 n = normalize(img(uNormal, uv).rgb * 2.0 - 1.0);
    vec3 alb = srgbToLin(img(uAlbedo, uv).rgb);
    float ndl = max(dot(n, uSun), 0.0);
    if (ndl > 0.0) col += uSunE * alb * (ndl / 3.14159265) * sunVis(p, n, uSun);
  }
  // Exposure and the studio's locked daylight white balance.
  col *= uExposure * vec3(0.930, 1.0, 1.110);
  float lumLin = dot(col, vec3(0.2126, 0.7152, 0.0722)) * 0.6;
  vec3 c = agx(col);
  float t = clamp(lumLin, 0.0, 1.0);
  c = clamp(c * mix(vec3(0.965, 0.985, 1.03), vec3(1.015, 1.0, 0.975), t), 0.0, 1.0);
  vec2 q = (uv - 0.5) * 2.0;                     // image coordinates, as post.py
  float ar = uAspect.x;
  float r2 = (q.x * q.x * ar * ar + q.y * q.y) / (1.0 + ar * ar);
  c *= 1.0 - uVignette * pow(r2, 1.4);
  gl_FragColor = vec4(c, 1.0);
}`;

// Exposure authored by sun altitude (matches the day sequence's curve).
function evForAlt(alt, az) {
  const keys = [[-2, 3.2], [4, 2.8], [12, 2.0], [27, 1.3], [31, 1.2], [50, 0.8], [70, 0.6], [88.3, 0.4]];
  for (let i = 1; i < keys.length; i++) {
    if (alt <= keys[i][0]) {
      const [a0, e0] = keys[i - 1], [a1, e1] = keys[i];
      const f = Math.max(0, Math.min(1, (alt - a0) / (a1 - a0)));
      return e0 + (e1 - e0) * f * f * (3 - 2 * f);
    }
  }
  return 0.4;
}

const angle = (a, b) => Math.acos(Math.max(-1, Math.min(1, a[0] * b[0] + a[1] * b[1] + a[2] * b[2])));

export class SunStage {
  constructor({ canvas, base, onSun }) {
    this.canvas = canvas;
    this.base = base;              // "/sun/sun-p.json" etc
    this.onSun = onSun;
    this.sun = null;
    this.dirty = true;
  }

  async start() {
    if (webglTier() !== "webgl") return false;
    this.meta = await fetch(this.base).then((r) => r.json());
    const gl = this.canvas.getContext("webgl", { antialias: false, alpha: false });
    if (!gl) return false;
    this.gl = gl;
    fullscreenTriangle(gl);
    this.prog = program(gl, FULLSCREEN_VS, FS);
    const load = async (url, linear = true) => texture(gl, await createImageBitmap(await (await fetch(url)).blob(),
      { colorSpaceConversion: "none", premultiplyAlpha: "none" }), { linear });
    const g = this.meta.gbuffer;
    // Depth is 16 bits split over two bytes: never filter between texels.
    [this.tDepth, this.tNormal, this.tAlbedo] = await Promise.all([load(g.depth, false), load(g.normal), load(g.albedo)]);
    this.basis = this.meta.basis.map((b) => ({ ...b, dir: sunDir(b.alt, b.az), tex: null }));
    await Promise.all(this.basis.map(async (b) => { b.tex = await load(b.file); }));
    const [W, H] = this.meta.size;
    this.cam = cameraFrom(this.meta.cam, W / H);
    this.imgAspect = W / H;
    this.ready = true;
    this.setSun(this.meta.handover.alt, this.meta.handover.az);
    return true;
  }

  resize() {
    const r = this.canvas.getBoundingClientRect();
    const dpr = Math.min(devicePixelRatio || 1, 1.5);
    const w = Math.max(1, Math.round(r.width * dpr)), h = Math.max(1, Math.round(r.height * dpr));
    if (this.canvas.width !== w || this.canvas.height !== h) { this.canvas.width = w; this.canvas.height = h; this.dirty = true; }
  }

  setSun(alt, az) {
    if (this.sun && Math.abs(this.sun.alt - alt) < 1e-3 && Math.abs(this.sun.az - az) < 1e-3) return;
    this.sun = { alt, az, dir: sunDir(alt, az) };
    this.dirty = true;
    this.onSun?.(this.sun);
  }

  draw() {
    if (!this.ready || !this.dirty) return;
    this.dirty = false;
    this.resize();
    const gl = this.gl, { p, uni } = this.prog;
    const s = this.sun;
    // The three nearest basis directions, inverse-angle weighted.
    const near = this.basis.map((b, i) => ({ i, d: angle(b.dir, s.dir) })).sort((a, b) => a.d - b.d).slice(0, 3);
    let w = near.map((n) => 1 / Math.pow(n.d + 1e-3, 2));
    const sum = w.reduce((a, b) => a + b, 0);
    w = w.map((x) => x / sum);
    const tr = this.meta.sunTrans;
    const a = Math.max(0, Math.min(90, s.alt)), k = Math.floor(a), f = a - k;
    const t0 = tr[k], t1 = tr[Math.min(90, k + 1)];
    const E = [0, 1, 2].map((c) => (t0[c] + (t1[c] - t0[c]) * f) * (s.alt > 0 ? 1 : 0));
    gl.viewport(0, 0, this.canvas.width, this.canvas.height);
    gl.useProgram(p);
    const bind = (u, tx, unit) => { gl.activeTexture(gl.TEXTURE0 + unit); gl.bindTexture(gl.TEXTURE_2D, tx); gl.uniform1i(u, unit); };
    bind(uni.uDepth, this.tDepth, 0); bind(uni.uNormal, this.tNormal, 1); bind(uni.uAlbedo, this.tAlbedo, 2);
    bind(uni.uB0, this.basis[near[0].i].tex, 3); bind(uni.uB1, this.basis[near[1].i].tex, 4); bind(uni.uB2, this.basis[near[2].i].tex, 5);
    gl.uniform3f(uni.uW, w[0], w[1], w[2]);
    gl.uniform3f(uni.uScale, this.basis[near[0].i].scale, this.basis[near[1].i].scale, this.basis[near[2].i].scale);
    gl.uniform4fv(uni.uCam, camUniform(this.cam));
    gl.uniform4fv(uni.uCover, coverUv(this.imgAspect, this.canvas.width / this.canvas.height));
    gl.uniform3f(uni.uSun, s.dir[0], s.dir[1], s.dir[2]);
    gl.uniform3f(uni.uSunE, E[0], E[1], E[2]);
    gl.uniform1f(uni.uDepthMax, this.meta.depthMax);
    gl.uniform1f(uni.uExposure, Math.pow(2, evForAlt(s.alt, s.az)) * 6);
    gl.uniform1f(uni.uVignette, 0.16);
    gl.uniform2f(uni.uAspect, this.imgAspect, 1);
    gl.drawArrays(gl.TRIANGLES, 0, 3);
  }
}

/** Screen position -> sun: x from east (left, facing south) to west, y from zenith (top) to horizon. */
export function sunFromPointer(x, y) {
  const az = 60 + x * 240;
  const alt = Math.max(1, Math.min(89.5, (1 - y) * 90));
  return { alt, az };
}

/** Horizontal position -> the real 21 June sun (sunrise 05:30 to sunset 19:11). */
export function sunOnJune21(x) {
  const minutes = 332 + x * (1149 - 332);
  const p = solarPosition(2026, 6, 21, minutes);
  return { alt: p.alt, az: p.az, minutes };
}

/** HUD line for a sun: the date and time in Dubai that has it, or none. */
export function describeSun(alt, az) {
  const w = whenIsTheSun(alt, az);
  const pos = `ALT ${alt.toFixed(0)}° AZ ${az.toFixed(0)}°`;
  return w ? { when: w.label, pos, never: false } : { when: null, pos, never: true };
}
export { clock };
