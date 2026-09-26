// Shader-trap proof (webgl-shader-interaction-engineer, known-traps.md).
//
// "Check the derivative, not the value." Evaluates the renderer's REAL height
// and noise functions (included from src/) and dumps raw fields and 1D
// profiles; tools/trap_proof.py differentiates and plots them next to
// deliberately broken controls built the common wrong way, so the sheet shows
// both that the test catches each bug and that ZAWAL's fields pass it.
//
//   bin/trap_proof <outdir>
// Writes <name>.f32 grids (N*N float32) and <name>.csv profiles.
#include <cstdio>
#include <functional>
#include <string>

#include "../src/geometry.h"
#include "../src/materials.h"
#include "../src/noise.h"

using namespace zw;

// ---- deliberately broken controls ----------------------------------------------
// Value noise, cubic smoothstep fade: extrema pinned to lattice points and a
// curvature jump across every cell face (trap #1).
static float valueNoiseBad(float x, float y) {
  float fx = std::floor(x), fy = std::floor(y);
  int ix = (int)fx, iy = (int)fy;
  float u = x - fx, v = y - fy;
  u = u * u * (3 - 2 * u);
  v = v * v * (3 - 2 * v);
  auto h = [](int a, int b) { return hashf(hash2i(a, b)) * 2.0f - 1.0f; };
  return lerpf(lerpf(h(ix, iy), h(ix + 1, iy), u), lerpf(h(ix, iy + 1), h(ix + 1, iy + 1), u), v);
}
static float fbmBad(float x, float y) {  // axis-aligned, exactly 2.0x octaves
  float s = 0, a = 0.5f;
  for (int i = 0; i < 4; i++) { s += a * valueNoiseBad(x, y); x *= 2.0f; y *= 2.0f; a *= 0.5f; }
  return s;
}
// Floor joint built with a clamp ramp and a hard max (trap #3).
static float jointBad(float x, float z) {
  const float T = 0.6f;
  float fx = x / T - std::floor(x / T), fz = z / T - std::floor(z / T);
  float dx = std::min(fx, 1.0f - fx) * T, dz = std::min(fz, 1.0f - fz) * T;
  float jx = 1.0f - clampf((dx - 0.0006f) / 0.002f, 0.0f, 1.0f);
  float jz = 1.0f - clampf((dz - 0.0006f) / 0.002f, 0.0f, 1.0f);
  return -0.0015f * std::max(jx, jz);
}
// ZAWAL's joint groove alone (floorHeight minus its texture terms), so the
// profile shows the combinator, not the stone grain.
static float jointGood(float x, float z) {
  const float T = 0.6f;
  float fx = x / T - std::floor(x / T), fz = z / T - std::floor(z / T);
  float dx = std::min(fx, 1.0f - fx) * T, dz = std::min(fz, 1.0f - fz) * T;
  float jx = 1.0f - smootherstep(0.0006f, 0.0026f, dx);
  float jz = 1.0f - smootherstep(0.0006f, 0.0026f, dz);
  return -0.0015f * smax(jx, jz, 0.25f);
}
static float crestBad(float u) { return 16.0f * (1.0f - std::fabs(std::sin(u))); }
static float crestGood(float u) { return 16.0f * (1.0f - sabs(std::sin(u), 0.02f)); }
static float grooveGood(float y) {  // rammed-earth lift groove, as in rammedHeight
  float ly = y / 0.9f, fl = ly - std::floor(ly);
  float dist = std::min(fl, 1.0f - fl) * 0.9f;
  return -0.0022f * (1.0f - smootherstep(0.0f, 0.009f, dist));
}

using F2 = std::function<float(float, float)>;

static void grid(const std::string& path, const F2& f, float x0, float y0, float span, int N) {
  std::vector<float> v((size_t)N * N);
  for (int j = 0; j < N; j++)
    for (int i = 0; i < N; i++) v[(size_t)j * N + i] = f(x0 + (i + 0.5f) * span / N, y0 + (j + 0.5f) * span / N);
  FILE* o = std::fopen(path.c_str(), "wb");
  std::fwrite(v.data(), 4, v.size(), o);
  std::fclose(o);
}
static void profile(const std::string& path, const std::function<float(float)>& f, float x0, float x1, int n) {
  FILE* o = std::fopen(path.c_str(), "w");
  for (int i = 0; i < n; i++) {
    double x = x0 + (x1 - x0) * i / (n - 1.0);
    std::fprintf(o, "%.9f,%.9g\n", x, (double)f((float)x));
  }
  std::fclose(o);
}

int main(int argc, char** argv) {
  std::string out = argc > 1 ? argv[1] : "proof";
  const int N = 512;
  // Trap 1: 8 lattice cells across the panel, so the grid is resolvable.
  grid(out + "/t1_bad.f32", [](float x, float y) { return fbmBad(x, y); }, 0.0f, 0.0f, 8.0f, N);
  grid(out + "/t1_good.f32", [](float x, float y) { return fbm(V3(x, y, 0.37f), 4); }, 0.0f, 0.0f, 8.0f, N);
  // The actual limewash height over 0.5 m of wall, in mm.
  grid(out + "/t1_lime.f32", [](float x, float y) { return limeHeight(V3(x, y, 0.37f)) * 1000.0f; }, 0.0f, 0.0f, 0.5f, N);
  // Trap 3: a 12 mm window centred on a joint crossing (corner of four slabs).
  grid(out + "/t3_joint_bad.f32", [](float x, float z) { return jointBad(x, z) * 1000.0f; }, 0.594f, 0.594f, 0.012f, N);
  grid(out + "/t3_joint_good.f32", [](float x, float z) { return jointGood(x, z) * 1000.0f; }, 0.594f, 0.594f, 0.012f, N);
  profile(out + "/p_joint_bad.csv", [](float x) { return jointBad(x, 0.3f) * 1000.0f; }, 0.595f, 0.605f, 2001);
  profile(out + "/p_joint_good.csv", [](float x) { return jointGood(x, 0.3f) * 1000.0f; }, 0.595f, 0.605f, 2001);
  profile(out + "/p_crest_bad.csv", crestBad, 3.14159f - 0.25f, 3.14159f + 0.25f, 2001);
  profile(out + "/p_crest_good.csv", crestGood, 3.14159f - 0.25f, 3.14159f + 0.25f, 2001);
  profile(out + "/p_groove.csv", [](float y) { return grooveGood(y) * 1000.0f; }, 0.9f - 0.02f, 0.9f + 0.02f, 2001);
  std::printf("proof data written to %s\n", out.c_str());
  return 0;
}
