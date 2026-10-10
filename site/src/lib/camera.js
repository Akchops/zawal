// The renderer's camera (renderer/src/render.cpp, Camera), in JS, so the page
// can reproject pixels between neighbouring frames of a camera path.
//
// Level camera (the default, as in the renderer): forward is the horizontal
// direction to the target, verticals stay vertical, framing is done with a
// vertical lens shift (like a tilt-shift architectural lens). A record with
// level: false is a free camera that looks straight at its target (the
// approach looks down from the air); the lens shift still applies. fovAxis
// "h" gives the horizontal field of view, "v" the vertical.

const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
const norm = (a) => { const l = Math.hypot(a[0], a[1], a[2]) || 1; return [a[0] / l, a[1] / l, a[2] / l]; };

/** Camera basis and lens terms for a frame's cam record at an aspect ratio. */
export function cameraFrom(cam, aspect) {
  const f = sub(cam.tgt, cam.pos);
  if (cam.level !== false) f[1] = 0;
  const fwd = norm(f);
  const right = norm(cross(fwd, [0, 1, 0]));
  const up = cross(right, fwd);
  const t = Math.tan((cam.fov * Math.PI) / 360);
  const tanV = cam.fovAxis === "v" ? t : t / aspect;
  return { pos: cam.pos, fwd, right, up, tanV, aspect, shift: cam.shift || 0 };
}

/**
 * Camera between two frames: position linear, view direction along the
 * great-circle arc between the two (slerp, so a camera pitching down as it
 * turns does both at an even rate), lens linear. The frames are spaced
 * closely enough that this matches the renderer's Catmull-Rom path to well
 * under a pixel. A level pair stays level.
 */
export function lerpCam(a, b, t) {
  const level = a.level !== false && b.level !== false;
  const dirOf = (c) => { const d = sub(c.tgt, c.pos); if (level) d[1] = 0; return norm(d); };
  const da = dirOf(a), db = dirOf(b);
  const cos = Math.max(-1, Math.min(1, da[0] * db[0] + da[1] * db[1] + da[2] * db[2]));
  const om = Math.acos(cos);
  let d;
  if (om < 1e-5) d = da;
  else {
    const s = Math.sin(om), wa = Math.sin((1 - t) * om) / s, wb = Math.sin(t * om) / s;
    d = norm([0, 1, 2].map((k) => da[k] * wa + db[k] * wb));
  }
  const pos = [0, 1, 2].map((k) => a.pos[k] + (b.pos[k] - a.pos[k]) * t);
  return {
    pos,
    tgt: [pos[0] + d[0] * 20, pos[1] + d[1] * 20, pos[2] + d[2] * 20],
    fov: a.fov + (b.fov - a.fov) * t,
    fovAxis: a.fovAxis,
    shift: a.shift + (b.shift - a.shift) * t,
    level,
  };
}

/** Flattens a camera into the uniform layout the reprojection shader uses. */
export function camUniform(c) {
  return new Float32Array([...c.pos, c.tanV, ...c.fwd, c.aspect, ...c.right, c.shift, ...c.up, 0]);
}
