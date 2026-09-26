// ZAWAL offline renderer — shared math, RNG and colour helpers.
//
// Conventions used everywhere in the renderer:
//   world space is right-handed, metres, +X = East, +Y = Up, +Z = South.
//   Radiance is linear Rec.709, relative to a top-of-atmosphere solar
//   irradiance of 1.0 at normal incidence. Exposure is applied in post.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace zw {

constexpr float PI = 3.14159265358979323846f;
constexpr float INV_PI = 1.0f / PI;
constexpr float INF = 1e30f;

struct V3 {
  float x, y, z;
  V3() : x(0), y(0), z(0) {}
  explicit V3(float a) : x(a), y(a), z(a) {}
  V3(float a, float b, float c) : x(a), y(b), z(c) {}
  inline float operator[](int i) const { return (&x)[i]; }
  inline float& operator[](int i) { return (&x)[i]; }
};
inline V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 operator*(V3 a, V3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
inline V3 operator/(V3 a, V3 b) { return {a.x / b.x, a.y / b.y, a.z / b.z}; }
inline V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline V3 operator*(float s, V3 a) { return {a.x * s, a.y * s, a.z * s}; }
inline V3 operator/(V3 a, float s) { float i = 1.0f / s; return {a.x * i, a.y * i, a.z * i}; }
inline V3 operator-(V3 a) { return {-a.x, -a.y, -a.z}; }
inline V3& operator+=(V3& a, V3 b) { a.x += b.x; a.y += b.y; a.z += b.z; return a; }
inline V3& operator-=(V3& a, V3 b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; return a; }
inline V3& operator*=(V3& a, V3 b) { a.x *= b.x; a.y *= b.y; a.z *= b.z; return a; }
inline V3& operator*=(V3& a, float s) { a.x *= s; a.y *= s; a.z *= s; return a; }
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float length(V3 a) { return std::sqrt(dot(a, a)); }
inline V3 normalize(V3 a) { float l = length(a); return l > 1e-20f ? a / l : V3(0, 1, 0); }
inline V3 vmin(V3 a, V3 b) { return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}; }
inline V3 vmax(V3 a, V3 b) { return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}; }
inline V3 vabs(V3 a) { return {std::fabs(a.x), std::fabs(a.y), std::fabs(a.z)}; }
inline V3 vexp(V3 a) { return {std::exp(a.x), std::exp(a.y), std::exp(a.z)}; }
inline float maxc(V3 a) { return std::max(a.x, std::max(a.y, a.z)); }
inline float lum(V3 c) { return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z; }
inline V3 lerp(V3 a, V3 b, float t) { return a + (b - a) * t; }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
inline float saturate(float x) { return clampf(x, 0.0f, 1.0f); }
// C1 ramp. Used instead of a clamp() ramp anywhere the value can end up
// differentiated into a normal (known-traps.md #3: clamp creases).
inline float smoothstep(float a, float b, float x) {
  float t = saturate((x - a) / (b - a));
  return t * t * (3.0f - 2.0f * t);
}
// C2 ramp for height fields that are differentiated twice (bump + lighting).
inline float smootherstep(float a, float b, float x) {
  float t = saturate((x - a) / (b - a));
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}
inline float fractf(float x) { return x - std::floor(x); }
// Smooth |x|: sqrt(x^2 + e) instead of abs(), which is C0 at zero and leaves a
// crease along the symmetry axis once lit (known-traps.md #3).
inline float sabs(float x, float e) { return std::sqrt(x * x + e); }
// Polynomial smooth min / max, C1 continuous. k is the blend width in the same
// units as a and b.
inline float smin(float a, float b, float k) {
  float h = saturate(0.5f + 0.5f * (b - a) / k);
  return lerpf(b, a, h) - k * h * (1.0f - h);
}
inline float smax(float a, float b, float k) {
  float h = saturate(0.5f + 0.5f * (a - b) / k);
  return lerpf(b, a, h) + k * h * (1.0f - h);
}

inline float srgb2linf(float c) {
  return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}
// Author colours as sRGB hex, render in linear.
inline V3 hex(uint32_t h) {
  return V3(srgb2linf(((h >> 16) & 255) / 255.0f), srgb2linf(((h >> 8) & 255) / 255.0f),
            srgb2linf((h & 255) / 255.0f));
}

// Branchless orthonormal basis (Duff et al. 2017).
inline void onb(V3 n, V3& t, V3& b) {
  float s = n.z >= 0.0f ? 1.0f : -1.0f;
  float a = -1.0f / (s + n.z);
  float c = n.x * n.y * a;
  t = V3(1.0f + s * n.x * n.x * a, s * c, -s * n.x);
  b = V3(c, s + n.y * n.y * a, -n.y);
}

// PCG-style integer hash: good avalanche, cheap, deterministic across runs.
inline uint32_t pcg(uint32_t v) {
  uint32_t state = v * 747796405u + 2891336453u;
  uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
}
inline uint32_t hash2i(int x, int y) { return pcg((uint32_t)x * 0x8da6b343u ^ pcg((uint32_t)y + 0x68e31da4u)); }
inline uint32_t hash3i(int x, int y, int z) {
  return pcg((uint32_t)x * 0x8da6b343u ^ pcg((uint32_t)y * 0xd8163841u ^ pcg((uint32_t)z + 0xcb1ab31fu)));
}
inline float hashf(uint32_t h) { return (pcg(h) >> 8) * (1.0f / 16777216.0f); }

struct RNG {
  uint32_t s;
  explicit RNG(uint32_t seed) : s(seed ? seed : 0x9e3779b9u) {}
  inline float next() {
    s = s * 747796405u + 2891336453u;
    uint32_t w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
    w = (w >> 22u) ^ w;
    return (w >> 8) * (1.0f / 16777216.0f);
  }
};

// NaN/Inf test that survives -ffast-math (which folds x == x to true).
inline bool isFiniteF(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return (u & 0x7f800000u) != 0x7f800000u;
}
inline bool isFiniteV(V3 v) { return isFiniteF(v.x) && isFiniteF(v.y) && isFiniteF(v.z); }

struct Ray {
  V3 o, d;
};

}  // namespace zw
