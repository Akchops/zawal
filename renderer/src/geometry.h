// Scene geometry: analytic primitives with a BVH.
//
// Architecture is mostly boxes, so boxes are exact (slab test) rather than
// sphere-traced. Detail that SDFs do better is sphere-traced only inside a
// bounding primitive:
//   * lattice slabs trace a 2D pattern SDF along the ray's in-plane projection,
//     giving straight-walled carved holes whose depth really blocks steep sun;
//   * dunes are a smooth height field.
// Shading normals on boxes and lattices come from a smoothed SDF of the local
// CSG, so every arris is slightly rounded as hand-applied lime plaster and
// worked timber are — hard CG edges are one of the first things that make an
// architectural render read as fake.
#pragma once
#include "common.h"
#include "noise.h"
#include "patterns.h"

namespace zw {

enum PrimType : uint8_t { P_BOX = 0, P_LATTICE, P_CYL, P_SPHERE, P_QUAD, P_HFIELD };

enum PrimFlags : uint8_t {
  F_NO_CAMERA = 1,   // invisible to camera rays (still casts shadows / bounces)
  F_NO_SHADOW = 2,   // does not occlude next-event rays
  F_WATER = 4,       // dielectric interface; transparent to shadow rays
};

// Face mask bits: which faces of a box are exposed (get rounded arrises).
enum FaceBits : uint8_t { FX0 = 1, FX1 = 2, FY0 = 4, FY1 = 8, FZ0 = 16, FZ1 = 32, FALL = 63 };

struct AABB {
  V3 lo = V3(INF), hi = V3(-INF);
  void grow(V3 p) { lo = vmin(lo, p); hi = vmax(hi, p); }
  void grow(const AABB& b) { lo = vmin(lo, b.lo); hi = vmax(hi, b.hi); }
  V3 centre() const { return (lo + hi) * 0.5f; }
};

struct Prim {
  PrimType type = P_BOX;
  uint8_t flags = 0;
  uint8_t faces = FALL;     // exposed faces (bevel mask), boxes only
  uint8_t axis = 1;         // lattice: thickness axis. cylinder: always Y
  uint16_t mat = 0;
  uint16_t pattern = 0;     // lattice pattern index
  uint32_t seed = 0;        // per-object variation
  V3 lo, hi;                // box / slab / quad bounds; cylinder bounds
  float bevel = 0.012f;     // arris radius, m
  int cutStart = 0, cutCount = 0;  // CSG subtraction boxes (boxes only)
  V3 c; float r = 0;        // sphere/cylinder centre + radius
  int hfield = 0;           // height-field id
  uint8_t grainAxis = 0;    // timber grain direction for materials
};

struct Hit {
  float t = INF;
  int prim = -1;
  V3 n;          // geometric normal, facing the incoming ray
  int face = -1; // box face (0..5) or -2 for a lattice hole wall
};

// Smooth dunes for the Qudra desert: large wind-shaped swells. Uses only
// gradient noise and smooth combinators so it can be differentiated.
inline float duneHeight(float x, float z) {
  // Long transverse dunes perpendicular to the prevailing NW shamal wind.
  float u = 0.80f * x + 0.60f * z, v = -0.60f * x + 0.80f * z;
  float swell = 3.2f * fbm2(u * 0.0065f, v * 0.012f, 4) + 1.4f * fbm2(u * 0.02f + 3.1f, v * 0.03f, 3);
  // Big crescentic ridges further out. The crest profile uses a smooth |sin|
  // (sqrt(s^2 + e)) so the brink is sharp to the eye but has no slope
  // discontinuity for the normal to turn into a drawn line (trap #3).
  float r = std::sqrt(x * x + z * z);
  float phase = u * 0.011f + 2.2f * fbm2(u * 0.003f, v * 0.004f, 3);
  float s = std::sin(phase);
  float ridge = 1.0f - sabs(s, 0.02f);
  float big = 16.0f * ridge * (0.6f + 0.4f * fbm2(v * 0.004f + 5.0f, u * 0.002f, 2));
  float far = smoothstep(90.0f, 420.0f, r);
  float flat = std::exp(-(x * x + z * z) / (2.0f * 55.0f * 55.0f));  // levelled pavilion site
  return swell * (1.0f - 0.92f * flat) - 0.25f * flat + big * far;
}

struct Scene {
  std::vector<Prim> prims;
  std::vector<AABB> cuts;          // CSG subtraction boxes
  std::vector<PatternParams> patterns;
  std::vector<int> planes;         // infinite ground planes (indices into prims) — kept out of BVH
  float groundY = 0.0f;
  int groundMat = -1;              // -1: no infinite ground

