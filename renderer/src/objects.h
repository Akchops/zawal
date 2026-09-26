// Small-object library: bounded signed distance fields for the things that
// make a scene lived-in rather than modelled — parked cars and a car under a
// dust cover, a pierced brass lantern, lathed planters and a water jar,
// cushions, a folded palm mat, a street lamp head.
//
// Each object is sphere-traced only inside its own bounding box (see
// geometry.h, P_SDF). Every combinator that shapes a visible surface is the
// smooth one (smin/smax) so no crease reaches a shading normal; hard min/max
// is used only between parts that meet at a genuine material boundary.
#pragma once
#include "common.h"
#include "noise.h"
#include "patterns.h"

namespace zw {

enum SdfKind : int {
  SDF_CAR = 0,        // generic small saloon, no brand features
  SDF_CAR_COVER,      // the same car under a fitted dust cover (very Dubai)
  SDF_LANTERN,        // pierced cylindrical brass lantern with domed cap
  SDF_PLANTER,        // tapered round planter with a rolled rim and soil
  SDF_JAR,            // unglazed water jar (lathed profile)
  SDF_CUSHION,        // floor cushion, soft rounded box with a bulge
  SDF_MAT,            // folded palm mat, three soft layers
  SDF_LAMPHEAD,       // street lamp luminaire
};

// Sub-material slots an object can return for a hit point.
enum SdfSlot : int { SLOT0 = 0, SLOT1, SLOT2, SLOT3 };

struct SdfObj {
  int kind = 0;
  V3 pos;                 // world position of the object's local origin (on the ground)
  float cosr = 1, sinr = 0;  // rotation about +Y
  float scale = 1;
  float prm[6] = {0, 0, 0, 0, 0, 0};
  int mats[4] = {0, 0, 0, 0};  // material per slot
  uint32_t seed = 0;
};

// ---- 2D/3D SDF building blocks ----------------------------------------------
inline float sdRoundBox(V3 p, V3 b, float r) {
  V3 q = vabs(p) - b + V3(r);
  return length(vmax(q, V3(0.0f))) + std::min(maxc(q), 0.0f) - r;
}
inline float sdCapsuleSeg(V3 p, V3 a, V3 b, float r) {
  V3 pa = p - a, ba = b - a;
  float h = saturate(dot(pa, ba) / dot(ba, ba));
  return length(pa - ba * h) - r;
}
inline float sdCylZ(V3 p, float r, float halfLen) {   // cylinder along Z
  float dx = std::sqrt(p.x * p.x + p.y * p.y) - r, dz = std::fabs(p.z) - halfLen;
  float ox = std::max(dx, 0.0f), oz = std::max(dz, 0.0f);
  return std::sqrt(ox * ox + oz * oz) + std::min(std::max(dx, dz), 0.0f);
}
inline float sdCylY(V3 p, float r, float halfLen) {
  float dx = std::sqrt(p.x * p.x + p.z * p.z) - r, dy = std::fabs(p.y) - halfLen;
  float ox = std::max(dx, 0.0f), oy = std::max(dy, 0.0f);
  return std::sqrt(ox * ox + oy * oy) + std::min(std::max(dx, dy), 0.0f);
}
inline float sdTorusY(V3 p, float R, float r) {
  float q = std::sqrt(p.x * p.x + p.z * p.z) - R;
  return std::sqrt(q * q + p.y * p.y) - r;
}

// ---- the car ------------------------------------------------------------------
// Local frame: +X forward, +Y up, +Z to the car's left, origin on the ground at
// the centre. Proportions of a generic 4.6 m saloon; nothing brand-specific.
inline float carBody(V3 p, float* lowerOut = nullptr, float* cabinOut = nullptr) {
  // Lower body: long rounded box, a touch of plan taper at the nose and tail.
  float taper = 1.0f - 0.05f * smoothstep(1.6f, 2.3f, std::fabs(p.x));
  V3 q(p.x, p.y - 0.64f, p.z / taper);
  float lower = sdRoundBox(q, V3(2.3f, 0.34f, 0.89f), 0.1f);
  // Glasshouse: shorter, narrower, raked front and rear by shearing x with y.
  float yy = p.y - 1.14f;
  float shear = p.x + 0.55f * yy * (p.x > -0.1f ? 1.0f : -0.7f);
  float narrow = 1.0f + 0.22f * smoothstep(0.0f, 0.3f, yy);
  V3 c(shear + 0.18f, yy, p.z * narrow);
  float cabin = sdRoundBox(c, V3(1.18f, 0.26f, 0.76f), 0.14f);
  if (lowerOut) *lowerOut = lower;
  if (cabinOut) *cabinOut = cabin;
  float body = smin(lower, cabin, 0.12f);
  // Wheel arches.
  for (float ax : {-1.42f, 1.38f}) {
    float arch = sdCylZ(V3(p.x - ax, p.y - 0.33f, p.z), 0.41f, 1.2f);
    body = smax(body, -arch, 0.03f);
  }
  return body;
}
inline float carWheels(V3 p) {
  float d = 1e9f;
  for (float ax : {-1.42f, 1.38f})
    for (float az : {-0.77f, 0.77f}) {
      V3 w(p.x - ax, p.y - 0.33f, p.z - az);
      float tyre = sdCylZ(w, 0.33f, 0.105f) - 0.02f;
      d = std::min(d, tyre);
    }
  return d;
}
// slot 0 paint, 1 glass, 2 tyre, 3 lamps
inline float sdfCar(V3 p, int* slot) {
  float lower, cabin;
  float b = carBody(p, &lower, &cabin), w = carWheels(p);
  if (slot) {
    if (w < b) *slot = SLOT2;
    else if (cabin < lower - 0.01f && p.y < 1.33f && p.y > 0.93f) *slot = SLOT1;   // side, front and rear glass
    else if (std::fabs(p.x) > 2.12f && p.y > 0.56f && p.y < 0.78f && std::fabs(p.z) > 0.42f) *slot = SLOT3;
    else *slot = SLOT0;
  }
  return std::min(b, w);
}
inline float sdfCarCover(V3 p, uint32_t seed, int* slot) {
  // A fitted fabric cover pulled taut over the car. Fabric in tension bridges
  // concave angles, so the cover spans the hollow at the windscreen and rear
  // window (a wide blend between body and cabin) and hangs straight past the
  // wheel arches (the arch-free body), but keeps the car's convex shoulders;
  // 1.5 cm proud. Drape folds run down the flanks, pleats gather at the
  // elastic hem, and the roof and bonnet carry a few shallow wind ripples.
  float lower, cabin;
  carBody(p, &lower, &cabin);
  float cover = smax(smin(lower, cabin, 0.32f) - 0.015f, 0.22f - p.y, 0.02f);
  float sd = (float)(seed & 255);
  float flank = smoothstep(0.55f, 0.85f, std::fabs(p.z)) * (1.0f - smoothstep(0.8f, 1.05f, p.y));
  float drape = gnoise(V3(p.x * 4.5f + sd, p.y * 0.7f, p.z * 4.5f));
  float pleat = std::sin(p.x * 38.0f + 3.0f * gnoise(V3(p.x * 2.0f, 0.0f, p.z * 2.0f) + V3(sd))) *
                (1.0f - smoothstep(0.22f, 0.36f, p.y));
  float top = gnoise(V3(p.x * 1.8f, p.y * 1.8f, p.z * 3.5f) + V3(sd + 7.0f)) * smoothstep(0.9f, 1.2f, p.y);
  cover += 0.012f * drape * flank - 0.004f * pleat - 0.006f * top;
  float w = carWheels(p);
  if (slot) *slot = w < cover ? SLOT2 : SLOT0;
  return std::min(cover, w);
}

// ---- the lantern ----------------------------------------------------------------
// A pierced brass cylinder (radius R, height H) with a pierced domed cap, a
// finial, base and shoulder rings, an open bottom, and a bulb on a socket.
// Holes are the site's star pattern: wrapped around the drum (sunlight from
// the side draws stars on a wall) and cut straight down through the dome (the
// overhead sun draws stars on the floor through the open bottom).
inline float sdfLantern(V3 p, int* slot) {
  const float R = 0.15f, H = 0.34f, T = 0.0025f;
  float r = std::sqrt(p.x * p.x + p.z * p.z);
  // Drum: a thin cylindrical wall.
  float drum = std::max(std::fabs(r - R) - T, std::fabs(p.y - H * 0.5f) - H * 0.5f);
  // Holes: the star pattern wrapped around the drum, u = arc length, v = height.
  // Twelve repeats fit the circumference exactly, so the atan2 seam at +-pi
  // lands on the same pattern phase and leaves no join.
  PatternParams pat;
  pat.type = PAT_STAR8; pat.period = 2.0f * PI * R / 12.0f; pat.a = 0.31f; pat.b = 0.1f; pat.c = 0.002f;
  float holes = patternSD(pat, std::atan2(p.z, p.x) * R, p.y - 0.02f);   // <0 inside a hole
  float zone = std::min(p.y - 0.035f, H - 0.035f - p.y);                // >0 in the pierced band
  float holeRegion = std::max(holes, -zone);                             // <0 where material is removed
  drum = std::max(drum, -holeRegion);
  // Dome: a flattened hemisphere shell. The ellipsoid distance is divided by
  // its largest axis stretch (1.6) so the field stays a lower bound and the
  // sphere tracer cannot step through the 4 mm shell.
  float ell = (length(V3(p.x, (p.y - H) * 1.6f, p.z)) - R) / 1.6f;
  float dome = std::max(std::fabs(ell) - 0.0022f, H - p.y);
  PatternParams dp = pat;
  dp.period = 0.05f;
  float dholes = patternSD(dp, p.x, p.z);
  float dzone = std::min(R * 0.86f - r, r - 0.032f);                     // solid rim and collar
  dome = std::max(dome, -std::max(dholes, -dzone));
  float ring = std::min(sdTorusY(p, R, 0.008f), sdTorusY(V3(p.x, p.y - H, p.z), R, 0.006f));
  // Finial and hanging loop.
  float top = H + R / 1.6f;
  float fin = sdCapsuleSeg(p, V3(0.0f, top - 0.01f, 0.0f), V3(0.0f, top + 0.05f, 0.0f), 0.012f);
  V3 lq(p.x, p.y - (top + 0.07f), p.z);                                  // vertical ring
  float lr = std::sqrt(lq.x * lq.x + lq.y * lq.y) - 0.02f;
  float loop = std::sqrt(lr * lr + lq.z * lq.z) - 0.004f;
  // Socket and a frosted bulb hanging inside.
  float socket = sdCapsuleSeg(p, V3(0.0f, top - 0.02f, 0.0f), V3(0.0f, H - 0.03f, 0.0f), 0.013f);
  float bulb = length(p - V3(0.0f, H - 0.08f, 0.0f)) - 0.03f;
  float d = std::min(std::min(drum, dome), std::min(ring, std::min(std::min(fin, loop), socket)));
  if (slot) *slot = bulb < d ? SLOT1 : SLOT0;
  return std::min(d, bulb);
}

// ---- lathed things ------------------------------------------------------------------
inline float sdfPlanter(V3 p, int* slot) {
  // prm-free: 0.9 m tall, 0.42 m top radius, tapering to 0.3 m, rolled rim.
  float r = std::sqrt(p.x * p.x + p.z * p.z);
  float H = 0.85f;
  float rad = lerpf(0.30f, 0.42f, saturate(p.y / H));
  float body = std::max(r - rad, std::max(-p.y, p.y - H));
  float inner = std::max(r - (rad - 0.035f), -(p.y - 0.12f));   // hollow above 12 cm
  body = smax(body, -inner, 0.01f);
  float rim = sdTorusY(V3(p.x, p.y - H, p.z), 0.42f, 0.04f);
  float soil = std::max(r - 0.39f, std::fabs(p.y - (H - 0.07f)) - 0.01f);
  float pot = smin(body, rim, 0.03f);
  if (slot) *slot = soil < pot ? SLOT1 : SLOT0;
  return std::min(pot, soil);
}
inline float sdfJar(V3 p, int* slot) {
  // Water jar: swollen body, short neck, thick lip.
  float r = std::sqrt(p.x * p.x + p.z * p.z);
  V3 q(p.x, (p.y - 0.52f) * 1.12f, p.z);
  float body = (length(q) - 0.44f) / 1.12f;
  float neck = std::max(r - 0.15f, std::fabs(p.y - 1.0f) - 0.12f);
  float lip = sdTorusY(V3(p.x, p.y - 1.11f, p.z), 0.155f, 0.03f);
  float foot = std::max(r - 0.22f, std::max(-p.y, p.y - 0.12f));
  float d = smin(smin(body, neck, 0.08f), smin(lip, foot, 0.03f), 0.02f);
  float mouth = std::max(r - 0.12f, -(p.y - 0.95f));
  d = smax(d, -mouth, 0.01f);
  if (slot) *slot = SLOT0;
  return d;
}

// ---- soft things -------------------------------------------------------------------
inline float sdfCushion(V3 p, const float* prm, int* slot) {
  // prm: half-length, half-depth, height. A rounded box whose top bulges.
  float hl = prm[0], hd = prm[1], h = prm[2];
  float bulge = 0.25f * h * (1.0f - (p.x * p.x) / (hl * hl)) * (1.0f - (p.z * p.z) / (hd * hd));
  V3 q(p.x, p.y - h * 0.5f - bulge * 0.5f, p.z);
  float d = sdRoundBox(q, V3(hl, h * 0.5f + bulge * 0.5f, hd), std::min(0.06f, h * 0.45f));
  if (slot) *slot = SLOT0;
  return d;
}
inline float sdfMat(V3 p, const float* prm, int* slot) {
  // Folded palm mat: three stacked layers, each slightly offset, rounded folds.
  float hl = prm[0], hd = prm[1];
  float d = 1e9f;
  for (int k = 0; k < 3; k++) {
    V3 q(p.x - 0.012f * k, p.y - (0.008f + 0.013f * k), p.z + 0.01f * (k - 1));
    d = std::min(d, sdRoundBox(q, V3(hl, 0.006f, hd), 0.005f));
  }
  // The fold edge: a rounded bead along one short side joins the layers.
  float fold = sdCapsuleSeg(p, V3(hl - 0.01f, 0.02f, -hd), V3(hl - 0.01f, 0.02f, hd), 0.017f);
  d = smin(d, fold, 0.008f);
  if (slot) *slot = SLOT0;
  return d;
}
inline float sdfLampHead(V3 p, int* slot) {
  float d = sdRoundBox(V3(p.x - 0.28f, p.y, p.z), V3(0.3f, 0.05f, 0.13f), 0.04f);
  float lens = sdRoundBox(V3(p.x - 0.28f, p.y + 0.05f, p.z), V3(0.24f, 0.012f, 0.09f), 0.01f);
  if (slot) *slot = lens < d ? SLOT1 : SLOT0;
  return std::min(d, lens);
}

inline float sdfLocal(const SdfObj& o, V3 p, int* slot) {
  switch (o.kind) {
    case SDF_CAR: return sdfCar(p, slot);
    case SDF_CAR_COVER: return sdfCarCover(p, o.seed, slot);
    case SDF_LANTERN: return sdfLantern(p, slot);
    case SDF_PLANTER: return sdfPlanter(p, slot);
    case SDF_JAR: return sdfJar(p, slot);
    case SDF_CUSHION: return sdfCushion(p, o.prm, slot);
    case SDF_MAT: return sdfMat(p, o.prm, slot);
    case SDF_LAMPHEAD: return sdfLampHead(p, slot);
  }
  return 1e9f;
}

inline V3 sdfToLocal(const SdfObj& o, V3 w) {
  V3 d = w - o.pos;
  // Inverse rotation about Y, then inverse scale.
  V3 l(o.cosr * d.x + o.sinr * d.z, d.y, -o.sinr * d.x + o.cosr * d.z);
  return l / o.scale;
}
inline float sdfWorld(const SdfObj& o, V3 w, int* slot) {
  return sdfLocal(o, sdfToLocal(o, w), slot) * o.scale;
}
// Conservative local-space bounds of each kind (before scale/rotation).
inline void sdfBounds(int kind, const float* prm, V3& lo, V3& hi) {
  switch (kind) {
    case SDF_CAR: case SDF_CAR_COVER: lo = V3(-2.5f, 0.0f, -1.05f); hi = V3(2.5f, 1.55f, 1.05f); return;
    case SDF_LANTERN: lo = V3(-0.17f, -0.01f, -0.17f); hi = V3(0.17f, 0.55f, 0.17f); return;
    case SDF_PLANTER: lo = V3(-0.47f, 0.0f, -0.47f); hi = V3(0.47f, 0.9f, 0.47f); return;
    case SDF_JAR: lo = V3(-0.5f, 0.0f, -0.5f); hi = V3(0.5f, 1.16f, 0.5f); return;
    case SDF_CUSHION: lo = V3(-prm[0], 0.0f, -prm[1]); hi = V3(prm[0], prm[2] * 1.3f + 0.01f, prm[1]); return;
    case SDF_MAT: lo = V3(-prm[0] - 0.03f, 0.0f, -prm[1] - 0.03f); hi = V3(prm[0] + 0.03f, 0.06f, prm[1] + 0.03f); return;
    case SDF_LAMPHEAD: lo = V3(-0.05f, -0.08f, -0.18f); hi = V3(0.62f, 0.1f, 0.18f); return;
  }
  lo = V3(-1.0f); hi = V3(1.0f);
}

}  // namespace zw
