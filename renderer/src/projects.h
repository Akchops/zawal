// The three project scenes that exist only as heroes (Qudra is in scenes.h).
// Each is built for one signature hour and one camera, with just enough
// around the frame to cast and bounce the right light.
#pragma once
#include "scenes.h"

namespace zw {

// A slatted teak chair; its back is on the side it faces away from.
// face: 0 = +x, 1 = -x, 2 = +z, 3 = -z.
inline void addChair(Scene& s, float cx, float cz, int face, int mat = M_TEAK) {
  const float sw = 0.46f, sd = 0.44f;
  auto box = [&](float x0, float y0, float z0, float x1, float y1, float z1) {
    // local: x across the seat (-sw/2..sw/2), z from back (0) to front (sd)
    auto P = [&](float lx, float lz) {
      switch (face) {
        case 0: return V3(cx - sd * 0.5f + lz, 0.0f, cz + lx);
        case 1: return V3(cx + sd * 0.5f - lz, 0.0f, cz - lx);
        case 2: return V3(cx - lx, 0.0f, cz - sd * 0.5f + lz);
        default: return V3(cx + lx, 0.0f, cz + sd * 0.5f - lz);
      }
    };
    V3 a = P(x0, z0), b = P(x1, z1);
    s.addBox(V3(std::min(a.x, b.x), y0, std::min(a.z, b.z)), V3(std::max(a.x, b.x), y1, std::max(a.z, b.z)), mat, FALL, 0.004f);
  };
  for (float lx : {-sw * 0.5f, sw * 0.5f - 0.04f})
    for (float lz : {0.0f, sd - 0.04f}) box(lx, 0.0f, lz, lx + 0.04f, 0.43f, lz + 0.04f);
  box(-sw * 0.5f - 0.01f, 0.43f, -0.01f, sw * 0.5f + 0.01f, 0.465f, sd + 0.01f);
  for (float lx : {-sw * 0.5f, sw * 0.5f - 0.04f}) box(lx, 0.465f, 0.0f, lx + 0.04f, 0.88f, 0.04f);
  for (float yy : {0.6f, 0.72f, 0.84f}) box(-sw * 0.5f, yy, 0.005f, sw * 0.5f, yy + 0.05f, 0.035f);
}

// ---- Ghaf House, Jumeirah (16:10) ------------------------------------------------
// A courtyard house wrapped around an old ghaf. The camera stands in the
// shaded west loggia and looks east: the afternoon sun comes over the roof
// behind it, lights the tree and lays its dappled shade on the east wing.
inline void buildGhafHouse(Scene& s) {
  s.groundMat = -1;
  const float CW = 14.0f, CD = 14.0f, H = 6.6f, T = 0.4f;
  // ground: courtyard stone, a street beyond
  s.addBox(V3(-60.0f, -0.6f, -60.0f), V3(60.0f, 0.0f, 60.0f), M_STREET, FY1, 0.0f);
  s.addBox(V3(-3.4f, 0.0f, 0.0f), V3(CW, 0.02f, CD), M_STONE_FLOOR, FY1, 0.0f);
  // enclosure (two storeys, parapet)
  int east = s.addBox(V3(CW, 0.0f, -T), V3(CW + T, H, CD + T), M_LIME, FALL, 0.012f);
  int north = s.addBox(V3(-3.4f - T, 0.0f, -T), V3(CW + T, H, 0.0f), M_LIME, FALL, 0.012f);
  int south = s.addBox(V3(-3.4f - T, 0.0f, CD), V3(CW + T, H, CD + T), M_LIME, FALL, 0.012f);
  s.addBox(V3(-3.4f - T, 0.0f, -T), V3(-3.4f, H, CD + T), M_LIME_SHADE, FALL, 0.012f);   // loggia back wall
  // east wing openings: a deep ground-floor door and upper windows with screens
  s.addCut(east, V3(CW - 0.1f, -1.0f, 6.3f), V3(CW + 0.25f, 2.7f, 7.7f));
  s.addBox(V3(CW + 0.23f, 0.0f, 6.3f), V3(CW + 0.3f, 2.7f, 7.7f), M_TEAK, FALL, 0.004f);
  for (float z : {2.6f, 11.4f}) {
    s.addCut(east, V3(CW - 0.1f, 0.9f, z - 0.6f), V3(CW + 0.2f, 2.5f, z + 0.6f));
    s.addBox(V3(CW + 0.18f, 0.9f, z - 0.6f), V3(CW + 0.24f, 2.5f, z + 0.6f), M_DARKROOM, FALL, 0.0f);
  }
  for (float z : {3.5f, 7.0f, 10.5f}) {
    s.addCut(east, V3(CW - 0.1f, 3.8f, z - 0.55f), V3(CW + T + 0.1f, 5.4f, z + 0.55f));
    s.addLattice(V3(CW + 0.16f, 3.8f, z - 0.55f), V3(CW + 0.26f, 5.4f, z + 0.55f), 0, screenStar(0.2f), M_TEAK, 0.003f);
    s.addBox(V3(CW + 0.5f, 3.6f, z - 0.8f), V3(CW + 0.7f, 5.6f, z + 0.8f), M_DARKROOM, FALL, 0.0f);
  }
  for (float x : {3.0f, 10.0f}) {
    s.addCut(north, V3(x - 0.7f, 3.8f, -T - 0.1f), V3(x + 0.7f, 5.3f, 0.1f));
    s.addBox(V3(x - 0.7f, 3.8f, -0.3f), V3(x + 0.7f, 5.3f, -0.22f), M_DARKROOM, FALL, 0.0f);
    s.addCut(south, V3(x - 0.7f, 3.8f, CD - 0.1f), V3(x + 0.7f, 5.3f, CD + T + 0.1f));
    s.addBox(V3(x - 0.7f, 3.8f, CD + 0.22f), V3(x + 0.7f, 5.3f, CD + 0.3f), M_DARKROOM, FALL, 0.0f);
  }
  s.addBox(V3(-3.4f - T - 0.03f, H, -T - 0.03f), V3(CW + T + 0.03f, H + 0.07f, 0.03f), M_STONE, FALL, 0.008f);
  s.addBox(V3(-3.4f - T - 0.03f, H, CD - 0.03f), V3(CW + T + 0.03f, H + 0.07f, CD + T + 0.03f), M_STONE, FALL, 0.008f);
  s.addBox(V3(CW - 0.03f, H, -T), V3(CW + T + 0.03f, H + 0.07f, CD + T), M_STONE, FALL, 0.008f);
  // west loggia: roof slab and upper storey over it, four piers facing the court
  s.addBox(V3(-3.4f - T, 3.4f, -T), V3(0.35f, H, CD + T), M_LIME_SHADE, FALL, 0.012f);
  for (float z : {0.9f, 4.95f, 9.05f, 13.1f}) s.addBox(V3(-0.1f, 0.0f, z - 0.25f), V3(0.4f, 3.4f, z + 0.25f), M_LIME, FALL, 0.02f);
  s.addBox(V3(-0.1f, 0.0f, -0.1f), V3(0.4f, 0.32f, CD + 0.1f), M_LIME_SHADE, FALL, 0.006f);  // loggia step
  // loggia life: floor cushions, a low table, a rolled rug
  s.addSdf(SDF_CUSHION, V3(-2.6f, 0.02f, 5.4f), 90.0f, 1.0f, {M_LINEN_INDIGO}, {0.9f, 0.34f, 0.15f});
  s.addSdf(SDF_CUSHION, V3(-2.6f, 0.02f, 8.6f), 90.0f, 1.0f, {M_LINEN_OCHRE}, {0.9f, 0.34f, 0.15f});
  s.addCyl(-1.4f, 7.0f, 0.45f, 0.34f, 0.37f, M_BRASS);
  s.addCyl(-1.4f, 7.0f, 0.14f, 0.02f, 0.34f, M_TEAK);
  // the tree bed: a low limestone drum the trunk rises from
  s.addCyl(7.0f, 7.0f, 2.3f, 0.0f, 0.46f, M_STONE);
  s.addCyl(7.0f, 7.0f, 2.05f, 0.4f, 0.47f, M_SOIL);
  TreeSpec g;
  g.base = V3(7.0f, 0.4f, 7.0f); g.height = 9.2f; g.crown = 5.6f; g.trunkR = 0.36f; g.lean = V3(0.18f, 0.0f, -0.12f);
  g.seed = 91; g.limbs = 9; g.fill = 0.85f; g.droop = 0.55f; g.crownBase = 0.36f;
  ghafSprays(g);
  g.cell = 0.017f; g.fill = 0.55f; g.dcell = 0.25f;
  addTree(s, g);
  // a second, younger ghaf beyond the south-east corner, over the roofs
  TreeSpec y2;
  y2.base = V3(17.0f, 0.0f, 16.5f); y2.height = 8.0f; y2.crown = 3.8f; y2.trunkR = 0.22f; y2.seed = 93; y2.limbs = 7; y2.fill = 0.85f; y2.droop = 0.5f;
  ghafSprays(y2);
  y2.cell = 0.03f;
  addTree(s, y2);
  // life under the tree: cushions on the drum's edge, two chairs and a tray
  s.addSdf(SDF_CUSHION, V3(5.35f, 0.4f, 5.9f), 38.0f, 1.0f, {M_LINEN_NATURAL}, {0.55f, 0.36f, 0.12f});
  s.addSdf(SDF_CUSHION, V3(5.05f, 0.4f, 7.9f), -24.0f, 1.0f, {M_LINEN_INDIGO}, {0.55f, 0.36f, 0.12f});
  s.addSdf(SDF_MAT, V3(8.6f, 0.4f, 5.2f), 60.0f, 1.0f, {M_PALM}, {0.28f, 0.19f});
  addChair(s, 3.2f, 4.2f, 2);
  addChair(s, 3.4f, 9.6f, 3);
  s.addCyl(3.3f, 6.9f, 0.3f, 0.36f, 0.385f, M_BRASS);
  s.addCyl(3.3f, 6.9f, 0.1f, 0.0f, 0.36f, M_TEAK);
  s.addSdf(SDF_JAR, V3(1.1f, 0.0f, 1.2f), 10.0f, 0.8f, {M_TERRACOTTA});
  // a copper rill from the loggia step to the tree bed
  s.addBox(V3(0.4f, -0.12f, 6.9f), V3(4.7f, 0.0f, 7.1f), M_COPPER_DEEP, FALL, 0.002f);
  int rw = s.addBox(V3(0.42f, -0.02f, 6.92f), V3(4.68f, -0.018f, 7.08f), M_WATER, FY1, 0.0f);
  s.prims[rw].flags |= F_WATER;
  // jars and a bench on the south side
  s.addBox(V3(9.0f, 0.0f, 13.3f), V3(12.6f, 0.45f, 13.95f), M_LIME, FALL, 0.02f);
  s.addSdf(SDF_JAR, V3(12.9f, 0.0f, 1.0f), 30.0f, 1.0f, {M_TERRACOTTA});
  s.addSdf(SDF_PLANTER, V3(1.2f, 0.0f, 13.1f), 0.0f, 0.8f, {M_TERRACOTTA, M_SOIL});
  addStain(s, 0, CW, 3.0f, 4.0f, 3.8f, 1.6f, V3(0.78f, 0.72f, 0.64f), 0.7f);
  addStain(s, 0, CW, 10.0f, 11.0f, 3.8f, 1.6f, V3(0.78f, 0.72f, 0.64f), 0.7f);
}

// ---- Hotel Sikka, Deira (09:40) ---------------------------------------------------
// A lane 1.8 m wide between 9 m coral-stone walls, roofed in palm-rib slats,
// running north to a small square with the wind tower. The camera looks
// north up the lane; the morning sun from the east draws the slats across the
// west wall and the floor.
inline void buildSikka(Scene& s) {
  s.groundMat = -1;
  const float W = 1.8f, H = 9.0f;
  s.addBox(V3(-60.0f, -0.6f, -60.0f), V3(60.0f, 0.0f, 60.0f), M_STREET, FY1, 0.0f);
  int west = s.addBox(V3(-4.0f, 0.0f, -9.0f), V3(0.0f, H, 30.0f), M_CORAL, FALL, 0.015f);
  int east = s.addBox(V3(W, 0.0f, -9.0f), V3(W + 4.0f, H, 30.0f), M_CORAL, FALL, 0.015f);
  // doors, windows and a bench along the lane
  const float doors[][2] = {{0.0f, 3.0f}, {1.0f, -3.5f}};
  for (auto& d : doors) {
    bool w = d[0] < 0.5f;
    float x0 = w ? -0.35f : W, x1 = w ? 0.0f : W + 0.35f, z = d[1];
    s.addCut(w ? west : east, V3(x0 - 0.05f, -1.0f, z - 0.6f), V3(x1 + 0.05f, 2.5f, z + 0.6f));
    s.addBox(V3(w ? -0.36f : W + 0.3f, 0.0f, z - 0.6f), V3(w ? -0.3f : W + 0.36f, 2.5f, z + 0.6f), M_TEAK_GREY, FALL, 0.004f);
  }
  for (float z : {-6.0f, 0.5f, 6.5f}) {
    s.addCut(west, V3(-0.3f, 3.4f, z - 0.4f), V3(0.05f, 4.6f, z + 0.4f));
    s.addLattice(V3(-0.2f, 3.4f, z - 0.4f), V3(-0.1f, 4.6f, z + 0.4f), 0, screenStar(0.16f), M_TEAK, 0.003f);
    s.addCut(east, V3(W - 0.05f, 5.4f, z + 1.2f), V3(W + 0.3f, 6.6f, z + 2.0f));
    s.addLattice(V3(W + 0.1f, 5.4f, z + 1.2f), V3(W + 0.2f, 6.6f, z + 2.0f), 0, screenStar(0.16f), M_TEAK, 0.003f);
  }
  s.addBox(V3(-0.02f, 0.0f, 7.8f), V3(0.42f, 0.44f, 10.5f), M_CORAL, FALL, 0.02f);        // plaster bench
  // wall lanterns, and the cables that feed them, sagging between hooks
  addWallLantern(s, 0.0f, 1.0f, -1.6f, 2.75f, 0.8f, 1);
  addWallLantern(s, 0.0f, 1.0f, 5.4f, 2.75f, 0.8f, 1);
  addWallLantern(s, W, -1.0f, -6.2f, 2.75f, 0.8f, 1);
  for (float z0 = -8.0f; z0 < 12.0f; z0 += 2.5f)
    for (int k = 0; k < 8; k++) {
      float u0 = k / 8.0f, u1 = (k + 1) / 8.0f;
      auto sag = [](float u) { return 0.09f * 4.0f * u * (1.0f - u); };
      s.addCapsule(V3(W - 0.03f, 3.55f - sag(u0), z0 + 2.5f * u0), V3(W - 0.03f, 3.55f - sag(u1), z0 + 2.5f * u1), 0.006f, M_RUBBER);
      s.addCapsule(V3(W - 0.045f, 3.62f - 0.7f * sag(u0), z0 + 2.5f * u0), V3(W - 0.045f, 3.62f - 0.7f * sag(u1), z0 + 2.5f * u1), 0.004f, M_RUBBER);
    }
  s.addSdf(SDF_PLANTER, V3(W - 0.35f, 0.0f, 2.4f), 0.0f, 0.62f, {M_TERRACOTTA, M_SOIL});
  s.addSdf(SDF_PLANTER, V3(0.33f, 0.0f, -4.6f), 0.0f, 0.55f, {M_TERRACOTTA, M_SOIL});
  TreeSpec pl;
  pl.base = V3(W - 0.35f, 0.5f, 2.4f); pl.height = 1.3f; pl.crown = 0.5f; pl.trunkR = 0.03f; pl.leafMat = M_LEAF_OLIVE;
  pl.cell = 0.04f; pl.fill = 1.0f; pl.limbs = 5; pl.droop = 0.1f; pl.dcell = 0.07f; pl.clumpScale = 1.3f; pl.sprayBias = 0.6f; pl.coreClump = 0.9f; pl.noiseAmt = 0.55f; pl.seed = 57;
  addTree(s, pl);
  TreeSpec p2 = pl; p2.base = V3(0.33f, 0.45f, -4.6f); p2.height = 1.0f; p2.crown = 0.4f; p2.seed = 58;
  addTree(s, p2);
  // palm-rib slats laid along the lane on the wall tops, on teak joists that
  // span it; at 09:40 their lines fall on the top of the west wall only
  PatternParams sl; sl.type = PAT_SLATS; sl.period = 0.085f; sl.width = 0.032f; sl.a = 0.6f; sl.b = 0.045f;
  sl.rot = 1.5707963f;
  s.addLattice(V3(-0.3f, 9.12f, -7.0f), V3(W + 0.3f, 9.155f, 12.0f), 1, sl, M_PALM, 0.003f);
  for (float z = -7.0f; z <= 12.0f; z += 1.6f) s.addBox(V3(-0.3f, 9.0f, z - 0.05f), V3(W + 0.3f, 9.12f, z + 0.05f), M_TEAK_GREY, FALL, 0.004f);
  // the square and the wind tower at the north end
  s.addBox(V3(-8.0f, 0.0f, -18.0f), V3(-4.0f, 7.5f, -9.0f), M_CORAL, FALL, 0.015f);
  s.addBox(V3(W + 4.0f, 0.0f, -18.0f), V3(W + 9.0f, 8.0f, -9.0f), M_CORAL, FALL, 0.015f);
  int tower = s.addBox(V3(-1.2f, 0.0f, -17.0f), V3(3.0f, 16.0f, -12.8f), M_CORAL, FALL, 0.015f);
  s.addCut(tower, V3(-0.9f, 10.5f, -16.7f), V3(2.7f, 15.4f, -13.1f));
  for (int k = 0; k < 4; k++) {
    float x = -0.95f + k * 1.0f;
    s.addCut(tower, V3(x + 0.18f, 10.9f, -17.2f), V3(x + 0.62f, 15.0f, -12.6f));
  }
  s.addCut(tower, V3(-0.2f, -1.0f, -13.0f), V3(2.0f, 3.0f, -12.6f));                        // its door
  s.addBox(V3(-0.2f, 0.0f, -13.05f), V3(2.0f, 3.0f, -12.98f), M_TEAK_GREY, FALL, 0.004f);
  s.addBox(V3(-1.3f, 16.0f, -17.1f), V3(3.1f, 16.12f, -12.7f), M_STONE, FALL, 0.01f);
  addStain(s, 0, 0.0f, 0.1f, 0.9f, 3.4f, 2.2f, V3(0.78f, 0.72f, 0.64f), 0.7f);
  addStain(s, 0, W, 7.7f, 8.5f, 5.4f, 2.6f, V3(0.78f, 0.72f, 0.64f), 0.7f);
}

// ---- Mushrif Reading Rooms (18:05) ------------------------------------------------
// A tall reading room behind a west screen carved 45 cm deep. At noon the
// screen passes no direct sun; late in the afternoon the low sun lines up
// with the carving and the tables are covered in stars.
inline void buildMushrif(Scene& s) {
  s.groundMat = -1;
  const float D = 9.0f, L = 16.0f, H = 6.2f;
  s.addBox(V3(-80.0f, -0.6f, -80.0f), V3(80.0f, 0.0f, 80.0f), M_STREET, FY1, 0.0f);
  s.addBox(V3(-0.45f, 0.0f, 0.0f), V3(D, 0.02f, L), M_STONE_FLOOR, FY1, 0.0f);
  // shell: the screen wall is the west side; the others are solid
  s.addBox(V3(-0.45f, H, -0.45f), V3(D + 0.45f, H + 0.5f, L + 0.45f), M_LIME, FALL, 0.01f);   // roof
  s.addBox(V3(D, 0.0f, -0.45f), V3(D + 0.45f, H, L + 0.45f), M_LIME, FALL, 0.01f);
  s.addBox(V3(-0.45f, 0.0f, -0.45f), V3(D, H, 0.0f), M_LIME, FALL, 0.01f);
  s.addBox(V3(-0.45f, 0.0f, L), V3(D, H, L + 0.45f), M_LIME, FALL, 0.01f);
  // the west wall: solid below 0.8 m and above 5.6 m, the carved screen between
  s.addBox(V3(-0.45f, 0.0f, 0.0f), V3(0.0f, 0.8f, L), M_STONE, FALL, 0.01f);
  s.addBox(V3(-0.45f, 5.6f, 0.0f), V3(0.0f, H, L), M_STONE, FALL, 0.01f);
  PatternParams cs; cs.type = PAT_STAR8; cs.period = 0.36f; cs.a = 0.31f; cs.b = 0.11f; cs.c = 0.012f;
  cs.ou = 0.0f; cs.ov = 0.0f;
  // The carving is cut along the sun's ray at 18:05 on 21 June (alt 13.2,
  // az 290): going in, each hole drops 0.249 m and moves 0.364 m south per
  // metre. At noon the 45 cm depth passes no direct sun at all.
  cs.shu = -0.2486f; cs.shv = 0.3639f;
  s.addLattice(V3(-0.45f, 0.8f, 0.0f), V3(0.0f, 5.6f, L), 0, cs, M_STONE, 0.01f);
  for (float z : {0.0f, 5.3f, 10.7f, 16.0f}) s.addBox(V3(-0.5f, 0.0f, z - 0.2f), V3(0.05f, H, z + 0.2f), M_STONE, FALL, 0.01f);
  // low bookshelves along the east wall; the plain wall above takes the stars
  s.addBox(V3(D - 0.42f, 0.0f, 0.4f), V3(D, 1.9f, L - 0.4f), M_BOOKS, FALL, 0.004f);
  s.addBox(V3(D - 0.46f, 1.9f, 0.35f), V3(D, 1.95f, L - 0.35f), M_TEAK, FALL, 0.004f);
  // three long teak tables with benches, parallel to the screen
  for (float x : {2.2f, 4.6f}) {
    s.addBox(V3(x - 0.5f, 0.72f, 1.6f), V3(x + 0.5f, 0.77f, 14.4f), M_TEAK_GREY, FALL, 0.006f);
    for (float z : {1.9f, 8.0f, 14.1f})
      for (float dx : {-0.42f, 0.38f}) s.addBox(V3(x + dx, 0.0f, z - 0.04f), V3(x + dx + 0.04f, 0.72f, z), M_TEAK, FALL, 0.004f);
    for (float bx : {x - 0.95f, x + 0.62f}) s.addBox(V3(bx, 0.42f, 2.0f), V3(bx + 0.33f, 0.46f, 14.0f), M_TEAK_GREY, FALL, 0.005f);
    // books and a brass lamp on each table
    s.addBox(V3(x - 0.2f, 0.77f, 5.0f), V3(x + 0.05f, 0.83f, 5.35f), M_LINEN_OCHRE, FALL, 0.004f);
    s.addBox(V3(x + 0.05f, 0.77f, 10.2f), V3(x + 0.3f, 0.81f, 10.5f), M_LINEN_INDIGO, FALL, 0.004f);
    s.addCyl(x, 7.0f, 0.07f, 0.77f, 0.79f, M_BRASS);
    s.addCapsule(V3(x, 0.79f, 7.0f), V3(x, 1.2f, 7.0f), 0.008f, M_BRASS);
  }
  // pendant lamps (off): brass drums on rods
  for (float x : {2.2f, 4.6f})
    for (float z : {8.0f, 12.5f}) {
      s.addCapsule(V3(x, H, z), V3(x, 3.75f, z), 0.006f, M_BRONZE);
      s.addCyl(x, z, 0.18f, 3.45f, 3.75f, M_BRASS);
    }
  // outside the screen: a garden of ghafs and a low wall, lit by the low sun
  s.addBox(V3(-22.0f, 0.0f, -6.0f), V3(-21.6f, 2.2f, 24.0f), M_LIME, FALL, 0.01f);
  TreeSpec g;
  g.base = V3(-9.0f, 0.0f, 4.0f); g.height = 7.5f; g.crown = 3.6f; g.trunkR = 0.24f; g.seed = 71; g.limbs = 7; g.fill = 0.85f; g.droop = 0.5f;
  ghafSprays(g); g.cell = 0.03f;
  addTree(s, g);
}

// ---- The studio, Al Quoz (12:20) ------------------------------------------------
// The converted warehouse: steel trusses, a skylight strip down the middle
// and, under it, a lattice ceiling cut from the studio's own shade drawings.
// At zawal the sun stands almost overhead and the floor and the long tables
// are covered in small stars. The roller door at the far end is open to the
// yard, the brightest thing in the room.
inline void buildStudio(Scene& s) {
  s.groundMat = -1;
  const float W = 14.0f, L = 30.0f, H = 6.6f, T = 0.3f;
  s.addBox(V3(-60.0f, -0.6f, -60.0f), V3(60.0f, 0.0f, 90.0f), M_STREET, FY1, 0.0f);          // the yard outside
  s.addBox(V3(0.0f, 0.0f, 0.0f), V3(W, 0.03f, L), M_STONE, FY1, 0.0f);                          // the screed floor
  // walls; the far wall has the roller-door opening (5.2 m x 4.6 m)
  s.addBox(V3(-T, 0.0f, -T), V3(0.0f, H, L + T), M_LIME, FALL, 0.01f);
  s.addBox(V3(W, 0.0f, -T), V3(W + T, H, L + T), M_LIME, FALL, 0.01f);
  s.addBox(V3(-T, 0.0f, -T), V3(W + T, H, 0.0f), M_LIME, FALL, 0.01f);
  int far = s.addBox(V3(-T, 0.0f, L), V3(W + T, H, L + T), M_LIME, FALL, 0.01f);
  s.addCut(far, V3(4.4f, -1.0f, L - 0.1f), V3(9.6f, 4.6f, L + T + 0.1f));
  s.addBox(V3(4.3f, 4.6f, L - 0.05f), V3(9.7f, 4.75f, L + 0.02f), M_STEEL, FALL, 0.004f);      // door head
  s.addBox(V3(4.4f, 4.75f, L - 0.35f), V3(9.6f, 5.25f, L - 0.02f), M_STEEL, FALL, 0.01f);       // the rolled-up door drum
  // the yard: a boundary wall and a ghaf across it, in full sun
  s.addBox(V3(-8.0f, 0.0f, L + 11.0f), V3(W + 8.0f, 3.2f, L + 11.4f), M_LIME, FALL, 0.01f);
  TreeSpec g;
  g.base = V3(4.6f, 0.0f, L + 8.0f); g.height = 6.4f; g.crown = 3.0f; g.trunkR = 0.2f; g.seed = 83; g.limbs = 6; g.fill = 0.85f; g.droop = 0.5f;
  ghafSprays(g); g.cell = 0.03f;
  addTree(s, g);
  // roof: flat sheeting on the trusses, open over a 7 m skylight strip
  const float RY = 7.7f;
  s.addBox(V3(-T, RY, -T), V3(3.5f, RY + 0.25f, L + T), M_LIME_SHADE, FALL, 0.01f);
  s.addBox(V3(10.5f, RY, -T), V3(W + T, RY + 0.25f, L + T), M_LIME_SHADE, FALL, 0.01f);
  s.addBox(V3(3.5f, RY, -T), V3(10.5f, RY + 0.25f, 0.6f), M_LIME_SHADE, FALL, 0.01f);
  s.addBox(V3(3.5f, RY, L - 0.6f), V3(10.5f, RY + 0.25f, L + T), M_LIME_SHADE, FALL, 0.01f);
  s.addBox(V3(0.0f, H, -T), V3(W, RY, 0.0f), M_LIME, FALL, 0.01f);                            // gable infill
  s.addBox(V3(0.0f, H, L), V3(W, RY, L + T), M_LIME, FALL, 0.01f);
  s.addBox(V3(-T, H, 0.0f), V3(0.0f, RY, L), M_LIME, FALL, 0.01f);
  s.addBox(V3(W, H, 0.0f), V3(W + T, RY, L), M_LIME, FALL, 0.01f);
  // steel Pratt trusses every 3.75 m, spanning the width
  for (float z = 3.75f; z < L - 0.5f; z += 3.75f) {
    s.addBox(V3(0.0f, 6.2f, z - 0.08f), V3(W, 6.4f, z + 0.08f), M_STEEL, FALL, 0.004f);       // bottom chord
    s.addBox(V3(0.0f, RY - 0.18f, z - 0.08f), V3(W, RY, z + 0.08f), M_STEEL, FALL, 0.004f);    // top chord
    for (int k = 0; k <= 8; k++) {
      float x = W * k / 8.0f;
      s.addCapsule(V3(x, 6.4f, z), V3(x, RY - 0.18f, z), 0.045f, M_STEEL);
      if (k < 8) {
        float x1 = W * (k + 1) / 8.0f;
        bool left = k < 4;
        s.addCapsule(V3(left ? x : x1, RY - 0.18f, z), V3(left ? x1 : x, 6.4f, z), 0.038f, M_STEEL);
      }
    }
  }
  // the lattice ceiling under the skylight: the courtyard's star, a little larger
  PatternParams st; st.type = PAT_STAR8; st.period = 0.42f; st.a = 0.325f; st.b = 0.09f; st.c = 0.007f; st.ou = 0.21f; st.ov = 0.21f;
  s.addLattice(V3(2.6f, 5.6f, 0.9f), V3(11.4f, 5.66f, L - 0.9f), 1, st, M_TEAK, 0.003f);
  for (float x : {2.6f, 11.4f}) s.addBox(V3(x - 0.08f, 5.5f, 0.9f), V3(x + 0.08f, 5.66f, L - 0.9f), M_TEAK, FALL, 0.004f);
  for (float z = 3.75f; z < L - 0.5f; z += 3.75f)
    for (float x : {2.7f, 7.0f, 11.3f}) s.addCapsule(V3(x, 5.66f, z), V3(x, 6.2f, z), 0.012f, M_STEEL);   // hangers
  // two rows of long work tables, their tops under the stars
  for (float x : {4.6f, 9.4f}) {
    for (float z0 : {3.2f, 11.2f, 19.2f}) {
      const float z1 = z0 + 6.4f;
      s.addBox(V3(x - 0.55f, 0.72f, z0), V3(x + 0.55f, 0.76f, z1), M_TEAK, FALL, 0.004f);
      for (float zz : {z0 + 0.1f, z1 - 0.14f})
        for (float dx : {-0.5f, 0.46f}) s.addBox(V3(x + dx, 0.0f, zz), V3(x + dx + 0.04f, 0.72f, zz + 0.04f), M_STEEL, FALL, 0.003f);
      for (float zc = z0 + 0.8f; zc < z1 - 0.4f; zc += 1.6f) {
        addChair(s, x - 0.95f, zc, 0, M_TEAK);
        if (zc > z0 + 2.0f) addChair(s, x + 0.95f, zc + 0.3f, 1, M_TEAK);
      }
      // on the table: white massing models, drawings, a book, a brass lamp
      uint32_t h = (uint32_t)(x * 131.0f + z0 * 71.0f);
      for (int k = 0; k < 6; k++) {
        h = h * 1664525u + 1013904223u;
        float u = (h >> 8) / 16777216.0f;
        h = h * 1664525u + 1013904223u;
        float v = (h >> 8) / 16777216.0f;
        float bz = z0 + 0.6f + u * 5.0f, bx = x - 0.35f + v * 0.5f;
        float bw = 0.08f + 0.1f * v, bd = 0.08f + 0.12f * u, bh = 0.05f + 0.22f * u * v;
        s.addBox(V3(bx, 0.76f, bz), V3(bx + bw, 0.76f + bh, bz + bd), M_LIME, FALL, 0.003f);
      }
      s.addBox(V3(x - 0.42f, 0.76f, z0 + 3.0f), V3(x + 0.18f, 0.762f, z0 + 3.84f), M_LINEN_NATURAL, FALL, 0.0f);   // a drawing
      s.addBox(V3(x + 0.1f, 0.76f, z0 + 4.6f), V3(x + 0.34f, 0.8f, z0 + 4.92f), M_LINEN_OCHRE, FALL, 0.004f);
      s.addCyl(x + 0.3f, z0 + 1.4f, 0.07f, 0.76f, 0.78f, M_BRASS);
      s.addCapsule(V3(x + 0.3f, 0.78f, z0 + 1.4f), V3(x + 0.3f, 1.15f, z0 + 1.4f), 0.008f, M_BRASS);
    }
  }
  // a site model on a plinth near the door of the room, and plan chests along the west wall
  s.addBox(V3(6.1f, 0.0f, 26.2f), V3(7.9f, 0.9f, 27.6f), M_STONE, FALL, 0.01f);
  for (int k = 0; k < 9; k++) {
    float bx = 6.25f + (k % 3) * 0.52f, bz = 26.35f + (k / 3) * 0.42f, bh = 0.06f + 0.05f * ((k * 7) % 5);
    s.addBox(V3(bx, 0.9f, bz), V3(bx + 0.38f, 0.9f + bh, bz + 0.3f), M_LIME, FALL, 0.003f);
  }
  for (float z = 2.0f; z < 24.0f; z += 1.25f) s.addBox(V3(0.02f, 0.0f, z), V3(0.92f, 0.96f, z + 1.2f), M_TEAK_GREY, FALL, 0.006f);
  // the pin-up wall: sheets of drawings on the east wall
  for (int k = 0; k < 14; k++) {
    float z = 2.6f + k * 1.55f, y0 = 1.25f + 0.1f * (k % 2);
    s.addBox(V3(W - 0.012f, y0, z), V3(W, y0 + 0.84f, z + 1.19f), M_LINEN_NATURAL, FALL, 0.0f);
  }
  // two olives in pots under the stars at the far end
  for (int k = 0; k < 2; k++) {
    float px = k ? 11.2f : 2.9f, pz = 24.8f + 0.9f * k;
    s.addSdf(SDF_PLANTER, V3(px, 0.0f, pz), 0.0f, 0.6f, {M_TERRACOTTA, M_SOIL});
    TreeSpec pl;
    pl.base = V3(px, 0.5f, pz); pl.height = 1.9f; pl.crown = 0.7f; pl.trunkR = 0.04f; pl.leafMat = M_LEAF_OLIVE;
    pl.cell = 0.04f; pl.fill = 1.0f; pl.limbs = 5; pl.droop = 0.1f; pl.dcell = 0.07f; pl.clumpScale = 1.3f; pl.sprayBias = 0.6f; pl.coreClump = 0.9f; pl.noiseAmt = 0.55f; pl.seed = 91 + k;
    addTree(s, pl);
  }
}

}  // namespace zw