  struct Node { AABB b; int left = -1, right = -1, start = 0, count = 0; };
  std::vector<Node> nodes;
  std::vector<int> order;

  // ---- construction helpers --------------------------------------------
  int addBox(V3 lo, V3 hi, int mat, uint8_t faces = FALL, float bevel = 0.012f, uint32_t seed = 0) {
    Prim p; p.type = P_BOX; p.lo = vmin(lo, hi); p.hi = vmax(lo, hi); p.mat = mat; p.faces = faces;
    p.bevel = bevel; p.seed = seed ? seed : (uint32_t)prims.size() * 2654435761u;
    V3 e = p.hi - p.lo;
    p.grainAxis = e.x >= e.y && e.x >= e.z ? 0 : (e.y >= e.z ? 1 : 2);
    if (p.lo.y <= groundY + 1e-4f) p.faces &= ~FY0;  // sits on the ground: no groove at its foot
    prims.push_back(p);
    return (int)prims.size() - 1;
  }
  void addCut(int box, V3 lo, V3 hi) {
    Prim& p = prims[box];
    if (p.cutCount == 0) p.cutStart = (int)cuts.size();
    // Cuts for one box must be contiguous.
    AABB b; b.lo = vmin(lo, hi); b.hi = vmax(lo, hi);
    cuts.insert(cuts.begin() + p.cutStart + p.cutCount, b);
    for (auto& q : prims) if (&q != &p && q.cutCount && q.cutStart >= p.cutStart + p.cutCount) q.cutStart++;
    p.cutCount++;
  }
  int addLattice(V3 lo, V3 hi, int axis, const PatternParams& pat, int mat, float bevel = 0.004f) {
    Prim p; p.type = P_LATTICE; p.lo = vmin(lo, hi); p.hi = vmax(lo, hi); p.axis = (uint8_t)axis; p.mat = mat;
    p.bevel = bevel; p.seed = (uint32_t)prims.size() * 2654435761u;
    patterns.push_back(pat);
    p.pattern = (uint16_t)(patterns.size() - 1);
    p.grainAxis = (uint8_t)((axis + 1) % 3);
    prims.push_back(p);
    return (int)prims.size() - 1;
  }
  int addCyl(float cx, float cz, float r, float y0, float y1, int mat) {
    Prim p; p.type = P_CYL; p.c = V3(cx, 0, cz); p.r = r; p.lo = V3(cx - r, y0, cz - r); p.hi = V3(cx + r, y1, cz + r);
    p.mat = mat; p.seed = (uint32_t)prims.size() * 2654435761u; p.grainAxis = 1;
    prims.push_back(p);
    return (int)prims.size() - 1;
  }
  int addSphere(V3 c, float r, int mat) {
    Prim p; p.type = P_SPHERE; p.c = c; p.r = r; p.lo = c - V3(r); p.hi = c + V3(r); p.mat = mat;
    p.seed = (uint32_t)prims.size() * 2654435761u;
    prims.push_back(p);
    return (int)prims.size() - 1;
  }
  int addQuadY(float y, float x0, float x1, float z0, float z1, int mat, uint8_t flags = 0) {
    Prim p; p.type = P_QUAD; p.lo = V3(std::min(x0, x1), y, std::min(z0, z1)); p.hi = V3(std::max(x0, x1), y, std::max(z0, z1));
    p.mat = mat; p.flags = flags; p.seed = (uint32_t)prims.size() * 2654435761u;
    prims.push_back(p);
    return (int)prims.size() - 1;
  }
  int addHField(V3 lo, V3 hi, int mat, int id) {
    Prim p; p.type = P_HFIELD; p.lo = lo; p.hi = hi; p.mat = mat; p.hfield = id;
    p.seed = (uint32_t)prims.size() * 2654435761u;
    prims.push_back(p);
    return (int)prims.size() - 1;
  }

