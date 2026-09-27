// Procedural materials — ZAWAL's palette of lime plaster, rammed earth, teak,
// brass, woven palm, limestone and weathered copper.
//
// Every material is a solid (3D) texture evaluated at the world-space hit, so
// there are no UV seams or stretching on any face. Albedo is authored in sRGB
// hex and converted to linear. Bump height fields are in metres and built only
// from C1-or-better functions (gradient noise, smoothstep/smootherstep, smin/
// smax, sin), because every surface meets a grazing sun at some hour and the
// grazing sun is exactly what turns a slope discontinuity into a visible line.
#pragma once
#include "common.h"
#include "geometry.h"
#include "noise.h"

namespace zw {

enum MaterialId : int {
  M_LIME = 0,       // limewash on lime plaster, warm sand-white
  M_LIME_SHADE,     // same wash, deeper sand, used for interior soffits/returns
  M_RAMMED,         // rammed earth, stratified
  M_STONE_FLOOR,    // honed limestone pavers 600x600
  M_STONE,          // limestone coping / sills / plinths, unjointed
  M_TEAK,           // oiled teak
  M_TEAK_GREY,      // sun-weathered teak, silvered
  M_BRASS,          // aged brass
  M_COPPER,         // copper with verdigris patina (the accent)
  M_WATER,          // still water in the rill
  M_SAND,           // Al Qudra dune sand
  M_STREET,         // lime-stabilised earth street with loose sand
  M_TERRACOTTA,     // unglazed clay
  M_DARKROOM,       // unlit room interior beyond an opening
  M_PALM,           // woven palm frond (khoos / areesh)
  M_BRONZE,         // dark bronze window and door frames
  M_BARK,           // ghaf / olive bark
  M_LEAF_GHAF,      // dusty grey-green ghaf leaflets (thin, translucent)
  M_LEAF_OLIVE,     // silvery olive leaves (thin, translucent)
  M_CARPAINT,       // dusty white car paint
  M_GLASS,          // car glass: dark, glossy
  M_RUBBER,         // tyres
  M_CARLAMP,        // lamp lenses and trim
  M_FABRIC_COVER,   // silver-beige car dust cover
  M_STEEL,          // galvanised steel: lamp posts, poles, boxes
  M_CABLE,          // black overhead cable
  M_LINEN_INDIGO,   // cushion linen, faded indigo
  M_LINEN_OCHRE,    // cushion linen, ochre
  M_LINEN_NATURAL,  // cushion linen, undyed
  M_SOIL,           // planter soil
  M_LENS,           // frosted luminaire lens
  M_COPPER_DEEP,    // pool lining: deep, even verdigris
  M_CORAL,          // coral-stone walls under a worn lime render (old Deira)
  M_BOOKS,          // a wall of book spines on teak shelves
  M_COUNT
};

struct Scene;

// Everything a material may need beyond the hit point.
struct ShadeCtx {
  const Scene* sc = nullptr;
  const Prim* prim = nullptr;
  V3 ng = V3(0, 1, 0);   // geometric normal, facing the viewer
  float cavity = -1.0f;  // 0..1 local occlusion within 30 cm; <0 when unknown (bounces)
  uint32_t sub = 0;      // per-part / per-leaf id
};

struct BSDF {
  V3 albedo;        // diffuse albedo, linear
  V3 f0;            // specular reflectance at normal incidence
  float alpha;      // GGX roughness (alpha = perceptual^2)
  float metal;      // 0 dielectric .. 1 metal
  V3 n;             // shading normal
  bool dielectric = false;  // smooth refractive interface (water)
  float ior = 1.333f;
  V3 emission = V3(0.0f);
  bool thin = false;        // two-sided translucent sheet (leaves)
  V3 trans = V3(0.0f);      // diffuse transmission albedo for thin sheets
};

// Perturb n by the surface gradient of a height field H (metres). Central
// differences, projected onto the tangent plane.
template <class F>
inline V3 bump(V3 n, V3 p, float eps, float strength, F H) {
  V3 g((H(p + V3(eps, 0, 0)) - H(p - V3(eps, 0, 0))) / (2 * eps),
       (H(p + V3(0, eps, 0)) - H(p - V3(0, eps, 0))) / (2 * eps),
       (H(p + V3(0, 0, eps)) - H(p - V3(0, 0, eps))) / (2 * eps));
  V3 surfGrad = g - n * dot(g, n);
  return normalize(n - surfGrad * strength);
}

// ---- individual materials ---------------------------------------------------

inline float limeHeight(V3 p) {
  // Trowel undulation (cm scale), brushed wash ridges (mm), sand grain (sub-mm).
  // Hand-floated lime is never flat: ~2 mm over 20 cm is what grazing morning
  // light picks out.
  return 0.0022f * fbm(p * 2.3f, 4) + 0.00030f * gnoise(p * 85.0f + V3(3.1f)) +
         0.00012f * gnoise(p * 240.0f + V3(7.7f));
}

// Patch repairs on a plastered wall: a 1.7 x 1.3 m cell grid in the wall
// plane (u along the wall, v up); about one cell in five holds a rectangle of
// re-applied wash. Albedo only, so the box distance may be hard-edged; the
// edge is softened and warped so it reads as a brush line, not a mask.
inline float limePatch(float u, float v, uint32_t seed, float& tone) {
  const float CU = 1.7f, CV = 1.3f;
  float wu = u + 0.05f * gnoise2(u * 3.1f, v * 3.1f + 5.0f);
  float wv = v + 0.05f * gnoise2(u * 3.1f + 9.0f, v * 3.1f);
  int iu = (int)std::floor(wu / CU), iv = (int)std::floor(wv / CV);
  float best = 0.0f;
  for (int du = -1; du <= 1; du++)
    for (int dv = -1; dv <= 1; dv++) {
      uint32_t h = hash3i(iu + du, iv + dv, (int)(seed & 0xffff) + 77);
      if (hashf(h) > 0.22f) continue;
      float cu = (iu + du + 0.5f + 0.5f * (hashf(h ^ 0x1234567u) - 0.5f)) * CU;
      float cv = (iv + dv + 0.5f + 0.5f * (hashf(h ^ 0x2345678u) - 0.5f)) * CV;
      float hu = 0.2f + 0.45f * hashf(h ^ 0x3456789u), hv = 0.16f + 0.34f * hashf(h ^ 0x456789au);
      float d = std::max(std::fabs(wu - cu) - hu, std::fabs(wv - cv) - hv);
      float m = 1.0f - smoothstep(-0.03f, 0.02f, d);
      if (m > best) { best = m; tone = hashf(h ^ 0x56789abu) * 2.0f - 1.0f; }
    }
  return best;
}

inline BSDF matLime(V3 p, V3 n, uint32_t seed, bool deep) {
  BSDF b;
  float s = (seed & 1023) * 0.173f;
  // Limewash clouding: domain-warped, 1-2 m scale. Warping is what removes
  // the last trace of lattice alignment on these huge flat walls (trap #1).
  float cloud = warpedFbm(p * 0.62f + V3(s, 0.0f, s * 0.7f), 5, 1.4f);
  float cloud2 = fbm(p * 2.1f + V3(9.1f, s, 2.3f), 4);           // patchier secondary coats
  // Crossed brush strokes: strongly anisotropic noise in two diagonal frames.
  V3 d1(p.x + p.y, p.y - p.x, p.z + p.y), d2(p.x - p.y, p.y + p.z, p.z - p.y);
  float strokes = 0.5f * (gnoise(V3(d1.x * 1.5f, d1.y * 9.0f, d1.z * 1.5f)) + gnoise(V3(d2.x * 9.0f, d2.y * 1.5f, d2.z * 9.0f)));
  float grain = gnoise(p * 150.0f);
  V3 base = deep ? hex(0xD8C8AE) : hex(0xE9DFCE);
  V3 warm = deep ? hex(0xCDB898) : hex(0xE0D0B6);   // where the wash is thin, the sandy render shows
  V3 cool = deep ? hex(0xD3CBBE) : hex(0xEDE8DE);   // fresher, chalkier coats
  float thin = smoothstep(0.0f, 0.5f, -cloud + 0.3f * cloud2);
  V3 a = lerp(base, warm, thin * 0.7f);
  a = lerp(a, cool, smoothstep(0.05f, 0.45f, cloud) * 0.7f);
  // Mostly tonal: limewash clouds in value, only a whisper in hue.
  a *= 1.0f + 0.055f * cloud + 0.035f * cloud2 + 0.025f * strokes + 0.02f * grain;
  // Dust and splash-back along the foot of the wall.
  float foot = 1.0f - smoothstep(0.0f, 0.6f, p.y);
  a = lerp(a, a * hex(0xC9B08F) * 1.22f, foot * (0.6f + 0.25f * fbm(p * 3.0f, 3)));
  if (std::fabs(n.y) < 0.5f) {
    float u = std::fabs(n.x) > std::fabs(n.z) ? p.z : p.x;
    // Patch repairs: squares of newer or older wash, edges never quite straight.
    float tone = 0.0f, pm = limePatch(u, p.y, seed, tone);
    // Linear multipliers within a few percent: a fresher coat is a touch
    // chalkier and cooler, an older one a touch warmer. Anything stronger reads
    // as a pasted rectangle.
    V3 pt = tone > 0.0f ? V3(1.0f, 1.0f, 1.012f) : V3(1.0f, 0.988f, 0.965f);
    a = lerp(a, a * pt * (1.0f + 0.05f * tone), pm * 0.85f);
    // Rising damp: ground salts wick up and dry as a white tide line with a
    // cooler, darker band under it.
    float yT = 0.46f + 0.10f * fbm2(u * 1.3f + s, 3.7f, 3) + 0.015f * gnoise2(u * 5.0f, 1.1f + s);
    float damp = 1.0f - smoothstep(yT - 0.05f, yT + 0.005f, p.y);
    float line = std::exp(-((p.y - yT) * (p.y - yT)) / (0.011f * 0.011f));
    float salt = smoothstep(0.15f, 0.45f, gnoise(p * 55.0f + V3(s)));
    a = lerp(a, a * V3(0.87f, 0.875f, 0.885f), damp * (0.75f + 0.25f * fbm(p * 6.0f, 2)));
    a = lerp(a, hex(0xF2EEE6), saturate(line * 0.45f + damp * salt * 0.18f));
  }
  b.albedo = a;
  b.f0 = V3(0.035f);
  b.alpha = 0.72f;
  b.metal = 0.0f;
  b.n = bump(n, p, 0.0009f, 1.0f, limeHeight);
  return b;
}

inline float rammedLayerCoord(V3 p) {
  // Lifts undulate slightly: rammed layers are never perfectly level.
  return p.y + 0.010f * gnoise(V3(p.x * 0.7f, p.y * 0.25f, p.z * 0.7f)) + 0.035f * fbm(p * 0.35f, 2);
}
inline float rammedHeight(V3 p) {
  float y = rammedLayerCoord(p);
  // Differential erosion: softer layers recede. Noise stretched along the
  // strata makes the relief read as bands, not blobs. All smooth.
  float bands = 0.0024f * gnoise(V3(p.x * 0.9f, y * 11.0f, p.z * 0.9f));
  float grit = 0.00035f * gnoise(p * 70.0f);
  float pits = -0.0018f * smoothstep(0.45f, 0.85f, gnoise(p * 38.0f + V3(11.0f)));
  // Formwork lift line every 0.9 m: a shallow smooth groove.
  float ly = y / 0.9f;
  float fl = ly - std::floor(ly);
  // Distance to the nearest lift line. min() kinks at the lift line itself and
  // half-way between lifts, but smootherstep has zero first AND second
  // derivative at 0 and is constant beyond 9 mm, so neither kink reaches the
  // normal (trap #3 checked, not just avoided).
  float dist = std::min(fl, 1.0f - fl) * 0.9f;
  float groove = -0.0022f * (1.0f - smootherstep(0.0f, 0.009f, dist));
  return bands + grit + pits + groove;
}
inline BSDF matRammed(V3 p, V3 n, uint32_t seed) {
  BSDF b;
  float y = rammedLayerCoord(p);
  // Courses 4-16 cm: thickness modulated along the wall by a slow warp, so
  // bands swell and pinch like real lifts rammed from different batches.
  float warp = 0.9f * fbm(V3(p.x * 0.22f, y * 0.3f, p.z * 0.22f), 3);
  float lc = y / 0.092f + warp;
  float li = std::floor(lc), lf = lc - li;
  uint32_t h = hash2i((int)li, (int)(seed & 7));
  float t = hashf(h);
  // Earths from different pits: sand, red ochre, pale lime-rich, dark loam.
  static const V3 pal[6] = {hex(0xCBA67E), hex(0xAF7D57), hex(0xDCC6A3), hex(0x94705A), hex(0xBF976D), hex(0xD1B08A)};
  V3 c0 = pal[h % 6], c1 = pal[(h / 6) % 6];
  V3 a = lerp(c0, c1, 0.3f * t);
  // Each course is densest (darkest) at its base where the rammer bites.
  a *= 0.88f + 0.12f * smoothstep(0.0f, 0.5f, lf);
  // Fine sub-laminations inside a course.
  a *= 1.0f + 0.05f * gnoise(V3(p.x * 0.8f, y * 60.0f, p.z * 0.8f));
  a *= 1.0f + 0.07f * fbm(p * V3(0.9f, 3.0f, 0.9f), 3);
  // Aggregate: stones and lime nodules (albedo only — cellular F1 is never
  // differentiated, see noise.h).
  Cell c = cellular(p * 42.0f);
  float stone = 1.0f - smoothstep(0.16f, 0.30f, c.f1);
  float sh = hashf(c.id);
  V3 pebble = sh < 0.45f ? hex(0x7D6A58) : (sh < 0.8f ? hex(0xE9DFCE) : hex(0x5E4B3C));
  a = lerp(a, pebble, stone * (sh < 0.9f ? 0.65f : 0.0f));
  // Lift line: a thin darker compaction seam every 0.9 m.
  float ly = y / 0.9f;
  float fl = ly - std::floor(ly);
  float dist = std::min(fl, 1.0f - fl) * 0.9f;
  a *= 1.0f - 0.22f * (1.0f - smoothstep(0.0f, 0.012f, dist));
  float foot = 1.0f - smoothstep(0.0f, 0.4f, p.y);
  a *= 1.0f - 0.12f * foot;
  b.albedo = a;
  b.f0 = V3(0.03f);
  b.alpha = 0.9f;
  b.metal = 0.0f;
  b.n = bump(n, p, 0.0011f, 1.0f, rammedHeight);
  return b;
}

// ---- the courtyard floor ------------------------------------------------------------
// Honed limestone laid in running-bond courses: 520 mm courses, slabs of
// random length (~0.55-1.0 m), 4 mm sand-lime joints. Each slab is its own
// piece of stone: its own tone, a fraction of a millimetre of lippage and a
// slight tilt, which grazing light turns into the faceted look of real
// paving. (A regular 600x600 grid is what made the Phase 1 floor read as
// porcelain.)
struct Slab { float dEdge; uint32_t id; float cx, cz; };
inline Slab floorSlab(float x, float z) {
  const float CWd = 0.52f, L = 0.78f;
  int kz = (int)std::floor(z / CWd);
  float off = hashf(hash2i(kz, 911)) * 1.3f;
  auto bnd = [&](int i) { return off + i * L + (hashf(hash2i(i, kz * 31 + 7)) - 0.5f) * 0.34f * L; };
  int i = (int)std::floor((x - off) / L);
  if (x < bnd(i)) i--;
  else if (x >= bnd(i + 1)) i++;
  float xa = bnd(i), xb = bnd(i + 1), za = kz * CWd, zb = za + CWd;
  Slab s;
  // Smooth min of the four edge distances: no diagonal crease where two
  // distances tie near a corner (trap #3).
  float dx = smin(x - xa, xb - x, 0.002f), dz = smin(z - za, zb - z, 0.002f);
  s.dEdge = smin(dx, dz, 0.002f);
  s.id = hash2i(i + 4099, kz - 77);
  s.cx = 0.5f * (xa + xb);
  s.cz = 0.5f * (za + zb);
  return s;
}
inline float floorHeight(V3 p) {
  Slab s = floorSlab(p.x, p.z);
  // Joint: 4 mm wide, grout 1.8 mm below the arris, C2 shoulders.
  float joint = 1.0f - smootherstep(0.0006f, 0.0032f, s.dEdge);
  float lip = (hashf(s.id) - 0.5f) * 0.0009f;
  float tx = (hashf(s.id ^ 0x3c6ef372u) - 0.5f) * 0.0016f, tz = (hashf(s.id ^ 0xa54ff53au) - 0.5f) * 0.0016f;
  float slab = lip + tx * (p.x - s.cx) + tz * (p.z - s.cz);
  // Honed surface: faint pitting and the ghost of the saw.
  float tex = 0.00016f * gnoise(p * 95.0f) - 0.0003f * smoothstep(0.55f, 0.9f, gnoise(p * 30.0f + V3(3.3f)));
  return -0.0018f * joint + (slab + tex) * (1.0f - joint);
}
inline BSDF matStoneFloor(V3 p, V3 n) {
  BSDF b;
  Slab s = floorSlab(p.x, p.z);
  float h0 = hashf(s.id), h1 = hashf(s.id ^ 0x51f15eu), h2 = hashf(s.id ^ 0x9b05688cu);
  // Each slab from a different part of the bed: creams, buffs, a few greyer.
  V3 a = lerp(hex(0xE4D8C4), hex(0xD6C4A6), h0 * 0.8f);
  a = lerp(a, hex(0xCFC8BA), h1 < 0.18f ? 0.6f : 0.0f);
  a *= 0.94f + 0.09f * h2;
  // Bedding: faint parallel veins running the slab's length, warped.
  float vein = gnoise(V3(p.x * 1.6f + h0 * 9.0f, 0.0f, p.z * 14.0f + 2.0f * fbm(p * 1.1f, 2)));
  a *= 1.0f - 0.05f * smoothstep(0.35f, 0.75f, vein);
  a *= 1.0f + 0.045f * fbm(V3(p.x * 4.0f, 0.0f, p.z * 4.0f) + V3(h1 * 13.0f), 3);
  // Fossil flecks.
  Cell c = cellular(p * 70.0f + V3(h0 * 31.0f, 0.0f, h1 * 17.0f));
  a = lerp(a, hex(0xB5A386), (1.0f - smoothstep(0.07f, 0.15f, c.f1)) * 0.45f);
  // Sand-lime grout, a shade darker, dusty.
  float joint = 1.0f - smoothstep(0.0008f, 0.0030f, s.dEdge);
  a = lerp(a, hex(0xA89779) * (1.0f + 0.1f * gnoise(p * 200.0f)), joint);
  // Chipped arrises: small lighter spalls along some slab edges.
  float chip = smoothstep(0.55f, 0.8f, gnoise(p * 45.0f + V3(h2 * 20.0f))) * (1.0f - smoothstep(0.003f, 0.012f, s.dEdge));
  a = lerp(a, hex(0xEFE6D6), chip * 0.7f);
  b.albedo = a;
  b.f0 = V3(0.035f);
  b.alpha = 0.66f;          // honed, matte; only a faint sheen at grazing angles
  b.metal = 0.0f;
  b.n = bump(n, p, 0.0005f, 1.0f, floorHeight);
  return b;
}

inline BSDF matStone(V3 p, V3 n) {
  BSDF b;
  V3 a = hex(0xE2D6C2) * (1.0f + 0.05f * fbm(p * 1.3f, 4));
  Cell c = cellular(p * 70.0f);
  a = lerp(a, hex(0xB8A68C), (1.0f - smoothstep(0.08f, 0.16f, c.f1)) * 0.45f);
  b.albedo = a;
  b.f0 = V3(0.04f);
  b.alpha = 0.55f;
  b.metal = 0.0f;
  b.n = bump(n, p, 0.0006f, 1.0f, [](V3 q) { return 0.00025f * gnoise(q * 60.0f) + 0.0006f * fbm(q * 4.0f, 3); });
  return b;
}

inline void grainFrame(int axis, V3 p, float& along, float& a1, float& a2) {
  if (axis == 0) { along = p.x; a1 = p.y; a2 = p.z; }
  else if (axis == 1) { along = p.y; a1 = p.x; a2 = p.z; }
  else { along = p.z; a1 = p.x; a2 = p.y; }
}
inline BSDF matTeak(V3 p, V3 n, int grainAxis, uint32_t seed, bool grey) {
  BSDF b;
  float along, a1, a2;
  grainFrame(grainAxis, p, along, a1, a2);
  float off = (seed & 255) * 0.37f;
  // Growth rings seen on the faces: noise stretched ~40x along the grain.
  float w = fbm(V3(along * 0.35f + off, a1 * 14.0f, a2 * 14.0f), 4);
  float rings = 0.5f + 0.5f * std::sin((a1 * 9.0f + a2 * 7.0f + 6.0f * w) * 6.2832f);
  float fine = gnoise(V3(along * 2.0f, a1 * 180.0f, a2 * 180.0f));
  V3 a;
  if (!grey) {
    a = lerp(hex(0x6B4428), hex(0x8E5E37), rings * 0.6f + 0.2f);
    a *= 1.0f + 0.10f * fine + 0.08f * w;
  } else {
    a = lerp(hex(0x8A8172), hex(0xA59C8C), rings * 0.5f + 0.25f);
    a *= 1.0f + 0.08f * fine + 0.10f * w;
  }
  b.albedo = a;
  b.f0 = V3(0.04f);
  b.alpha = grey ? 0.75f : 0.38f;
  b.metal = 0.0f;
  int ga = grainAxis;
  b.n = bump(n, p, 0.0004f, 1.0f, [ga](V3 q) {
    float al, x1, x2;
    grainFrame(ga, q, al, x1, x2);
    return 0.00018f * gnoise(V3(al * 3.0f, x1 * 160.0f, x2 * 160.0f));
  });
  return b;
}

inline BSDF matBrass(V3 p, V3 n) {
  BSDF b;
  float pat = fbm(p * 6.0f, 4);
  V3 clean(0.887f, 0.721f, 0.380f);   // aged brass reflectance (linear)
  V3 dark(0.36f, 0.27f, 0.14f);
  b.f0 = lerp(clean, dark, smoothstep(0.0f, 0.6f, pat) * 0.6f);
  b.albedo = V3(0.0f);
  b.alpha = 0.10f + 0.12f * smoothstep(-0.3f, 0.5f, pat);
  b.metal = 1.0f;
  b.n = bump(n, p, 0.0003f, 1.0f, [](V3 q) { return 0.00008f * gnoise(q * 90.0f); });
  return b;
}

inline BSDF matCopper(V3 p, V3 n) {
  BSDF b;
  // Verdigris collects where water runs: vertical streaks plus blotches.
  float streak = fbm(V3(p.x * 18.0f, p.y * 1.6f, p.z * 18.0f), 4);
  float blotch = fbm(p * 3.5f, 4);
  float patina = smoothstep(-0.25f, 0.35f, 0.6f * streak + 0.7f * blotch + 0.25f);
  V3 green = lerp(hex(0x4F7F6E), hex(0x6F9E8A), saturate(0.5f + streak));
  b.albedo = green * patina;
  b.f0 = lerp(V3(0.80f, 0.52f, 0.42f), V3(0.04f), patina);
  b.metal = 1.0f - patina;
  b.alpha = lerpf(0.22f, 0.8f, patina);
  b.n = bump(n, p, 0.0004f, 1.0f, [](V3 q) { return 0.0002f * gnoise(q * 50.0f); });
  return b;
}

// ---- water ------------------------------------------------------------------------
// One height field drives both what you see (reflection/refraction normals)
// and the caustics on the pool floor (render.cpp), so they always agree.
// Ripples: ~9 cm and ~4 cm gradient-noise swell plus a faint wave train
// fanning out from the rill inlet. Amplitudes of a few mm are what a courtyard
// pool with a trickle feeding it actually carries.
inline float waterHeight(float x, float z) {
  float h = 0.0024f * gnoise2(x * 11.0f + 1.3f, z * 11.0f) + 0.0011f * gnoise2(x * 24.0f + 7.0f, z * 22.0f + 5.1f);
  h += 0.0005f * std::sin((x * 0.3f + z) * 70.0f + 2.5f * gnoise2(x * 4.0f, z * 4.0f));
  return h;
}
// Hessian of the water surface (for the caustic Jacobian).
inline void waterHessian(float x, float z, float& hxx, float& hzz, float& hxz) {
  const float e = 0.0025f;
  float c = waterHeight(x, z);
  hxx = (waterHeight(x + e, z) - 2 * c + waterHeight(x - e, z)) / (e * e);
  hzz = (waterHeight(x, z + e) - 2 * c + waterHeight(x, z - e)) / (e * e);
  hxz = (waterHeight(x + e, z + e) - waterHeight(x + e, z - e) - waterHeight(x - e, z + e) + waterHeight(x - e, z - e)) / (4 * e * e);
}
inline BSDF matWater(V3 p, V3 n) {
  BSDF b;
  b.dielectric = true;
  b.ior = 1.333f;
  b.albedo = V3(0.0f);
  b.f0 = V3(0.02f);
  b.alpha = 0.0f;
  b.metal = 0.0f;
  const float e = 0.0012f;
  float gx = (waterHeight(p.x + e, p.z) - waterHeight(p.x - e, p.z)) / (2 * e);
  float gz = (waterHeight(p.x, p.z + e) - waterHeight(p.x, p.z - e)) / (2 * e);
  V3 up = normalize(V3(-gx, 1.0f, -gz));
  b.n = n.y >= 0.0f ? up : -up;
  return b;
}

// ---- Al Qudra: the sand remembers who was here ------------------------------------
// Under the roof people walk between the columns and the benches: ripples are
// trodden flat along those paths and the sand is pocked with prints. Outside,
// a 4x4's twin ruts cross the site. All profiles are C1 (squared falloffs), so
// no crease reaches the bump normal.
inline float trampleMask(V3 p) {
  float r = std::sqrt(p.x * p.x + p.z * p.z);
  float under = 1.0f - smoothstep(15.0f, 23.0f, r);
  float paths = smoothstep(0.05f, 0.4f, fbm2(p.x * 0.16f + 11.0f, p.z * 0.16f, 3));
  return under * (0.3f + 0.7f * paths);
}
inline float footprints(V3 p, float mask) {
  if (mask <= 0.02f) return 0.0f;
  const float C = 0.34f;
  int ix = (int)std::floor(p.x / C), iz = (int)std::floor(p.z / C);
  float h = 0.0f;
  // Walking direction drifts slowly across the site.
  float walk = 3.0f * fbm2(p.x * 0.05f + 2.0f, p.z * 0.05f, 2);
  for (int dz = -1; dz <= 1; dz++)
    for (int dx = -1; dx <= 1; dx++) {
      uint32_t hh = hash2i(ix + dx + 9001, iz + dz - 77);
      if (hashf(hh) > 0.75f * mask) continue;
      float cx = (ix + dx + 0.15f + 0.7f * hashf(hh ^ 0x1234567u)) * C;
      float cz = (iz + dz + 0.15f + 0.7f * hashf(hh ^ 0x7654321u)) * C;
      float ang = walk + (hashf(hh ^ 0x55aa55aau) < 0.5f ? 0.0f : 3.14159f) + 0.5f * (hashf(hh ^ 0x0f0f0f0fu) - 0.5f);
      float ca = std::cos(ang), sa = std::sin(ang);
      float u = (p.x - cx) * ca + (p.z - cz) * sa, v = -(p.x - cx) * sa + (p.z - cz) * ca;
      // Sole: a longer front pad and a heel, blended; 27 x 10 cm.
      float e1 = sqr((u - 0.035f) / 0.10f) + sqr(v / 0.05f);
      float e2 = sqr((u + 0.085f) / 0.05f) + sqr(v / 0.038f);
      float k1 = e1 < 1.0f ? sqr(1.0f - e1) : 0.0f, k2 = e2 < 1.0f ? sqr(1.0f - e2) : 0.0f;
      float depth = 0.012f + 0.008f * hashf(hh ^ 0x3c3c3c3cu);
      h -= depth * std::max(k1, k2);
      // Displaced sand heaped in a low rim around the print.
      float er = sqr(u / 0.15f) + sqr(v / 0.075f);
      float rim = std::fabs(er - 1.1f) < 0.45f ? sqr(1.0f - sqr((er - 1.1f) / 0.45f)) : 0.0f;
      h += 0.0035f * rim;
    }
  return h;
}
// Twin ruts of a 4x4 crossing the site in a lazy curve.
inline float tyreTrack(V3 p, float& inRut) {
  // Runs past the roof's rim (24.5 m from its centre at the closest) and on
  // toward the dunes, so from under the roof the ruts converge on the horizon.
  const float ax = 8.5f, az = 28.6f, bx = 58.6f, bz = -33.8f;
  float dx = bx - ax, dz = bz - az, L = std::sqrt(dx * dx + dz * dz);
  float tx = dx / L, tz = dz / L;
  float s = (p.x - ax) * tx + (p.z - az) * tz;
  float d = -(p.x - ax) * tz + (p.z - az) * tx - 1.5f * std::sin(s * 0.05f);
  inRut = 0.0f;
  if (std::fabs(d) > 1.4f) return 0.0f;
  float h = 0.0f;
  for (float off : {-0.8f, 0.8f}) {
    float q = (d - off) / 0.22f;                              // 4x4 tyre spread in loose sand
    if (std::fabs(q) < 1.0f) { h -= 0.04f * sqr(1.0f - q * q); inRut = std::max(inRut, 1.0f - q * q); }
    float b = (std::fabs(d - off) - 0.3f) / 0.11f;            // pushed-up berm either side
    if (std::fabs(b) < 1.0f) h += 0.012f * sqr(1.0f - b * b);
  }
  // Tread: shallow chevrons across the rut.
  h += 0.0025f * inRut * std::sin((s + 0.4f * std::fabs(d)) / 0.045f);
  return h;
}

inline float sandHeight(V3 p) {
  // Wind ripples, ~9 cm crest spacing, perpendicular to a NW wind, bent by a
  // low-frequency warp. Pure sin + gradient noise: C-infinity.
  float phase = (0.60f * p.x + 0.80f * p.z) / 0.09f + 3.0f * fbm2(p.x * 0.4f, p.z * 0.4f, 3);
  float amp = 0.0045f * (0.55f + 0.45f * fbm2(p.x * 0.15f + 7.0f, p.z * 0.15f, 2));
  // Asymmetric ripple profile (gentle stoss, steeper lee) from two harmonics.
  float r = std::sin(phase * 6.2832f) + 0.25f * std::sin(phase * 12.5664f + 1.0f);
  float tm = trampleMask(p), rut;
  float tracks = tyreTrack(p, rut);
  amp *= (1.0f - 0.85f * tm) * (1.0f - 0.9f * rut);
  return amp * r + 0.00025f * gnoise(p * 140.0f) + footprints(p, tm) + 0.0025f * tm * gnoise(p * 7.0f) + tracks;
}
inline BSDF matSand(V3 p, V3 n) {
  BSDF b;
  float big = fbm2(p.x * 0.05f, p.z * 0.05f, 3);
  V3 a = lerp(hex(0xD9A873), hex(0xC98E5A), saturate(0.5f + big));
  a *= 1.0f + 0.05f * gnoise(p * 60.0f);
  float crest = std::sin(((0.60f * p.x + 0.80f * p.z) / 0.09f + 3.0f * fbm2(p.x * 0.4f, p.z * 0.4f, 3)) * 6.2832f);
  a *= 1.0f + 0.04f * crest;  // coarser, paler grains collect on crests
  // Trodden and rutted sand is churned: less sorted, a shade darker.
  float rut;
  tyreTrack(p, rut);
  a *= (1.0f - 0.07f * trampleMask(p)) * (1.0f - 0.16f * rut);
  b.albedo = a;
  b.f0 = V3(0.03f);
  b.alpha = 0.95f;
  b.metal = 0.0f;
  b.n = bump(n, p, 0.004f, 1.0f, sandHeight);
  return b;
}

inline float streetSand(V3 p) {
  // Wind-blown sand collects against the walls and in long drifts along the street.
  float drift = fbm(V3(p.x * 0.55f + 3.0f, 0.0f, p.z * 0.12f), 4) + 0.25f * fbm(V3(p.x * 0.3f, 0.0f, p.z * 0.3f), 3);
  return smoothstep(0.02f, 0.38f, drift);
}
inline float streetHeight(V3 p) {
  float sand = streetSand(p);
  // Packed lime-stabilised earth: gravel texture; sand: soft ripples.
  float gravel = 0.0009f * gnoise(p * 38.0f) + 0.0004f * gnoise(p * 110.0f);
  float ripple = 0.0025f * std::sin((p.x * 0.9f + p.z * 0.44f) / 0.07f + 4.0f * fbm(V3(p.x * 0.5f, 0.0f, p.z * 0.5f), 2));
  return 0.006f * fbm(V3(p.x * 1.2f, 0.0f, p.z * 1.2f), 3) + lerpf(gravel, ripple, sand) + 0.012f * sand;
}
inline BSDF matStreet(V3 p, V3 n) {
  BSDF b;
  float big = fbm(V3(p.x * 0.3f, 0.0f, p.z * 0.3f), 4);
  float sand = streetSand(p);
  V3 packed = hex(0xC7B69A) * (1.0f + 0.09f * big + 0.05f * fbm(p * 3.0f, 3));
  V3 loose = hex(0xDDBE93) * (1.0f + 0.05f * gnoise(p * 20.0f));
  V3 a = lerp(packed, loose, sand);
  Cell c = cellular(p * 32.0f);
  float stone = (1.0f - smoothstep(0.12f, 0.24f, c.f1)) * (1.0f - sand);
  float sh = hashf(c.id);
  a = lerp(a, sh < 0.5f ? hex(0x9A8871) : hex(0xE3D9C8), stone * 0.5f);
  b.albedo = a;
  b.f0 = V3(0.03f);
  b.alpha = 0.92f;
  b.metal = 0.0f;
  b.n = bump(n, p, 0.0015f, 1.0f, streetHeight);
  return b;
}

inline BSDF matTerracotta(V3 p, V3 n) {
  BSDF b;
  // Unglazed, wood-fired: the body colour drifts between red and buff across
  // a pot; watering leaches salts that dry as a white bloom, heaviest low
  // down; the foot stays dark where it sits in damp.
  V3 a = lerp(hex(0xB06A45), hex(0xC2865C), smoothstep(-0.3f, 0.4f, fbm(p * 1.7f + V3(9.0f), 3)));
  a *= 1.0f + 0.08f * fbm(p * 6.0f, 3);
  // The bloom is a haze wicked up from the damp foot, thinning with height and
  // broken into vertical streaks where water ran; never blotches.
  float low = 1.0f - smoothstep(0.06f, 0.42f, p.y);
  float streak = 0.5f + 0.5f * fbm(V3(p.x * 7.0f, p.y * 1.1f, p.z * 7.0f) + V3(4.0f), 3);
  a = lerp(a, hex(0xD8CDB8), 0.4f * smoothstep(0.05f, 0.95f, low * (0.45f + 0.55f * streak)));
  a *= 1.0f - 0.28f * (1.0f - smoothstep(0.01f, 0.09f, p.y));
  b.albedo = a; b.f0 = V3(0.035f); b.alpha = 0.85f; b.metal = 0.0f;
  b.n = bump(n, p, 0.0006f, 1.0f, [](V3 q) { return 0.0003f * gnoise(q * 40.0f); });
  return b;
}

inline BSDF matPalm(V3 p, V3 n) {
  BSDF b;
  // Plaited palm leaf: over-under weave of ~2 cm strips on the diagonal.
  float u = (p.x + p.z) / 0.02f, v = (p.x - p.z) / 0.02f;
  float wu = std::sin(u * 3.14159f) * std::sin(v * 3.14159f);
  V3 a = lerp(hex(0xC9B07F), hex(0xA68A5A), 0.5f + 0.5f * wu);
  a *= 1.0f + 0.12f * fbm(p * 8.0f, 3);
  b.albedo = a; b.f0 = V3(0.04f); b.alpha = 0.6f; b.metal = 0.0f;
  b.n = bump(n, p, 0.001f, 1.0f, [](V3 q) {
    float uu = (q.x + q.z) / 0.02f, vv = (q.x - q.z) / 0.02f;
    return 0.0012f * std::sin(uu * 3.14159f) * std::sin(vv * 3.14159f);
  });
  return b;
}

inline BSDF matBronze(V3 p, V3 n) {
  BSDF b;
  b.albedo = V3(0.0f);
  b.f0 = V3(0.30f, 0.22f, 0.15f) * (1.0f + 0.2f * fbm(p * 8.0f, 3));
  b.alpha = 0.35f; b.metal = 1.0f; b.n = n;
  return b;
}

// ---- trees -----------------------------------------------------------------------------
inline BSDF matBark(V3 p, V3 n, const Prim* prim) {
  BSDF b;
  // Fissures run along the branch: use the capsule axis when there is one.
  V3 ax = prim && prim->type == P_CAPSULE ? normalize(prim->pb - prim->pa) : V3(0, 1, 0);
  V3 t1, t2;
  onb(ax, t1, t2);
  float along = dot(p, ax), c1 = dot(p, t1), c2 = dot(p, t2);
  float fiss = gnoise(V3(along * 3.0f, c1 * 38.0f, c2 * 38.0f));
  V3 a = lerp(hex(0x5E5249), hex(0x8A7D6E), 0.5f + 0.5f * fiss);
  a *= 1.0f + 0.1f * fbm(p * 7.0f, 3);
  a = lerp(a, hex(0xB9AA8E), 0.25f * smoothstep(0.3f, 0.8f, fbm(p * 2.0f + V3(3.0f), 2)));   // dust in the crevices of the sunward side
  b.albedo = a; b.f0 = V3(0.03f); b.alpha = 0.92f; b.metal = 0.0f;
  b.n = bump(n, p, 0.0015f, 1.0f, [ax, t1, t2](V3 q) {
    return 0.004f * gnoise(V3(dot(q, ax) * 3.0f, dot(q, t1) * 38.0f, dot(q, t2) * 38.0f));
  });
  return b;
}
inline BSDF matLeaf(V3 p, V3 n, uint32_t id, bool olive) {
  BSDF b;
  float h = hashf(id), h2 = hashf(id ^ 0x7feb352du);
  // Ghaf is grey-green and matt with dust; olive is silvery. Neither is the
  // saturated green of a watered garden.
  V3 a = olive ? lerp(hex(0x6E7760), hex(0x8D977F), h) : lerp(hex(0x6B6F5A), hex(0x878A70), h);
  if (h2 < 0.07f) a = olive ? hex(0xA59B72) : hex(0xA8986E);   // a few dry leaves
  a *= 0.88f + 0.22f * hashf(id ^ 0x1b873593u);
  // Gulf trees carry a film of dust on their upper surfaces.
  a = lerp(a, hex(0xB5A98E), (olive ? 0.28f : 0.36f) * saturate(n.y));
  b.albedo = a * 0.8f;
  // Thick, dusty leaflets pass little light, and what they pass is olive,
  // not the yellow-green of a thin spring leaf.
  b.trans = olive ? V3(a.x * 0.32f, a.y * 0.40f, a.z * 0.16f) : V3(a.x * 0.27f, a.y * 0.31f, a.z * 0.17f);
  b.thin = true;
  b.f0 = V3(olive ? 0.045f : 0.035f);
  b.alpha = olive ? 0.45f : 0.6f;
  b.metal = 0.0f;
  b.n = n;
  return b;
}

// ---- the street's hardware ------------------------------------------------------------
inline BSDF matCarPaint(V3 p, V3 n) {
  BSDF b;
  b.albedo = hex(0xE9E6DF) * (1.0f + 0.02f * gnoise(p * 3.0f));
  b.f0 = V3(0.05f); b.alpha = 0.16f; b.metal = 0.0f; b.n = n;
  return b;
}
inline BSDF matGlass(V3 p, V3 n) {
  BSDF b;
  b.albedo = V3(0.012f, 0.016f, 0.018f); b.f0 = V3(0.045f); b.alpha = 0.05f; b.metal = 0.0f; b.n = n;
  return b;
}
inline BSDF matRubber(V3 p, V3 n) {
  BSDF b; b.albedo = V3(0.035f); b.f0 = V3(0.02f); b.alpha = 0.8f; b.metal = 0.0f;
  b.n = bump(n, p, 0.0006f, 1.0f, [](V3 q) { return 0.0004f * gnoise(q * 60.0f); });
  return b;
}
inline BSDF matCarLamp(V3 p, V3 n) {
  BSDF b; b.albedo = hex(0xD9D6CF) * 0.7f; b.f0 = V3(0.06f); b.alpha = 0.08f; b.metal = 0.0f; b.n = n;
  return b;
}
inline BSDF matFabricCover(V3 p, V3 n) {
  BSDF b;
  V3 a = hex(0xBDB5A3) * (1.0f + 0.06f * fbm(p * 2.5f, 3));
  b.albedo = a; b.f0 = V3(0.045f); b.alpha = 0.72f; b.metal = 0.0f;
  b.n = bump(n, p, 0.0008f, 1.0f, [](V3 q) { return 0.0015f * fbm(q * 9.0f, 3) + 0.0001f * gnoise(q * 300.0f); });
  return b;
}
inline BSDF matSteel(V3 p, V3 n) {
  BSDF b;
  // Galvanised: mottled zinc spangle, dulled by weather.
  float sp = fbm(p * 14.0f, 3);
  b.albedo = V3(0.0f);
  b.f0 = V3(0.42f, 0.43f, 0.44f) * (0.85f + 0.3f * sp);
  b.alpha = 0.48f + 0.1f * sp; b.metal = 1.0f; b.n = n;
  return b;
}
inline BSDF matCable(V3 p, V3 n) {
  // Dusty PVC sheath: matte. A glossy cable throws a sun glint the denoiser
  // smears into a white line against the sky.
  BSDF b; b.albedo = V3(0.045f); b.f0 = V3(0.03f); b.alpha = 0.85f; b.metal = 0.0f; b.n = n;
  return b;
}
inline BSDF matLinen(V3 p, V3 n, V3 base) {
  BSDF b;
  float weave = 0.5f + 0.5f * std::sin(p.x * 900.0f) * std::sin(p.z * 900.0f + p.y * 900.0f);
  b.albedo = base * (0.94f + 0.08f * weave) * (1.0f + 0.06f * fbm(p * 5.0f, 3));
  b.f0 = V3(0.03f); b.alpha = 0.9f; b.metal = 0.0f;
  b.n = bump(n, p, 0.0006f, 1.0f, [](V3 q) { return 0.0008f * fbm(q * 14.0f, 2); });
  return b;
}
inline BSDF matSoil(V3 p, V3 n) {
  BSDF b; b.albedo = hex(0x5C4838) * (0.8f + 0.4f * hashf(cellular(p * 90.0f).id)); b.f0 = V3(0.02f); b.alpha = 0.95f;
  b.metal = 0.0f; b.n = bump(n, p, 0.001f, 1.0f, [](V3 q) { return 0.002f * gnoise(q * 60.0f); });
  return b;
}
inline BSDF matLens(V3 p, V3 n) {
  BSDF b; b.albedo = hex(0xE8E4DA) * 0.8f; b.f0 = V3(0.04f); b.alpha = 0.3f; b.metal = 0.0f; b.n = n;
  return b;
}
inline BSDF matCopperDeep(V3 p, V3 n) {
  BSDF b;
  // A pool lining that has been under water for years: even, deep verdigris
  // with darker bloom — reads as depth, not as a mottled box.
  float m = fbm(p * 5.0f, 4);
  b.albedo = lerp(hex(0x2D4A40), hex(0x416552), 0.5f + 0.5f * m) * 0.85f;
  b.f0 = V3(0.05f); b.alpha = 0.55f; b.metal = 0.0f;
  b.n = bump(n, p, 0.0005f, 1.0f, [](V3 q) { return 0.0002f * gnoise(q * 40.0f); });
  return b;
}

inline BSDF matCoral(V3 p, V3 n, uint32_t seed) {
  BSDF b;
  float s = (seed & 1023) * 0.113f;
  // Lime render over coral stone, maintained: the render is worn back to the
  // stone in a few large, soft patches (one noise at a building's scale),
  // more of them low down, where hands, carts and splash wear it. Colour and
  // relief use the SAME mask, so every edge in the shading is a real edge of
  // the render (a different or narrower mask draws contour lines instead).
  auto exposedAt = [s](V3 q) {
    float lowWear = 0.16f * (1.0f - smoothstep(0.3f, 2.0f, q.y));
    float w = fbm(q * 0.55f + V3(s), 4) + 0.08f * fbm(q * 2.3f + V3(2.0f, s, 5.0f), 2);
    return smoothstep(0.40f, 0.52f, w + lowWear);
  };
  float exposed = exposedAt(p);
  V3 render = lerp(hex(0xE6D9C1), hex(0xDCCBAD), 0.5f + 0.5f * fbm(p * 1.7f + V3(s), 3));
  V3 stone = lerp(hex(0xD2C2A6), hex(0xC4B192), 0.5f + 0.5f * gnoise(p * 9.0f + V3(s)));
  float pits = smoothstep(0.25f, 0.45f, gnoise(p * 55.0f + V3(s * 3.0f)));
  stone *= 1.0f - 0.2f * pits;
  V3 a = lerp(render, stone, exposed);
  a *= 1.0f + 0.035f * gnoise(p * 140.0f);
  float foot = 1.0f - smoothstep(0.0f, 0.9f, p.y);
  a = lerp(a, a * hex(0xC4AE8C) * 1.2f, 0.6f * foot);
  b.albedo = a;
  b.f0 = V3(0.03f); b.alpha = 0.85f; b.metal = 0.0f;
  // Relief: the render stands 2.5 mm proud of the stone; the render's own
  // trowel undulation; pits only where the stone shows.
  b.n = bump(n, p, 0.0012f, 1.0f, [&](V3 q) {
    float e = exposedAt(q);
    return -0.0025f * e + 0.0012f * gnoise(q * 18.0f) - 0.0009f * e * smoothstep(0.25f, 0.45f, gnoise(q * 55.0f + V3(s * 3.0f)));
  });
  return b;
}

inline BSDF matBooks(V3 p, V3 n) {
  BSDF b;
  // Shelves every 0.38 m; within a shelf, spines of random width (2-6 cm),
  // height and colour, and dark gaps above the shorter books.
  float y = p.y, row = std::floor(y / 0.38f), fy = y - row * 0.38f;
  float u = std::fabs(n.x) > std::fabs(n.z) ? p.z : p.x;
  float acc = 0.0f, w = 0.0f; uint32_t h = 0; int k = 0;
  float start = std::floor(u / 0.9f) * 0.9f;               // restart every 0.9 m: a shelf bay
  uint32_t bay = hash2i((int)std::floor(u / 0.9f), (int)row);
  acc = start;
  for (k = 0; k < 40; k++) {
    h = pcg(bay + (uint32_t)k * 2654435761u);
    w = 0.02f + 0.04f * hashf(h);
    if (acc + w > u) break;
    acc += w;
  }
  static const uint32_t cols[8] = {0x8C5A3C, 0xB08D57, 0x3F4E5E, 0x6B6B5A, 0xC9B79A, 0x7A3E2E, 0x4E5B45, 0x9A8F80};
  V3 a = hex(cols[pcg(h) & 7u]) * (0.8f + 0.35f * hashf(h ^ 0x5bd1e995u));
  float top = 0.22f + 0.13f * hashf(h ^ 0x27d4eb2du);
  float edge = std::min(u - acc, acc + w - u);               // gutter between spines
  if (fy < 0.025f) a = hex(0x6E4A30);                         // the shelf board
  else if (fy > top) a *= 0.18f;                              // the dark back of the shelf
  a *= 1.0f - 0.5f * (1.0f - smoothstep(0.0f, 0.003f, edge));
  b.albedo = a;
  b.f0 = V3(0.035f); b.alpha = 0.7f; b.metal = 0.0f; b.n = n;
  return b;
}

// ---- weathering ---------------------------------------------------------------------
// Applied on top of every opaque material at the camera-visible vertex.
// Everything is driven by geometry the scene already knows: which way a surface
// faces (dust settles on what faces up), how enclosed it is (dust collects in
// corners), where water can run (stains under sills and spouts), where plaster
// is stressed (cracks off the corners of openings), and where hands go.
inline V3 dustColour() { return hex(0xD1C09E); }

// Craquelure: the edges of a jittered Voronoi tessellation of the wall plane
// (F2 - F1 ~ 0 on a cell edge), 12-15 cm cells, in patches. Polygonal networks
// of short straight-ish runs are what shrinkage cracking in lime looks like;
// meandering noise contours look like scribbles. Albedo only.
inline float crackNet(float u, float v, uint32_t seed) {
  const float C = 0.135f;
  float wu = u + 0.018f * gnoise2(u * 9.0f, v * 9.0f), wv = v + 0.018f * gnoise2(u * 9.0f + 4.0f, v * 9.0f);
  int iu = (int)std::floor(wu / C), iv = (int)std::floor(wv / C);
  float f1 = 1e9f, f2 = 1e9f;
  uint32_t c1 = 0, c2 = 0;
  for (int du = -1; du <= 1; du++)
    for (int dv = -1; dv <= 1; dv++) {
      uint32_t h = hash3i(iu + du, iv + dv, (int)(seed & 0xffff));
      float pu = (iu + du + 0.1f + 0.8f * hashf(h)) * C, pv = (iv + dv + 0.1f + 0.8f * hashf(h ^ 0x9e3779b9u)) * C;
      float d = std::sqrt((wu - pu) * (wu - pu) + (wv - pv) * (wv - pv));
      if (d < f1) { f2 = f1; c2 = c1; f1 = d; c1 = h; } else if (d < f2) { f2 = d; c2 = h; }
    }
  // Only some edges open: real crazing is a network of incomplete polygons.
  // The pair hash is symmetric, so an edge is the same seen from either cell.
  uint32_t pair = pcg((c1 ^ c2) + (c1 & c2) * 0x27d4eb2du);
  float open = hashf(pair);
  if (open > 0.5f) return 0.0f;
  float strength = 0.45f + 0.55f * hashf(pair ^ 0x85ebca6bu);
  return strength * (1.0f - smoothstep(0.0009f, 0.0028f, f2 - f1));
}
inline float mapCracks(V3 p, V3 ng, uint32_t seed) {
  float u = std::fabs(ng.x) > std::fabs(ng.z) ? p.z : p.x;
  float s = (float)(seed & 255) * 0.61f;
  // Patches: roughly one wall area in eight, likelier low down.
  float region = smoothstep(0.28f, 0.46f, fbm2(u * 0.7f + s, p.y * 0.7f, 3) + 0.1f * (1.0f - smoothstep(0.8f, 3.0f, p.y)));
  if (region <= 0.0f) return 0.0f;
  // Inside a patch the network thins out toward the edge, runs ending one by one.
  float keep = smoothstep(0.0f, 0.6f, region + 0.35f * gnoise2(u * 6.0f, p.y * 6.0f + s));
  return crackNet(u, p.y, seed) * keep;
}

// Diagonal stress cracks from the corners of openings cut in this wall.
inline float cornerCracks(const Scene& sc, const Prim& pr, V3 p, V3 ng) {
  V3 e = pr.hi - pr.lo;
  int ax = e.x < e.z ? 0 : 2;           // thin horizontal axis = wall normal
  if (std::fabs(ng[ax]) < 0.9f || e.y < 2.5f) return 0.0f;
  int ua = ax == 0 ? 2 : 0;
  float u = p[ua], y = p.y, best = 0.0f;
  for (int k = 0; k < pr.cutCount; k++) {
    const AABB& c = sc.cuts[pr.cutStart + k];
    float u0 = c.lo[ua], u1 = c.hi[ua], y0 = std::max(c.lo.y, pr.lo.y), y1 = std::min(c.hi.y, pr.hi.y);
    if (y1 - y0 < 0.4f || u1 - u0 < 0.3f) continue;
    if (u < u0 - 0.8f || u > u1 + 0.8f || y < y0 - 0.8f || y > y1 + 0.8f) continue;
    const float cu[4] = {u0, u1, u0, u1}, cy[4] = {y1, y1, y0, y0}, su[4] = {-1, 1, -1, 1}, sy[4] = {1, 1, -1, -1};
    for (int q = 0; q < 4; q++) {
      if (q >= 2 && y0 < 0.05f) continue;                     // no sill corner on a door
      uint32_t h = hash3i(k, q, (int)(pr.seed & 0xffff));
      if (hashf(h) > (q < 2 ? 0.9f : 0.45f)) continue;        // top corners crack more often
      float ang = (38.0f + 22.0f * hashf(h ^ 0x68e31da4u)) * PI / 180.0f;
      float L = 0.25f + 0.5f * hashf(h ^ 0xb5297a4du);
      // Main run, then one branch leaving it part-way at a shallower angle.
      for (int br = 0; br < 2; br++) {
        float a2 = br == 0 ? ang : ang + (hashf(h ^ 0x1b873593u) < 0.5f ? -0.45f : 0.4f);
        float s0 = br == 0 ? 0.0f : L * (0.35f + 0.3f * hashf(h ^ 0xcc9e2d51u));
        float L2 = br == 0 ? L : L * 0.45f;
        float ox = cu[q] + su[q] * std::cos(ang) * s0, oy = cy[q] + sy[q] * std::sin(ang) * s0;
        float dx = su[q] * std::cos(a2), dy = sy[q] * std::sin(a2);
        float rx = u - ox, ry = y - oy;
        float s = rx * dx + ry * dy;
        if (s < 0.0f || s > L2) continue;
        float wig = 0.025f * (s / L2) * gnoise2(s * 9.0f + (float)(h & 1023) + br * 17.0f, 0.37f) +
                    0.004f * gnoise2(s * 60.0f + (float)(h & 511), 1.7f);
        float o = std::fabs((-rx * dy + ry * dx) - wig);
        float taper = std::sqrt(1.0f - s / L2) * (br == 0 ? 1.0f : 0.7f);
        float w = 0.0019f * taper + 0.0004f;
        float line = 1.0f - smoothstep(w * 0.5f, w * 1.5f, o);
        float halo = 0.22f * taper * (1.0f - smoothstep(0.0f, 0.012f, o));   // dirt caught along the crack
        best = std::max(best, std::max(line, halo));
      }
    }
  }
  return best;
}

// Hand grime: hand height, close to the vertical arrises of door openings.
inline float touchWear(const Scene& sc, const Prim& pr, V3 p, V3 ng) {
  V3 e = pr.hi - pr.lo;
  int ax = e.x < e.z ? 0 : 2;
  if (std::fabs(ng[ax]) < 0.9f) return 0.0f;
  int ua = ax == 0 ? 2 : 0;
  float y = p.y;
  if (y < 0.7f || y > 1.8f) return 0.0f;
  float best = 0.0f;
  for (int k = 0; k < pr.cutCount; k++) {
    const AABB& c = sc.cuts[pr.cutStart + k];
    if (c.lo.y > 0.05f || c.hi.y < 1.9f) continue;            // doors only
    float du = std::min(std::fabs(p[ua] - c.lo[ua]), std::fabs(p[ua] - c.hi[ua]));
    if (du > 0.22f) continue;
    float f = 1.0f - du / 0.22f;
    best = std::max(best, f * smoothstep(0.75f, 1.0f, y) * (1.0f - smoothstep(1.45f, 1.75f, y)));
  }
  // Smudges, not a gradient: blotches at palm scale.
  return best * smoothstep(-0.35f, 0.3f, fbm(p * 8.0f, 3) + 0.2f * best);
}

inline void applyStains(const Scene& sc, BSDF& b, V3 p, V3 ng) {
  for (const Stain& st : sc.stains) {
    if (std::fabs(ng[st.axis]) < 0.9f) continue;
    if (std::fabs(p[st.axis] - st.plane) > 0.02f) continue;
    float dy = st.yTop - p.y;
    if (dy < 0.0f || dy > st.len) continue;
    int ua = st.axis == 0 ? 2 : 0;
    float u = p[ua];
    float mid = 0.5f * (st.u0 + st.u1), half = 0.5f * (st.u1 - st.u0);
    float du = std::fabs(u - mid) - half;
    if (du > 0.12f) continue;
    // Individual rivulets: narrow, of varying length, fading downward.
    float rivN = gnoise2(u * 16.0f + (float)(st.seed & 255), 3.7f);
    float streak = 0.3f + 0.7f * smoothstep(-0.2f, 0.4f, rivN);   // a faint wash with rivulets in it
    float lenVar = st.len * (0.35f + 0.65f * (0.5f + 0.5f * gnoise2(u * 5.0f, (float)(st.seed & 127))));
    float fall = 1.0f - smoothstep(0.05f * st.len, lenVar, dy);
    float side = 1.0f - smoothstep(-0.02f, 0.12f, du);
    float amt = st.strength * streak * fall * side * (0.7f + 0.3f * fbm(p * 11.0f, 2));
    b.albedo = lerp(b.albedo, b.albedo * st.tint, saturate(amt));
  }
}

inline void applyWeather(BSDF& b, int mat, V3 p, const ShadeCtx& c) {
  if (b.dielectric || b.thin) return;
  bool plaster = mat == M_LIME || mat == M_LIME_SHADE || mat == M_CORAL;
  bool masonry = plaster || mat == M_RAMMED || mat == M_STONE;
  // 1. Dust on anything that faces up and is not the floor itself.
  float up = b.n.y;
  if (up > 0.5f && p.y > 0.04f) {
    float dust = smoothstep(0.5f, 0.95f, up) * (0.35f + 0.45f * (0.5f + 0.5f * fbm(p * 5.0f, 3)));
    if (mat == M_CARPAINT || mat == M_GLASS) dust *= 0.8f;
    b.albedo = lerp(b.albedo, dustColour(), dust * 0.6f);
    b.f0 = lerp(b.f0, V3(0.03f), dust);
    b.alpha = lerpf(b.alpha, 0.9f, dust);
    b.metal *= 1.0f - dust * 0.8f;
  }
  // 2. Dust collects in corners and at the foot of walls (local occlusion).
  if (c.cavity > 0.0f) {
    float settle = 0.35f + 0.65f * saturate(up);
    float dust = smoothstep(0.12f, 0.5f, c.cavity) * settle * (0.55f + 0.45f * (0.5f + 0.5f * fbm(p * 7.0f, 2)));
    // On the pale floor, settled sand reads by its grey grit, not its colour.
    V3 dc = mat == M_STONE_FLOOR ? hex(0xB4A487) * (0.9f + 0.2f * (0.5f + 0.5f * gnoise(p * 140.0f))) : dustColour();
    b.albedo = lerp(b.albedo, dc, dust * 0.55f);
    b.alpha = lerpf(b.alpha, 0.92f, dust);
    b.metal *= 1.0f - dust;
  }
  if (!c.sc || !c.prim) return;
  // 3. Run-off from sills, spouts and copings.
  if (masonry) applyStains(*c.sc, b, p, c.ng);
  // 4. Parapet run-off: faint streaks from the top of tall plastered walls.
  if (plaster && c.prim->type == P_BOX && (c.prim->hi.y - c.prim->lo.y) > 3.0f && std::fabs(c.ng.y) < 0.2f) {
    float dy = c.prim->hi.y - p.y;
    if (dy < 1.8f) {
      float u = p.x + p.z;
      float riv = smoothstep(0.0f, 0.6f, gnoise2(u * 11.0f, 9.1f)) * (1.0f - smoothstep(0.0f, 1.2f + 0.6f * gnoise2(u * 3.0f, 2.0f), dy));
      b.albedo = lerp(b.albedo, b.albedo * hex(0xC8BFB0), 0.35f * riv);
    }
  }
  // 5. Cracks: hairline craquelure and stress cracks off opening corners.
  if (plaster && c.prim->type == P_BOX) {
    float cr = std::fabs(c.ng.y) < 0.5f ? std::max(mapCracks(p, c.ng, c.prim->seed) * 0.8f, cornerCracks(*c.sc, *c.prim, p, c.ng)) : 0.0f;
    b.albedo = b.albedo * (1.0f - 0.42f * cr);
    // 6. Hands: grime at the arrises of doorways, burnished a little.
    float w = touchWear(*c.sc, *c.prim, p, c.ng);
    // Skin oil holds fine dust: a grey-brown, never an orange.
    b.albedo = lerp(b.albedo, b.albedo * V3(0.66f, 0.63f, 0.585f), 0.9f * w);
    b.alpha = lerpf(b.alpha, 0.5f, 0.5f * w);
  }
}

inline BSDF shadeMaterial(int mat, V3 p, V3 n, const ShadeCtx& c) {
  const Prim* prim = c.prim;
  uint32_t seed = prim ? prim->seed : 0u;
  int grain = prim ? prim->grainAxis : 0;
  BSDF b;
  switch (mat) {
    case M_LIME: b = matLime(p, n, seed, false); break;
    case M_LIME_SHADE: b = matLime(p, n, seed, true); break;
    case M_RAMMED: b = matRammed(p, n, seed); break;
    case M_STONE_FLOOR: b = matStoneFloor(p, n); break;
    case M_STONE: b = matStone(p, n); break;
    case M_TEAK: b = matTeak(p, n, grain, seed, false); break;
    case M_TEAK_GREY: b = matTeak(p, n, grain, seed, true); break;
    case M_BRASS: b = matBrass(p, n); break;
    case M_COPPER: b = matCopper(p, n); break;
    case M_WATER: return matWater(p, n);
    case M_SAND: b = matSand(p, n); break;
    case M_STREET: b = matStreet(p, n); break;
    case M_CORAL: b = matCoral(p, n, seed); break;
    case M_BOOKS: b = matBooks(p, n); break;
    case M_TERRACOTTA: b = matTerracotta(p, n); break;
    case M_PALM: b = matPalm(p, n); break;
    case M_BRONZE: b = matBronze(p, n); break;
    case M_DARKROOM: b = matLime(p, n, seed, true); b.albedo *= 0.55f; break;
    case M_BARK: b = matBark(p, n, prim); break;
    case M_LEAF_GHAF: return matLeaf(p, n, c.sub, false);
    case M_LEAF_OLIVE: return matLeaf(p, n, c.sub, true);
    case M_CARPAINT: b = matCarPaint(p, n); break;
    case M_GLASS: b = matGlass(p, n); break;
    case M_RUBBER: b = matRubber(p, n); break;
    case M_CARLAMP: b = matCarLamp(p, n); break;
    case M_FABRIC_COVER: b = matFabricCover(p, n); break;
    case M_STEEL: b = matSteel(p, n); break;
    case M_CABLE: b = matCable(p, n); break;
    case M_LINEN_INDIGO: b = matLinen(p, n, hex(0x46566F)); break;
    case M_LINEN_OCHRE: b = matLinen(p, n, hex(0xB98845)); break;
    case M_LINEN_NATURAL: b = matLinen(p, n, hex(0xD8CCB4)); break;
    case M_SOIL: b = matSoil(p, n); break;
    case M_LENS: b = matLens(p, n); break;
    case M_COPPER_DEEP: b = matCopperDeep(p, n); break;
    default: b.albedo = V3(0.5f); b.f0 = V3(0.04f); b.alpha = 0.8f; b.metal = 0.0f; b.n = n; break;
  }
  applyWeather(b, mat, p, c);
  return b;
}

// Indirect-bounce version: same mean albedo and roughness, no bump, few octaves.
// Light that has already bounced once cannot resolve millimetre texture, and
// this is where most shading calls happen, so it is most of the render time.
inline BSDF shadeMaterialLite(int mat, V3 p, V3 n, const ShadeCtx& c) {
  BSDF b; b.n = n; b.f0 = V3(0.035f); b.metal = 0.0f; b.alpha = 0.8f;
  float v = 0.05f * gnoise(p * 0.75f);
  switch (mat) {
    case M_LIME: b.albedo = hex(0xE3D6C1) * (1.0f + v); return b;
    case M_LIME_SHADE: b.albedo = hex(0xD2C1A6) * (1.0f + v); return b;
    case M_DARKROOM: b.albedo = hex(0xD2C1A6) * 0.55f; return b;
    case M_RAMMED: b.albedo = hex(0xC19A72) * (1.0f + v); b.alpha = 0.9f; return b;
    case M_STONE_FLOOR: b.albedo = hex(0xDCCFB9) * (1.0f + v); b.alpha = 0.66f; b.f0 = V3(0.035f); return b;
    case M_STONE: b.albedo = hex(0xE0D3BF) * (1.0f + v); b.alpha = 0.55f; return b;
    case M_TEAK: b.albedo = hex(0x7B5131); b.alpha = 0.4f; b.f0 = V3(0.04f); return b;
    case M_TEAK_GREY: b.albedo = hex(0x978E7E); b.alpha = 0.75f; return b;
    case M_SAND: b.albedo = hex(0xD29F6C) * (1.0f + v); b.alpha = 0.95f; return b;
    case M_STREET: b.albedo = hex(0xD0BC9C) * (1.0f + v); b.alpha = 0.92f; return b;
    case M_CORAL: b.albedo = hex(0xD2BF9E) * (1.0f + v); b.alpha = 0.85f; return b;
    case M_BOOKS: b.albedo = hex(0x7A6650) * (1.0f + v); b.alpha = 0.7f; return b;
    case M_TERRACOTTA: b.albedo = hex(0xB06A45); return b;
    case M_PALM: b.albedo = hex(0xB89C6B); return b;
    case M_BARK: b.albedo = hex(0x6E6155); return b;
    case M_LEAF_GHAF: case M_LEAF_OLIVE: {
      b.albedo = hex(0x7A845A) * 0.85f; b.trans = V3(0.12f, 0.2f, 0.05f); b.thin = true; return b;
    }
    case M_CARPAINT: b.albedo = hex(0xE0DCD2); b.alpha = 0.3f; b.f0 = V3(0.05f); return b;
    case M_GLASS: b.albedo = V3(0.015f); b.alpha = 0.1f; b.f0 = V3(0.045f); return b;
    case M_RUBBER: case M_CABLE: b.albedo = V3(0.035f); return b;
    case M_FABRIC_COVER: b.albedo = hex(0xBDB5A3); return b;
    case M_STEEL: b.albedo = V3(0.0f); b.f0 = V3(0.4f); b.metal = 1.0f; b.alpha = 0.5f; return b;
    case M_LINEN_INDIGO: b.albedo = hex(0x46566F); return b;
    case M_LINEN_OCHRE: b.albedo = hex(0xB98845); return b;
    case M_LINEN_NATURAL: b.albedo = hex(0xD8CCB4); return b;
    case M_SOIL: b.albedo = hex(0x5C4838); return b;
    case M_COPPER_DEEP: b.albedo = hex(0x375748) * 0.85f; return b;
    default: break;
  }
  return shadeMaterial(mat, p, n, c);
}

}  // namespace zw
