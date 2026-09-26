// ZAWAL offline renderer — unidirectional path tracer.
//
//   zawal --scene house --cam court --date 2026-06-21 --time 08:00 \
//         --w 1600 --h 900 --spp 256 --out out/court_0800
//
// Writes linear HDR colour plus the AOVs the post pipeline and the site need:
//   color.f32   RGB radiance (relative to TOA solar irradiance = 1)
//   albedo.f32  first-surface albedo   (denoiser guide)
//   normal.f32  first-surface normal   (denoiser guide, relighting)
//   depth.f32   camera distance, m     (parallax, fog, relighting)
//   sunvis.f32  fraction of the solar disc visible at the first surface
//               (the "sun mask" that drives the project morphs)
//   meta.json   sun position, time, camera — the HUD reads the same numbers
#include <omp.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>

#include "bsdf.h"
#include "common.h"
#include "geometry.h"
#include "materials.h"
#include "noise.h"
#include "scenes.h"
#include "sky.h"
#include "sun.h"

using namespace zw;

struct Camera {
  V3 pos, fwd, right, up;
  float tanHalfV = 0.5f, aspect = 1.0f, shiftX = 0.0f, shiftY = 0.0f;
  void setup(const CameraDesc& d, int W, int H, bool useVfov, float vfovDeg) {
    pos = d.pos;
    aspect = (float)W / H;
    V3 f = d.target - d.pos;
    if (d.level) f.y = 0.0f;
    fwd = normalize(f);
    right = normalize(cross(fwd, V3(0, 1, 0)));
    up = cross(right, fwd);
    if (!d.level) {
      fwd = normalize(d.target - d.pos);
      right = normalize(cross(fwd, V3(0, 1, 0)));
      up = cross(right, fwd);
    }
    if (useVfov) tanHalfV = std::tan(0.5f * vfovDeg * PI / 180.0f);
    else tanHalfV = std::tan(0.5f * d.hfovDeg * PI / 180.0f) / aspect;
    shiftX = d.shiftX;
    shiftY = d.shiftY;
  }
  Ray gen(float px, float py, int W, int H) const {
    float sx = ((2.0f * px / W) - 1.0f + shiftX) * tanHalfV * aspect;
    float sy = (1.0f - (2.0f * py / H) + shiftY) * tanHalfV;
    Ray r;
    r.o = pos;
    r.d = normalize(fwd + right * sx + up * sy);
    return r;
  }
};

struct Settings {
  int W = 960, H = 540, spp = 32, maxDepth = 6;
  uint32_t seed = 1;
  bool aerial = true;
};

struct Aov {
  V3 albedo, normal;
  float depth = 0.0f, sunvis = 0.0f;
  bool set = false;
};

struct Integrator {
  const Scene& sc;
  const Sky& sky;
  Settings set;

  static float powerH(float a, float b) { float a2 = a * a, b2 = b * b; return a2 / (a2 + b2 + 1e-30f); }

  V3 sampleSunDir(float u1, float u2) const {
    float cosT = 1.0f - u1 * (1.0f - sky.sunCosMax);
    float sinT = std::sqrt(std::max(0.0f, 1.0f - cosT * cosT));
    float phi = 2.0f * PI * u2;
    V3 t, b;
    onb(sky.sunDir, t, b);
    return t * (sinT * std::cos(phi)) + b * (sinT * std::sin(phi)) + sky.sunDir * cosT;
  }

