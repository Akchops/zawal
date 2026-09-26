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
  M_COUNT
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

inline float floorHeight(V3 p) {
  const float T = 0.6f;
  float fx = p.x / T - std::floor(p.x / T), fz = p.z / T - std::floor(p.z / T);
  float dx = std::min(fx, 1.0f - fx) * T, dz = std::min(fz, 1.0f - fz) * T;
  // Joint profile: 3 mm wide, 1.5 mm deep, C2 shoulders. The min() above
  // only engages at the tile centre line, far from where the groove profile
  // has any slope, so it never reaches the normal.
  float jx = 1.0f - smootherstep(0.0006f, 0.0026f, dx);
  float jz = 1.0f - smootherstep(0.0006f, 0.0026f, dz);
  // Smooth max where the two joint families cross, so the tile corners do not
  // get a diagonal crease in the groove shoulder (trap #3).
  float joint = smax(jx, jz, 0.25f);
  int ix = (int)std::floor(p.x / T), iz = (int)std::floor(p.z / T);
  // Each slab sits a fraction of a millimetre proud or low — lippage.
  float lip = (hashf(hash2i(ix, iz)) - 0.5f) * 0.0006f;
  return -0.0015f * joint + lip * (1.0f - joint) + 0.00012f * gnoise(p * 120.0f);
}
inline BSDF matStoneFloor(V3 p, V3 n) {
  BSDF b;
  const float T = 0.6f;
  int ix = (int)std::floor(p.x / T), iz = (int)std::floor(p.z / T);
  uint32_t h = hash2i(ix + 1000, iz - 77);
  V3 base = hex(0xE6DCCB);
  V3 a = base * (0.95f + 0.08f * hashf(h));
  a = lerp(a, hex(0xDCCDB5), 0.4f * hashf(h ^ 0x51f15e));
  // Fossil flecks and faint bedding veins.
  a *= 1.0f + 0.035f * fbm(V3(p.x * 3.0f + ix * 7.0f, 0.0f, p.z * 11.0f + iz * 5.0f), 3);
  Cell c = cellular(p * 90.0f + V3((float)ix * 3.7f, 0.0f, (float)iz * 1.3f));
  a = lerp(a, hex(0xBFAE95), (1.0f - smoothstep(0.08f, 0.16f, c.f1)) * 0.5f);
  float fx = p.x / T - std::floor(p.x / T), fz = p.z / T - std::floor(p.z / T);
  float d = std::min(std::min(fx, 1.0f - fx), std::min(fz, 1.0f - fz)) * T;
  a = lerp(a, hex(0x9C8C76), 1.0f - smoothstep(0.0008f, 0.0024f, d));
  // Traffic wear and dust: lighter polish in the middle, dust toward walls.
  a *= 1.0f + 0.04f * fbm(p * 0.6f, 3);
  b.albedo = a;
  b.f0 = V3(0.04f);
  b.alpha = 0.32f;
  b.metal = 0.0f;
  b.n = bump(n, p, 0.0004f, 1.0f, floorHeight);
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

inline BSDF matWater(V3 p, V3 n) {
  BSDF b;
  b.dielectric = true;
  b.ior = 1.333f;
  b.albedo = V3(0.0f);
  b.f0 = V3(0.02f);
  b.alpha = 0.0f;
  b.metal = 0.0f;
  b.n = bump(n, p, 0.002f, 1.0f, [](V3 q) {
    return 0.0006f * gnoise(V3(q.x * 5.0f, 0.0f, q.z * 3.0f)) + 0.00015f * gnoise(V3(q.x * 23.0f, 0.3f, q.z * 17.0f));
  });
  return b;
}

inline float sandHeight(V3 p) {
  // Wind ripples, ~9 cm crest spacing, perpendicular to a NW wind, bent by a
  // low-frequency warp. Pure sin + gradient noise: C-infinity.
  float phase = (0.60f * p.x + 0.80f * p.z) / 0.09f + 3.0f * fbm2(p.x * 0.4f, p.z * 0.4f, 3);
  float amp = 0.0045f * (0.55f + 0.45f * fbm2(p.x * 0.15f + 7.0f, p.z * 0.15f, 2));
  // Asymmetric ripple profile (gentle stoss, steeper lee) from two harmonics.
  float r = std::sin(phase * 6.2832f) + 0.25f * std::sin(phase * 12.5664f + 1.0f);
  return amp * r + 0.00025f * gnoise(p * 140.0f);
}
inline BSDF matSand(V3 p, V3 n) {
  BSDF b;
  float big = fbm2(p.x * 0.05f, p.z * 0.05f, 3);
  V3 a = lerp(hex(0xD9A873), hex(0xC98E5A), saturate(0.5f + big));
  a *= 1.0f + 0.05f * gnoise(p * 60.0f);
  float crest = std::sin(((0.60f * p.x + 0.80f * p.z) / 0.09f + 3.0f * fbm2(p.x * 0.4f, p.z * 0.4f, 3)) * 6.2832f);
  a *= 1.0f + 0.04f * crest;  // coarser, paler grains collect on crests
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
  V3 a = hex(0xB06A45) * (1.0f + 0.08f * fbm(p * 6.0f, 3));
  a = lerp(a, hex(0xD9C3A5), 0.25f * smoothstep(0.1f, 0.6f, fbm(p * 2.0f + V3(4.0f), 3)));  // salt bloom
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

inline BSDF shadeMaterial(int mat, V3 p, V3 n, const Prim* prim);

// Indirect-bounce version: same mean albedo and roughness, no bump, few octaves.
// Light that has already bounced once cannot resolve millimetre texture, and
// this is where most shading calls happen, so it is most of the render time.
inline BSDF shadeMaterialLite(int mat, V3 p, V3 n, const Prim* prim) {
  BSDF b; b.n = n; b.f0 = V3(0.035f); b.metal = 0.0f; b.alpha = 0.8f;
  float v = 0.05f * gnoise(p * 0.75f);
  switch (mat) {
    case M_LIME: b.albedo = hex(0xE3D6C1) * (1.0f + v); return b;
    case M_LIME_SHADE: b.albedo = hex(0xD2C1A6) * (1.0f + v); return b;
    case M_DARKROOM: b.albedo = hex(0xD2C1A6) * 0.55f; return b;
    case M_RAMMED: b.albedo = hex(0xC19A72) * (1.0f + v); b.alpha = 0.9f; return b;
    case M_STONE_FLOOR: b.albedo = hex(0xE1D5C2) * (1.0f + v); b.alpha = 0.35f; b.f0 = V3(0.04f); return b;
    case M_STONE: b.albedo = hex(0xE0D3BF) * (1.0f + v); b.alpha = 0.55f; return b;
    case M_TEAK: b.albedo = hex(0x7B5131); b.alpha = 0.4f; b.f0 = V3(0.04f); return b;
    case M_TEAK_GREY: b.albedo = hex(0x978E7E); b.alpha = 0.75f; return b;
    case M_SAND: b.albedo = hex(0xD29F6C) * (1.0f + v); b.alpha = 0.95f; return b;
    case M_STREET: b.albedo = hex(0xD0BC9C) * (1.0f + v); b.alpha = 0.92f; return b;
    case M_TERRACOTTA: b.albedo = hex(0xB06A45); return b;
    case M_PALM: b.albedo = hex(0xB89C6B); return b;
    default: break;
  }
  return shadeMaterial(mat, p, n, prim);
}

inline BSDF shadeMaterial(int mat, V3 p, V3 n, const Prim* prim) {
  uint32_t seed = prim ? prim->seed : 0u;
  int grain = prim ? prim->grainAxis : 0;
  switch (mat) {
    case M_LIME: return matLime(p, n, seed, false);
    case M_LIME_SHADE: return matLime(p, n, seed, true);
    case M_RAMMED: return matRammed(p, n, seed);
    case M_STONE_FLOOR: return matStoneFloor(p, n);
    case M_STONE: return matStone(p, n);
    case M_TEAK: return matTeak(p, n, grain, seed, false);
    case M_TEAK_GREY: return matTeak(p, n, grain, seed, true);
    case M_BRASS: return matBrass(p, n);
    case M_COPPER: return matCopper(p, n);
    case M_WATER: return matWater(p, n);
    case M_SAND: return matSand(p, n);
    case M_STREET: return matStreet(p, n);
    case M_TERRACOTTA: return matTerracotta(p, n);
    case M_PALM: return matPalm(p, n);
    case M_BRONZE: return matBronze(p, n);
    case M_DARKROOM: {
      BSDF b = matLime(p, n, seed, true);
      b.albedo *= 0.55f;
      return b;
    }
  }
  BSDF b; b.albedo = V3(0.5f); b.f0 = V3(0.04f); b.alpha = 0.8f; b.metal = 0.0f; b.n = n;
  return b;
}

}  // namespace zw
