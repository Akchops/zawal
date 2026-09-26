// The renderer's camera (renderer/src/render.cpp, Camera), in JS, so the page
// can reproject pixels between neighbouring frames of a camera path.
//
// Level camera: forward is the horizontal direction to the target, verticals
// stay vertical, framing is done with a vertical lens shift (like a
// tilt-shift architectural lens). fovAxis "h" gives the horizontal field of
// view, "v" the vertical.

const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
const norm = (a) => { const l = Math.hypot(a[0], a[1], a[2]) || 1; return [a[0] / l, a[1] / l, a[2] / l]; };

/** Camera basis and lens terms for a frame's cam record at an aspect ratio. */
export function cameraFrom(cam, aspect) {
  const f = sub(cam.tgt, cam.pos);
  f[1] = 0;
  const fwd = norm(f);
  const right = norm(cross(fwd, [0, 1, 0]));
  const up = cross(right, fwd);
  const t = Math.tan((cam.fov * Math.PI) / 360);
  const tanV = cam.fovAxis === "v" ? t : t / aspect;
  return { pos: cam.pos, fwd, right, up, tanV, aspect, shift: cam.shift || 0 };
}

/**
 * Camera between two frames: position linear, heading along the short arc,
 * lens linear. The walk's frames are spaced closely enough that this matches
 * the renderer's Catmull-Rom path to well under a pixel.
 */
export function lerpCam(a, b, t) {
  const yaw = (c) => Math.atan2(c.tgt[0] - c.pos[0], c.tgt[2] - c.pos[2]);
  let ya = yaw(a), yb = yaw(b);
  while (yb - ya > Math.PI) yb -= 2 * Math.PI;
  while (yb - ya < -Math.PI) yb += 2 * Math.PI;
  const y = ya + (yb - ya) * t;
  const pos = [0, 1, 2].map((k) => a.pos[k] + (b.pos[k] - a.pos[k]) * t);
  return {
    pos,
    tgt: [pos[0] + Math.sin(y) * 20, pos[1], pos[2] + Math.cos(y) * 20],
    fov: a.fov + (b.fov - a.fov) * t,
    fovAxis: a.fovAxis,
    shift: a.shift + (b.shift - a.shift) * t,
  };
}

/** Flattens a camera into the uniform layout the reprojection shader uses. */
export function camUniform(c) {
  return new Float32Array([...c.pos, c.tanV, ...c.fwd, c.aspect, ...c.right, c.shift, ...c.up, 0]);
}
