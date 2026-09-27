// The approach (a proposal, blocked out for a low-resolution preview of the
// orbit-and-enter move): the film's courtyard house seen from outside and
// above, in a quarter of other courtyard houses. buildHouse() stays exactly as
// the film's sequences use it; this adds what the street and courtyard
// cameras never needed: parapets on the outer roof edges, a stair head, the
// quarter around the house, and a low town beyond it out to where the dust
// haze (the sky's aerial perspective) takes over.
#pragma once
#include "scenes.h"

namespace zw {

inline uint32_t qhash(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
  return x;
}
inline float qrand(uint32_t x) { return (qhash(x) >> 8) / 16777216.0f; }

// What the house lacks from above: parapets on the three outer roof edges
// (the street facade and the courtyard walls already rise past the roof),
// limestone copings, and a stair head with its door facing the courtyard.
inline void completeHouse(Scene& s) {
  const float NZ = -4.45f, WX = -4.45f, SZ = 17.1f, EX = 12.05f;
  const float R = 7.0f, P = 7.55f, t = 0.25f;
  s.addBox(V3(WX, R, NZ), V3(WX + t, P, SZ), M_LIME, FALL, 0.01f);
  s.addBox(V3(WX + t, R, NZ), V3(EX, P, NZ + t), M_LIME, FALL, 0.01f);
  s.addBox(V3(WX + t, R, SZ - t), V3(EX, P, SZ), M_LIME, FALL, 0.01f);
  s.addBox(V3(WX - 0.03f, P, NZ - 0.03f), V3(WX + t + 0.03f, P + 0.06f, SZ + 0.03f), M_STONE, FALL, 0.006f);
  s.addBox(V3(WX + t + 0.03f, P, NZ - 0.03f), V3(EX, P + 0.06f, NZ + t + 0.03f), M_STONE, FALL, 0.006f);
  s.addBox(V3(WX + t + 0.03f, P, SZ - t - 0.03f), V3(EX, P + 0.06f, SZ + 0.03f), M_STONE, FALL, 0.006f);
  int sh = s.addBox(V3(-4.2f, R, 12.2f), V3(-1.6f, 9.7f, 15.2f), M_LIME, FALL, 0.012f);
  s.addCut(sh, V3(-1.8f, R - 0.1f, 13.2f), V3(-1.5f, R + 2.2f, 14.2f));
  s.addBox(V3(-1.72f, R, 13.2f), V3(-1.66f, R + 2.2f, 14.2f), M_TEAK_GREY, FALL, 0.004f);
  s.addBox(V3(-4.23f, 9.7f, 12.17f), V3(-1.57f, 9.76f, 15.23f), M_STONE, FALL, 0.006f);
}

// One house of the quarter: rooms around an open court (set off-centre), a
// parapet ring with its coping, now and then a wind tower on a corner and a
// ghaf in the court.
inline void addQuarterHouse(Scene& s, float x0, float z0, float w, float d, float h, uint32_t seed, bool tree) {
  const int mats[4] = {M_LIME, M_LIME_SHADE, M_LIME, M_RAMMED};
  int mat = mats[qhash(seed) % 4];
  int b = s.addBox(V3(x0, 0.0f, z0), V3(x0 + w, h, z0 + d), mat, FALL, 0.012f, seed);
  float cw = w * (0.38f + 0.14f * qrand(seed + 2)), cd = d * (0.38f + 0.14f * qrand(seed + 3));
  float cx = x0 + (w - cw) * (0.3f + 0.4f * qrand(seed + 4)), cz = z0 + (d - cd) * (0.3f + 0.4f * qrand(seed + 5));
  s.addCut(b, V3(cx, 0.02f, cz), V3(cx + cw, h + 1.0f, cz + cd));
  int p = s.addBox(V3(x0, h, z0), V3(x0 + w, h + 0.5f, z0 + d), mat, FALL, 0.01f, seed + 9);
  s.addCut(p, V3(x0 + 0.25f, h - 0.5f, z0 + 0.25f), V3(x0 + w - 0.25f, h + 1.0f, z0 + d - 0.25f));
  int c = s.addBox(V3(x0 - 0.03f, h + 0.5f, z0 - 0.03f), V3(x0 + w + 0.03f, h + 0.56f, z0 + d + 0.03f), M_STONE, FALL, 0.005f);
  s.addCut(c, V3(x0 + 0.28f, h, z0 + 0.28f), V3(x0 + w - 0.28f, h + 1.0f, z0 + d - 0.28f));
  if (qrand(seed + 6) < 0.3f) {
    float tx = qrand(seed + 7) < 0.5f ? x0 + 0.6f : x0 + w - 3.0f;
    float tz = qrand(seed + 8) < 0.5f ? z0 + 0.6f : z0 + d - 3.0f;
    float th = h + 3.6f + 1.2f * qrand(seed + 10);
    int tw = s.addBox(V3(tx, h, tz), V3(tx + 2.4f, th, tz + 2.4f), M_LIME, FALL, 0.012f);
    for (int k = 0; k < 2; k++) {
      s.addCut(tw, V3(tx + 0.5f + k * 0.9f, th - 2.8f, tz - 0.2f), V3(tx + 0.85f + k * 0.9f, th - 0.5f, tz + 2.6f));
      s.addCut(tw, V3(tx - 0.2f, th - 2.8f, tz + 0.5f + k * 0.9f), V3(tx + 2.6f, th - 0.5f, tz + 0.85f + k * 0.9f));
    }
    s.addBox(V3(tx - 0.05f, th, tz - 0.05f), V3(tx + 2.45f, th + 0.08f, tz + 2.45f), M_STONE, FALL, 0.006f);
  }
  if (tree) {
    TreeSpec g;
    g.base = V3(cx + 0.5f * cw, 0.0f, cz + 0.5f * cd);
    g.height = 5.0f + 2.0f * qrand(seed + 11); g.crown = std::min(0.45f * std::min(cw, cd), 3.2f); g.trunkR = 0.18f;
    g.seed = seed; g.limbs = 6; g.fill = 0.8f; g.droop = 0.5f;
    ghafSprays(g);
    g.cell = 0.05f;   // coarser leaves: these are only ever seen from far off
    addTree(s, g);
  }
}

inline void buildApproach(Scene& s) {
  buildHouse(s);
  completeHouse(s);
  // Ground to the horizon, just under the film scene's own ground boxes.
  s.addBox(V3(-4000.0f, -1.0f, -4000.0f), V3(4000.0f, -0.002f, 4000.0f), M_STREET, FY1, 0.0f);
  // Areas the film scene already dresses (house, street, the far side, the
  // junction): the quarter's plots keep out of them.
  struct Rect { float x0, z0, x1, z1; };
  const Rect keep[3] = {{-12.0f, -48.0f, 50.0f, 42.0f}, {17.0f, 42.0f, 50.0f, 64.0f}, {-22.0f, -48.0f, 57.0f, -38.0f}};
  auto clear = [&](float x0, float z0, float x1, float z1) {
    for (const Rect& k : keep)
      if (x1 > k.x0 && x0 < k.x1 && z1 > k.z0 && z0 < k.z1) return false;
    return true;
  };
  // The quarter: plots on a 26 x 30 m grid, lanes about 6 m wide.
  int trees = 0;
  for (int gx = -6; gx <= 6; gx++)
    for (int gz = -5; gz <= 5; gz++) {
      uint32_t seed = (uint32_t)((gx + 50) * 7919 + (gz + 50) * 104729);
      float w = 14.0f + 6.0f * qrand(seed), d = 16.0f + 8.0f * qrand(seed + 1);
      float x0 = gx * 26.0f + 3.0f + (26.0f - 6.0f - w) * qrand(seed + 12);
      float z0 = gz * 30.0f + 3.0f + (30.0f - 6.0f - d) * qrand(seed + 13);
      if (!clear(x0 - 3.0f, z0 - 3.0f, x0 + w + 3.0f, z0 + d + 3.0f)) continue;
      float h = qrand(seed + 14) < 0.25f ? 4.2f + 0.8f * qrand(seed + 15) : 6.2f + 1.4f * qrand(seed + 15);
      float dist = std::sqrt((x0 + 0.5f * w - 4.0f) * (x0 + 0.5f * w - 4.0f) + (z0 + 0.5f * d - 6.0f) * (z0 + 0.5f * d - 6.0f));
      bool tree = dist < 95.0f && qrand(seed + 16) < 0.4f && trees < 14;
      if (tree) trees++;
      addQuarterHouse(s, x0, z0, w, d, h, seed, tree);
    }
  // Beyond the quarter: a low town of plain masses, sparser with distance,
  // for the haze to swallow.
  for (int gx = -22; gx <= 22; gx++)
    for (int gz = -22; gz <= 22; gz++) {
      float cx = gx * 40.0f, cz = gz * 40.0f;
      if (std::fabs(cx - 4.0f) < 190.0f && std::fabs(cz - 6.0f) < 190.0f) continue;
      uint32_t seed = (uint32_t)((gx + 100) * 6151 + (gz + 100) * 3079);
      if (qrand(seed) < 0.3f) continue;
      float w = 12.0f + 18.0f * qrand(seed + 1), d = 12.0f + 18.0f * qrand(seed + 2);
      float h = qrand(seed + 3) < 0.85f ? 4.0f + 4.0f * qrand(seed + 4) : 10.0f + 12.0f * qrand(seed + 4);
      float x0 = cx + (40.0f - 6.0f - w) * qrand(seed + 5) - 20.0f, z0 = cz + (40.0f - 6.0f - d) * qrand(seed + 6) - 20.0f;
      s.addBox(V3(x0, 0.0f, z0), V3(x0 + w, h, z0 + d), qrand(seed + 7) < 0.5f ? M_LIME : M_LIME_SHADE, FALL, 0.02f, seed);
    }
}

}  // namespace zw
