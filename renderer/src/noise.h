// Procedural noise for materials.
//
// Written against the shader-trap checklist (webgl-shader-interaction-engineer,
// known-traps.md), because every material in the renderer is lit by a sun at a
// grazing angle at some point in the day, which is exactly the condition that
// exposes each trap:
//
//  #1 value-noise lattice  -> gradient noise (value is 0 at lattice points, so
//     extrema sit between them), quintic fade (C2 across cell faces), octaves
//     rotated by a non-axis-aligned orthonormal matrix and scaled by 1.97, not 2.
//  #3 clamp creases        -> nothing here uses abs/min/max/floor on a value
//     that is later differentiated into a normal. Cellular noise is only ever
//     used for albedo (never differentiated) unless the smooth variant is used.
//
// The proof renders (tools/trap_proof.py) differentiate these fields and plot
// the gradient magnitude; a lattice or crease would show as a bright grid/line.
#pragma once
#include "common.h"

namespace zw {

// 256 pseudo-random unit gradients, generated once from a fixed seed so every
// render (and every frame of a sequence) sees the identical field.
struct GradTable {
  V3 g3[256];
  float g2x[256], g2y[256];
  GradTable() {
    uint32_t s = 0x2545F491u;
    for (int i = 0; i < 256; i++) {
      // Uniform direction on the sphere from two hashed uniforms.
      s = pcg(s + (uint32_t)i);
      float u = (s >> 8) * (1.0f / 16777216.0f);
      s = pcg(s ^ 0x9E3779B9u);
      float v = (s >> 8) * (1.0f / 16777216.0f);
      float z = 1.0f - 2.0f * u, r = std::sqrt(std::max(0.0f, 1.0f - z * z)), ph = 2.0f * PI * v;
      g3[i] = V3(r * std::cos(ph), r * std::sin(ph), z);
      g2x[i] = std::cos(2.0f * PI * (i + 0.5f) / 256.0f + 0.37f * std::sin(i * 12.9898f));
      g2y[i] = std::sin(2.0f * PI * (i + 0.5f) / 256.0f + 0.37f * std::sin(i * 12.9898f));
    }
  }
};
inline const GradTable& GT() { static GradTable t; return t; }

// Quintic fade 6t^5 - 15t^4 + 10t^3: value, slope AND curvature continuous at
// cell faces. The cubic smoothstep is only C1 and its curvature jump lights up
// as a grid once the field drives a normal (trap #1).
inline float quintic(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

// 3D gradient noise, range approximately [-1, 1].
inline float gnoise(V3 p) {
  const GradTable& T = GT();
  float fx = std::floor(p.x), fy = std::floor(p.y), fz = std::floor(p.z);
  int ix = (int)fx, iy = (int)fy, iz = (int)fz;
  float x = p.x - fx, y = p.y - fy, z = p.z - fz;
  float u = quintic(x), v = quintic(y), w = quintic(z);
  auto G = [&](int dx, int dy, int dz) {
    const V3& g = T.g3[hash3i(ix + dx, iy + dy, iz + dz) & 255];
    return g.x * (x - dx) + g.y * (y - dy) + g.z * (z - dz);
  };
  float n000 = G(0, 0, 0), n100 = G(1, 0, 0), n010 = G(0, 1, 0), n110 = G(1, 1, 0);
  float n001 = G(0, 0, 1), n101 = G(1, 0, 1), n011 = G(0, 1, 1), n111 = G(1, 1, 1);
  float nx00 = lerpf(n000, n100, u), nx10 = lerpf(n010, n110, u);
  float nx01 = lerpf(n001, n101, u), nx11 = lerpf(n011, n111, u);
  float nxy0 = lerpf(nx00, nx10, v), nxy1 = lerpf(nx01, nx11, v);
  // 1.6 rescales the theoretical +-0.87 peak of unit-gradient noise to ~+-1.
  return 1.6f * lerpf(nxy0, nxy1, w);
}

// 2D gradient noise for flat pattern work (lattice irregularity, sand ripples).
inline float gnoise2(float px, float py) {
  const GradTable& T = GT();
  float fx = std::floor(px), fy = std::floor(py);
  int ix = (int)fx, iy = (int)fy;
  float x = px - fx, y = py - fy;
  float u = quintic(x), v = quintic(y);
  auto G = [&](int dx, int dy) {
    uint32_t h = hash2i(ix + dx, iy + dy) & 255;
    return T.g2x[h] * (x - dx) + T.g2y[h] * (y - dy);
  };
  return 1.4f * lerpf(lerpf(G(0, 0), G(1, 0), u), lerpf(G(0, 1), G(1, 1), u), v);
}

// Octave step: an orthonormal rotation that is not a multiple of 90 degrees
// about any axis, times 1.97. With exactly 2.0 and no rotation, each octave's
// lattice lands on the previous one's and the grids reinforce (trap #1, fix 3).
inline V3 octaveStep(V3 p) {
  return V3(0.00f * p.x + 0.80f * p.y + 0.60f * p.z,
            -0.80f * p.x + 0.36f * p.y - 0.48f * p.z,
            -0.60f * p.x - 0.48f * p.y + 0.64f * p.z) * 1.97f;
}
inline void octaveStep2(float& x, float& y) {
  // ~0.5 rad rotation, scale 1.97.
  float nx = 0.8776f * x - 0.4794f * y, ny = 0.4794f * x + 0.8776f * y;
  x = nx * 1.97f; y = ny * 1.97f;
}

inline float fbm(V3 p, int octaves, float gain = 0.5f) {
  float sum = 0.0f, amp = 0.5f, norm = 0.0f;
  for (int i = 0; i < octaves; i++) {
    sum += amp * gnoise(p);
    norm += amp;
    p = octaveStep(p);
    amp *= gain;
  }
  return sum / norm;
}

inline float fbm2(float x, float y, int octaves, float gain = 0.5f) {
  float sum = 0.0f, amp = 0.5f, norm = 0.0f;
  for (int i = 0; i < octaves; i++) {
    sum += amp * gnoise2(x, y);
    norm += amp;
    octaveStep2(x, y);
    amp *= gain;
  }
  return sum / norm;
}

// Domain warp (trap #1, fix 4): for large, flat, evenly lit walls — exactly the
// limewash case — offsetting the lookup by another field destroys any residual
// axis alignment and gives the cloudy, brushed character of real limewash.
inline float warpedFbm(V3 p, int octaves, float warp) {
  V3 q(fbm(p + V3(0.0f, 0.0f, 0.0f), 3), fbm(p + V3(5.2f, 1.3f, 2.8f), 3), fbm(p + V3(1.7f, 9.2f, 4.1f), 3));
  return fbm(p + q * warp, octaves);
}

// Cellular noise: F1 distance and a stable per-cell id. Used for aggregate and
// speckle in ALBEDO only — its F1 field has creases along Voronoi edges, so it
// must never be differentiated (see smoothCell for the bump-safe version).
struct Cell { float f1; uint32_t id; V3 offset; };
inline Cell cellular(V3 p) {
  float fx = std::floor(p.x), fy = std::floor(p.y), fz = std::floor(p.z);
  int ix = (int)fx, iy = (int)fy, iz = (int)fz;
  Cell c{1e9f, 0u, V3(0.0f)};
  for (int dz = -1; dz <= 1; dz++)
    for (int dy = -1; dy <= 1; dy++)
      for (int dx = -1; dx <= 1; dx++) {
        uint32_t h = hash3i(ix + dx, iy + dy, iz + dz);
        V3 fp(ix + dx + hashf(h), iy + dy + hashf(h ^ 0x68bc21ebu), iz + dz + hashf(h ^ 0x02e5be93u));
        V3 d = fp - p;
        float dd = dot(d, d);
        if (dd < c.f1) { c.f1 = dd; c.id = h; c.offset = d; }
      }
  c.f1 = std::sqrt(c.f1);
  return c;
}

// Smooth F1 (exponential smooth-min over the neighbourhood): C-infinity, so it
// can drive a bump normal without drawing Voronoi edges as creases.
inline float smoothCell(V3 p, float k) {
  float fx = std::floor(p.x), fy = std::floor(p.y), fz = std::floor(p.z);
  int ix = (int)fx, iy = (int)fy, iz = (int)fz;
  float acc = 0.0f;
  for (int dz = -1; dz <= 1; dz++)
    for (int dy = -1; dy <= 1; dy++)
      for (int dx = -1; dx <= 1; dx++) {
        uint32_t h = hash3i(ix + dx, iy + dy, iz + dz);
        V3 fp(ix + dx + hashf(h), iy + dy + hashf(h ^ 0x68bc21ebu), iz + dz + hashf(h ^ 0x02e5be93u));
        float d = length(fp - p);
        acc += std::exp(-k * d);
      }
  return -std::log(std::max(acc, 1e-30f)) / k;
}

}  // namespace zw
