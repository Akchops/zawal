// Minimal raw-WebGL helpers (~2 kB): a silent capability probe, program
// linking, a full-screen triangle, texture upload. No three.js: every
// real-time effect on this site is one full-screen shader over rendered frames.

let probed = null;

/**
 * Silent probe, run once. A context that would be software-rendered
 * (failIfMajorPerformanceCaveat, or a software renderer by name) or lacks what
 * we need counts as "no WebGL", and the page drops to the Canvas 2D tier. The
 * probe context is released.
 */
export function webglTier() {
  if (probed !== null) return probed;
  probed = "none";
  try {
    if (new URLSearchParams(location.search).has("nogl")) return (probed = "2d");
    const c = document.createElement("canvas");
    // ?forcegl accepts a software context (headless QA only).
    const strict = !new URLSearchParams(location.search).has("forcegl");
    const gl = c.getContext("webgl", { failIfMajorPerformanceCaveat: strict, antialias: false });
    if (gl && gl.getParameter(gl.MAX_TEXTURE_SIZE) >= 4096) {
      // Some browsers hand out a software rasteriser (SwiftShader, llvmpipe,
      // WARP) without flagging the performance caveat, so read the renderer
      // string too. RENDERER is the real name in Firefox; Chrome and Safari
      // say "WebKit WebGL" there and give it through the debug extension
      // (asking Firefox for that extension would log a deprecation warning).
      let name = String(gl.getParameter(gl.RENDERER));
      if (/^webkit webgl$/i.test(name)) {
        const dbg = gl.getExtension("WEBGL_debug_renderer_info");
        if (dbg) name = String(gl.getParameter(dbg.UNMASKED_RENDERER_WEBGL));
      }
      if (!strict || !/swiftshader|llvmpipe|softpipe|software|basic render|warp/i.test(name)) probed = "webgl";
    }
    gl?.getExtension("WEBGL_lose_context")?.loseContext();
    if (probed === "none" && c.getContext("2d")) probed = "2d";
  } catch {
    probed = "2d";
  }
  return probed;
}

export function program(gl, vsSrc, fsSrc) {
  const sh = (type, src) => {
    const s = gl.createShader(type);
    gl.shaderSource(s, src);
    gl.compileShader(s);
    if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(s) || "shader");
    return s;
  };
  const p = gl.createProgram();
  gl.attachShader(p, sh(gl.VERTEX_SHADER, vsSrc));
  gl.attachShader(p, sh(gl.FRAGMENT_SHADER, fsSrc));
  gl.bindAttribLocation(p, 0, "aPos");
  gl.linkProgram(p);
  if (!gl.getProgramParameter(p, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(p) || "link");
  const uni = {};
  const n = gl.getProgramParameter(p, gl.ACTIVE_UNIFORMS);
  for (let i = 0; i < n; i++) {
    const info = gl.getActiveUniform(p, i);
    uni[info.name.replace(/\[0\]$/, "")] = gl.getUniformLocation(p, info.name);
  }
  return { p, uni };
}

// One triangle that covers the viewport; vUv runs 0..1 over the screen.
export const FULLSCREEN_VS = `
attribute vec2 aPos;
varying vec2 vUv;
void main() {
  vUv = aPos * 0.5 + 0.5;
  gl_Position = vec4(aPos, 0.0, 1.0);
}`;

export function fullscreenTriangle(gl) {
  const b = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, b);
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 3, -1, -1, 3]), gl.STATIC_DRAW);
  gl.enableVertexAttribArray(0);
  gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
  return b;
}

export function texture(gl, source, { linear = true } = {}) {
  const t = gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D, t);
  // Not flipped: WebGL ignores UNPACK_FLIP_Y for ImageBitmaps anyway, so the
  // shaders sample with v = 1 - y (image rows top-down) for every source.
  gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, false);
  gl.pixelStorei(gl.UNPACK_PREMULTIPLY_ALPHA_WEBGL, false);
  gl.pixelStorei(gl.UNPACK_COLORSPACE_CONVERSION_WEBGL, gl.NONE);
  gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, source);
  const f = linear ? gl.LINEAR : gl.NEAREST;
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, f);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, f);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  return t;
}

/** uv scale/offset that makes an image of aspect `img` cover a view of aspect `view`. */
export function coverUv(img, view) {
  if (img > view) { const s = view / img; return [s, 1, (1 - s) / 2, 0]; }
  const s = img / view;
  return [1, s, 0, (1 - s) / 2];
}

export const prefersReducedMotion = () => matchMedia("(prefers-reduced-motion: reduce)").matches;
