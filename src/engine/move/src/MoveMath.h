// A3 internal vector-algebra helpers shared by the closed-form/iterative Move
// solvers (CrossProduct, RotateLine, RTouch, Match, Gravity, BestFit,
// LeastSquaresAxis). Header-only, anonymous-namespace-free so multiple TUs can
// include it; all functions are `inline`. Original clean-room code, no external
// linear-algebra dependency.
#pragma once
#include <algorithm>
#include <cmath>

#include "opendva/Types.h"

namespace opendva {
namespace movemath {

constexpr double kPi = 3.14159265358979323846;

inline Vec3 add(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 sub(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 scale(const Vec3& a, double s) { return {a.x * s, a.y * s, a.z * s}; }
inline double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline double norm(const Vec3& a) { return std::hypot(a.x, a.y, a.z); }

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline Vec3 normalise(const Vec3& v) {
    const double len = norm(v);
    if (!std::isfinite(len) || len < 1e-12) return {0, 0, 1};
    return scale(v, 1.0 / len);
}

inline Vec3 perpendicularToAxis(const Vec3& v, const Vec3& k) {
    const double valueScale =
        std::max(std::fabs(v.x), std::max(std::fabs(v.y), std::fabs(v.z)));
    if (valueScale <= 0.0 || !std::isfinite(valueScale)) return {0, 0, 0};
    const Vec3 sv{v.x / valueScale, v.y / valueScale, v.z / valueScale};
    const double axial = dot(sv, k);
    const Vec3 perpScaled = sub(sv, scale(k, axial));
    return scale(perpScaled, valueScale);
}

inline double scaledDotForDirection(const Vec3& v, const Vec3& dir) {
    const double valueScale =
        std::max(std::fabs(v.x), std::max(std::fabs(v.y), std::fabs(v.z)));
    if (valueScale <= 0.0 || !std::isfinite(valueScale)) return 0.0;
    return (v.x / valueScale) * dir.x + (v.y / valueScale) * dir.y +
           (v.z / valueScale) * dir.z;
}

inline double componentScale(const Vec3& v) {
    return std::max(std::fabs(v.x), std::max(std::fabs(v.y), std::fabs(v.z)));
}

inline double perpendicularMagnitudeAboutAxis(const Vec3& v, const Vec3& k) {
    const Vec3 seed = std::fabs(k.z) < 0.9 ? Vec3{0, 0, 1} : Vec3{1, 0, 0};
    const Vec3 u = normalise(cross(seed, k));
    const Vec3 w = cross(k, u);
    const double vu = scaledDotForDirection(v, u);
    const double vw = scaledDotForDirection(v, w);
    return componentScale(v) * std::hypot(vu, vw);
}

// Rodrigues rotation of v about unit axis k by angle theta (radians):
//   v' = v cos t + (k x v) sin t + k (k . v)(1 - cos t).
inline Vec3 rotateAxis(const Vec3& v, const Vec3& k, double theta) {
    const double c = std::cos(theta);
    const double s = std::sin(theta);
    const Vec3 kxv = cross(k, v);
    const double kdv = dot(k, v);
    return add(add(scale(v, c), scale(kxv, s)), scale(k, kdv * (1.0 - c)));
}

// Rotate point p about an axis through pivot P with unit direction k by theta.
inline Vec3 rotateAboutPoint(const Vec3& p, const Vec3& pivot, const Vec3& k, double theta) {
    return add(rotateAxis(sub(p, pivot), k, theta), pivot);
}

// Build a Mat34 [R|t] for "rotate by theta about the axis (pivot, unit k)".
// R is the Rodrigues rotation; t = pivot - R*pivot keeps the pivot fixed.
inline Mat34 rotationMat34(const Vec3& pivot, const Vec3& k, double theta) {
    Mat34 out{};
    const Vec3 ex = rotateAxis({1, 0, 0}, k, theta);
    const Vec3 ey = rotateAxis({0, 1, 0}, k, theta);
    const Vec3 ez = rotateAxis({0, 0, 1}, k, theta);
    out.m[0][0] = ex.x; out.m[0][1] = ey.x; out.m[0][2] = ez.x;
    out.m[1][0] = ex.y; out.m[1][1] = ey.y; out.m[1][2] = ez.y;
    out.m[2][0] = ex.z; out.m[2][1] = ey.z; out.m[2][2] = ez.z;
    // R*pivot.
    const Vec3 rp = {out.m[0][0] * pivot.x + out.m[0][1] * pivot.y + out.m[0][2] * pivot.z,
                     out.m[1][0] * pivot.x + out.m[1][1] * pivot.y + out.m[1][2] * pivot.z,
                     out.m[2][0] * pivot.x + out.m[2][1] * pivot.y + out.m[2][2] * pivot.z};
    out.m[0][3] = pivot.x - rp.x;
    out.m[1][3] = pivot.y - rp.y;
    out.m[2][3] = pivot.z - rp.z;
    return out;
}

// Pure-translation Mat34 (R = I, t = delta).
inline Mat34 translationMat34(const Vec3& delta) {
    Mat34 out{};
    out.m[0][3] = delta.x;
    out.m[1][3] = delta.y;
    out.m[2][3] = delta.z;
    return out;
}

inline bool finiteMat34(const Mat34& T) {
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 4; ++j)
            if (!std::isfinite(T.m[i][j])) return false;
    return true;
}

inline Vec3 applyMat(const Mat34& T, const Vec3& p) {
    return {T.m[0][0] * p.x + T.m[0][1] * p.y + T.m[0][2] * p.z + T.m[0][3],
            T.m[1][0] * p.x + T.m[1][1] * p.y + T.m[1][2] * p.z + T.m[1][3],
            T.m[2][0] * p.x + T.m[2][1] * p.y + T.m[2][2] * p.z + T.m[2][3]};
}

// Signed angle to rotate vector a onto vector b about unit axis k. Both a and b
// are first projected onto the plane perpendicular to k. Returns 0 if either
// projection is degenerate. Result in (-pi, pi].
inline double signedAngleAboutAxis(const Vec3& a, const Vec3& b, const Vec3& k) {
    const Vec3 seed = std::fabs(k.z) < 0.9 ? Vec3{0, 0, 1} : Vec3{1, 0, 0};
    const Vec3 u = normalise(cross(seed, k));
    const Vec3 v = cross(k, u);
    const double au0 = scaledDotForDirection(a, u);
    const double av0 = scaledDotForDirection(a, v);
    const double bu0 = scaledDotForDirection(b, u);
    const double bv0 = scaledDotForDirection(b, v);
    const double na = std::hypot(au0, av0);
    const double nb = std::hypot(bu0, bv0);
    const double aPhysical = componentScale(a) * na;
    const double bPhysical = componentScale(b) * nb;
    if (aPhysical < 1e-12 || bPhysical < 1e-12) return 0.0;
    const double au = au0 / na;
    const double av = av0 / na;
    const double bu = bu0 / nb;
    const double bv = bv0 / nb;
    const double c = std::max(-1.0, std::min(1.0, au * bu + av * bv));
    const double s = au * bv - av * bu;
    return std::atan2(s, c);
}

}  // namespace movemath
}  // namespace opendva