  AABB primBounds(const Prim& p) const {
    AABB b; b.lo = p.lo; b.hi = p.hi;
    if (p.type == P_QUAD) { b.lo.y -= 1e-3f; b.hi.y += 1e-3f; }
    return b;
  }

  // ---- BVH ---------------------------------------------------------------
  void build() {
    order.clear();
    for (int i = 0; i < (int)prims.size(); i++) order.push_back(i);
    nodes.clear();
    nodes.reserve(prims.size() * 2 + 1);
    buildNode(0, (int)order.size());
  }
  int buildNode(int start, int count) {
    Node n;
    for (int i = start; i < start + count; i++) n.b.grow(primBounds(prims[order[i]]));
    int idx = (int)nodes.size();
    nodes.push_back(n);
    if (count <= 2) { nodes[idx].start = start; nodes[idx].count = count; return idx; }
    AABB cb;
    for (int i = start; i < start + count; i++) cb.grow(primBounds(prims[order[i]]).centre());
    V3 e = cb.hi - cb.lo;
    int ax = e.x > e.y && e.x > e.z ? 0 : (e.y > e.z ? 1 : 2);
    int mid = start + count / 2;
    std::nth_element(order.begin() + start, order.begin() + mid, order.begin() + start + count, [&](int a, int b) {
      return primBounds(prims[a]).centre()[ax] < primBounds(prims[b]).centre()[ax];
    });
    int l = buildNode(start, mid - start);
    int r = buildNode(mid, start + count - mid);
    nodes[idx].left = l; nodes[idx].right = r; nodes[idx].count = 0;
    return idx;
  }

  static inline bool slab(const V3& lo, const V3& hi, const Ray& r, const V3& inv, float& t0, float& t1) {
    float tx0 = (lo.x - r.o.x) * inv.x, tx1 = (hi.x - r.o.x) * inv.x;
    float ty0 = (lo.y - r.o.y) * inv.y, ty1 = (hi.y - r.o.y) * inv.y;
    float tz0 = (lo.z - r.o.z) * inv.z, tz1 = (hi.z - r.o.z) * inv.z;
    t0 = std::max(std::max(std::min(tx0, tx1), std::min(ty0, ty1)), std::min(tz0, tz1));
    t1 = std::min(std::min(std::max(tx0, tx1), std::max(ty0, ty1)), std::max(tz0, tz1));
    return t1 >= std::max(t0, 0.0f);
  }

  // Entry face (0..5 = -X,+X,-Y,+Y,-Z,+Z) of a box for a ray entering at t0.
  static inline int entryFace(const V3& lo, const V3& hi, const Ray& r, const V3& inv) {
    float tx = (r.d.x > 0 ? lo.x - r.o.x : hi.x - r.o.x) * inv.x;
    float ty = (r.d.y > 0 ? lo.y - r.o.y : hi.y - r.o.y) * inv.y;
    float tz = (r.d.z > 0 ? lo.z - r.o.z : hi.z - r.o.z) * inv.z;
    if (tx >= ty && tx >= tz) return r.d.x > 0 ? 0 : 1;
    if (ty >= tz) return r.d.y > 0 ? 2 : 3;
    return r.d.z > 0 ? 4 : 5;
  }
  static inline int exitFace(const V3& lo, const V3& hi, const Ray& r, const V3& inv) {
    float tx = (r.d.x > 0 ? hi.x - r.o.x : lo.x - r.o.x) * inv.x;
    float ty = (r.d.y > 0 ? hi.y - r.o.y : lo.y - r.o.y) * inv.y;
    float tz = (r.d.z > 0 ? hi.z - r.o.z : lo.z - r.o.z) * inv.z;
    if (tx <= ty && tx <= tz) return r.d.x > 0 ? 1 : 0;
    if (ty <= tz) return r.d.y > 0 ? 3 : 2;
    return r.d.z > 0 ? 5 : 4;
  }
  static inline V3 faceNormal(int f) {
    static const V3 N[6] = {V3(-1, 0, 0), V3(1, 0, 0), V3(0, -1, 0), V3(0, 1, 0), V3(0, 0, -1), V3(0, 0, 1)};
    return N[f];
  }