  V3 Li(Ray r, RNG& rng, Aov& aov) const {
    V3 L(0.0f), beta(1.0f);
    bool specular = true;
    float lastPdf = 0.0f;
    float travelled = 0.0f;
    const float sunPdf = 1.0f / sky.sunSolidAngle;
    for (int depth = 0; depth < set.maxDepth; depth++) {
      Hit h;
      if (!sc.intersect(r, 1e-4f, INF, h)) {
        V3 Le = sky.envLookup(r.d);
        float w = specular ? 1.0f : powerH(lastPdf, sky.pdfEnv(r.d));
        L += beta * Le * w;
        if (dot(r.d, sky.sunDir) >= sky.sunCosMax) {
          float ws = specular ? 1.0f : powerH(lastPdf, sunPdf);
          L += beta * sky.sunRadiance * ws;
        }
        if (!aov.set) {
          float m = std::max(maxc(Le), 1e-6f);
          aov.albedo = Le / m;
          aov.normal = V3(0.0f);
          aov.depth = 1e4f;
          aov.sunvis = 0.0f;
          aov.set = true;
        }
        break;
      }
      V3 p = r.o + r.d * h.t;
      if (depth == 0 && set.aerial) {
        V3 ins, tr;
        sky.aerial(r.d, h.t, ins, tr);
        L += beta * ins;
        beta *= tr;
      }
      travelled += h.t;
      const Prim* prim = h.prim >= 0 ? &sc.prims[h.prim] : nullptr;
      int mat = prim ? prim->mat : sc.groundMat;
      // Full material detail and rounded arrises only where the camera can
      // resolve them: the first surface, or the first one seen through water.
      bool full = depth == 0 || (specular && depth <= 2);
      V3 ns = full ? sc.shadingNormal(h.prim, p, h.n) : h.n;
      BSDF b = full ? shadeMaterial(mat, p, ns, prim) : shadeMaterialLite(mat, p, ns, prim);
      if (dot(b.n, h.n) < 0.02f) b.n = ns;   // bump never flips a face away from the viewer
      V3 wo = -r.d;

      if (b.dielectric) {
        V3 n = b.n;
        bool entering = r.d.y < 0.0f;
        float eta = entering ? 1.0f / b.ior : b.ior;
        float cosi = clampf(dot(wo, n), 0.0f, 1.0f);
        float F = fresnelDielectric(cosi, eta);
        if (rng.next() < F) {
          r.d = normalize(r.d + n * (2.0f * cosi));
          r.o = p + h.n * 2e-4f;
        } else {
          float k = 1.0f - eta * eta * (1.0f - cosi * cosi);
          float cost = std::sqrt(std::max(0.0f, k));
          r.d = normalize(r.d * eta + n * (eta * cosi - cost));
          r.o = p - h.n * 2e-4f;
          if (entering) beta *= V3(0.93f, 0.97f, 0.96f);   // a few cm of slightly green water
        }
        specular = true;
        continue;
      }

      V3 geomN = h.n;
      V3 origin = p + geomN * (2e-4f + 2e-6f * travelled);
      if (!aov.set) {
        aov.albedo = V3(std::min(1.0f, b.albedo.x + b.metal * b.f0.x), std::min(1.0f, b.albedo.y + b.metal * b.f0.y),
                        std::min(1.0f, b.albedo.z + b.metal * b.f0.z));
        aov.normal = b.n;
        aov.depth = travelled;
      }

      // --- next event: sun ---
      bool sunVisibleHere = false;
      if (sky.sunTrans.x + sky.sunTrans.y + sky.sunTrans.z > 0.0f) {
        V3 wi = sampleSunDir(rng.next(), rng.next());
        float cosL = dot(b.n, wi);
        if (cosL > 0.0f && dot(geomN, wi) > 0.0f) {
          if (!sc.occluded(Ray{origin, wi}, 1e-4f, INF)) {
            sunVisibleHere = true;
            float pdfB;
            V3 f = evalBSDF(b, wo, wi, pdfB);
            float w = powerH(sunPdf, pdfB);
            L += beta * f * sky.sunRadiance * (cosL * w / sunPdf);
          }
        }
      }
      if (!aov.set) {
        aov.sunvis = sunVisibleHere ? 1.0f : 0.0f;
        aov.set = true;
      }

      // --- next event: sky ---
      {
        V3 wi; float pdfL;
        V3 Ls = sky.sampleEnv(rng.next(), rng.next(), wi, pdfL);
        float cosL = dot(b.n, wi);
        if (cosL > 0.0f && dot(geomN, wi) > 0.0f && pdfL > 0.0f) {
          if (!sc.occluded(Ray{origin, wi}, 1e-4f, INF)) {
            float pdfB;
            V3 f = evalBSDF(b, wo, wi, pdfB);
            float w = powerH(pdfL, pdfB);
            L += beta * f * Ls * (cosL * w / pdfL);
          }
        }
      }

      // --- continue the path ---
      V3 wi, f; float pdf;
      if (!sampleBSDF(b, wo, rng.next(), rng.next(), rng.next(), wi, f, pdf)) break;
      if (dot(geomN, wi) <= 0.0f) break;
      beta *= f * (dot(b.n, wi) / pdf);
      specular = false;
      lastPdf = pdf;
      r.o = origin;
      r.d = wi;
      if (depth >= 2) {
        float q = std::max(0.05f, 1.0f - maxc(beta));
        if (rng.next() < q) break;
        beta *= 1.0f / (1.0f - q);
      }
      if (!(beta.x == beta.x) || maxc(beta) > 1e4f) break;  // guard against NaN / fireflies
    }
    return L;
  }
};

static void writeF32(const std::string& path, const std::vector<float>& v) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) { std::fprintf(stderr, "cannot write %s\n", path.c_str()); std::exit(2); }
  std::fwrite(v.data(), sizeof(float), v.size(), f);
  std::fclose(f);
}

