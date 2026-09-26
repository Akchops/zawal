// Foliage: trees made of real leaves, so dappled light is made of real gaps.
//
// A canopy is a grid of small cells (4-6 cm). Each cell holds at most one
// leaf — a thin ellipse with a hashed position, size and orientation — with a
// probability set by a smooth density field built from branch-end clumps. A
// ray walks the cells with a 3D DDA and tests one ellipse per cell, so the
// SAME leaves occlude camera rays and sun rays: sun flecks on the ground are
// genuine images of the sun through genuine gaps (with the physical 0.53 deg
// disc they come out as soft ellipses, as they do under a real tree).
//
// Leaves stay inside their cell (centre jitter + semi-axis <= half a cell),
// which keeps the DDA front-to-back order exact without neighbour tests.
#pragma once
#include "common.h"
#include "noise.h"

namespace zw {

inline float sqr(float x) { return x * x; }

struct Foliage {
  V3 lo, hi;               // bounds of the leaf-cell grid
  float cell = 0.05f;      // leaf cell edge, m
  int nx = 0, ny = 0, nz = 0;
  // coarse density grid (voxel centres), values 0..1
  float dcell = 0.2f;
  int dx = 0, dy = 0, dz = 0;
  std::vector<float> dens;
  float fill = 0.5f;       // leaf probability at density 1
  float leafA = 0.42f;     // ellipse semi-major, fraction of cell
  float leafB = 0.16f;     // ellipse semi-minor, fraction of cell
  float upBias = 0.6f;     // leaves tilt toward the sky
  float sprayFreq = 6.5f;  // 1/m: spray cluster frequency
  float sprayBias = 0.0f;  // raises the share of cells inside a spray
  float noiseAmt = 1.0f;   // how hard the density noise and gaps break the clumps up
  uint32_t seed = 1;
  // Block occupancy for the two-level DDA: blocks of B^3 leaf cells, a block
  // is occupied if any cell centre in it has density > 0. Block faces are
  // cell faces, so skipping an empty block can never skip a leaf.
  static constexpr int B = 8;
  int bx = 0, by = 0, bz = 0;
  std::vector<uint8_t> occ;
  int mat = 0;

  float densAt(V3 p) const {
    int i = (int)((p.x - lo.x) / dcell), j = (int)((p.y - lo.y) / dcell), k = (int)((p.z - lo.z) / dcell);
    if (i < 0 || j < 0 || k < 0 || i >= dx || j >= dy || k >= dz) return 0.0f;
    return dens[((size_t)k * dy + j) * dx + i];
  }

  struct Leaf { V3 c, n, t1, t2; float a, b; uint32_t id; };
  // Whether cell (i,j,k) holds a leaf: the density/spray/hash draw. Evaluated
  // once per cell at build time into `bits` (see buildFoliageOccupancy).
  bool leafDraw(int i, int j, int k) const {
    V3 cc(lo.x + (i + 0.5f) * cell, lo.y + (j + 0.5f) * cell, lo.z + (k + 0.5f) * cell);
    float d = densAt(cc);
    if (d <= 0.0f) return false;
    uint32_t h = hash3i(i + (int)(seed * 7919u), j, k);
    // Leaves come in sprays (a ghaf leaf is dozens of leaflets on one
    // rachis): a fine noise field groups them into ~12-20 cm clusters with
    // sky between, instead of an even confetti of single leaves.
    float spray = smoothstep(0.12f, 0.38f, gnoise(cc * sprayFreq + V3((float)(seed & 255))) + sprayBias + 0.25f * (d - 0.5f));
    return hashf(h) < fill * spray * (0.4f + 0.6f * d);
  }
  std::vector<uint64_t> bits;   // leaf presence per cell
  inline bool hasLeaf(int i, int j, int k) const {
    size_t c = ((size_t)k * ny + j) * nx + i;
    return (bits[c >> 6] >> (c & 63)) & 1u;
  }
  // Geometry of the leaf in cell (i,j,k); only called where hasLeaf().
  void leafGeom(int i, int j, int k, Leaf& L) const {
    V3 cc(lo.x + (i + 0.5f) * cell, lo.y + (j + 0.5f) * cell, lo.z + (k + 0.5f) * cell);
    uint32_t h = hash3i(i + (int)(seed * 7919u), j, k);
    // Random direction on the sphere, pulled toward +Y.
    float u = hashf(h ^ 0x2545f491u), v = hashf(h ^ 0x9e3779b9u);
    float z = 1.0f - 2.0f * u, r = std::sqrt(std::max(0.0f, 1.0f - z * z)), ph = 2.0f * PI * v;
    V3 rnd(r * std::cos(ph), z, r * std::sin(ph));
    L.n = normalize(rnd + V3(0.0f, upBias, 0.0f));
    V3 ref = std::fabs(L.n.y) < 0.9f ? V3(0, 1, 0) : V3(1, 0, 0);
    float spin = hashf(h ^ 0x85ebca6bu) * 2.0f * PI;
    V3 a0 = normalize(cross(L.n, ref)), b0 = cross(L.n, a0);
    L.t1 = a0 * std::cos(spin) + b0 * std::sin(spin);
    L.t2 = cross(L.n, L.t1);
    float s = 0.75f + 0.5f * hashf(h ^ 0xc2b2ae35u);
    L.a = std::min(leafA * cell * s, 0.49f * cell);
    L.b = std::min(leafB * cell * s, L.a);
    // Position: as far from the cell centre as the leaf's own extent allows
    // while staying inside the cell (so the DDA stays exact). A small fixed
    // jitter leaves the leaves on a visible 3D lattice wherever a crown thins
    // out; this breaks it along every axis the leaf is thin in.
    V3 half(std::sqrt(sqr(L.a * L.t1.x) + sqr(L.b * L.t2.x)), std::sqrt(sqr(L.a * L.t1.y) + sqr(L.b * L.t2.y)),
            std::sqrt(sqr(L.a * L.t1.z) + sqr(L.b * L.t2.z)));
    V3 room(std::max(0.0f, 0.5f * cell - half.x), std::max(0.0f, 0.5f * cell - half.y), std::max(0.0f, 0.5f * cell - half.z));
    float jx = 2.0f * hashf(h ^ 0xa511e9b3u) - 1.0f, jy = 2.0f * hashf(h ^ 0x63d83595u) - 1.0f, jz = 2.0f * hashf(h ^ 0x94d049bbu) - 1.0f;
    L.c = cc + V3(jx * room.x, jy * room.y, jz * room.z) * 0.98f;
    L.id = h;
  }
  // Deterministic leaf of cell (i,j,k), if any.
  bool leafIn(int i, int j, int k, Leaf& L) const {
    if (!hasLeaf(i, j, k)) return false;
    leafGeom(i, j, k, L);
    return true;
  }