  // Box minus its cut boxes. Walks the ray through the cut intervals.
  bool hitBox(const Prim& p, const Ray& r, const V3& inv, float tmin, float tmax, Hit& h) const {
    float a0, a1;
    if (!slab(p.lo, p.hi, r, inv, a0, a1)) return false;
    if (a0 > tmax || a1 < tmin) return false;
    float t = std::max(a0, tmin);
    int face = a0 >= tmin ? entryFace(p.lo, p.hi, r, inv) : -1;
    bool fromCut = false;
    V3 cutN;
    for (int iter = 0; iter < 16; iter++) {
      bool inside = false;
      for (int k = 0; k < p.cutCount; k++) {
        const AABB& c = cuts[p.cutStart + k];
        float c0, c1;
        if (slab(c.lo, c.hi, r, inv, c0, c1) && c0 <= t + 1e-6f && c1 > t + 1e-6f) {
          t = c1;
          inside = true;
          fromCut = true;
          // Leaving the cut box we meet material whose normal faces back into the cut.
          cutN = -faceNormal(exitFace(c.lo, c.hi, r, inv));
        }
      }
      if (!inside) break;
    }
    if (t > a1 - 1e-7f || t > tmax) return false;
    if (face < 0 && !fromCut) {
      // Origin inside solid material (numerical leak); report the exit so the
      // surface still occludes rather than letting light through a wall.
      face = exitFace(p.lo, p.hi, r, inv);
      h.t = std::max(a1, tmin); h.n = faceNormal(face); h.face = face;
      return true;
    }
    h.t = t;
    h.n = fromCut ? cutN : faceNormal(face);
    h.face = fromCut ? 6 : face;
    return true;
  }

  // Lattice slab: exact faces, sphere-traced hole walls.
  bool hitLattice(const Prim& p, const Ray& r, const V3& inv, float tmin, float tmax, Hit& h, bool anyHit) const {
    float a0, a1;
    if (!slab(p.lo, p.hi, r, inv, a0, a1)) return false;
    if (a0 > tmax || a1 < tmin) return false;
    const PatternParams& pat = patterns[p.pattern];
    int ax = p.axis, ua = (ax + 1) % 3, va = (ax + 2) % 3;
    float t = std::max(a0, tmin);
    V3 q = r.o + r.d * t;
    float sd = patternSD(pat, q[ua], q[va]);
    if (sd > 0.0f) {
      if (a0 >= tmin) {
        h.t = a0;
        h.face = entryFace(p.lo, p.hi, r, inv);
        h.n = faceNormal(h.face);
        return true;
      }
      // Started inside solid: offset ray from a hole wall that went the wrong
      // way. Treat as occluded right here.
      h.t = tmin; h.face = -2;
      float gu, gv; patternGrad(pat, q[ua], q[va], gu, gv);
      V3 n(0.0f); n[ua] = -gu; n[va] = -gv; h.n = n;
      return true;
    }
    float s2 = std::sqrt(r.d[ua] * r.d[ua] + r.d[va] * r.d[va]);
    if (s2 < 1e-6f) return false;  // straight through a hole, parallel to its walls
    float tEnd = std::min(a1, tmax);
    for (int i = 0; i < 96; i++) {
      // -sd is the in-plane distance to the nearest hole wall; divide by the
      // in-plane speed to get a safe step along the 3D ray.
      float step = std::max(-sd / s2, 2e-5f);
      t += step;
      if (t >= tEnd) return false;
      q = r.o + r.d * t;
      sd = patternSD(pat, q[ua], q[va]);
      if (sd > -2e-5f) {
        h.t = t; h.face = -2;
        float gu, gv; patternGrad(pat, q[ua], q[va], gu, gv);
        V3 n(0.0f); n[ua] = -gu; n[va] = -gv;   // wall faces back into the hole
        h.n = n;
        (void)anyHit;
        return true;
      }
    }
    return false;
  }

