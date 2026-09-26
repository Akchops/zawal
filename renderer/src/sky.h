// Physically based sky for Dubai in June.
//
// Single-scattering spherical atmosphere (Rayleigh + dust-laden Mie + ozone)
// integrated numerically for the actual sun direction, plus a cheap isotropic
// multiple-scattering term. Baked into a lat-long environment map that is
// importance-sampled by luminance for next-event estimation.
//
// Summer Gulf air is dusty: aerosol optical depth ~0.3-0.6 at 550 nm. That is
// what makes the sky white near the horizon, the shade bright and slightly
// warm, and the sun visibly amber below ~15 deg. AOD is a parameter so the
// look can be tuned toward "clear winter" or "shamal haze" per scene.
#pragma once
#include "common.h"

namespace zw {

struct AtmosphereParams {
  float aod = 0.38f;                 // aerosol optical depth at 550 nm
  float angstrom = 0.45f;            // spectral slope of dust extinction
  V3 dustSSA = V3(0.965f, 0.935f, 0.885f);  // dust single-scattering albedo: absorbs blue a little
  float dustG = 0.74f;               // forward-scattering asymmetry (Cornette-Shanks)
  float dustH = 1300.0f;             // aerosol scale height, m
  float msStrength = 0.55f;          // multiple-scattering fill, tuned against measured Dubai DHI/GHI
};

struct Sky {
  // Sun
  V3 sunDir;
  float sunCosMax = std::cos(0.2666f * PI / 180.0f);   // physical disc radius 0.2666 deg
  float sunSolidAngle = 0.0f;
  V3 sunTrans;        // atmospheric transmittance along the sun path at ground
  V3 sunRadiance;     // radiance of the disc as seen from the ground
  V3 sunIrradiance;   // = sunRadiance * solid angle, at normal incidence

  // Environment (sky dome without the sun disc), lat-long, theta from zenith.
  int W = 512, H = 256;
  std::vector<V3> env;
  std::vector<float> rowCdf, colCdf;  // (H+1) and H*(W+1)
  float envTotal = 0.0f;

  // Aerial perspective LUT: azimuth-relative-to-sun x elevation x distance.
  static constexpr int AP_AZ = 48, AP_EL = 24, AP_D = 20;
  std::vector<V3> apInscatter, apTrans;
  float apMaxDist = 40000.0f;

  AtmosphereParams P;
  float groundAlbedo = 0.32f;

  // ---- atmosphere model -------------------------------------------------
  static constexpr float Re = 6360e3f, Ra = 6420e3f;
  V3 betaR() const { return V3(5.802e-6f, 13.558e-6f, 33.1e-6f); }
  V3 betaO() const { return V3(0.650e-6f, 1.881e-6f, 0.085e-6f); }
  V3 betaMext() const {
    float b550 = P.aod / P.dustH;
    return V3(b550 * std::pow(550.0f / 680.0f, P.angstrom), b550, b550 * std::pow(550.0f / 440.0f, P.angstrom));
  }
  V3 betaMsca() const { return betaMext() * P.dustSSA; }

  static float densR(float h) { return std::exp(-h / 8000.0f); }
  float densM(float h) const { return std::exp(-h / P.dustH); }
  static float densO(float h) { return std::max(0.0f, 1.0f - std::fabs(h - 25000.0f) / 15000.0f); }

  // Distance along (o,d) to the sphere of radius R centred at Earth centre.
  static bool raySphere(V3 o, V3 d, float R, float& t0, float& t1) {
    float b = dot(o, d), c = dot(o, o) - R * R, disc = b * b - c;
    if (disc < 0) return false;
    float s = std::sqrt(disc);
    t0 = -b - s; t1 = -b + s;
    return true;
  }

  // Optical depths (Rayleigh, Mie, ozone) from point o along d to the top.
  void opticalDepthToTop(V3 o, V3 d, float& tR, float& tM, float& tO, bool& blocked) const {
    float a0, a1, g0, g1;
    blocked = false;
    tR = tM = tO = 0;
    if (raySphere(o, d, Re, g0, g1) && g0 > 0) { blocked = true; return; }
    raySphere(o, d, Ra, a0, a1);
    float len = a1;
    const int N = 16;
    // Quadratic sample spacing: dense near the start where the air is thick.
    float prev = 0;
    for (int i = 1; i <= N; i++) {
      float u = (float)i / N, t = len * u * u, dt = t - prev;
      float tm = 0.5f * (t + prev);
      prev = t;
      float h = length(o + d * tm) - Re;
      tR += densR(h) * dt; tM += densM(h) * dt; tO += densO(h) * dt;
    }
  }

