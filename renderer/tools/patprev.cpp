// Pattern preview: writes a PGM of the hole/solid field for quick visual checks.
#include "../src/patterns.h"
#include <cstdio>
#include <cstdlib>
using namespace zw;
int main(int argc, char** argv) {
  PatternParams p; p.type = atoi(argv[1]); p.period = atof(argv[2]); p.width = atof(argv[3]);
  p.a = atof(argv[4]); p.b = atof(argv[5]); p.c = atof(argv[6]);
  float span = atof(argv[7]); int N = 600; const char* out = argv[8];
  FILE* f = fopen(out, "wb"); fprintf(f, "P5\n%d %d\n255\n", N, N);
  for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) {
    float u = (i + 0.5f) / N * span, v = (j + 0.5f) / N * span;
    float h = patternSD(p, u, v);
    float px = span / N;
    float a = saturate(0.5f + h / px);          // antialiased: 1 solid, 0 hole
    unsigned char c = (unsigned char)(255 * (0.12f + 0.8f * (1.0f - a)));
    fputc(c, f);
  }
  fclose(f);
}