  bool hitCyl(const Prim& p, const Ray& r, float tmin, float tmax, Hit& h) const {
    float ox = r.o.x - p.c.x, oz = r.o.z - p.c.z;
    float a = r.d.x * r.d.x + r.d.z * r.d.z;
    float best = INF; V3 bn;
    if (a > 1e-12f) {
      float b = ox * r.d.x + oz * r.d.z, c = ox * ox + oz * oz - p.r * p.r;
      float disc = b * b - a * c;
      if (disc >= 0) {
        float s = std::sqrt(disc);
        for (float t : {(-b - s) / a, (-b + s) / a}) {
          if (t > tmin && t < tmax && t < best) {
            float y = r.o.y + r.d.y * t;
            if (y >= p.lo.y && y <= p.hi.y) { best = t; bn = V3(ox + r.d.x * t, 0.0f, oz + r.d.z * t) / p.r; break; }
          }
        }
      }
    }
    if (std::fabs(r.d.y) > 1e-12f) {
      for (float yc : {p.lo.y, p.hi.y}) {
        float t = (yc - r.o.y) / r.d.y;
        if (t > tmin && t < tmax && t < best) {
          float x = ox + r.d.x * t, z = oz + r.d.z * t;
          if (x * x + z * z <= p.r * p.r) { best = t; bn = V3(0.0f, yc == p.hi.y ? 1.0f : -1.0f, 0.0f); }
        }
      }
    }
    if (best >= INF) return false;
    h.t = best; h.n = bn; h.face = 0;
    return true;
  }

  bool hitSphere(const Prim& p, const Ray& r, float tmin, float tmax, Hit& h) const {
    V3 oc = r.o - p.c;
    float b = dot(oc, r.d), c = dot(oc, oc) - p.r * p.r, disc = b * b - c;
    if (disc < 0) return false;
    float s = std::sqrt(disc);
    float t = -b - s;
    if (t < tmin) t = -b + s;
    if (t < tmin || t > tmax) return false;
    h.t = t; h.n = (oc + r.d * t) / p.r; h.face = 0;
    return true;
  }

  bool hitQuad(const Prim& p, const Ray& r, float tmin, float tmax, Hit& h) const {
    if (std::fabs(r.d.y) < 1e-12f) return false;
    float t = (p.lo.y - r.o.y) / r.d.y;
    if (t < tmin || t > tmax) return false;
    float x = r.o.x + r.d.x * t, z = r.o.z + r.d.z * t;
    if (x < p.lo.x || x > p.hi.x || z < p.lo.z || z > p.hi.z) return false;
    h.t = t; h.n = V3(0.0f, 1.0f, 0.0f); h.face = 3;
    return true;
  }

  bool hitHField(const Prim& p, const Ray& r, const V3& inv, float tmin, float tmax, Hit& h) const {
    float a0, a1;
    if (!slab(p.lo, p.hi, r, inv, a0, a1)) return false;
    float t = std::max(a0, tmin), tEnd = std::min(a1, tmax);
    V3 q = r.o + r.d * t;
    float f = q.y - duneHeight(q.x, q.z);
    if (f < 0) return false;
    float prevT = t, prevF = f;
    for (int i = 0; i < 512 && t < tEnd; i++) {
      // Lipschitz-safe step: the dune slope never exceeds ~0.35.
      float step = std::max(0.02f + 0.004f * t, f * 0.55f / (std::fabs(r.d.y) + 0.6f));
      prevT = t; prevF = f;
      t += step;
      q = r.o + r.d * t;
      f = q.y - duneHeight(q.x, q.z);
      if (f < 0) {
        float lo = prevT, hi = t;
        for (int k = 0; k < 12; k++) {
          float m = 0.5f * (lo + hi);
          V3 qm = r.o + r.d * m;
          if (qm.y - duneHeight(qm.x, qm.z) < 0) hi = m; else lo = m;
        }
        (void)prevF;
        h.t = hi;
        V3 qh = r.o + r.d * hi;
        const float e = 0.05f;
        float dx = duneHeight(qh.x + e, qh.z) - duneHeight(qh.x - e, qh.z);
        float dz = duneHeight(qh.x, qh.z + e) - duneHeight(qh.x, qh.z - e);
        h.n = normalize(V3(-dx, 2.0f * e, -dz));
        h.face = 3;
        return true;
      }
    }
    return false;
  }

