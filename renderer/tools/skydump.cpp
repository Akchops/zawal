// Dumps the sky environment for a given time as a small lat-long float image + stats.
#include "../src/sky.h"
#include "../src/sun.h"
#include <cstdio>
#include <cstdlib>
using namespace zw;
int main(int argc, char** argv) {
  double minutes = atof(argv[1]) * 60.0;
  float aod = argc > 3 ? atof(argv[3]) : 0.38f;
  SunPos sp = solarPosition(2026, 6, 21, minutes);
  AtmosphereParams ap; ap.aod = aod;
  Sky sky; sky.build(sp.dir, ap, 0.34f);
  // horizontal irradiance from sky (numerical integration over the upper hemisphere)
  V3 E(0.0f);
  for (int j = 0; j < sky.H / 2; j++) for (int i = 0; i < sky.W; i++) {
    float th = (j + 0.5f) / sky.H * PI; float dOmega = (2 * PI / sky.W) * (PI / sky.H) * std::sin(th);
    E += sky.env[(size_t)j * sky.W + i] * (std::cos(th) * dOmega);
  }
  V3 Es = sky.sunIrradiance * std::max(0.0f, sp.dir.y);
  printf("t=%s alt %.1f sunT %.3f %.3f %.3f  sunE_h %.3f %.3f %.3f  skyE_h %.3f %.3f %.3f  diffuse fraction %.2f\n", argv[1], sp.altitudeDeg,
    sky.sunTrans.x, sky.sunTrans.y, sky.sunTrans.z, Es.x, Es.y, Es.z, E.x, E.y, E.z, lum(E) / (lum(E) + lum(Es)));
  V3 zen = sky.envLookup(V3(0, 1, 0)); V3 hor = sky.envLookup(normalize(V3(-sp.dir.x, 0.03f, -sp.dir.z)));
  printf("   zenith %.4f %.4f %.4f   horizon(anti-sun) %.4f %.4f %.4f\n", zen.x, zen.y, zen.z, hor.x, hor.y, hor.z);
  FILE* f = fopen(argv[2], "wb"); fwrite(sky.env.data(), sizeof(V3), sky.env.size(), f); fclose(f);
}
