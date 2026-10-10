// The approach: the film's courtyard house seen from outside and from the
// air, in its quarter. buildHouse() stays exactly as the day's frames (SEQ-A)
// were rendered with it; everything here is added around it, and none of it
// can be seen from the courtyard camera, so the approach can land on the
// day's first frame without a jump (checked by tools/check_landing.py).
//
// Until now the camera had only ever seen the courtyard and the street. From
// above it now sees, for the first time: the roofs (a lime screed, matRoof),
// the parapets and copings on their outer edges, the blank west facade, the
// neighbours' roofs, and the quarter around them. All of it gets the same
// weathering as the street facade: the procedural dust, sand drifts, parapet
// run-off and cracks apply to any plastered box; the stains under sills and
// spouts are placed here by hand, as they were on the street.
#pragma once
#include "scenes.h"

namespace zw {

inline uint32_t qhash(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
  return x;
}
inline float qrand(uint32_t x) { return (qhash(x) >> 8) / 16777216.0f; }

// The index of a box buildHouse() added, found by its bounds (-1 if absent).
inline int findBox(const Scene& s, V3 lo, V3 hi) {
  for (int i = 0; i < (int)s.prims.size(); i++) {
    const Prim& p = s.prims[i];
    if (p.type != P_BOX) continue;
    if (length(p.lo - lo) < 2e-3f && length(p.hi - hi) < 2e-3f) return i;
  }
  // buildHouse() changed under us: say so loudly rather than leave a box bare.
  std::fprintf(stderr, "approach: box (%.2f %.2f %.2f)-(%.2f %.2f %.2f) not found\n", lo.x, lo.y, lo.z, hi.x, hi.y, hi.z);
  return -1;
}

// Things that live on a roof: water jars, a rolled mat, a low wall dividing a
// terrace, a bench of lime plaster.
inline void roofLife(Scene& s, float x0, float z0, float x1, float z1, float y, uint32_t seed) {
  int n = 1 + (int)(qhash(seed) % 3);
  for (int k = 0; k < n; k++) {
    float u = qrand(seed + 3 * k), v = qrand(seed + 3 * k + 1), t = qrand(seed + 3 * k + 2);
    float x = x0 + 0.8f + u * (x1 - x0 - 1.6f), z = z0 + 0.8f + v * (z1 - z0 - 1.6f);
    if (t < 0.4f) { s.addSdf(SDF_JAR, V3(x, y, z), 360.0f * t, 0.9f, {M_TERRACOTTA}); s.addSdf(SDF_JAR, V3(x + 0.45f, y, z + 0.2f), 70.0f, 0.75f, {M_TERRACOTTA}); }
    else if (t < 0.65f) s.addBox(V3(x, y, z), V3(x + 2.4f, y + 0.42f, z + 0.5f), M_LIME, FALL, 0.03f, seed + k);
    else if (t < 0.85f) s.addBox(V3(x, y, z), V3(x + 0.2f, y + 1.1f, std::min(z + 4.0f, z1 - 0.3f)), M_LIME_SHADE, FALL, 0.01f, seed + k);
    else s.addSdf(SDF_MAT, V3(x, y, z), 360.0f * u, 1.0f, {M_PALM}, {0.9f, 0.6f});
  }
}

// Roof kit: the things every lived-in roof here carries. A water tank or two
// on a block stand, an AC condenser, a satellite-dish mast's base.
inline void roofKit(Scene& s, float x0, float z0, float x1, float z1, float y, uint32_t seed) {
  if (x1 - x0 < 3.0f || z1 - z0 < 3.0f) return;
  int nt = qrand(seed) < 0.75f ? 1 + (qrand(seed + 1) < 0.35f ? 1 : 0) : 0;
  float bx = qrand(seed + 2) < 0.5f ? x0 + 1.0f : x1 - 1.0f, bz = qrand(seed + 3) < 0.5f ? z0 + 1.0f : z1 - 1.0f;
  for (int k = 0; k < nt; k++) {
    float r = 0.5f + 0.18f * qrand(seed + 4 + k), hh = 1.1f + 0.4f * qrand(seed + 6 + k);
    float cx = bx + (bx > 0.5f * (x0 + x1) ? -1.0f : 1.0f) * 1.3f * k, cz = bz;
    s.addBox(V3(cx - r - 0.05f, y, cz - r - 0.05f), V3(cx + r + 0.05f, y + 0.35f, cz + r + 0.05f), M_LIME_SHADE, FALL, 0.01f, seed + 8 + k);
    int t = s.addCyl(cx, cz, r, y + 0.35f, y + 0.35f + hh, M_TANK);
    s.prims[t].seed = seed * 31u + k;
    s.addCyl(cx, cz, 0.22f, y + 0.35f + hh, y + 0.42f + hh, M_TANK);
  }
  int na = (int)(qhash(seed + 10) % 3);
  for (int k = 0; k < na; k++) {
    float ax = x0 + 0.5f + (x1 - x0 - 1.5f) * qrand(seed + 11 + k), az = qrand(seed + 14 + k) < 0.5f ? z0 + 0.35f : z1 - 0.75f;
    int a = s.addBox(V3(ax, y + 0.12f, az), V3(ax + 0.9f, y + 0.8f, az + 0.38f), M_TANK, FALL, 0.02f, seed + 20 + k);
    s.addBox(V3(ax + 0.05f, y, az + 0.05f), V3(ax + 0.85f, y + 0.12f, az + 0.33f), M_STEEL, FALL, 0.005f);
    (void)a;
  }
}

inline void roofSlab(Scene& s, float x0, float z0, float x1, float z1, float y, uint32_t seed) {
  s.addBox(V3(x0, y, z0), V3(x1, y + 0.05f, z1), M_ROOF, FY1, 0.0f, seed);
}

// A screened window cut into a wall face: the opening, a teak star screen set
// back in it, a dark room behind, and the grey run-off under the sill.
// axis 0: the face is x = plane, the wall body on the +dir side.
inline void screenedWindow(Scene& s, int wall, int axis, float plane, float dir, float uc, float w, float y0, float y1) {
  float a = plane - dir * 0.1f, b = plane + dir * 0.95f;
  float lo = std::min(a, b), hi = std::max(a, b);
  float s0 = plane + dir * 0.1f, s1 = plane + dir * 0.2f, d0 = plane + dir * 0.75f, d1 = plane + dir * 0.95f;
  if (axis == 0) {
    s.addCut(wall, V3(lo, y0, uc - 0.5f * w), V3(hi, y1, uc + 0.5f * w));
    s.addLattice(V3(std::min(s0, s1), y0, uc - 0.5f * w), V3(std::max(s0, s1), y1, uc + 0.5f * w), 0, screenStar(0.18f), M_TEAK, 0.003f);
    s.addBox(V3(std::min(d0, d1), y0 - 0.2f, uc - 0.5f * w - 0.2f), V3(std::max(d0, d1), y1 + 0.2f, uc + 0.5f * w + 0.2f), M_DARKROOM, FALL, 0.0f);
  } else {
    s.addCut(wall, V3(uc - 0.5f * w, y0, lo), V3(uc + 0.5f * w, y1, hi));
    s.addLattice(V3(uc - 0.5f * w, y0, std::min(s0, s1)), V3(uc + 0.5f * w, y1, std::max(s0, s1)), 2, screenStar(0.18f), M_TEAK, 0.003f);
    s.addBox(V3(uc - 0.5f * w - 0.2f, y0 - 0.2f, std::min(d0, d1)), V3(uc + 0.5f * w + 0.2f, y1 + 0.2f, std::max(d0, d1)), M_DARKROOM, FALL, 0.0f);
  }
  addStain(s, axis, plane, uc - 0.5f * w + 0.05f, uc + 0.5f * w - 0.05f, y0, 1.4f + 0.6f * qrand((uint32_t)(uc * 97.0f)), V3(0.76f, 0.71f, 0.63f), 0.8f);
}

// A copper waterspout (marzam) through a parapet and the green wash under it.
inline void spout(Scene& s, int axis, float plane, float dir, float uc, float y, float runLen) {
  float o0 = plane - dir * 0.2f, o1 = plane - dir * 0.6f;   // pokes out of the face
  if (axis == 0) s.addBox(V3(std::min(o0, o1), y, uc - 0.07f), V3(std::max(o0, o1), y + 0.12f, uc + 0.07f), M_COPPER, FALL, 0.01f);
  else s.addBox(V3(uc - 0.07f, y, std::min(o0, o1)), V3(uc + 0.07f, y + 0.12f, std::max(o0, o1)), M_COPPER, FALL, 0.01f);
  addStain(s, axis, plane, uc - 0.2f, uc + 0.2f, y + 0.05f, runLen, V3(0.58f, 0.74f, 0.66f), 0.9f);
}

// Parapet ring with a stone coping on the outer edge of a flat roof.
inline void parapetRing(Scene& s, float x0, float z0, float x1, float z1, float y, int mat, uint32_t seed, float hgt = 0.45f) {
  int p = s.addBox(V3(x0, y, z0), V3(x1, y + hgt, z1), mat, FALL, 0.01f, seed);
  s.addCut(p, V3(x0 + 0.25f, y - 0.5f, z0 + 0.25f), V3(x1 - 0.25f, y + hgt + 1.0f, z1 - 0.25f));
  int c = s.addBox(V3(x0 - 0.03f, y + hgt, z0 - 0.03f), V3(x1 + 0.03f, y + hgt + 0.06f, z1 + 0.03f), M_STONE, FALL, 0.005f);
  s.addCut(c, V3(x0 + 0.28f, y, z0 + 0.28f), V3(x1 - 0.28f, y + hgt + 1.0f, z1 - 0.28f));
}

inline void stairHead(Scene& s, float x0, float z0, float y, int doorSide, uint32_t seed) {
  // 2.6 x 2.4 m, 2.5 m high, door on one side (0: +x, 1: -x, 2: +z, 3: -z)
  float x1 = x0 + 2.6f, z1 = z0 + 2.4f, yt = y + 2.5f;
  int b = s.addBox(V3(x0, y, z0), V3(x1, yt, z1), M_LIME, FALL, 0.012f, seed);
  float cx = 0.5f * (x0 + x1), cz = 0.5f * (z0 + z1);
  if (doorSide == 0) s.addCut(b, V3(x1 - 0.2f, y - 0.1f, cz - 0.45f), V3(x1 + 0.1f, y + 2.1f, cz + 0.45f));
  if (doorSide == 1) s.addCut(b, V3(x0 - 0.1f, y - 0.1f, cz - 0.45f), V3(x0 + 0.2f, y + 2.1f, cz + 0.45f));
  if (doorSide == 2) s.addCut(b, V3(cx - 0.45f, y - 0.1f, z1 - 0.2f), V3(cx + 0.45f, y + 2.1f, z1 + 0.1f));
  if (doorSide == 3) s.addCut(b, V3(cx - 0.45f, y - 0.1f, z0 - 0.1f), V3(cx + 0.45f, y + 2.1f, z0 + 0.2f));
  s.addBox(V3(x0 - 0.04f, yt, z0 - 0.04f), V3(x1 + 0.04f, yt + 0.07f, z1 + 0.04f), M_STONE, FALL, 0.006f);
}

// A palm-frond pergola on teak posts: shade on the roof terrace.
inline void pergola(Scene& s, float x0, float z0, float x1, float z1, float y) {
  const float H = y + 2.4f;
  for (float x : {x0 + 0.1f, x1 - 0.1f})
    for (float z : {z0 + 0.1f, z1 - 0.1f}) s.addBox(V3(x - 0.06f, y, z - 0.06f), V3(x + 0.06f, H, z + 0.06f), M_TEAK_GREY, FALL, 0.006f);
  for (float z : {z0 + 0.1f, z1 - 0.1f}) s.addBox(V3(x0, H, z - 0.07f), V3(x1, H + 0.14f, z + 0.07f), M_TEAK_GREY, FALL, 0.006f);
  PatternParams sl; sl.type = PAT_SLATS; sl.period = 0.09f; sl.width = 0.035f; sl.a = 0.6f; sl.b = 0.045f; sl.rot = 1.5707963f;
  s.addLattice(V3(x0, H + 0.14f, z0), V3(x1, H + 0.17f, z1), 1, sl, M_PALM, 0.003f);
}

// ---- the house: what the street and courtyard cameras never needed ------------------
inline void completeHouse(Scene& s) {
  const float T = 0.45f, CW = 7.2f, CL = 13.2f;
  const float SZ = CL + T, BZ = SZ + 3.0f;            // 13.65, 16.65
  const float EX = CW + T + 4.0f;                     // 12.05
  const float NZ = -4.45f, WX = -4.45f, SB = BZ + T;  // north face, west face, south face (17.1)
  const float R = 7.0f, P = 7.4f;                     // roof top, parapet top (as the courtyard walls)
  // Roof screed on every wing (none overlap, so no two tops are coplanar).
  roofSlab(s, WX, -T, -T, SB, R, 11);                 // west wing
  roofSlab(s, WX, NZ, EX, -T, R, 12);                 // north wing
  roofSlab(s, CW + T, -T, EX, SB, R, 13);             // east wing
  roofSlab(s, -T, SZ, CW + T, SB, R, 14);             // over the loggia
  // Parapets and copings on the three outer edges (the street facade and the
  // courtyard walls already rise to 7.4 m).
  s.addBox(V3(WX, R, NZ), V3(WX + 0.25f, P, SB), M_LIME, FALL, 0.01f, 21);
  s.addBox(V3(WX + 0.25f, R, NZ), V3(EX, P, NZ + 0.25f), M_LIME, FALL, 0.01f, 22);
  s.addBox(V3(WX + 0.25f, R, SB - 0.25f), V3(EX, P, SB), M_LIME, FALL, 0.01f, 23);
  s.addBox(V3(WX - 0.03f, P, NZ - 0.03f), V3(WX + 0.28f, P + 0.07f, SB + 0.03f), M_STONE, FALL, 0.008f);
  s.addBox(V3(WX + 0.28f, P, NZ - 0.03f), V3(EX, P + 0.07f, NZ + 0.28f), M_STONE, FALL, 0.008f);
  s.addBox(V3(WX + 0.28f, P, SB - 0.28f), V3(EX, P + 0.07f, SB + 0.03f), M_STONE, FALL, 0.008f);
  // The west facade, blank until now: five screened windows upstairs, two
  // small high ones and a teak door below, copper spouts through the parapet.
  int ww = findBox(s, V3(WX, 0.0f, -T), V3(-T, 7.0f, SB));
  if (ww >= 0) {
    for (float zc : {1.6f, 5.0f, 8.4f, 11.8f, 15.2f}) screenedWindow(s, ww, 0, WX, 1.0f, zc, 0.72f, 4.7f, 5.85f);
    for (float zc : {3.5f, 14.0f}) screenedWindow(s, ww, 0, WX, 1.0f, zc, 0.5f, 1.9f, 2.6f);
    s.addCut(ww, V3(WX - 0.1f, -1.0f, 8.6f), V3(WX + 0.55f, 2.6f, 9.8f));
    s.addBox(V3(WX + 0.45f, 0.0f, 8.6f), V3(WX + 0.51f, 2.6f, 9.8f), M_TEAK_GREY, FALL, 0.004f);
    s.addBox(V3(WX - 0.04f, 2.6f, 8.45f), V3(WX + 0.1f, 2.78f, 9.95f), M_TEAK_GREY, FALL, 0.006f);   // lintel
  }
  for (float zc : {-2.2f, 6.7f, 13.0f}) spout(s, 0, WX, 1.0f, zc, R + 0.04f, 4.2f);
  s.addSdf(SDF_PLANTER, V3(WX - 0.55f, 0.0f, 10.6f), 0.0f, 0.7f, {M_TERRACOTTA, M_SOIL});
  // South face above the neighbour: three vents into the upper rooms, two spouts.
  int up = findBox(s, V3(-T, 3.5f, SZ), V3(CW + T, P - 0.4f, SB));
  if (up >= 0)
    for (float xc : {0.8f, 3.6f, 6.4f}) screenedWindow(s, up, 2, SB, -1.0f, xc, 0.6f, 5.9f, 6.6f);
  for (float xc : {-2.4f, 9.8f}) spout(s, 2, SB, -1.0f, xc, R + 0.04f, 1.3f);
  // North face above its neighbour: two spouts.
  for (float xc : {-1.5f, 4.0f}) spout(s, 2, NZ, 1.0f, xc, R + 0.04f, 0.6f);
  // Roof life: a stair head on the north wing (behind the courtyard camera), a
  // palm-frond pergola over the west roof's north end, jars and a mat under it.
  stairHead(s, -3.9f, -4.0f, R, 2, 31);
  pergola(s, -4.1f, -0.25f, -0.75f, 4.2f, R);
  s.addSdf(SDF_JAR, V3(-3.5f, R + 0.05f, 3.6f), 30.0f, 1.0f, {M_TERRACOTTA});
  s.addSdf(SDF_JAR, V3(-3.0f, R + 0.05f, 3.85f), 75.0f, 0.8f, {M_TERRACOTTA});
  s.addSdf(SDF_MAT, V3(-2.3f, R + 0.05f, 1.8f), 10.0f, 1.0f, {M_PALM}, {0.9f, 0.6f});
}

// ---- the neighbours buildHouse() made as plain boxes ---------------------------------
// Each becomes a house seen from the air: a court cut into its mass (away
// from the street), roof screed around it, a parapet and coping. Their
// street faces are untouched.
inline void dressBlock(Scene& s, int idx, bool court, uint32_t seed, bool stair) {
  if (idx < 0) return;
  V3 lo = s.prims[idx].lo, hi = s.prims[idx].hi;
  float w = hi.x - lo.x, d = hi.z - lo.z, h = hi.y;
  // Some of these already carry a stone coping slab over their whole top: the
  // court is cut through it too, and the roof goes on top of it.
  int coping = -1;
  for (int i = 0; i < (int)s.prims.size(); i++) {
    const Prim& q = s.prims[i];
    if (i == idx || q.type != P_BOX || std::fabs(q.lo.y - h) > 1e-3f) continue;
    if (q.lo.x <= lo.x + 0.01f && q.hi.x >= hi.x - 0.01f && q.lo.z <= lo.z + 0.01f && q.hi.z >= hi.z - 0.01f) { coping = i; break; }
  }
  float cx0 = 0, cz0 = 0, cx1 = 0, cz1 = 0;
  bool hasCourt = court && w > 9.0f && d > 9.0f;
  if (hasCourt) {
    float cw = std::min(7.0f, w * 0.45f), cd = std::min(8.0f, d * 0.45f);
    cx0 = lo.x + 0.35f * (w - cw); cz0 = lo.z + 0.5f * (d - cd);
    cx1 = cx0 + cw; cz1 = cz0 + cd;
    s.addCut(idx, V3(cx0, 0.03f, cz0), V3(cx1, h + 1.0f, cz1));
    s.addBox(V3(cx0, 0.0f, cz0), V3(cx1, 0.03f, cz1), M_STONE_FLOOR, FY1, 0.0f);
    if (coping >= 0) s.addCut(coping, V3(cx0, h - 1.0f, cz0), V3(cx1, h + 1.0f, cz1));
  }
  if (coping >= 0) h = s.prims[coping].hi.y;
  int r = s.addBox(V3(lo.x, h, lo.z), V3(hi.x, h + 0.05f, hi.z), M_ROOF, FY1, 0.0f, seed);
  if (hasCourt) s.addCut(r, V3(cx0, h - 1.0f, cz0), V3(cx1, h + 1.0f, cz1));
  parapetRing(s, lo.x, lo.z, hi.x, hi.z, h, s.prims[idx].mat, seed + 5);
  if (stair && w > 6.0f && d > 6.0f) stairHead(s, lo.x + 0.6f, lo.z + 0.6f, h + 0.05f, 0, seed + 9);
  if (w > 7.0f && d > 7.0f) roofKit(s, lo.x + 3.4f, lo.z + 0.3f, hi.x - 0.3f, hi.z - 0.3f, h + 0.05f, seed + 13);
}

inline void dressNeighbours(Scene& s) {
  const float T = 0.45f, CW = 7.2f, CL = 13.2f, BZ = CL + T + 3.0f;
  const float EX = CW + T + 4.0f, EF = EX + T, NZ = -4.45f, SJ = -34.0f, SW = EF + 7.2f;
  dressBlock(s, findBox(s, V3(-4.45f, 0.0f, NZ - 16.0f), V3(EX, 6.3f, NZ)), true, 101, true);            // north
  dressBlock(s, findBox(s, V3(-4.45f, 0.0f, BZ + T), V3(EF - 0.3f, 5.6f, BZ + 22.0f)), true, 102, true);  // south
  dressBlock(s, findBox(s, V3(EX - 3.0f, 0.0f, SJ), V3(EF + 0.2f, 6.0f, NZ - 16.0f)), false, 103, false);  // by the junction
  dressBlock(s, findBox(s, V3(-20.0f, 0.0f, SJ - 7.0f - 3.0f), V3(55.0f, 6.6f, SJ - 7.0f)), false, 104, false);  // across the cross street
  // Across our street: the stepped run of nine.
  float z = -60.0f;
  const float hts[9] = {5.2f, 6.8f, 4.1f, 6.1f, 3.4f, 7.0f, 5.6f, 4.6f, 6.4f};
  const float wds[9] = {14.0f, 9.0f, 12.0f, 10.0f, 11.0f, 13.0f, 9.0f, 12.0f, 30.0f};
  for (int k = 0; k < 9; k++) {
    float set = 11.0f + (k % 3) * 2.5f;
    dressBlock(s, findBox(s, V3(SW + set, 0.0f, z), V3(SW + set + 12.0f, hts[k], z + wds[k])), true, 110 + k, k % 2 == 0);
    z += wds[k];
  }
  // The lot between the north neighbour and the cross street was empty.
}

// ---- the quarter ---------------------------------------------------------------------
// Plots on a grid of 22 x 26 m cells; the lanes between them vary, 3 m
// sikkas mostly, 7 m streets every few rows. Houses fill their plots (old
// quarters are built wall to wall). Near the house (within `near` m) they get
// windows, spouts and stains; further out the procedural weathering carries.
struct QuarterBudget { int trees = 0, stains = 0; };

inline void quarterHouse(Scene& s, float x0, float z0, float w, float d, uint32_t seed, float dist, QuarterBudget& qb) {
  const int mats[6] = {M_LIME, M_LIME_SHADE, M_CORAL, M_RAMMED, M_LIME_SHADE, M_CORAL};
  int mat = mats[qhash(seed) % 6];
  float h = qrand(seed + 14) < 0.22f ? 4.0f + 0.9f * qrand(seed + 15) : 6.0f + 1.6f * qrand(seed + 15);
  int b = s.addBox(V3(x0, 0.0f, z0), V3(x0 + w, h, z0 + d), mat, FALL, 0.012f, seed);
  // the court
  float cw = w * (0.32f + 0.16f * qrand(seed + 2)), cd = d * (0.32f + 0.16f * qrand(seed + 3));
  float cx = x0 + (w - cw) * (0.3f + 0.4f * qrand(seed + 4)), cz = z0 + (d - cd) * (0.3f + 0.4f * qrand(seed + 5));
  s.addCut(b, V3(cx, 0.03f, cz), V3(cx + cw, h + 1.0f, cz + cd));
  s.addBox(V3(cx, 0.0f, cz), V3(cx + cw, 0.03f, cz + cd), M_STONE_FLOOR, FY1, 0.0f);
  int r = s.addBox(V3(x0, h, z0), V3(x0 + w, h + 0.05f, z0 + d), M_ROOF, FY1, 0.0f, seed + 20);
  s.addCut(r, V3(cx, h - 1.0f, cz), V3(cx + cw, h + 1.0f, cz + cd));
  parapetRing(s, x0, z0, x0 + w, z0 + d, h, mat, seed + 21);
  // the court's own low upstand, so its edge reads from above
  int cu = s.addBox(V3(cx - 0.2f, h, cz - 0.2f), V3(cx + cw + 0.2f, h + 0.25f, cz + cd + 0.2f), mat, FALL, 0.008f, seed + 22);
  s.addCut(cu, V3(cx, h - 1.0f, cz), V3(cx + cw, h + 1.0f, cz + cd));
  // roof life
  float rx = (cx - x0 > x0 + w - cx - cw) ? x0 + 0.5f : x0 + w - 3.2f;   // the wider side of the court
  if (qrand(seed + 6) < 0.3f) {
    float tz = qrand(seed + 8) < 0.5f ? z0 + 0.6f : z0 + d - 3.0f;
    float th = h + 3.4f + 1.4f * qrand(seed + 10);
    int tw = s.addBox(V3(rx, h, tz), V3(rx + 2.4f, th, tz + 2.4f), M_LIME, FALL, 0.012f, seed + 23);
    for (int k = 0; k < 2; k++) {
      s.addCut(tw, V3(rx + 0.5f + k * 0.9f, th - 2.8f, tz - 0.2f), V3(rx + 0.85f + k * 0.9f, th - 0.5f, tz + 2.6f));
      s.addCut(tw, V3(rx - 0.2f, th - 2.8f, tz + 0.5f + k * 0.9f), V3(rx + 2.6f, th - 0.5f, tz + 0.85f + k * 0.9f));
    }
    s.addBox(V3(rx - 0.05f, th, tz - 0.05f), V3(rx + 2.45f, th + 0.08f, tz + 2.45f), M_STONE, FALL, 0.006f);
  } else if (qrand(seed + 7) < 0.6f) {
    stairHead(s, rx, qrand(seed + 9) < 0.5f ? z0 + 0.5f : z0 + d - 2.9f, h + 0.05f, (int)(qhash(seed + 11) % 4), seed + 24);
  }
  if (qrand(seed + 12) < 0.18f && w > 12.0f) {
    float pz = qrand(seed + 13) < 0.5f ? z0 + 0.6f : z0 + d - 4.2f;
    float px = rx > x0 + 1.0f ? x0 + 0.6f : x0 + w - 4.6f;
    pergola(s, px, pz, px + 4.0f, pz + 3.6f, h + 0.05f);
  }
  if (dist < 160.0f) {   // clear of the tower / stair corner
    bool west = rx < x0 + 1.0f;
    roofKit(s, west ? x0 + 3.3f : x0 + 0.3f, z0 + 0.3f, west ? x0 + w - 0.3f : x0 + w - 3.4f, z0 + d - 0.3f, h + 0.05f, seed + 70);
  }
  if (dist < 90.0f) roofLife(s, x0 + 0.3f, z0 + 0.3f, cx - 0.3f > x0 + 3.0f ? cx - 0.3f : x0 + w - 0.3f, z0 + d - 0.3f, h + 0.05f, seed + 60);
  // a tree in the court, near the house
  if (dist < 120.0f && qrand(seed + 16) < 0.55f && qb.trees < 34 && std::min(cw, cd) > 4.0f) {
    qb.trees++;
    TreeSpec g;
    g.base = V3(cx + 0.5f * cw, 0.0f, cz + 0.5f * cd);
    g.height = std::min(h + 1.5f + 2.0f * qrand(seed + 11), 9.0f);
    g.crown = std::min(0.42f * std::min(cw, cd), 3.4f); g.trunkR = 0.18f;
    g.seed = seed; g.limbs = 6; g.fill = 0.8f; g.droop = 0.5f;
    ghafSprays(g);
    g.cell = dist < 50.0f ? 0.035f : 0.05f;   // coarser leaves further off
    addTree(s, g);
  }
  // windows, spouts and their stains on the faces the camera passes close to
  if (dist < 70.0f && qb.stains < 70) {
    for (int face = 0; face < 4; face++) {
      if (qrand(seed + 30 + face) < 0.35f) continue;
      int axis = face < 2 ? 0 : 2;
      float plane = face == 0 ? x0 : face == 1 ? x0 + w : face == 2 ? z0 : z0 + d;
      float dir = (face == 0 || face == 2) ? 1.0f : -1.0f;
      float len = axis == 0 ? d : w, u0 = axis == 0 ? z0 : x0;
      int n = 1 + (int)(len / 7.0f);
      for (int k = 0; k < n && qb.stains < 70; k++) {
        float uc = u0 + (k + 0.5f) * len / n;
        if (h > 5.0f) { screenedWindow(s, b, axis, plane, dir, uc, 0.6f, h - 2.4f, h - 1.4f); qb.stains++; }
      }
      if (qb.stains < 70) { spout(s, axis, plane, dir, u0 + len * (0.2f + 0.6f * qrand(seed + 40 + face)), h + 0.04f, 0.6f * h); qb.stains++; }
    }
  }
}

inline void buildApproach(Scene& s) {
  buildHouse(s);
  s.aerial = true;
  s.aerialFrom = (int)s.prims.size();
  completeHouse(s);
  dressNeighbours(s);
  // Ground to the horizon, just under the film scene's own ground boxes.
  int g = s.addBox(V3(-4000.0f, -1.0f, -4000.0f), V3(4000.0f, -0.002f, 4000.0f), M_STREET, FY1, 0.0f);
  // ...but not under the house: it would fill the courtyard's channel and pool
  // (the film's day frame has water there, and the landing must match it).
  s.addCut(g, V3(-4.6f, -2.0f, -4.6f), V3(12.6f, 0.5f, 17.2f));
  // The empty lot between the north neighbour and the cross street.
  QuarterBudget qb;
  quarterHouse(s, -4.45f, -33.4f, 13.0f, 12.5f, 777u, 30.0f, qb);
  // Areas the film scene already dresses (house, street, the far side, the
  // junction): the quarter keeps out of them.
  struct Rect { float x0, z0, x1, z1; };
  const Rect keep[4] = {{-8.0f, -40.6f, 52.0f, 39.4f}, {17.0f, 39.0f, 52.0f, 63.0f}, {-21.0f, -45.0f, 56.0f, -40.0f}, {-8.0f, -40.6f, 9.0f, -20.0f}};
  auto clear = [&](float x0, float z0, float x1, float z1) {
    for (const Rect& k : keep)
      if (x1 > k.x0 && x0 < k.x1 && z1 > k.z0 && z0 < k.z1) return false;
    return true;
  };
  // Lanes: 3 m sikkas, a 7 m street every fourth line; the first lane west of
  // the house is the 3.5 m sikka its west door opens on.
  auto laneX = [](int k) { return (k % 4 == 0) ? 7.0f : 3.0f + 0.5f * qrand((uint32_t)(k + 900)); };
  auto laneZ = [](int m) { return (m % 4 == 1) ? 7.0f : 3.0f + 0.5f * qrand((uint32_t)(m + 1900)); };
  for (int k = -8; k <= 8; k++)
    for (int m = -7; m <= 7; m++) {
      float cx0 = -8.0f + 22.0f * k, cz0 = -46.0f + 26.0f * m;
      // the cell west of the house starts after its sikka
      float lx = laneX(k), lz = laneZ(m);
      uint32_t seed = (uint32_t)((k + 50) * 7919 + (m + 50) * 104729);
      // a cell is [cx0 - 22, cx0] x [cz0, cz0 + 26]; its lanes are on its west and north sides
      float x0 = cx0 - 22.0f + lx, z0 = cz0 + lz;
      float w = 22.0f - lx, d = 26.0f - lz;
      // now and then a plot is split in two
      bool split = qrand(seed + 50) < 0.3f;
      float cxm = x0 + 0.5f * w, czm = z0 + 0.5f * d;
      float dist = std::sqrt((cxm - 4.0f) * (cxm - 4.0f) + (czm - 6.0f) * (czm - 6.0f));
      if (dist > 190.0f) continue;
      if (qrand(seed + 51) < 0.06f) {
        // an open yard behind a wall, with an old ghaf
        if (!clear(x0, z0, x0 + w, z0 + d)) continue;
        int wl = s.addBox(V3(x0, 0.0f, z0), V3(x0 + w, 2.4f, z0 + d), M_LIME_SHADE, FALL, 0.01f, seed);
        s.addCut(wl, V3(x0 + 0.3f, -1.0f, z0 + 0.3f), V3(x0 + w - 0.3f, 3.0f, z0 + d - 0.3f));
        s.addCut(wl, V3(x0 + 0.4f * w, -1.0f, z0 - 0.1f), V3(x0 + 0.4f * w + 1.6f, 2.2f, z0 + 0.4f));
        if (qb.trees < 34) {
          qb.trees++;
          TreeSpec g; g.base = V3(cxm, 0.0f, czm); g.height = 7.5f; g.crown = 4.2f; g.trunkR = 0.3f;
          g.seed = seed; g.limbs = 8; g.fill = 0.85f; g.droop = 0.55f; ghafSprays(g); g.cell = 0.045f;
          addTree(s, g);
        }
        continue;
      }
      if (split) {
        float wa = w * (0.42f + 0.16f * qrand(seed + 52));
        if (clear(x0, z0, x0 + wa, z0 + d)) quarterHouse(s, x0, z0, wa, d, seed, dist, qb);
        if (clear(x0 + wa, z0, x0 + w, z0 + d)) quarterHouse(s, x0 + wa, z0, w - wa, d, seed + 999, dist, qb);
      } else if (clear(x0, z0, x0 + w, z0 + d)) {
        quarterHouse(s, x0, z0, w, d, seed, dist, qb);
      }
    }
  // A few cars parked in the wide streets near the house, under dust covers
  // or not (the summer habit).
  // (the 7 m street is the lane on the west side of cell k = 0, x -30..-23,
  // and the lane on the north side of row m = 1, z -20..-13)
  for (int k = 0; k < 9; k++) {
    uint32_t sd = 5000u + k * 77u;
    bool alongZ = k < 5;
    float px = alongZ ? (k & 1 ? -24.6f : -28.4f) : -36.0f - 15.0f * (k - 5) - 4.0f * qrand(sd);
    float pz = alongZ ? -36.0f + 17.0f * k + 5.0f * qrand(sd) : (k & 1 ? -14.6f : -18.4f);
    float yaw = alongZ ? (k & 2 ? 90.0f : -90.0f) : (k & 2 ? 0.0f : 180.0f);
    if (!clear(px - 2.5f, pz - 2.5f, px + 2.5f, pz + 2.5f)) continue;
    if (qrand(sd + 4) < 0.5f)
      s.addSdf(SDF_CAR_COVER, V3(px, 0.0f, pz), yaw, 1.0f, {M_FABRIC_COVER, M_FABRIC_COVER, M_RUBBER, M_FABRIC_COVER});
    else
      s.addSdf(SDF_CAR, V3(px, 0.0f, pz), yaw, 1.0f, {M_CARPAINT, M_GLASS, M_RUBBER, M_CARLAMP});
  }
  // Beyond the quarter: a low town of plain masses, sparser with distance,
  // for the haze to swallow.
  for (int gx = -22; gx <= 22; gx++)
    for (int gz = -22; gz <= 22; gz++) {
      float cx = gx * 40.0f, cz = gz * 40.0f;
      if (std::fabs(cx - 4.0f) < 215.0f && std::fabs(cz - 6.0f) < 215.0f) continue;
      uint32_t seed = (uint32_t)((gx + 100) * 6151 + (gz + 100) * 3079);
      if (qrand(seed) < 0.25f) continue;
      float w = 12.0f + 18.0f * qrand(seed + 1), d = 12.0f + 18.0f * qrand(seed + 2);
      float h = qrand(seed + 3) < 0.88f ? 4.0f + 4.0f * qrand(seed + 4) : 10.0f + 12.0f * qrand(seed + 4);
      float x0 = cx + (40.0f - 6.0f - w) * qrand(seed + 5) - 20.0f, z0 = cz + (40.0f - 6.0f - d) * qrand(seed + 6) - 20.0f;
      s.addBox(V3(x0, 0.0f, z0), V3(x0 + w, h, z0 + d), qrand(seed + 7) < 0.5f ? M_LIME : M_LIME_SHADE, FALL, 0.02f, seed);
    }
}

}  // namespace zw