  inline bool hitPrim(int i, const Ray& r, const V3& inv, float tmin, float tmax, Hit& h, bool anyHit) const {
    const Prim& p = prims[i];
    switch (p.type) {
      case P_BOX: return hitBox(p, r, inv, tmin, tmax, h);
      case P_LATTICE: return hitLattice(p, r, inv, tmin, tmax, h, anyHit);
      case P_CYL: return hitCyl(p, r, tmin, tmax, h);
      case P_SPHERE: return hitSphere(p, r, tmin, tmax, h);
      case P_QUAD: return hitQuad(p, r, tmin, tmax, h);
      case P_HFIELD: return hitHField(p, r, inv, tmin, tmax, h);
    }
    return false;
  }

  // Closest hit. mask: flags that exclude a primitive from this query.
  bool intersect(const Ray& r, float tmin, float tmax, Hit& best, uint8_t excludeFlags = 0) const {
    V3 inv(1.0f / (std::fabs(r.d.x) > 1e-12f ? r.d.x : 1e-12f), 1.0f / (std::fabs(r.d.y) > 1e-12f ? r.d.y : 1e-12f),
           1.0f / (std::fabs(r.d.z) > 1e-12f ? r.d.z : 1e-12f));
    best.t = tmax; best.prim = -1;
    // Infinite ground.
    if (groundMat >= 0 && r.d.y < -1e-9f) {
      float t = (groundY - r.o.y) / r.d.y;
      if (t > tmin && t < best.t) { best.t = t; best.prim = -2; best.n = V3(0, 1, 0); best.face = 3; }
    }
    int stack[64], sp = 0;
    stack[sp++] = 0;
    while (sp) {
      const Node& n = nodes[stack[--sp]];
      float t0, t1;
      if (!slab(n.b.lo, n.b.hi, r, inv, t0, t1) || t0 > best.t || t1 < tmin) continue;
      if (n.count) {
        for (int k = n.start; k < n.start + n.count; k++) {
          int pi = order[k];
          if (prims[pi].flags & excludeFlags) continue;
          Hit h;
          if (hitPrim(pi, r, inv, tmin, best.t, h, false) && h.t < best.t) { best = h; best.prim = pi; }
        }
      } else {
        // Visit the nearer child first.
        const Node& L = nodes[n.left];
        const Node& R = nodes[n.right];
        float l0, l1, r0, r1;
        bool hl = slab(L.b.lo, L.b.hi, r, inv, l0, l1), hr = slab(R.b.lo, R.b.hi, r, inv, r0, r1);
        if (hl && hr) {
          if (l0 < r0) { stack[sp++] = n.right; stack[sp++] = n.left; }
          else { stack[sp++] = n.left; stack[sp++] = n.right; }
        } else if (hl) stack[sp++] = n.left;
        else if (hr) stack[sp++] = n.right;
      }
    }
    if (best.prim == -1) return false;
    if (dot(best.n, r.d) > 0) best.n = -best.n;
    return true;
  }

  bool occluded(const Ray& r, float tmin, float tmax) const {
    Hit h;
    return intersect(r, tmin, tmax, h, F_NO_SHADOW | F_WATER);
  }

