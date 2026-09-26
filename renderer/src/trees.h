// Procedural trees: bark capsules for the wood, a leaf-cell canopy for the
// crown (foliage.h). Tuned for the two trees the site needs:
//  * ghaf (Prosopis cineraria): short twisted trunk, wide open crown of
//    drooping branchlets, tiny grey-green leaflets, dust on everything;
//  * olive in a planter: gnarled, compact, silvery.
#pragma once
#include "foliage.h"
#include "geometry.h"
#include "materials.h"

namespace zw {

struct TreeSpec {
  V3 base;
  float height = 6.0f;      // overall height
  float trunkR = 0.17f;     // trunk radius at the foot
  float crown = 3.0f;       // crown radius
  V3 lean = V3(0.0f);       // horizontal lean of the whole tree
  uint32_t seed = 1;
  int leafMat = M_LEAF_GHAF;
  float cell = 0.028f;      // leaf cell (sets leaf size): ghaf leaflets are ~1 cm
  float fill = 0.5f;        // leaf density
  int limbs = 5;
  float droop = 0.35f;      // how much branch ends hang
  float crownBase = 0.42f;  // fraction of height where limbs start
  float dcell = 0.2f;       // density grid voxel: smaller for small crowns
  float clumpScale = 1.0f;  // leaf clump radius multiplier
  float sprayBias = 0.0f;   // >0 fills the sprays in (1 = no spray gaps at all)
  float coreClump = 0.0f;   // >0 adds one central crown mass of this radius (x crown)
  float noiseAmt = 1.0f;    // density break-up (see Foliage::noiseAmt)
  // Spray shape (see Foliage): defaults are the olive/scrub look; the ghaf
  // overrides them so its fringe is whole sprays, not scattered leaves.
  float sprayLo = 0.12f, sprayHi = 0.38f, sprayDens = 0.25f, fillBase = 0.4f;
  float leafA = 0.0f, leafB = 0.0f, combN = 0.0f;   // 0 = the species default
};

inline void addTree(Scene& s, const TreeSpec& t) {
  RNG rng(t.seed * 7919u + 13u);
  std::vector<V3> cc;
  std::vector<float> cr;
  // Trunk: three wandering segments to the crown base.
  float hb = t.height * t.crownBase;
  V3 cur = t.base - V3(0.0f, 0.05f, 0.0f);
  V3 dir = normalize(V3(0.0f, 1.0f, 0.0f) + t.lean);
  float r = t.trunkR;
  for (int i = 0; i < 3; i++) {
    V3 wob(rng.next() - 0.5f, 0.0f, rng.next() - 0.5f);
    V3 next = cur + normalize(dir + wob * 0.35f) * (hb / 3.0f);
    s.addCapsule(cur, next, r, M_BARK);
    cur = next;
    r *= 0.88f;
  }
  // Limbs radiate from the crown base, rise, then bend outward and droop.
  // Five crooked segments each (ghaf wood zig-zags), with twigs off every
  // joint and hanging branchlets that carry the leaf sprays.
  for (int k = 0; k < t.limbs; k++) {
    float az = (k + 0.6f * rng.next()) / t.limbs * 2.0f * PI;
    float el = 0.55f + 0.45f * rng.next();
    V3 out(std::cos(az), 0.0f, std::sin(az));
    V3 d = normalize(out * std::cos(el) + V3(0.0f, std::sin(el), 0.0f) + t.lean * 0.6f);
    V3 a = cur + V3(0.0f, (rng.next() - 0.3f) * 0.25f * hb, 0.0f);
    float lr = r * 0.6f;
    float L = t.crown * (0.8f + 0.35f * rng.next());
    float riseBudget = (t.height - hb) * (0.75f + 0.3f * rng.next());
    const int SEG = 5;
    for (int seg = 0; seg < SEG; seg++) {
      V3 kink(rng.next() - 0.5f, (rng.next() - 0.5f) * 0.6f, rng.next() - 0.5f);
      V3 b = a + normalize(d + kink * 0.55f) * (L / SEG);
      if (b.y > t.base.y + hb + riseBudget) b.y = t.base.y + hb + riseBudget;
      s.addCapsule(a, b, lr, M_BARK);
      int twigs = seg < 2 ? 1 : 2;
      for (int tw = 0; tw < twigs; tw++) {
        V3 td = normalize(d + V3(rng.next() - 0.5f, rng.next() * 0.5f - 0.2f, rng.next() - 0.5f) * 1.4f);
        V3 tb = b + td * (L * (0.14f + 0.1f * rng.next()));
        s.addCapsule(b, tb, std::max(0.006f, lr * 0.4f), M_BARK);
        // hanging branchlet
        V3 hb2 = tb + normalize(td * 0.4f - V3(0.0f, 0.9f * t.droop + 0.2f, 0.0f)) * (t.crown * 0.18f);
        s.addCapsule(tb, hb2, std::max(0.004f, lr * 0.2f), M_BARK);
        cc.push_back(0.5f * (tb + hb2));
        cr.push_back(t.clumpScale * t.crown * (0.16f + 0.08f * rng.next()));
      }
      d = normalize(d + out * 0.2f - V3(0.0f, t.droop * 0.3f, 0.0f));
      a = b;
      lr *= 0.72f;
    }
    cc.push_back(a - V3(0.0f, t.droop * t.crown * 0.15f, 0.0f));
    cr.push_back(t.clumpScale * t.crown * (0.3f + 0.1f * rng.next()));
  }
  // A few interior clumps so the crown is not hollow from below.
  for (int k = 0; k < 3; k++) {
    float az = rng.next() * 2.0f * PI;
    cc.push_back(cur + V3(std::cos(az) * t.crown * 0.4f, (t.height - hb) * (0.35f + 0.3f * rng.next()), std::sin(az) * t.crown * 0.4f));
    cr.push_back(t.clumpScale * t.crown * 0.38f);
  }
  if (t.coreClump > 0.0f) {
    cc.push_back(cur + V3(0.0f, (t.height - hb) * 0.5f, 0.0f));
    cr.push_back(t.crown * t.coreClump);
  }
  Foliage f;
  f.noiseAmt = t.noiseAmt;
  f.sprayLo = t.sprayLo; f.sprayHi = t.sprayHi; f.sprayDens = t.sprayDens; f.fillBase = t.fillBase;
  f.cell = t.cell;
  f.dcell = t.dcell;
  f.fill = t.fill;
  f.seed = t.seed;
  f.mat = t.leafMat;
  f.leafA = 0.45f; f.leafB = 0.2f; f.sprayFreq = 5.0f;
  // Olive: cloud-pruned clumps of narrow 4-6 cm leaves; the clumps are the
  // clustering, so sprays are filled in.
  if (t.leafMat == M_LEAF_OLIVE) { f.leafA = 0.46f; f.leafB = 0.12f; f.upBias = 0.25f; f.sprayFreq = 11.0f; }
  f.sprayBias = t.sprayBias;
  if (t.leafA > 0.0f) f.leafA = t.leafA;
  if (t.leafB > 0.0f) f.leafB = t.leafB;
  f.combN = t.combN;
  buildFoliageDensity(f, cc, cr);
  s.addFoliage(f);
}

}  // namespace zw