static double parseClock(const std::string& s) {
  int h = 0, m = 0; double sec = 0;
  if (std::sscanf(s.c_str(), "%d:%d:%lf", &h, &m, &sec) >= 2) return h * 60.0 + m + sec / 60.0;
  return std::atof(s.c_str()) * 60.0;
}

static bool cameraPreset(const std::string& name, CameraDesc& c) {
  if (name == "court") { c.pos = V3(3.6f, 1.55f, 0.9f); c.target = V3(3.6f, 1.55f, 14.0f); c.hfovDeg = 84.0f; c.shiftY = 0.22f; return true; }
  if (name == "court_p") { c.pos = V3(3.6f, 1.55f, 0.9f); c.target = V3(3.6f, 1.55f, 14.0f); c.hfovDeg = 74.0f; c.shiftY = 0.30f; return true; }
  // Street hero: looking north up the street, the blazing east facade on the
  // left with the gate recess, the shadow of the far wall on the right.
  if (name == "street") { c.pos = V3(16.4f, 1.62f, 8.2f); c.target = V3(10.4f, 1.62f, -20.0f); c.hfovDeg = 76.0f; c.shiftY = 0.46f; return true; }
  if (name == "street_p") { c.pos = V3(16.2f, 1.62f, 8.6f); c.target = V3(11.6f, 1.62f, -20.0f); c.hfovDeg = 64.0f; c.shiftY = 0.30f; return true; }
  if (name == "qudra") { c.pos = V3(-3.0f, 1.5f, 17.2f); c.target = V3(14.0f, 1.5f, -4.0f); c.hfovDeg = 80.0f; c.shiftY = 0.24f; return true; }
  if (name == "qudra_p") { c.pos = V3(-13.5f, 1.55f, 12.5f); c.target = V3(6.0f, 1.55f, -6.0f); c.hfovDeg = 62.0f; c.shiftY = 0.3f; return true; }
  return false;
}

int main(int argc, char** argv) {
  std::map<std::string, std::string> a;
  for (int i = 1; i + 1 < argc; i += 2) a[std::string(argv[i]).substr(2)] = argv[i + 1];
  auto get = [&](const char* k, const char* d) { return a.count(k) ? a[k] : std::string(d); };

  Settings set;
  set.W = std::atoi(get("w", "960").c_str());
  set.H = std::atoi(get("h", "540").c_str());
  set.spp = std::atoi(get("spp", "32").c_str());
  set.maxDepth = std::atoi(get("depth", "6").c_str());
  set.seed = (uint32_t)std::atoi(get("seed", "1").c_str());
  std::string sceneName = get("scene", "house"), camName = get("cam", "court"), out = get("out", "out");
  int year = 2026, month = 6, day = 21;
  std::sscanf(get("date", "2026-06-21").c_str(), "%d-%d-%d", &year, &month, &day);
  double minutes = parseClock(get("time", "12:20:43"));
  double lat = std::atof(get("lat", "25.2048").c_str()), lon = std::atof(get("lon", "55.2708").c_str());

  Scene sc;
  if (sceneName == "house") buildHouse(sc);
  else if (sceneName == "qudra") buildQudra(sc);
  else { std::fprintf(stderr, "unknown scene %s\n", sceneName.c_str()); return 2; }
  sc.build();

  CameraDesc cd;
  if (!cameraPreset(camName, cd)) { std::fprintf(stderr, "unknown camera %s\n", camName.c_str()); return 2; }
  if (a.count("campos")) std::sscanf(a["campos"].c_str(), "%f,%f,%f", &cd.pos.x, &cd.pos.y, &cd.pos.z);
  if (a.count("camtgt")) std::sscanf(a["camtgt"].c_str(), "%f,%f,%f", &cd.target.x, &cd.target.y, &cd.target.z);
  if (a.count("hfov")) cd.hfovDeg = std::atof(a["hfov"].c_str());
  if (a.count("shift")) cd.shiftY = std::atof(a["shift"].c_str());
  Camera cam;
  cam.setup(cd, set.W, set.H, a.count("vfov") > 0, a.count("vfov") ? std::atof(a["vfov"].c_str()) : 0.0f);

  SunPos sp = solarPosition(year, month, day, minutes, lat, lon);
  // Scene orientation: rotate the world sun direction into the scene's local
  // frame (buildings are modelled axis-aligned; the site is not).
  float orientDeg = a.count("orient") ? std::atof(a["orient"].c_str()) : (sceneName == "house" ? HOUSE_ORIENT_DEG : 0.0f);
  {
    float psi = orientDeg * PI / 180.0f;
    V3 xl(std::cos(psi), 0.0f, std::sin(psi)), zl(-std::sin(psi), 0.0f, std::cos(psi));
    V3 w = sp.dir;
    sp.dir = normalize(V3(dot(w, xl), w.y, dot(w, zl)));
  }
  AtmosphereParams ap;
  if (sceneName == "qudra") { ap.aod = 0.45f; ap.angstrom = 0.2f; }   // inland desert: more, coarser dust
  if (a.count("aod")) ap.aod = std::atof(a["aod"].c_str());
  Sky sky;
  auto t0 = std::chrono::steady_clock::now();
  sky.build(sp.dir, ap, sceneName == "qudra" ? 0.40f : 0.34f);
  auto t1 = std::chrono::steady_clock::now();

  Integrator integ{sc, sky, set};
  const int W = set.W, H = set.H;
  std::vector<float> color((size_t)W * H * 3), albedo((size_t)W * H * 3), normal((size_t)W * H * 3),
      depth((size_t)W * H), sunvis((size_t)W * H);
  int rowsDone = 0;
#pragma omp parallel for schedule(dynamic, 1)
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      V3 c(0.0f), al(0.0f), nm(0.0f);
      float dp = 0.0f, sv = 0.0f;
      for (int s = 0; s < set.spp; s++) {
        // Seed depends on pixel and sample only — not on the frame — so a fixed
        // camera scrubbed through time keeps a stable noise pattern (no boil).
        RNG rng(pcg((uint32_t)(y * W + x) * 9781u + pcg((uint32_t)s * 6271u + set.seed)));
        Ray r = cam.gen(x + rng.next(), y + rng.next(), W, H);
        Aov aov;
        V3 li = integ.Li(r, rng, aov);
        c += li;
        al += aov.albedo;
        nm += aov.normal;
        dp += aov.depth;
        sv += aov.sunvis;
      }
      float inv = 1.0f / set.spp;
      size_t i = (size_t)y * W + x;
      color[i * 3 + 0] = c.x * inv; color[i * 3 + 1] = c.y * inv; color[i * 3 + 2] = c.z * inv;
      albedo[i * 3 + 0] = al.x * inv; albedo[i * 3 + 1] = al.y * inv; albedo[i * 3 + 2] = al.z * inv;
      V3 n = nm * inv;
      normal[i * 3 + 0] = n.x; normal[i * 3 + 1] = n.y; normal[i * 3 + 2] = n.z;
      depth[i] = dp * inv;
      sunvis[i] = sv * inv;
    }
