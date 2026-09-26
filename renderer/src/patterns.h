// 2D lattice patterns — the shadow vocabulary of the whole site.
//
// Every pattern is a signed distance field in the plane of its screen, metres:
//   h(u,v) < 0  inside a hole (light passes)
//   h(u,v) > 0  inside solid material
//   |h|         distance to the nearest hole wall (exact or a lower bound)
//
// The renderer extrudes these into slabs (see geometry.h: the ray's 2D
// projection is sphere-traced through the hole), so a screen's THICKNESS is
// real: at a steep sun, a deep carved screen blocks the sun its holes would
// otherwise pass. The same functions are ported verbatim to GLSL for the
// real-time "you are the sun" shader so offline and live shadows agree.
#pragma once
#include "common.h"
#include "noise.h"

namespace zw {

enum PatternType : int {
  PAT_SOLID = 0,
  PAT_STAR8 = 1,    // 8-point star + cross: the carved mashrabiya of the courtyard and library
  PAT_KAGOME = 2,   // trihexagonal rib lattice: the Qudra Canopy roof
  PAT_SLATS = 3,    // irregular palm-rib slats (areesh) with cross battens: Hotel Sikka
  PAT_DIAMOND = 4,  // diagonal trellis
  PAT_GRID = 5,     // orthogonal grid (turned-wood mashrabiya read from afar)
};

struct PatternParams {
  int type = PAT_SOLID;
  float period = 0.3f;   // repeat distance, m
  float width = 0.03f;   // rib / strap width, m
  float a = 0.0f, b = 0.0f, c = 0.0f;  // pattern-specific
  float ou = 0.0f, ov = 0.0f;          // origin offset in the plane
  float rot = 0.0f;                    // rotation of the pattern in the plane, radians
  float discR = 0.0f;                  // >0: clip to a disc of this radius (centred on ou,ov)
  float ringW = 0.0f;                  // rim beam width when clipped to a disc
};

// Distance to the nearest line of a family with unit normal (nx,ny), spacing P,
// offset o. The abs() crease sits on the rib CENTRE line, inside solid material,
// where the field is never used to build a surface normal.
inline float lineFamily(float x, float y, float nx, float ny, float P, float o) {
  float s = x * nx + y * ny - o;
  return std::fabs(s - P * std::floor(s / P + 0.5f));
}

inline float sdBox2(float x, float y, float bx, float by) {
  float dx = std::fabs(x) - bx, dy = std::fabs(y) - by;
  float ox = std::max(dx, 0.0f), oy = std::max(dy, 0.0f);
  return std::sqrt(ox * ox + oy * oy) + std::min(std::max(dx, dy), 0.0f);
}

inline float patternCore(const PatternParams& p, float u, float v);

// Full pattern: the core lattice, optionally clipped to a disc with a rim beam.
// Solid = (lattice AND inside disc) OR rim. min/max only meet at the rim, where
// the wall normal is taken from whichever field is active — a real corner.
inline float patternSD(const PatternParams& p, float u, float v) {
  float h = patternCore(p, u, v);
  if (p.discR > 0.0f) {
    float x = u - p.ou, y = v - p.ov;
    float r = std::sqrt(x * x + y * y);
    float inDisc = p.discR - r;
    float rim = 0.5f * p.ringW - std::fabs(r - (p.discR - 0.5f * p.ringW));
    h = std::max(std::min(h, inDisc), rim);
  }
  return h;
}

inline float patternCore(const PatternParams& p, float u, float v) {
  float x = u - p.ou, y = v - p.ov;
  if (p.rot != 0.0f) {
    float c = std::cos(p.rot), s = std::sin(p.rot);
    float nx = c * x - s * y, ny = s * x + c * y;
    x = nx; y = ny;
  }
  switch (p.type) {
    case PAT_STAR8: {
      // Holes: an 8-point star (union of two squares, one turned 45 deg) at
      // every cell centre, and a small turned square at every cell corner.
      // Solid: the strapwork left between them. a = star half-size as a
      // fraction of the period, b = corner-hole half-size fraction, c = fillet
      // radius (m) on the hole corners, as a carving tool would leave.
      float P = p.period;
      float cx = x - P * std::floor(x / P) - 0.5f * P;   // cell-centred coords
      float cy = y - P * std::floor(y / P) - 0.5f * P;
      float s = p.a * P;
      const float k = 0.70710678f;
      float r1 = sdBox2(cx, cy, s - p.c, s - p.c) - p.c;                                 // square
      float r2 = sdBox2(k * (cx + cy), k * (cy - cx), s - p.c, s - p.c) - p.c;           // square at 45 deg
      float star = std::min(r1, r2);
      // Corner hole: the four cell corners are the same point of the lattice,
      // so measure from the nearest corner.
      float qx = cx - (cx > 0 ? 0.5f * P : -0.5f * P);
      float qy = cy - (cy > 0 ? 0.5f * P : -0.5f * P);
      float cb = p.b * P;
      float corner = sdBox2(k * (qx + qy), k * (qy - qx), cb, cb) - 0.25f * p.c;
      return std::min(star, corner);  // union of holes: negative inside any hole
    }
    case PAT_KAGOME: {
      // Three rib families at 60 deg. Family offsets that do not meet at a
      // single point produce the trihexagonal (kagome) net: hexagons ringed by
      // triangles. a = offset of the third family as a fraction of the period.
      float P = p.period;
      float d1 = lineFamily(x, y, 0.0f, 1.0f, P, 0.0f);
      float d2 = lineFamily(x, y, 0.8660254f, -0.5f, P, 0.0f);
      float d3 = lineFamily(x, y, -0.8660254f, -0.5f, P, p.a * P);
      // Smooth union of the ribs leaves a small fillet where ribs meet, as cast
      // or glulam nodes do; c = fillet width (m).
      float d = smin(smin(d1, d2, p.c), d3, p.c);
      return 0.5f * p.width - d;
    }
    case PAT_SLATS: {
      // Palm-rib slats running along v, irregular in width and spacing, with
      // a slight wobble, tied to cross battens every a metres.
      float P = p.period;
      float i = std::floor(x / P);
      float best = 1e9f;
      for (int k = -1; k <= 1; k++) {
        float ii = i + k;
        uint32_t h = hash2i((int)ii, 17);
        float centre = (ii + 0.5f) * P + (hashf(h) - 0.5f) * 0.35f * P + 0.012f * gnoise2(ii * 3.1f, y * 0.9f);
        float w = p.width * (0.75f + 0.5f * hashf(h ^ 0xabcdefu));
        best = std::min(best, std::fabs(x - centre) - 0.5f * w);
      }
      float batten = p.a > 0 ? lineFamily(x, y, 0.0f, 1.0f, p.a, 0.0f) - 0.5f * p.b : 1e9f;
      return -std::min(best, batten);
    }
    case PAT_DIAMOND: {
      float P = p.period;
      float d = std::min(lineFamily(x, y, 0.70710678f, 0.70710678f, P, 0.0f),
                         lineFamily(x, y, 0.70710678f, -0.70710678f, P, 0.0f));
      return 0.5f * p.width - d;
    }
    case PAT_GRID: {
      float P = p.period;
      float d = std::min(lineFamily(x, y, 1.0f, 0.0f, P, 0.0f), lineFamily(x, y, 0.0f, 1.0f, P, 0.0f));
      return 0.5f * p.width - d;
    }
    default:
      return 1.0f;
  }
}

// Gradient of the pattern field (points from hole into solid), for hole-wall normals.
inline void patternGrad(const PatternParams& p, float u, float v, float& gu, float& gv) {
  const float e = 2.5e-4f;
  gu = patternSD(p, u + e, v) - patternSD(p, u - e, v);
  gv = patternSD(p, u, v + e) - patternSD(p, u, v - e);
  float l = std::sqrt(gu * gu + gv * gv);
  if (l > 1e-12f) { gu /= l; gv /= l; } else { gu = 1.0f; gv = 0.0f; }
}

}  // namespace zw