  V3 transmittance(float tR, float tM, float tO) const {
    return vexp(-(betaR() * tR + betaMext() * tM + betaO() * tO));
  }

  static float phaseR(float mu) { return 3.0f / (16.0f * PI) * (1.0f + mu * mu); }
  float phaseM(float mu) const {
    float g = P.dustG, g2 = g * g;
    return 3.0f / (8.0f * PI) * ((1.0f - g2) * (1.0f + mu * mu)) /
           ((2.0f + g2) * std::pow(std::max(1e-4f, 1.0f + g2 - 2.0f * g * mu), 1.5f));
  }

  // Inscattered radiance and transmittance along a view ray from an observer at
  // height h0, over distance maxDist (or to the atmosphere top).
  void integrate(V3 d, float maxDist, V3& L, V3& Tview) const {
    V3 o(0.0f, Re + 2.0f, 0.0f);
    float a0, a1;
    raySphere(o, d, Ra, a0, a1);
    float len = std::min(a1, maxDist);
    float g0, g1;
    if (raySphere(o, d, Re, g0, g1) && g0 > 0) len = std::min(len, g0);
    const int N = 40;
    float mu = dot(d, sunDir);
    float pR = phaseR(mu), pM = phaseM(mu);
    V3 bR = betaR(), bMs = betaMsca(), bMe = betaMext(), bO = betaO();
    float tR = 0, tM = 0, tO = 0, prev = 0;
    V3 acc(0.0f), accMS(0.0f);
    for (int i = 1; i <= N; i++) {
      float u = (float)i / N, t = len * u * u, dt = t - prev;
      float tm = 0.5f * (t + prev);
      prev = t;
      V3 x = o + d * tm;
      float h = length(x) - Re;
      float dR = densR(h), dM = densM(h), dO = densO(h);
      tR += dR * dt; tM += dM * dt; tO += dO * dt;
      V3 Tv = vexp(-(bR * tR + bMe * tM + bO * tO));
      float sR, sM, sO; bool blocked;
      V3 up = normalize(x);
      V3 xs = up * (Re + std::max(h, 1.0f));
      opticalDepthToTop(xs, sunDir, sR, sM, sO, blocked);
      V3 scatterCoef = bR * dR + bMs * dM;
      if (!blocked) {
        V3 Ts = vexp(-(bR * sR + bMe * sM + bO * sO));
        acc += Tv * Ts * (bR * (dR * pR) + bMs * (dM * pM)) * dt;
        // Multiple-scattering fill: isotropic re-emission of the light that the
        // column has already scattered once. Keeps shade bright and the horizon
        // milky, as in real hazy air; single scattering alone reads as a
        // too-dark, too-saturated "CG" sky.
        float cosSun = std::max(0.0f, dot(up, sunDir));
        accMS += Tv * Ts * scatterCoef * (dt * (0.25f + 0.75f * cosSun) / (4.0f * PI));
      }
    }
    L = acc + accMS * P.msStrength * 2.2f;
    Tview = vexp(-(bR * tR + bMe * tM + bO * tO));
  }

  // ---- environment map ----------------------------------------------------
  static V3 dirFromUV(float u, float v) {
    float theta = v * PI, phi = u * 2.0f * PI;
    return V3(std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi));
  }
  static void uvFromDir(V3 d, float& u, float& v) {
    float phi = std::atan2(d.z, d.x);
    if (phi < 0) phi += 2.0f * PI;
    u = phi / (2.0f * PI);
    v = std::acos(clampf(d.y, -1.0f, 1.0f)) / PI;
  }