  // Closest leaf hit in (tmin, tmax). Returns t and fills normal/id.
  // Two-level DDA: walk blocks of B^3 cells, and walk cells only inside
  // occupied blocks. Most of a crown's bounding box is air, and most rays
  // that cross it (shadow rays, bounces) never enter a leafy block.
  bool trace(const V3& o, const V3& d, float tmin, float tmax, float& tHit, V3& nHit, uint32_t& idHit) const {
    float t0 = tmin, t1 = tmax;
    for (int a = 0; a < 3; a++) {
      float inv = 1.0f / (std::fabs(d[a]) > 1e-12f ? d[a] : 1e-12f);
      float ta = (lo[a] - o[a]) * inv, tb = (hi[a] - o[a]) * inv;
      if (ta > tb) std::swap(ta, tb);
      t0 = std::max(t0, ta);
      t1 = std::min(t1, tb);
      if (t0 > t1) return false;
    }
    const float bs = cell * B;
    const int nb[3] = {bx, by, bz}, nn[3] = {nx, ny, nz};
    int bi[3], bstep[3];
    float bMax[3], bDelta[3];
    {
      V3 q = o + d * (t0 + 1e-6f);
      for (int a = 0; a < 3; a++) {
        bi[a] = std::max(0, std::min(nb[a] - 1, (int)std::floor((q[a] - lo[a]) / bs)));
        if (d[a] > 1e-12f) { bstep[a] = 1; bDelta[a] = bs / d[a]; bMax[a] = t0 + ((lo[a] + (bi[a] + 1) * bs) - q[a]) / d[a]; }
        else if (d[a] < -1e-12f) { bstep[a] = -1; bDelta[a] = -bs / d[a]; bMax[a] = t0 + ((lo[a] + bi[a] * bs) - q[a]) / d[a]; }
        else { bstep[a] = 0; bDelta[a] = INF; bMax[a] = INF; }
      }
    }
    float tb0 = t0;
    for (int bit = 0; bit < 1024; bit++) {
      float tb1 = std::min(bMax[0], std::min(bMax[1], bMax[2]));
      if (occ[((size_t)bi[2] * by + bi[1]) * bx + bi[0]]) {
        // Cell walk confined to this block's cells and t-range.
        float te = std::min(tb1, t1);
        V3 q = o + d * (tb0 + 1e-6f);
        int idx[3], stepv[3], cl[3], ch[3];
        float tMax[3], tDelta[3];
        for (int a = 0; a < 3; a++) {
          cl[a] = bi[a] * B;
          ch[a] = std::min(nn[a] - 1, cl[a] + B - 1);
          idx[a] = std::max(cl[a], std::min(ch[a], (int)std::floor((q[a] - lo[a]) / cell)));
          if (d[a] > 1e-12f) { stepv[a] = 1; tDelta[a] = cell / d[a]; tMax[a] = tb0 + ((lo[a] + (idx[a] + 1) * cell) - q[a]) / d[a]; }
          else if (d[a] < -1e-12f) { stepv[a] = -1; tDelta[a] = -cell / d[a]; tMax[a] = tb0 + ((lo[a] + idx[a] * cell) - q[a]) / d[a]; }
          else { stepv[a] = 0; tDelta[a] = INF; tMax[a] = INF; }
        }
        float tc0 = tb0;
        for (int it = 0; it < 3 * B + 3; it++) {
          float tc1 = std::min(tMax[0], std::min(tMax[1], tMax[2]));
          Leaf L;
          if (leafIn(idx[0], idx[1], idx[2], L)) {
            float den = dot(d, L.n);
            if (std::fabs(den) > 1e-7f) {
              float t = dot(L.c - o, L.n) / den;
              if (t > std::max(tc0 - 1e-5f, tmin) && t < std::min(tc1 + 1e-5f, t1)) {
                V3 p = o + d * t - L.c;
                float u = dot(p, L.t1) / L.a, v = dot(p, L.t2) / L.b;
                if (u * u + v * v < 1.0f) { tHit = t; nHit = L.n; idHit = L.id; return true; }
              }
            }
          }
          tc0 = tc1;
          if (tc0 > te) break;
          int ax = tMax[0] < tMax[1] ? (tMax[0] < tMax[2] ? 0 : 2) : (tMax[1] < tMax[2] ? 1 : 2);
          idx[ax] += stepv[ax];
          if (idx[ax] < cl[ax] || idx[ax] > ch[ax]) break;
          tMax[ax] += tDelta[ax];
        }
      }
      tb0 = tb1;
      if (tb0 > t1) return false;
      int ax = bMax[0] < bMax[1] ? (bMax[0] < bMax[2] ? 0 : 2) : (bMax[1] < bMax[2] ? 1 : 2);
      bi[ax] += bstep[ax];
      if (bi[ax] < 0 || bi[ax] >= nb[ax]) return false;
      bMax[ax] += bDelta[ax];
    }
    return false;
  }