#pragma omp atomic
    rowsDone++;
    if (omp_get_thread_num() == 0 && (rowsDone % 32 == 0))
      std::fprintf(stderr, "\r  %s %s %5.1f%%", sceneName.c_str(), camName.c_str(), 100.0 * rowsDone / H);
  }
  auto t2 = std::chrono::steady_clock::now();
  std::fprintf(stderr, "\n");

  std::string cmd = "mkdir -p '" + out + "'";
  if (std::system(cmd.c_str()) != 0) return 2;
  writeF32(out + "/color.f32", color);
  writeF32(out + "/albedo.f32", albedo);
  writeF32(out + "/normal.f32", normal);
  writeF32(out + "/depth.f32", depth);
  writeF32(out + "/sunvis.f32", sunvis);
  double skySec = std::chrono::duration<double>(t1 - t0).count();
  double renderSec = std::chrono::duration<double>(t2 - t1).count();
  std::ofstream m(out + "/meta.json");
  m << "{\n"
    << "  \"scene\": \"" << sceneName << "\", \"camera\": \"" << camName << "\",\n"
    << "  \"width\": " << W << ", \"height\": " << H << ", \"spp\": " << set.spp << ",\n"
    << "  \"date\": \"" << year << "-" << month << "-" << day << "\", \"localMinutes\": " << minutes << ",\n"
    << "  \"lat\": " << lat << ", \"lon\": " << lon << ",\n"
    << "  \"sunAltitudeDeg\": " << sp.altitudeDeg << ", \"sunAzimuthDeg\": " << sp.azimuthDeg << ",\n"
    << "  \"sunDir\": [" << sp.dir.x << ", " << sp.dir.y << ", " << sp.dir.z << "],\n"
    << "  \"sunTransmittance\": [" << sky.sunTrans.x << ", " << sky.sunTrans.y << ", " << sky.sunTrans.z << "],\n"
    << "  \"skyBuildSeconds\": " << skySec << ", \"renderSeconds\": " << renderSec << "\n"
    << "}\n";
  std::fprintf(stderr, "  sky %.1fs render %.1fs  alt %.2f az %.2f\n", skySec, renderSec, sp.altitudeDeg, sp.azimuthDeg);
  return 0;
}