  void build(V3 sun, const AtmosphereParams& params, float groundAlb = 0.32f) {
    P = params;
    groundAlbedo = groundAlb;
    sunDir = normalize(sun);
    sunSolidAngle = 2.0f * PI * (1.0f - sunCosMax);
    // Transmittance for the sun path from the ground observer.
    float tR, tM, tO; bool blocked;
    opticalDepthToTop(V3(0.0f, Re + 2.0f, 0.0f), sunDir, tR, tM, tO, blocked);
    // Near and below the horizon, fade instead of switching: the disc sinks
    // behind the horizon over ~0.53 deg, not instantly.
    float horizonFade = smoothstep(-0.0093f, 0.0093f, sunDir.y);
    sunTrans = blocked && sunDir.y < -0.0093f ? V3(0.0f) : transmittance(tR, tM, tO) * horizonFade;
    sunRadiance = sunTrans / sunSolidAngle;
    sunIrradiance = sunTrans;

    env.assign((size_t)W * H, V3(0.0f));
    // Ground irradiance estimate for the below-horizon half: sun on a horizontal
    // plane plus a rough sky term; only seen by rays that escape the scene's
    // own ground plane, which is rare.
    V3 groundE = sunTrans * std::max(0.0f, sunDir.y);
#pragma omp parallel for schedule(dynamic, 4)
    for (int j = 0; j < H; j++) {
      for (int i = 0; i < W; i++) {
        V3 d = dirFromUV((i + 0.5f) / W, (j + 0.5f) / H);
        V3 L, Tv;
        if (d.y > -0.02f) {
          V3 dd = d;
          if (dd.y < 0.002f) dd = normalize(V3(d.x, 0.002f, d.z));
          integrate(dd, 1e9f, L, Tv);
          if (d.y < 0.0f) {
            // Blend to ground just below the horizon so the seam is soft.
            float t = smoothstep(0.0f, -0.02f, d.y);
            V3 G = groundE * (groundAlbedo * INV_PI) + L * 0.35f;
            L = lerp(L, G, t);
          }
        } else {
          L = groundE * (groundAlbedo * INV_PI);
          V3 Lh, Th;
          integrate(normalize(V3(d.x, 0.002f, d.z)), 1e9f, Lh, Th);
          L += Lh * 0.35f;
        }
        env[(size_t)j * W + i] = L;
      }
    }
    buildCdf();
    buildAerial();
  }

  void buildCdf() {
    rowCdf.assign(H + 1, 0.0f);
    colCdf.assign((size_t)H * (W + 1), 0.0f);
    for (int j = 0; j < H; j++) {
      float sinT = std::sin((j + 0.5f) / H * PI);
      float* c = &colCdf[(size_t)j * (W + 1)];
      c[0] = 0.0f;
      for (int i = 0; i < W; i++) c[i + 1] = c[i] + (lum(env[(size_t)j * W + i]) + 1e-6f) * sinT;
      rowCdf[j + 1] = rowCdf[j] + c[W];
    }
    envTotal = rowCdf[H];
  }

  V3 envLookup(V3 d) const {
    float u, v;
    uvFromDir(d, u, v);
    float x = u * W - 0.5f, y = v * H - 0.5f;
    int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
    float fx = x - x0, fy = y - y0;
    auto at = [&](int i, int j) {
      i = (i % W + W) % W;
      j = std::max(0, std::min(H - 1, j));
      return env[(size_t)j * W + i];
    };
    return lerp(lerp(at(x0, y0), at(x0 + 1, y0), fx), lerp(at(x0, y0 + 1), at(x0 + 1, y0 + 1), fx), fy);
  }

  // Sample a direction proportional to env luminance * sin(theta).
  V3 sampleEnv(float r1, float r2, V3& wi, float& pdf) const {
    float target = r1 * envTotal;
    int j = (int)(std::upper_bound(rowCdf.begin(), rowCdf.end(), target) - rowCdf.begin()) - 1;
    j = std::max(0, std::min(H - 1, j));
    const float* c = &colCdf[(size_t)j * (W + 1)];
    float t2 = r2 * c[W];
    int i = (int)(std::upper_bound(c, c + W + 1, t2) - c) - 1;
    i = std::max(0, std::min(W - 1, i));
    // Jitter inside the texel using the leftover fraction of each uniform.
    float fu = (t2 - c[i]) / std::max(1e-12f, c[i + 1] - c[i]);
    float fv = (target - rowCdf[j]) / std::max(1e-12f, rowCdf[j + 1] - rowCdf[j]);
    float u = (i + saturate(fu)) / W, v = (j + saturate(fv)) / H;
    wi = dirFromUV(u, v);
    pdf = pdfEnv(wi);
    return envLookup(wi);
  }