  // ---- shading normals with rounded arrises --------------------------------
  // SDF of an axis-aligned box.
  static float sdBox(V3 p, const V3& lo, const V3& hi) {
    V3 c = (lo + hi) * 0.5f, e = (hi - lo) * 0.5f;
    V3 q = vabs(p - c) - e;
    return length(vmax(q, V3(0.0f))) + std::min(maxc(q), 0.0f);
  }
  // Box with rounded edges only where faces are exposed: hidden faces are
  // pushed outward by 2r so their rounding happens inside the neighbour.
  float sdSolidBox(const Prim& p, V3 x) const {
    float r = p.bevel;
    V3 lo = p.lo, hi = p.hi;
    if (!(p.faces & FX0)) lo.x -= 2 * r; if (!(p.faces & FX1)) hi.x += 2 * r;
    if (!(p.faces & FY0)) lo.y -= 2 * r; if (!(p.faces & FY1)) hi.y += 2 * r;
    if (!(p.faces & FZ0)) lo.z -= 2 * r; if (!(p.faces & FZ1)) hi.z += 2 * r;
    float d = sdBox(x, lo + V3(r), hi - V3(r)) - r;
    for (int k = 0; k < p.cutCount; k++) {
      const AABB& c = cuts[p.cutStart + k];
      d = smax(d, -sdBox(x, c.lo, c.hi), 1.2f * r);   // rounded arris where the cut meets the face
    }
    return d;
  }
  float sdSolidLattice(const Prim& p, V3 x) const {
    const PatternParams& pat = patterns[p.pattern];
    int ax = p.axis, ua = (ax + 1) % 3, va = (ax + 2) % 3;
    float half = 0.5f * (p.hi[ax] - p.lo[ax]), mid = 0.5f * (p.hi[ax] + p.lo[ax]);
    float slabD = std::fabs(x[ax] - mid) - half;
    float solid = -patternSD(pat, x[ua], x[va]);
    return smax(slabD, solid, 1.2f * p.bevel);
  }

  // Cheap test: is x within reach of a rounded arris? Most hits are in the
  // middle of a face, where the bevel normal equals the face normal anyway.
  bool nearArris(const Prim& p, V3 x) const {
    float reach = 2.6f * p.bevel;
    if (p.type == P_LATTICE) {
      int ax = p.axis, ua = (ax + 1) % 3, va = (ax + 2) % 3;
      float sd = std::fabs(patternSD(patterns[p.pattern], x[ua], x[va]));
      bool nearFace = std::fabs(x[ax] - p.lo[ax]) < reach || std::fabs(x[ax] - p.hi[ax]) < reach;
      return nearFace && sd < reach * 1.5f;
    }
    int close = 0;
    for (int a = 0; a < 3; a++)
      if (std::fabs(x[a] - p.lo[a]) < reach || std::fabs(x[a] - p.hi[a]) < reach) close++;
    if (close >= 2) return true;
    for (int k = 0; k < p.cutCount; k++) {
      const AABB& c = cuts[p.cutStart + k];
      if (x.x > c.lo.x - reach && x.x < c.hi.x + reach && x.y > c.lo.y - reach && x.y < c.hi.y + reach &&
          x.z > c.lo.z - reach && x.z < c.hi.z + reach)
        return true;
    }
    return false;
  }

  V3 shadingNormal(int pi, V3 x, V3 ng) const {
    if (pi < 0) return ng;
    const Prim& p = prims[pi];
    if (p.type != P_BOX && p.type != P_LATTICE) return ng;
    float r = p.bevel;
    if (r <= 0) return ng;
    if (!nearArris(p, x)) return ng;
    auto f = [&](V3 q) { return p.type == P_BOX ? sdSolidBox(p, q) : sdSolidLattice(p, q); };
    float e = std::max(1e-4f, 0.12f * r);
    V3 g(f(x + V3(e, 0, 0)) - f(x - V3(e, 0, 0)), f(x + V3(0, e, 0)) - f(x - V3(0, e, 0)),
         f(x + V3(0, 0, e)) - f(x - V3(0, 0, e)));
    float gl = length(g);
    if (gl < 1e-9f) return ng;
    V3 n = g / gl;
    // Never let the bevel normal face away from the viewer's side of the
    // geometric surface (would shade black at grazing angles).
    if (dot(n, ng) < 0.05f) return ng;
    return n;
  }
};

}  // namespace zw