  // Reference single-level walk (kept to verify trace() against).
  bool traceFlat(const V3& o, const V3& d, float tmin, float tmax, float& tHit, V3& nHit, uint32_t& idHit) const {
    // Clip to the grid bounds.
    float t0 = tmin, t1 = tmax;
    for (int a = 0; a < 3; a++) {
      float inv = 1.0f / (std::fabs(d[a]) > 1e-12f ? d[a] : 1e-12f);
      float ta = (lo[a] - o[a]) * inv, tb = (hi[a] - o[a]) * inv;
      if (ta > tb) std::swap(ta, tb);
      t0 = std::max(t0, ta);
      t1 = std::min(t1, tb);
      if (t0 > t1) return false;
    }
    V3 q = o + d * (t0 + 1e-6f);
    int idx[3], stepv[3], nn[3] = {nx, ny, nz};
    float tMax[3], tDelta[3];
    for (int a = 0; a < 3; a++) {
      float g = (q[a] - lo[a]) / cell;
      idx[a] = std::max(0, std::min(nn[a] - 1, (int)std::floor(g)));
      if (d[a] > 1e-12f) {
        stepv[a] = 1;
        tDelta[a] = cell / d[a];
        tMax[a] = t0 + ((lo[a] + (idx[a] + 1) * cell) - q[a]) / d[a];
      } else if (d[a] < -1e-12f) {
        stepv[a] = -1;
        tDelta[a] = -cell / d[a];
        tMax[a] = t0 + ((lo[a] + idx[a] * cell) - q[a]) / d[a];
      } else {
        stepv[a] = 0;
        tDelta[a] = INF;
        tMax[a] = INF;
      }
    }
    float tc0 = t0;
    for (int it = 0; it < 4096; it++) {
      float tc1 = std::min(tMax[0], std::min(tMax[1], tMax[2]));
      Leaf L;
      if (leafIn(idx[0], idx[1], idx[2], L)) {
        float den = dot(d, L.n);
        if (std::fabs(den) > 1e-7f) {
          float t = dot(L.c - o, L.n) / den;
          if (t > std::max(tc0 - 1e-5f, tmin) && t < std::min(tc1 + 1e-5f, t1)) {
            V3 p = o + d * t - L.c;
            float u = dot(p, L.t1) / L.a, v = dot(p, L.t2) / L.b;
            if (u * u + v * v < 1.0f) {
              tHit = t;
              nHit = L.n;
              idHit = L.id;
              return true;
            }
          }
        }
      }
      tc0 = tc1;
      if (tc0 > t1) return false;
      int ax = tMax[0] < tMax[1] ? (tMax[0] < tMax[2] ? 0 : 2) : (tMax[1] < tMax[2] ? 1 : 2);
      idx[ax] += stepv[ax];
      if (idx[ax] < 0 || idx[ax] >= nn[ax]) return false;
      tMax[ax] += tDelta[ax];
    }
    return false;
  }
};

inline void buildFoliageOccupancy(Foliage& f) {
  const int B = Foliage::B;
  size_t ncell = (size_t)f.nx * f.ny * f.nz;
  f.bits.assign((ncell + 63) / 64, 0ull);
  f.bx = (f.nx + B - 1) / B; f.by = (f.ny + B - 1) / B; f.bz = (f.nz + B - 1) / B;
  f.occ.assign((size_t)f.bx * f.by * f.bz, 0);
  // Slabs of 64 cells along x share one word, so rows can be filled in
  // parallel without two threads writing the same word only if a row is a
  // whole number of words; it is not in general, so draw into a byte buffer
  // per z-slab and pack serially.
  std::vector<uint8_t> draw(ncell, 0);
#pragma omp parallel for schedule(dynamic, 1)
  for (int k = 0; k < f.nz; k++)
    for (int j = 0; j < f.ny; j++)
      for (int i = 0; i < f.nx; i++)
        draw[((size_t)k * f.ny + j) * f.nx + i] = f.leafDraw(i, j, k) ? 1 : 0;
  for (int k = 0; k < f.nz; k++)
    for (int j = 0; j < f.ny; j++)
      for (int i = 0; i < f.nx; i++) {
        size_t c = ((size_t)k * f.ny + j) * f.nx + i;
        if (!draw[c]) continue;
        f.bits[c >> 6] |= 1ull << (c & 63);
        f.occ[((size_t)(k / B) * f.by + j / B) * f.bx + i / B] = 1;
      }
}

// Density from clumps: each clump is a sphere of leaves, leafier toward its
// outer shell (real canopies are a skin of foliage around a sparse interior),
// broken up by low-frequency noise so no clump reads as a ball.
inline void buildFoliageDensity(Foliage& f, const std::vector<V3>& centres, const std::vector<float>& radii) {
  V3 blo(INF), bhi(-INF);
  for (size_t k = 0; k < centres.size(); k++) {
    blo = vmin(blo, centres[k] - V3(radii[k]));
    bhi = vmax(bhi, centres[k] + V3(radii[k]));
  }
  f.lo = blo;
  f.hi = bhi;
  f.nx = std::max(1, (int)std::ceil((f.hi.x - f.lo.x) / f.cell));
  f.ny = std::max(1, (int)std::ceil((f.hi.y - f.lo.y) / f.cell));
  f.nz = std::max(1, (int)std::ceil((f.hi.z - f.lo.z) / f.cell));
  f.hi = f.lo + V3(f.nx * f.cell, f.ny * f.cell, f.nz * f.cell);
  f.dx = std::max(1, (int)std::ceil((f.hi.x - f.lo.x) / f.dcell));
  f.dy = std::max(1, (int)std::ceil((f.hi.y - f.lo.y) / f.dcell));
  f.dz = std::max(1, (int)std::ceil((f.hi.z - f.lo.z) / f.dcell));
  f.dens.assign((size_t)f.dx * f.dy * f.dz, 0.0f);
  float off = (float)(f.seed & 1023) * 0.37f;
  for (int k = 0; k < f.dz; k++)
    for (int j = 0; j < f.dy; j++)
      for (int i = 0; i < f.dx; i++) {
        V3 p = f.lo + V3((i + 0.5f) * f.dcell, (j + 0.5f) * f.dcell, (k + 0.5f) * f.dcell);
        float best = 0.0f;
        for (size_t c = 0; c < centres.size(); c++) {
          float x = length(p - centres[c]) / radii[c];
          if (x >= 1.0f) continue;
          float shell = 0.35f + 0.65f * smoothstep(0.15f, 0.75f, x);
          float edge = 1.0f - smoothstep(0.7f, 1.0f, x);
          best = std::max(best, shell * edge);
        }
        // Break the clumps up: open gaps you can see sky through, denser knots.
        float n = 0.5f + 0.5f * fbm(p * 1.1f + V3(off), 4) * 1.8f;
        float gaps = smoothstep(-0.25f, 0.2f, fbm(p * 2.3f + V3(off * 1.7f), 3));
        n = lerpf(1.0f, n, f.noiseAmt);
        gaps = lerpf(1.0f, gaps, f.noiseAmt);
        f.dens[((size_t)k * f.dy + j) * f.dx + i] = saturate(best * n * (0.35f + 0.65f * gaps));
      }
  buildFoliageOccupancy(f);
}

}  // namespace zw
