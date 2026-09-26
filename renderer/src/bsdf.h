// Microfacet BSDF: Lambert diffuse + GGX specular (Smith separable G),
// and a smooth dielectric for water. Perceptual roughness r maps to alpha = r^2.
#pragma once
#include "common.h"
#include "materials.h"

namespace zw {

inline float ggxD(float NoH, float a) {
  float a2 = a * a;
  float d = NoH * NoH * (a2 - 1.0f) + 1.0f;
  return a2 / (PI * d * d + 1e-12f);
}
inline float smithG1(float NoX, float a) {
  float a2 = a * a;
  return 2.0f * NoX / (NoX + std::sqrt(a2 + (1.0f - a2) * NoX * NoX) + 1e-12f);
}
inline V3 fresnelSchlick(V3 f0, float c) {
  float f = std::pow(1.0f - saturate(c), 5.0f);
  return f0 + (V3(1.0f) - f0) * f;
}
inline float fresnelDielectric(float cosi, float eta) {
  // eta = n_incident / n_transmitted
  float sint2 = eta * eta * std::max(0.0f, 1.0f - cosi * cosi);
  if (sint2 >= 1.0f) return 1.0f;
  float cost = std::sqrt(1.0f - sint2);
  float rs = (eta * cosi - cost) / (eta * cosi + cost);
  float rp = (cosi - eta * cost) / (cosi + eta * cost);
  return 0.5f * (rs * rs + rp * rp);
}

inline float specularProbability(const BSDF& b, float NoV) {
  float fs = lum(fresnelSchlick(b.f0, NoV));
  float fd = lum(b.albedo) * (1.0f - b.metal);
  return clampf(fs / (fs + fd + 1e-4f), 0.04f, 0.96f);
}

// Thin translucent sheet (a leaf): Lambert reflection on the lit side,
// Lambert transmission through it. Choice of side is proportional to the
// albedo of each lobe.
inline float thinReflectProb(const BSDF& b) {
  float r = lum(b.albedo), t = lum(b.trans);
  return clampf(r / (r + t + 1e-6f), 0.1f, 0.9f);
}
inline V3 evalThin(const BSDF& b, V3 wo, V3 wi, float& pdf) {
  float NoL = dot(b.n, wi), NoV = dot(b.n, wo);
  float pr = thinReflectProb(b);
  if (NoL * NoV > 0.0f) { pdf = pr * std::fabs(NoL) * INV_PI; return b.albedo * INV_PI; }
  pdf = (1.0f - pr) * std::fabs(NoL) * INV_PI;
  return b.trans * INV_PI;
}

// Evaluates f(wo, wi) and the combined sampling pdf (solid angle).
inline V3 evalBSDF(const BSDF& b, V3 wo, V3 wi, float& pdf) {
  if (b.thin) return evalThin(b, wo, wi, pdf);
  float NoL = dot(b.n, wi), NoV = dot(b.n, wo);
  pdf = 0.0f;
  if (NoL <= 0.0f || NoV <= 0.0f) return V3(0.0f);
  float a = std::max(b.alpha * b.alpha, 0.0025f);
  V3 h = normalize(wi + wo);
  float NoH = std::max(dot(b.n, h), 0.0f), VoH = std::max(dot(wo, h), 1e-6f);
  float D = ggxD(NoH, a);
  float G = smithG1(NoL, a) * smithG1(NoV, a);
  V3 F = fresnelSchlick(b.f0, VoH);
  V3 spec = F * (D * G / (4.0f * NoL * NoV));
  // Diffuse loses what the coat reflects at this view angle (energy balance).
  V3 kd = b.albedo * ((1.0f - b.metal) * (1.0f - lum(fresnelSchlick(b.f0, NoV))));
  V3 f = kd * INV_PI + spec;
  float ps = specularProbability(b, NoV);
  // Visible-normal sampling pdf (Heitz 2018): G1(wo) D(h) / (4 NoV). Unlike
  // the classic D NoH / (4 VoH) it stays bounded at grazing angles, which is
  // where thin cylinders (cables, twigs) live; the classic form handed rare
  // direct-sun hits an MIS weight of 1 and made fireflies.
  pdf = (1.0f - ps) * NoL * INV_PI + ps * smithG1(NoV, a) * D / (4.0f * NoV);
  (void)NoH; (void)VoH;
  return f;
}

// Samples wi; returns false if the sample is invalid.
inline bool sampleBSDF(const BSDF& b, V3 wo, float u0, float u1, float u2, V3& wi, V3& f, float& pdf) {
  if (b.thin) {
    V3 nv = dot(b.n, wo) >= 0.0f ? b.n : -b.n;          // the viewer's side
    V3 side = u0 < thinReflectProb(b) ? nv : -nv;       // reflect or transmit
    V3 t, bt;
    onb(side, t, bt);
    float r = std::sqrt(u1), phi = 2.0f * PI * u2;
    wi = t * (r * std::cos(phi)) + bt * (r * std::sin(phi)) + side * std::sqrt(std::max(0.0f, 1.0f - u1));
    f = evalThin(b, wo, wi, pdf);
    return pdf > 1e-8f;
  }
  float NoV = dot(b.n, wo);
  if (NoV <= 0.0f) return false;
  V3 t, bt;
  onb(b.n, t, bt);
  float ps = specularProbability(b, NoV);
  if (u0 < ps) {
    // GGX visible-normal sampling (Heitz 2018, "Sampling the GGX
    // Distribution of Visible Normals"), in the local frame (t, bt, n).
    float a = std::max(b.alpha * b.alpha, 0.0025f);
    V3 Ve(dot(wo, t), dot(wo, bt), NoV);
    V3 Vh = normalize(V3(a * Ve.x, a * Ve.y, Ve.z));            // stretch to the hemisphere config
    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    V3 T1 = lensq > 1e-12f ? V3(-Vh.y, Vh.x, 0.0f) * (1.0f / std::sqrt(lensq)) : V3(1.0f, 0.0f, 0.0f);
    V3 T2 = cross(Vh, T1);
    float r = std::sqrt(u1), phi = 2.0f * PI * u2;
    float t1 = r * std::cos(phi), t2 = r * std::sin(phi);
    float sv = 0.5f * (1.0f + Vh.z);
    t2 = (1.0f - sv) * std::sqrt(std::max(0.0f, 1.0f - t1 * t1)) + sv * t2;   // warp to the visible half-disc
    V3 Nh = T1 * t1 + T2 * t2 + Vh * std::sqrt(std::max(0.0f, 1.0f - t1 * t1 - t2 * t2));
    V3 Ne = normalize(V3(a * Nh.x, a * Nh.y, std::max(0.0f, Nh.z)));  // unstretch
    V3 h = t * Ne.x + bt * Ne.y + b.n * Ne.z;
    wi = h * (2.0f * dot(wo, h)) - wo;
  } else {
    float r = std::sqrt(u1), phi = 2.0f * PI * u2;
    wi = t * (r * std::cos(phi)) + bt * (r * std::sin(phi)) + b.n * std::sqrt(std::max(0.0f, 1.0f - u1));
  }
  if (dot(b.n, wi) <= 0.0f) return false;
  f = evalBSDF(b, wo, wi, pdf);
  return pdf > 1e-8f;
}

}  // namespace zw