  float pdfEnv(V3 d) const {
    float u, v;
    uvFromDir(d, u, v);
    int i = std::min(W - 1, (int)(u * W)), j = std::min(H - 1, (int)(v * H));
    float sinT = std::sin((j + 0.5f) / H * PI);
    float p = (lum(env[(size_t)j * W + i]) + 1e-6f) * sinT / envTotal;
    float sinTheta = std::max(1e-4f, std::sin(v * PI));
    return p * (float)(W * H) / (2.0f * PI * PI * sinTheta);
  }

  // ---- aerial perspective ------------------------------------------------
  void buildAerial() {
    apInscatter.assign((size_t)AP_AZ * AP_EL * AP_D, V3(0.0f));
    apTrans.assign((size_t)AP_AZ * AP_EL * AP_D, V3(1.0f));
    float sunAz = std::atan2(sunDir.z, sunDir.x);
#pragma omp parallel for schedule(dynamic, 2)
    for (int a = 0; a < AP_AZ; a++) {
      for (int e = 0; e < AP_EL; e++) {
        float az = sunAz + (a + 0.5f) / AP_AZ * 2.0f * PI;
        // Elevation -0.3..+1.2 rad, denser near the horizon where it matters.
        float el = -0.3f + 1.5f * std::pow((e + 0.5f) / AP_EL, 1.6f);
        V3 d(std::cos(el) * std::cos(az), std::sin(el), std::cos(el) * std::sin(az));
        V3 dd = d.y < 0.001f ? normalize(V3(d.x, 0.001f, d.z)) : d;
        for (int k = 0; k < AP_D; k++) {
          float dist = apDist(k);
          V3 L, T;
          integrate(dd, dist, L, T);
          size_t idx = ((size_t)a * AP_EL + e) * AP_D + k;
          apInscatter[idx] = L;
          apTrans[idx] = T;
        }
      }
    }
  }
  float apDist(int k) const { return 5.0f * std::pow(apMaxDist / 5.0f, (float)k / (AP_D - 1)); }

  void aerial(V3 d, float dist, V3& inscatter, V3& trans) const {
    if (dist < 5.0f) { inscatter = V3(0.0f); trans = V3(1.0f); return; }
    float sunAz = std::atan2(sunDir.z, sunDir.x);
    float az = std::atan2(d.z, d.x) - sunAz;
    az = az - 2.0f * PI * std::floor(az / (2.0f * PI));
    float fa = az / (2.0f * PI) * AP_AZ - 0.5f;
    float el = std::asin(clampf(d.y, -1.0f, 1.0f));
    float fe = std::pow(saturate((el + 0.3f) / 1.5f), 1.0f / 1.6f) * AP_EL - 0.5f;
    float fk = std::log(std::min(dist, apMaxDist) / 5.0f) / std::log(apMaxDist / 5.0f) * (AP_D - 1);
    int a0 = (int)std::floor(fa), e0 = (int)std::floor(fe), k0 = (int)std::floor(fk);
    float ta = fa - a0, te = fe - e0, tk = fk - k0;
    auto I = [&](int a, int e, int k, bool tr) {
      a = (a % AP_AZ + AP_AZ) % AP_AZ;
      e = std::max(0, std::min(AP_EL - 1, e));
      k = std::max(0, std::min(AP_D - 1, k));
      size_t idx = ((size_t)a * AP_EL + e) * AP_D + k;
      return tr ? apTrans[idx] : apInscatter[idx];
    };
    auto tri = [&](bool tr) {
      V3 c00 = lerp(I(a0, e0, k0, tr), I(a0, e0, k0 + 1, tr), tk);
      V3 c01 = lerp(I(a0, e0 + 1, k0, tr), I(a0, e0 + 1, k0 + 1, tr), tk);
      V3 c10 = lerp(I(a0 + 1, e0, k0, tr), I(a0 + 1, e0, k0 + 1, tr), tk);
      V3 c11 = lerp(I(a0 + 1, e0 + 1, k0, tr), I(a0 + 1, e0 + 1, k0 + 1, tr), tk);
      return lerp(lerp(c00, c01, te), lerp(c10, c11, te), ta);
    };
    inscatter = tri(false);
    trans = tri(true);
  }
};

}  // namespace zw
