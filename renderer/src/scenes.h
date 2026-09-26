// Scene builders. All buildings are fictional; dimensions are in metres.
//
// THE HOUSE (the film's courtyard house — "The Ghaf House" archetype)
//   Courtyard (open to sky) ....... X 0..8.4, Z 0..10.8 (+X east, +Z south)
//   South loggia (riwaq) .......... Z 11.25..14.25, behind a rammed-earth wall
//   East wing + street facade ..... X 8.85..13.45, gate + bent passage (dahliz)
//   Street ........................ runs north-south along X = 13.45
// One model serves the street hero, the walk-in and the courtyard day, so
// every camera in sequence B and A sees the same, continuous building.
#pragma once
#include "geometry.h"
#include "materials.h"
#include "objects.h"
#include "patterns.h"
#include "trees.h"

namespace zw {

struct CameraDesc {
  V3 pos, target;
  float hfovDeg = 80.0f;   // horizontal FOV for landscape; portrait uses vfov
  float shiftY = 0.0f;     // vertical lens shift, fraction of half-height (keeps verticals vertical)
  float shiftX = 0.0f;
  bool level = true;       // architectural: pitch 0, verticals parallel
};

// Canopy / screen patterns shared with the real-time shader.
inline PatternParams courtyardStar() {
  PatternParams p; p.type = PAT_STAR8; p.period = 0.36f; p.a = 0.325f; p.b = 0.09f; p.c = 0.007f;
  p.ou = 0.18f; p.ov = 0.18f;
  return p;
}
inline PatternParams screenStar(float period) {
  PatternParams p; p.type = PAT_STAR8; p.period = period; p.a = 0.31f; p.b = 0.10f; p.c = 0.004f;
  return p;
}

// A sagging overhead cable: a parabola (close enough to a catenary at this
// sag/span) built from short capsules.
inline void addCable(Scene& s, V3 a, V3 b, float sag, float r = 0.0085f) {
  const int N = 22;
  V3 prev = a;
  for (int i = 1; i <= N; i++) {
    float t = (float)i / N;
    V3 p = lerp(a, b, t);
    p.y -= sag * 4.0f * t * (1.0f - t);
    s.addCapsule(prev, p, r, M_CABLE);
    prev = p;
  }
}
// A galvanised street lamp: base, tapered shaft, arm reaching over the street.
inline V3 addLampPost(Scene& s, float x, float z, float armDirX, float height = 7.6f) {
  s.addCyl(x, z, 0.14f, 0.0f, 0.38f, M_STEEL);
  s.addCapsule(V3(x, 0.3f, z), V3(x, 3.9f, z), 0.078f, M_STEEL);
  s.addCapsule(V3(x, 3.9f, z), V3(x, height, z), 0.062f, M_STEEL);
  V3 armEnd(x + armDirX * 1.25f, height + 0.28f, z);
  s.addCapsule(V3(x, height - 0.1f, z), armEnd, 0.042f, M_STEEL);
  s.addSdf(SDF_LAMPHEAD, armEnd - V3(0.0f, 0.06f, 0.0f), armDirX < 0 ? 180.0f : 0.0f, 1.0f, {M_STEEL, M_LENS});
  return V3(x, height - 0.35f, z);   // cable anchor
}
// Run-off under an opening or spout on a wall face (axis: wall normal axis).
inline void addStain(Scene& s, int axis, float plane, float u0, float u1, float yTop, float len, V3 tint, float strength) {
  Stain st;
  st.axis = axis; st.plane = plane; st.u0 = u0; st.u1 = u1; st.yTop = yTop; st.len = len; st.tint = tint;
  st.strength = strength; st.seed = (uint32_t)s.stains.size() * 2654435761u + 7u;
  s.stains.push_back(st);
}

inline void buildHouse(Scene& s) {
  s.groundMat = -1;
  const float H = 7.4f;     // parapet top
  const float T = 0.45f;    // wall thickness
  const float CW = 7.2f;    // courtyard width  (X)
  const float CL = 13.2f;   // courtyard length (Z)
  const float CX = 0.5f * CW;
  const float HC = 5.2f;    // canopy soffit height
  const float LD = 3.0f;    // loggia depth
  const float SZ = CL + T;  // south wall outer face / loggia start
  const float BZ = SZ + LD; // loggia back wall inner face

  // ---- ground -------------------------------------------------------------
  int fl = s.addBox(V3(-4.45f, -0.6f, -4.45f), V3(CW + T + 4.0f + T, 0.0f, BZ + T), M_STONE_FLOOR, FY1, 0.0f);
  // rill and pool cut into the courtyard floor
  const float RZ0 = 0.9f, PZ0 = 6.0f, PZ1 = 7.4f, RZ1 = CL - 0.6f;
  s.addCut(fl, V3(CX - 0.15f, -0.09f, RZ0), V3(CX + 0.15f, 0.05f, PZ0));
  s.addCut(fl, V3(CX - 0.15f, -0.09f, PZ1), V3(CX + 0.15f, 0.05f, RZ1));
  s.addCut(fl, V3(CX - 0.745f, -0.30f, PZ0 - 0.045f), V3(CX + 0.745f, 0.05f, PZ1 + 0.045f));
  s.prims[fl].bevel = 0.004f;
  s.prims[fl].faces = FALL;
  // street and surroundings
  // Street and surroundings (the street runs north-south, east of the house).
  s.addBox(V3(CW + T + 4.0f + T, -0.6f, -120.0f), V3(160.0f, 0.0f, 120.0f), M_STREET, FY1, 0.0f);
  s.addBox(V3(-120.0f, -0.6f, -120.0f), V3(CW + T + 4.0f + T, 0.0f, -4.45f), M_STREET, FY1, 0.0f);
  s.addBox(V3(-120.0f, -0.6f, BZ + T), V3(CW + T + 4.0f + T, 0.0f, 120.0f), M_STREET, FY1, 0.0f);
  s.addBox(V3(-120.0f, -0.6f, -4.45f), V3(-4.45f, 0.0f, BZ + T), M_STREET, FY1, 0.0f);

  // water
  s.addQuadY(-0.022f, CX - 0.15f, CX + 0.15f, RZ0, PZ0, M_WATER, F_WATER);
  s.addQuadY(-0.022f, CX - 0.15f, CX + 0.15f, PZ1, RZ1, M_WATER, F_WATER);
  s.addQuadY(-0.035f, CX - 0.7f, CX + 0.7f, PZ0, PZ1, M_WATER, F_WATER);
  // Copper-lined pool: the site's single accent colour, in the building.
  // Years under water give the lining an even, deep patina (reads as depth);
  // the coping ring at the floor is weathered verdigris copper, worn where feet go.
  s.addBox(V3(CX - 0.7f, -0.30f, PZ0), V3(CX + 0.7f, -0.285f, PZ1), M_COPPER_DEEP, FALL, 0.002f);
  s.addBox(V3(CX - 0.7f, -0.30f, PZ0), V3(CX - 0.685f, -0.04f, PZ1), M_COPPER_DEEP, FALL, 0.002f);
  s.addBox(V3(CX + 0.685f, -0.30f, PZ0), V3(CX + 0.7f, -0.04f, PZ1), M_COPPER_DEEP, FALL, 0.002f);
  s.addBox(V3(CX - 0.685f, -0.30f, PZ0), V3(CX + 0.685f, -0.04f, PZ0 + 0.015f), M_COPPER_DEEP, FALL, 0.002f);
  s.addBox(V3(CX - 0.685f, -0.30f, PZ1 - 0.015f), V3(CX + 0.685f, -0.04f, PZ1), M_COPPER_DEEP, FALL, 0.002f);
  const float CPW = 0.045f;   // coping width
  s.addBox(V3(CX - 0.7f - CPW, -0.04f, PZ0 - CPW), V3(CX + 0.7f + CPW, 0.003f, PZ0), M_COPPER, FALL, 0.004f);
  s.addBox(V3(CX - 0.7f - CPW, -0.04f, PZ1), V3(CX + 0.7f + CPW, 0.003f, PZ1 + CPW), M_COPPER, FALL, 0.004f);
  s.addBox(V3(CX - 0.7f - CPW, -0.04f, PZ0), V3(CX - 0.7f, 0.003f, PZ1), M_COPPER, (uint8_t)(FALL & ~FZ0 & ~FZ1), 0.004f);
  s.addBox(V3(CX + 0.7f, -0.04f, PZ0), V3(CX + 0.7f + CPW, 0.003f, PZ1), M_COPPER, (uint8_t)(FALL & ~FZ0 & ~FZ1), 0.004f);

  // ---- courtyard walls ------------------------------------------------------
  // North (behind the film camera), lime. Door to the north wing.
  int wn = s.addBox(V3(-T, 0.0f, -T), V3(CW + T, H, 0.0f), M_LIME);

  // South: rammed earth, loggia arcade below, carved screen above.
  int ws = s.addBox(V3(-T, 0.0f, CL), V3(CW + T, H, SZ), M_RAMMED, FALL, 0.010f);
  const float OW = 1.7f, pier = (CW - 3.0f * OW) / 4.0f;
  for (int k = 0; k < 3; k++) {
    float x0 = pier + k * (OW + pier);
    s.addCut(ws, V3(x0, -1.0f, CL - 0.1f), V3(x0 + OW, 2.9f, SZ + 0.1f));
    // teak lintel, recessed 5 mm into its own pocket
    s.addCut(ws, V3(x0 - 0.2f, 2.9f, CL - 0.1f), V3(x0 + OW + 0.2f, 3.12f, SZ + 0.1f));
    s.addBox(V3(x0 - 0.2f, 2.9f, CL + 0.005f), V3(x0 + OW + 0.2f, 3.12f, SZ - 0.005f), M_TEAK, FALL, 0.006f);
  }
  const float SX0 = 1.3f, SX1 = CW - 1.3f;
  s.addCut(ws, V3(SX0, 4.3f, CL - 0.1f), V3(SX1, 6.1f, SZ + 0.1f));
  s.addLattice(V3(SX0, 4.3f, CL + 0.17f), V3(SX1, 6.1f, CL + 0.28f), 2, screenStar(0.26f), M_TEAK, 0.004f);
  s.addBox(V3(SX0, 4.3f, CL + 0.15f), V3(SX1, 4.36f, CL + 0.30f), M_TEAK, FALL, 0.004f);
  s.addBox(V3(SX0, 6.04f, CL + 0.15f), V3(SX1, 6.1f, CL + 0.30f), M_TEAK, FALL, 0.004f);

  // Loggia: ceiling, back wall with a teak double door, upper mass with the
  // room that glows behind the screen.
  int up = s.addBox(V3(-T, 3.5f, SZ), V3(CW + T, H - 0.4f, BZ + T), M_LIME_SHADE, FALL, 0.01f);
  s.addCut(up, V3(SX0 - 0.2f, 4.1f, SZ - 0.05f), V3(SX1 + 0.2f, 6.3f, SZ + 1.2f));
  int back = s.addBox(V3(-T, 0.0f, BZ), V3(CW + T, 3.5f, BZ + T), M_LIME_SHADE);
  s.addCut(back, V3(CX - 0.65f, -1.0f, BZ - 0.05f), V3(CX + 0.65f, 2.7f, BZ + 0.11f));
  s.addBox(V3(CX - 0.65f, 0.0f, BZ + 0.09f), V3(CX, 2.7f, BZ + 0.15f), M_TEAK, FALL, 0.004f);
  s.addBox(V3(CX, 0.0f, BZ + 0.09f), V3(CX + 0.65f, 2.7f, BZ + 0.15f), M_TEAK, FALL, 0.004f);
  // loggia side walls
  s.addBox(V3(-T, 0.0f, SZ), V3(0.0f, 3.5f, BZ), M_LIME_SHADE);
  s.addBox(V3(CW, 0.0f, SZ), V3(CW + T, 3.5f, BZ), M_LIME_SHADE);

  // East wall (film left): passage doorway from the gate, a recessed teak door, an upper screen.
  int we = s.addBox(V3(CW, 0.0f, -T), V3(CW + T, H, SZ), M_LIME);
  s.addCut(we, V3(CW - 0.1f, -1.0f, 8.6f), V3(CW + 0.22f, 2.5f, 9.8f));
  s.addBox(V3(CW + 0.2f, 0.0f, 8.6f), V3(CW + 0.28f, 2.5f, 9.8f), M_TEAK, FALL, 0.004f);
  s.addCut(we, V3(CW - 0.1f, 4.9f, 11.0f), V3(CW + T + 0.1f, 6.4f, 12.0f));
  s.addLattice(V3(CW + 0.18f, 4.9f, 11.0f), V3(CW + 0.28f, 6.4f, 12.0f), 0, screenStar(0.2f), M_TEAK, 0.003f);

  // West wall (film right): copper door in a deep recess, two small screened windows.
  int ww = s.addBox(V3(-T, 0.0f, -T), V3(0.0f, H, SZ), M_LIME);
  s.addCut(ww, V3(-0.3f, -1.0f, 8.4f), V3(0.1f, 2.9f, 10.0f));
  s.addBox(V3(-0.36f, 0.0f, 8.4f), V3(-0.3f, 2.9f, 10.0f), M_COPPER, FALL, 0.003f);
  for (float z0 : {3.0f, 11.2f}) {
    s.addCut(ww, V3(-0.6f, 5.6f, z0), V3(0.1f, 6.8f, z0 + 0.8f));
    s.addLattice(V3(-0.3f, 5.6f, z0), V3(-0.2f, 6.8f, z0 + 0.8f), 0, screenStar(0.16f), M_TEAK, 0.003f);
  }

  // Limestone copings (N/S run full width, E/W stop against them: no coplanar overlap).
  s.addBox(V3(-T - 0.03f, H, -T - 0.03f), V3(CW + T + 0.03f, H + 0.07f, 0.03f), M_STONE, FALL, 0.008f);
  s.addBox(V3(-T - 0.03f, H, CL - 0.03f), V3(CW + T + 0.03f, H + 0.07f, SZ + 0.03f), M_STONE, FALL, 0.008f);
  s.addBox(V3(CW - 0.03f, H, 0.03f), V3(CW + T + 0.03f, H + 0.07f, CL - 0.03f), M_STONE, (uint8_t)(FALL & ~FZ0 & ~FZ1), 0.008f);
  s.addBox(V3(-T - 0.03f, H, 0.03f), V3(0.03f, H + 0.07f, CL - 0.03f), M_STONE, (uint8_t)(FALL & ~FZ0 & ~FZ1), 0.008f);

  // ---- canopy -----------------------------------------------------------------
  const float KZ0 = 3.0f, KZ1 = 10.0f;
  // 5 cm GRC lattice: thin enough that a 27-degree evening sun still passes
  // (a 12 cm slab blocks it completely — the ray drifts 23 cm sideways through
  // the depth and the holes are 20 cm).
  s.addLattice(V3(0.0f, HC, KZ0), V3(CW, HC + 0.05f, KZ1), 1, courtyardStar(), M_STONE, 0.004f);
  for (int k = 0; k < 4; k++) {
    float z = KZ0 + 0.07f + k * (KZ1 - KZ0 - 0.14f) / 3.0f;
    s.addBox(V3(-0.2f, HC - 0.3f, z - 0.07f), V3(CW + 0.2f, HC, z + 0.07f), M_TEAK, FALL, 0.006f);
  }
  s.addBox(V3(0.0f, HC - 0.05f, KZ0 - 0.04f), V3(CW, HC + 0.09f, KZ0 + 0.04f), M_TEAK, FALL, 0.004f);
  s.addBox(V3(0.0f, HC - 0.05f, KZ1 - 0.04f), V3(CW, HC + 0.09f, KZ1 + 0.04f), M_TEAK, FALL, 0.004f);

  // Copper waterspouts (marzam) through the courtyard parapets.
  for (float z : {2.2f, 7.8f, 12.1f}) {
    s.addBox(V3(-0.1f, H - 0.62f, z - 0.07f), V3(0.62f, H - 0.5f, z + 0.07f), M_COPPER, FALL, 0.01f);
    s.addBox(V3(CW - 0.62f, H - 0.62f, z + 0.5f), V3(CW + 0.1f, H - 0.5f, z + 0.64f), M_COPPER, FALL, 0.01f);
  }
  // Lime plinth: a 30 cm skirting, 15 mm proud, casts the thin shadow line
  // every real plastered wall has at its foot.
  s.addBox(V3(0.0f, 0.0f, 0.0f), V3(0.015f, 0.3f, 8.4f), M_LIME_SHADE, (uint8_t)(FALL & ~FX0), 0.004f);
  s.addBox(V3(0.0f, 0.0f, 10.0f), V3(0.015f, 0.3f, CL), M_LIME_SHADE, (uint8_t)(FALL & ~FX0), 0.004f);
  s.addBox(V3(CW - 0.015f, 0.0f, 0.0f), V3(CW, 0.3f, 8.6f), M_LIME_SHADE, (uint8_t)(FALL & ~FX1), 0.004f);
  s.addBox(V3(CW - 0.015f, 0.0f, 9.8f), V3(CW, 0.3f, CL), M_LIME_SHADE, (uint8_t)(FALL & ~FX1), 0.004f);

  // ---- furniture and craft at human scale ------------------------------------
  s.addBox(V3(0.02f, 0.38f, 4.4f), V3(0.52f, 0.45f, 6.6f), M_TEAK, FALL, 0.006f);         // bench seat
  s.addBox(V3(0.06f, 0.0f, 4.52f), V3(0.48f, 0.38f, 4.82f), M_LIME, FALL, 0.01f);          // bench plinths
  s.addBox(V3(0.06f, 0.0f, 6.18f), V3(0.48f, 0.38f, 6.48f), M_LIME, FALL, 0.01f);
  s.addBox(V3(CW - 0.75f, 0.0f, 11.6f), V3(CW - 0.05f, 0.12f, 12.3f), M_STONE, FALL, 0.01f);  // jar plinth
  // On the bench: a folded palm mat and a linen cushion.
  s.addSdf(SDF_MAT, V3(0.27f, 0.45f, 5.35f), 90.0f, 1.0f, {M_PALM}, {0.28f, 0.19f});
  s.addSdf(SDF_CUSHION, V3(0.27f, 0.45f, 4.85f), 4.0f, 1.0f, {M_LINEN_NATURAL}, {0.2f, 0.19f, 0.1f});
  // A teak chair turned toward the pool.
  {
    const float x0 = 1.05f, z0 = 6.95f, sw = 0.46f, sd = 0.44f;
    for (float lx : {x0, x0 + sw - 0.04f})
      for (float lz : {z0, z0 + sd - 0.04f}) s.addBox(V3(lx, 0.0f, lz), V3(lx + 0.04f, 0.43f, lz + 0.04f), M_TEAK, FALL, 0.004f);
    s.addBox(V3(x0 - 0.01f, 0.43f, z0 - 0.01f), V3(x0 + sw + 0.01f, 0.465f, z0 + sd + 0.01f), M_TEAK, FALL, 0.005f);
    for (float lz : {z0, z0 + sd - 0.04f}) s.addBox(V3(x0, 0.465f, lz), V3(x0 + 0.04f, 0.88f, lz + 0.04f), M_TEAK, FALL, 0.004f);
    for (float yy : {0.6f, 0.72f, 0.84f}) s.addBox(V3(x0 - 0.005f, yy, z0), V3(x0 + 0.035f, yy + 0.05f, z0 + sd), M_TEAK, FALL, 0.004f);
  }
  // Olive trees in terracotta planters, one in front of each inner pier.
  for (float px : {2.49f, 4.71f}) {
    s.addSdf(SDF_PLANTER, V3(px, 0.0f, CL - 0.62f), 0.0f, 0.95f, {M_TERRACOTTA, M_SOIL});
    TreeSpec ol;
    ol.base = V3(px, 0.74f, CL - 0.62f); ol.height = 2.05f; ol.crown = 0.8f; ol.trunkR = 0.05f;
    ol.leafMat = M_LEAF_OLIVE; ol.cell = 0.048f; ol.fill = 1.0f; ol.limbs = 6; ol.droop = 0.1f; ol.dcell = 0.07f;
    ol.clumpScale = 1.3f; ol.sprayBias = 0.6f; ol.coreClump = 0.9f; ol.noiseAmt = 0.55f;
    ol.crownBase = 0.45f; ol.seed = px > 3.0f ? 71u : 53u; ol.lean = V3(px > 3.0f ? 0.08f : -0.08f, 0.0f, -0.05f);
    addTree(s, ol);
  }
  // Loggia: floor cushions either side of the door, a rolled rug, a brass tray table.
  s.addSdf(SDF_CUSHION, V3(1.25f, 0.0f, BZ - 0.42f), 0.0f, 1.0f, {M_LINEN_INDIGO}, {0.7f, 0.32f, 0.14f});
  s.addSdf(SDF_CUSHION, V3(5.95f, 0.0f, BZ - 0.42f), 0.0f, 1.0f, {M_LINEN_OCHRE}, {0.7f, 0.32f, 0.14f});
  s.addSdf(SDF_CUSHION, V3(1.25f, 0.14f, BZ - 0.42f), 3.0f, 1.0f, {M_LINEN_NATURAL}, {0.62f, 0.28f, 0.1f});
  s.addCapsule(V3(0.25f, 0.09f, SZ + 0.55f), V3(0.25f, 0.09f, SZ + 2.1f), 0.085f, M_LINEN_OCHRE);
  s.addCyl(CX + 1.6f, BZ - 1.1f, 0.34f, 0.26f, 0.285f, M_BRASS);
  s.addCyl(CX + 1.6f, BZ - 1.1f, 0.12f, 0.0f, 0.26f, M_TEAK);
  s.addSdf(SDF_JAR, V3(CW - 0.4f, 0.12f, 11.95f), 20.0f, 1.0f, {M_TERRACOTTA});
  // Pierced brass lantern on a rod from the lattice, south of the pool: at
  // noon the sun comes straight down through its pierced dome and open
  // bottom and prints a disc of small stars on the stone.
  {
    const float LSC = 1.5f, LZ = 8.8f, LBOT = 3.1f, LLOOP = 0.524f;   // loop top, local units
    s.addSdf(SDF_LANTERN, V3(CX, LBOT, LZ), 0.0f, LSC, {M_BRASS, M_LENS});
    s.addCapsule(V3(CX, LBOT + LLOOP * LSC - 0.004f, LZ), V3(CX, HC - 0.02f, LZ), 0.006f, M_BRONZE);
    s.addCyl(CX, LZ, 0.045f, HC - 0.025f, HC, M_BRONZE);                                 // ceiling rose
  }

  // ---- north wing, gate, street facade, wind tower ---------------------------------
  // The street runs north-south along the EAST facade. At 07:30 on 21 June the
  // sun (az 74, alt 24) strikes that limewash almost head-on — the blazing
  // wall of the hero — while the wall across the street throws a shadow band.
  // The gate opens into a bent passage (dahliz): west through the north wing,
  // then a turn south, in darkness, onto the courtyard's axis. The camera's
  // one hidden cut-free turn happens where nothing can be seen.
  const float EX = CW + T + 4.0f;    // east facade inner face (wing depth 4 m)
  const float EF = EX + T;           // east facade street face
  const float NZ = -4.45f;           // north wing outer (north) face
  const float GZ0 = -3.3f, GZ1 = -1.6f;  // passage E-W leg, Z range
  // North wing mass: E-W passage leg + N-S leg onto the courtyard axis.
  int nw = s.addBox(V3(-4.45f, 0.0f, NZ), V3(EX, 7.0f, -T), M_LIME_SHADE, FALL, 0.01f);
  s.addCut(nw, V3(CX - 0.7f, -1.0f, GZ0), V3(EX + 0.1f, 3.0f, GZ1));
  s.addCut(nw, V3(CX - 0.7f, -1.0f, GZ0), V3(CX + 0.7f, 3.0f, -T + 0.1f));
  s.addCut(wn, V3(CX - 0.7f, -1.0f, -T - 0.1f), V3(CX + 0.7f, 3.0f, 0.1f));
  // East wing mass (courtyard side rooms).
  s.addBox(V3(CW + T, 0.0f, -T), V3(EX, 7.0f, BZ + T), M_LIME_SHADE, FALL, 0.01f);
  // Street facade, full length, with a deep gate recess.
  int fac = s.addBox(V3(EX, 0.0f, NZ - 16.0f), V3(EF, H, BZ + T), M_LIME, FALL, 0.012f);
  const float GC = 0.5f * (GZ0 + GZ1);
  s.addCut(fac, V3(EX - 0.1f, -1.0f, GC - 1.1f), V3(EF + 0.1f, 3.8f, GC + 1.1f));
  s.addCut(nw, V3(EX - 1.25f, -1.0f, GC - 1.1f), V3(EX + 0.1f, 3.8f, GC + 1.1f));
  // open teak door leaves folded against the recess returns
  s.addBox(V3(EX - 1.2f, 0.0f, GC - 1.08f), V3(EX - 0.2f, 2.95f, GC - 1.02f), M_TEAK, FALL, 0.004f);
  s.addBox(V3(EX - 1.2f, 0.0f, GC + 1.02f), V3(EX - 0.2f, 2.95f, GC + 1.08f), M_TEAK, FALL, 0.004f);
  s.addSphere(V3(EX - 0.15f, 3.35f, GC + 0.85f), 0.09f, M_BRASS);
  // A pierced brass lantern on a bracket beside the gate. At 07:30 the low
  // sun passes clean through its drum and prints the star pattern, doubled
  // by the front and back walls, onto the facade behind it.
  {
    const float LZ = GC + 1.65f, AY = 3.05f, AX = EF + 0.34f;
    s.addBox(V3(EF, AY - 0.3f, LZ - 0.05f), V3(EF + 0.012f, AY + 0.06f, LZ + 0.05f), M_BRONZE, FALL, 0.003f);   // wall plate
    s.addCapsule(V3(EF + 0.01f, AY, LZ), V3(AX + 0.02f, AY, LZ), 0.009f, M_BRONZE);                           // arm
    s.addCapsule(V3(EF + 0.01f, AY - 0.26f, LZ), V3(EF + 0.22f, AY - 0.005f, LZ), 0.007f, M_BRONZE);          // brace
    s.addSdf(SDF_LANTERN, V3(AX, AY - 0.524f + 0.006f, LZ), 15.0f, 1.0f, {M_BRASS, M_LENS});
  }
  // plaster bench (dakka) along the facade, both sides of the gate
  s.addBox(V3(EF, 0.0f, NZ - 14.0f), V3(EF + 0.46f, 0.48f, GC - 1.6f), M_LIME, (uint8_t)(FALL & ~FX0), 0.03f);
  s.addBox(V3(EF, 0.0f, GC + 1.6f), V3(EF + 0.46f, 0.48f, BZ - 0.5f), M_LIME, (uint8_t)(FALL & ~FX0), 0.03f);
  // high windows with screens
  for (float zc : {-12.5f, -7.0f, 2.2f, 5.6f, 9.0f, 12.4f}) {
    s.addCut(fac, V3(EX - 0.1f, 4.7f, zc - 0.36f), V3(EF + 0.1f, 5.85f, zc + 0.36f));
    s.addLattice(V3(EF - 0.18f, 4.7f, zc - 0.36f), V3(EF - 0.08f, 5.85f, zc + 0.36f), 0, screenStar(0.18f), M_TEAK, 0.003f);
    s.addBox(V3(EX - 0.9f, 4.5f, zc - 0.6f), V3(EX - 0.7f, 6.1f, zc + 0.6f), M_DARKROOM, FALL, 0.0f);
  }
  s.addBox(V3(EX - 0.03f, H, NZ - 16.03f), V3(EF + 0.03f, H + 0.07f, BZ + T + 0.03f), M_STONE, FALL, 0.008f);
  // copper waterspouts (marzam) through the parapet — the accent at street scale
  for (float zc : {-9.5f, 0.3f, 7.3f, 14.0f})
    s.addBox(V3(EF - 0.2f, H - 0.55f, zc - 0.07f), V3(EF + 0.55f, H - 0.43f, zc + 0.07f), M_COPPER, FALL, 0.01f);
  // North of the house the facade line continues as a neighbour, a little lower.
  s.addBox(V3(-4.45f, 0.0f, NZ - 16.0f), V3(EX, 6.3f, NZ), M_LIME_SHADE, FALL, 0.01f);

  // Wind tower (barjeel) rising from the north wing.
  const float TX = CW - 1.2f, TZ = NZ + 0.5f;
  int tw = s.addBox(V3(TX, 7.0f, TZ), V3(TX + 3.0f, 12.9f, TZ + 3.0f), M_LIME, FALL, 0.012f);
  s.addCut(tw, V3(TX + 0.3f, 7.2f, TZ + 0.3f), V3(TX + 2.7f, 12.6f, TZ + 2.7f));
  for (int k = 0; k < 3; k++) {
    float c = TZ + 0.62f + k * 0.88f;
    s.addCut(tw, V3(TX - 0.2f, 8.9f, c), V3(TX + 3.2f, 12.2f, c + 0.38f));
    float cx = TX + 0.62f + k * 0.88f;
    s.addCut(tw, V3(cx, 8.9f, TZ - 0.2f), V3(cx + 0.38f, 12.2f, TZ + 3.2f));
  }
  s.addBox(V3(TX - 0.06f, 12.9f, TZ - 0.06f), V3(TX + 3.06f, 13.0f, TZ + 3.06f), M_STONE, FALL, 0.008f);
  for (float y : {9.2f, 11.6f})
    s.addBox(V3(TX - 0.3f, y, TZ + 1.46f), V3(TX + 3.3f, y + 0.08f, TZ + 1.54f), M_TEAK_GREY, FALL, 0.01f);

  // West wing (mass).
  s.addBox(V3(-4.45f, 0.0f, -T), V3(-T, 7.0f, BZ + T), M_LIME_SHADE, FALL, 0.01f);
  // South neighbour continuing the street wall.
  s.addBox(V3(-4.45f, 0.0f, BZ + T), V3(EF - 0.3f, 5.6f, BZ + 22.0f), M_LIME, FALL, 0.012f);

  // East side of the street, 7.2 m across: a low garden wall, and behind it a
  // stepped run of neighbours so the skyline is not a flat line.
  const float SW = EF + 7.2f;        // street west face of the far side
  s.addBox(V3(SW, 0.0f, -34.0f), V3(SW + 0.35f, 1.9f, 60.0f), M_LIME_SHADE, FALL, 0.015f);
  s.addBox(V3(SW - 0.03f, 1.9f, -34.0f), V3(SW + 0.38f, 1.96f, 60.0f), M_STONE, FALL, 0.006f);
  float z = -60.0f;
  const float hts[9] = {5.2f, 6.8f, 4.1f, 6.1f, 3.4f, 7.0f, 5.6f, 4.6f, 6.4f};
  const float wds[9] = {14.0f, 9.0f, 12.0f, 10.0f, 11.0f, 13.0f, 9.0f, 12.0f, 30.0f};
  for (int k = 0; k < 9; k++) {
    float set = 11.0f + (k % 3) * 2.5f;
    int b = s.addBox(V3(SW + set, 0.0f, z), V3(SW + set + 12.0f, hts[k], z + wds[k]), k % 2 ? M_LIME : M_LIME_SHADE, FALL, 0.012f);
    s.addBox(V3(SW + set - 0.03f, hts[k], z - 0.03f), V3(SW + set + 12.03f, hts[k] + 0.06f, z + wds[k] + 0.03f), M_STONE, FALL, 0.006f);
    // small high windows facing the street
    for (float wz = z + 2.0f; wz < z + wds[k] - 2.0f; wz += 3.4f)
      if (hts[k] > 4.5f) s.addCut(b, V3(SW + set - 0.2f, hts[k] - 2.2f, wz), V3(SW + set + 0.25f, hts[k] - 1.3f, wz + 0.6f));
    z += wds[k];
  }
  // a garden gate in the low wall, and the canopy of a neighbour's ghaf showing over it is left
  // for the foliage pass (Phase 2).
  // a second wind tower across the street for the skyline
  const float TW2 = SW + 14.0f;
  int tw2 = s.addBox(V3(TW2, 5.0f, -16.0f), V3(TW2 + 2.4f, 11.2f, -13.6f), M_LIME_SHADE, FALL, 0.012f);
  for (int k = 0; k < 2; k++) {
    s.addCut(tw2, V3(TW2 + 0.55f + k * 0.9f, 8.2f, -16.2f), V3(TW2 + 0.9f + k * 0.9f, 10.6f, -13.4f));
    s.addCut(tw2, V3(TW2 - 0.2f, 8.2f, -16.0f + 0.55f + k * 0.9f), V3(TW2 + 2.6f, 10.6f, -16.0f + 0.9f + k * 0.9f));
  }
  // ================= street dressing (realism pass) =================
  // The vista: past the house the west side continues as a neighbour, then
  // the street tees into a cross street; across it, a two-storey house in
  // morning shade closes the view. A lit tree stands in front of it.
  const float SJ = -34.0f;            // junction (north end of our street)
  int wnb = s.addBox(V3(EX - 3.0f, 0.0f, SJ), V3(EF + 0.2f, 6.0f, NZ - 16.0f), M_LIME_SHADE, FALL, 0.012f);
  s.addCut(wnb, V3(EF - 0.3f, -1.0f, -27.6f), V3(EF + 0.4f, 2.5f, -26.4f));          // a neighbour's door recess
  s.addBox(V3(EF - 0.26f, 0.0f, -27.6f), V3(EF - 0.2f, 2.5f, -26.4f), M_TEAK_GREY, FALL, 0.004f);
  for (float zc : {-31.0f, -23.2f})
    s.addCut(wnb, V3(EF - 0.3f, 3.6f, zc - 0.35f), V3(EF + 0.4f, 4.6f, zc + 0.35f));
  s.addBox(V3(EF - 0.03f, 6.0f, SJ - 0.03f), V3(EF + 0.23f, 6.06f, NZ - 16.0f + 0.03f), M_STONE, FALL, 0.006f);
  // Far side of the cross street: 7 m across, facing south, in shade at 07:30.
  const float FZ = SJ - 7.0f;
  int far = s.addBox(V3(-20.0f, 0.0f, FZ - 3.0f), V3(55.0f, 6.6f, FZ), M_LIME, FALL, 0.012f);
  s.addCut(far, V3(14.6f, -1.0f, FZ - 0.4f), V3(16.4f, 3.2f, FZ + 0.1f));               // gate on our axis, recessed
  s.addBox(V3(14.6f, 0.0f, FZ - 0.42f), V3(16.4f, 3.2f, FZ - 0.36f), M_TEAK, FALL, 0.004f);
  for (float xc : {8.0f, 11.2f, 19.8f, 23.0f, 27.5f})
    s.addCut(far, V3(xc - 0.4f, 3.9f, FZ - 0.3f), V3(xc + 0.4f, 5.1f, FZ + 0.1f));
  s.addBox(V3(-20.0f, 6.6f, FZ - 3.03f), V3(55.0f, 6.67f, FZ + 0.03f), M_STONE, FALL, 0.006f);
  // A corner house on the east side of the junction, behind its own wall.
  s.addBox(V3(SW, 0.0f, SJ - 0.4f), V3(SW + 9.0f, 5.2f, SJ), M_LIME_SHADE, FALL, 0.012f);

  // Street lamps on the east side; arms reach west over the street.
  V3 lampA = addLampPost(s, SW - 0.35f, -1.2f, -1.0f);
  V3 lampB = addLampPost(s, SW - 0.35f, -22.0f, -1.0f);
  // Overhead cables: from a bracket on the house parapet across the street
  // to lamp A, along the street between the lamps, and across to the neighbour.
  V3 bracket(EF + 0.12f, 6.75f, -7.2f);
  s.addBox(V3(EF, 6.6f, -7.26f), V3(EF + 0.16f, 6.8f, -7.14f), M_STEEL, FALL, 0.004f);
  addCable(s, bracket, lampA + V3(0.0f, 0.1f, 0.0f), 0.35f);
  addCable(s, bracket + V3(0.0f, -0.12f, 0.0f), lampA + V3(0.0f, -0.05f, 0.0f), 0.42f);
  addCable(s, lampA, lampB, 0.7f);
  addCable(s, lampA + V3(0.0f, -0.15f, 0.0f), lampB + V3(0.0f, -0.15f, 0.0f), 0.85f);
  addCable(s, lampB, V3(EF + 0.3f, 5.7f, -29.5f), 0.4f);

  // Parked cars: one under a fitted dust cover against the garden wall (the
  // summer habit), one white saloon further up on the west side, nose to us.
  s.addSdf(SDF_CAR_COVER, V3(SW - 1.15f, 0.0f, -10.2f), -90.0f, 1.0f, {M_FABRIC_COVER, M_FABRIC_COVER, M_RUBBER, M_FABRIC_COVER});
  s.addSdf(SDF_CAR, V3(EF + 1.35f, 0.0f, -25.2f), 90.0f, 1.0f, {M_CARPAINT, M_GLASS, M_RUBBER, M_CARLAMP});
  // Utility cabinet against the garden wall.
  s.addBox(V3(SW - 0.34f, 0.0f, -15.6f), V3(SW, 1.25f, -14.9f), M_STEEL, FALL, 0.01f);

  // Dusty ghaf trees: one leaning over the garden wall into the street, one
  // in the cross street in front of the shaded house, a smaller one in a
  // raised bed at the corner.
  TreeSpec ta; ta.base = V3(SW + 2.3f, 0.0f, -6.2f); ta.height = 6.8f; ta.crown = 3.4f; ta.trunkR = 0.2f;
  ta.lean = V3(-0.35f, 0.0f, 0.05f); ta.seed = 11; ta.limbs = 7; ta.fill = 0.85f; ta.droop = 0.5f;
  addTree(s, ta);
  TreeSpec tb; tb.base = V3(22.5f, 0.0f, FZ + 3.2f); tb.height = 7.4f; tb.crown = 3.7f; tb.trunkR = 0.22f;
  tb.lean = V3(-0.1f, 0.0f, 0.1f); tb.seed = 23; tb.limbs = 7; tb.fill = 0.85f; tb.droop = 0.5f;
  addTree(s, tb);
  s.addBox(V3(EF + 0.2f, 0.0f, SJ + 0.6f), V3(EF + 1.6f, 0.45f, SJ + 2.4f), M_LIME, FALL, 0.02f);
  TreeSpec tc; tc.base = V3(EF + 0.9f, 0.45f, SJ + 1.5f); tc.height = 5.0f; tc.crown = 2.3f; tc.trunkR = 0.13f;
  tc.lean = V3(0.15f, 0.0f, 0.0f); tc.seed = 37; tc.limbs = 5; tc.fill = 0.85f; tc.droop = 0.45f;
  addTree(s, tc);

  // Run-off: green copper wash under every spout, grey water marks under sills.
  for (float zc : {-9.5f, 0.3f, 7.3f, 14.0f})
    addStain(s, 0, EF, zc - 0.2f, zc + 0.2f, H - 0.45f, 3.8f, V3(0.58f, 0.74f, 0.66f), 1.0f);
  for (float zc : {-12.5f, -7.0f, 2.2f, 5.6f, 9.0f, 12.4f})
    addStain(s, 0, EF, zc - 0.4f, zc + 0.4f, 4.7f, 1.9f, V3(0.76f, 0.71f, 0.63f), 0.85f);
  // Courtyard: under the screened windows and the courtyard spouts.
  for (float z0 : {3.0f, 11.2f}) addStain(s, 0, 0.0f, z0 - 0.05f, z0 + 0.85f, 5.6f, 2.2f, V3(0.8f, 0.76f, 0.7f), 0.55f);
  addStain(s, 0, CW, 10.95f, 12.05f, 4.9f, 2.0f, V3(0.8f, 0.76f, 0.7f), 0.55f);
  for (float z : {2.2f, 7.8f, 12.1f}) {
    addStain(s, 0, 0.0f, z - 0.2f, z + 0.2f, H - 0.62f, 3.2f, V3(0.72f, 0.86f, 0.80f), 0.8f);
    addStain(s, 0, CW, z + 0.37f, z + 0.77f, H - 0.62f, 3.2f, V3(0.72f, 0.86f, 0.80f), 0.8f);
  }
  addStain(s, 2, CL, SX0 - 0.1f, SX1 + 0.1f, 4.3f, 1.6f, V3(0.82f, 0.78f, 0.72f), 0.5f);
  // Tannin from the teak beam ends, leached down the lime below them.
  for (int k = 0; k < 4; k++) {
    float z = 3.0f + 0.07f + k * (10.0f - 3.0f - 0.14f) / 3.0f;
    float l0 = 0.8f + 0.25f * (float)((k * 7) % 3), l1 = 1.1f + 0.2f * (float)((k * 5) % 3);
    addStain(s, 0, 0.0f, z - 0.08f, z + 0.08f, HC - 0.3f, l0, V3(0.80f, 0.72f, 0.60f), 0.75f);
    addStain(s, 0, CW, z - 0.08f, z + 0.08f, HC - 0.3f, l1, V3(0.80f, 0.72f, 0.60f), 0.75f);
  }
}


// The house is modelled on true north-south axes.
constexpr float HOUSE_ORIENT_DEG = 0.0f;

inline void buildQudra(Scene& s) {
  s.groundMat = -1;
  s.addHField(V3(-1400.0f, -14.0f, -1400.0f), V3(1400.0f, 24.0f, 1400.0f), M_SAND, 0);
  // The roof: a 40 m disc of 1.2 m deep ribs on a triangular grid, rimmed by a ring beam.
  PatternParams p;
  p.type = PAT_KAGOME; p.period = 2.3f; p.width = 0.22f; p.a = 0.0f; p.c = 0.12f;
  p.discR = 20.0f; p.ringW = 0.9f;
  // 0.75 m deep ribs: deep enough that the roof reads as structure and cuts
  // the low sun, shallow enough that the noon triangles come through whole.
  s.addLattice(V3(-20.5f, 6.4f, -20.5f), V3(20.5f, 7.15f, 20.5f), 1, p, M_STONE, 0.02f);
  // Secondary infill: a 6 cm lattice at a quarter of the structural period,
  // laid on top of the ribs. From below it reads as lace inside each bay; on
  // the sand it turns the grid of lines into a field of small light triangles
  // — the shade, not the structure, is what people remember.
  PatternParams q = p;
  q.period = p.period / 4.0f; q.width = 0.2f; q.c = 0.03f;
  s.addLattice(V3(-20.5f, 7.15f, -20.5f), V3(20.5f, 7.21f, 20.5f), 1, q, M_STONE, 0.006f);
  // Seven slender dark-bronze columns.
  s.addCyl(0.0f, 0.0f, 0.2f, -1.0f, 6.4f, M_BRONZE);
  for (int k = 0; k < 6; k++) {
    float a = k * PI / 3.0f + 0.3f;
    s.addCyl(11.5f * std::cos(a), 11.5f * std::sin(a), 0.17f, -1.0f, 6.4f, M_BRONZE);
  }
  // Low rammed-earth benches for scale.
  s.addBox(V3(-8.0f, -0.4f, 3.0f), V3(-4.6f, 0.45f, 3.6f), M_RAMMED, FALL, 0.02f);
  s.addBox(V3(4.0f, -0.4f, -7.5f), V3(4.6f, 0.45f, -3.8f), M_RAMMED, FALL, 0.02f);
  s.addBox(V3(6.5f, -0.4f, 8.0f), V3(9.8f, 0.45f, 8.6f), M_RAMMED, FALL, 0.02f);
  // Signs of use: water jars by two benches, a folded mat left on one.
  s.addSdf(SDF_JAR, V3(10.35f, duneHeight(10.35f, 8.4f) - 0.04f, 8.4f), 30.0f, 0.72f, {M_TERRACOTTA});
  s.addSdf(SDF_JAR, V3(4.95f, duneHeight(4.95f, -6.2f) - 0.04f, -6.2f), 200.0f, 0.62f, {M_TERRACOTTA});
  s.addSdf(SDF_MAT, V3(7.6f, 0.45f, 8.3f), 8.0f, 1.0f, {M_PALM}, {0.34f, 0.22f});

  // The desert is not empty: ghaf trees on the dunes, and low scrub
  // (Calligonum, Leptadenia) scattered wherever the sand holds still.
  // Placed in the view wedge of the qudra cameras; coarse leaf cells at range.
  const V3 cam(-3.0f, 0.0f, 17.2f), fwd(0.626f, 0.0f, -0.78f), rgt(0.78f, 0.0f, 0.626f);
  const float trees[][4] = {{70.0f, -25.0f, 7.0f, 11.0f}, {112.0f, 22.0f, 8.0f, 12.0f}, {58.0f, 40.0f, 6.0f, 13.0f},
                            {165.0f, -72.0f, 7.5f, 14.0f}, {95.0f, -48.0f, 5.5f, 15.0f}};
  for (auto& t : trees) {
    V3 b = cam + fwd * t[0] + rgt * t[1];
    TreeSpec g;
    g.base = V3(b.x, duneHeight(b.x, b.z) - 0.15f, b.z);
    g.height = t[2]; g.crown = t[2] * 0.55f; g.trunkR = 0.2f; g.seed = (uint32_t)t[3] * 7u + 3u;
    g.cell = 0.09f; g.fill = 0.95f; g.limbs = 7; g.droop = 0.5f; g.dcell = 0.35f;
    addTree(s, g);
  }
  RNG rs(4242u);
  for (int k = 0; k < 30; k++) {
    float D = 24.0f + 46.0f * rs.next(), L = (rs.next() * 1.8f - 0.9f) * D;
    V3 b = cam + fwd * D + rgt * L;
    if (b.x * b.x + b.z * b.z < 23.0f * 23.0f) continue;       // keep the roof's ground clear
    TreeSpec sh;
    sh.base = V3(b.x, duneHeight(b.x, b.z) - 0.05f, b.z);
    sh.height = 0.6f + 0.8f * rs.next(); sh.crown = 0.45f + 0.5f * rs.next(); sh.trunkR = 0.025f;
    sh.seed = 500u + (uint32_t)k; sh.cell = 0.04f; sh.fill = 0.9f; sh.limbs = 6; sh.droop = 0.15f; sh.crownBase = 0.12f;
    sh.sprayBias = 0.25f; sh.coreClump = 0.8f; sh.noiseAmt = 0.6f; sh.dcell = 0.1f;
    addTree(s, sh);
  }
}

}  // namespace zw
