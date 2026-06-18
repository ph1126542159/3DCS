// A4 GeomRule applier — maps a sampled scalar deviation to a per-point offset
// vector according to the tolerance's geometric action (README §5.1 "公差→几何
// 偏差映射"). Also exposes the RSS combination (§5.4 RSS) and Bonus expansion
// (§5.6 Bonus) helpers used when several control frames stack on one feature.
//
// Public module API: consumed by the sim layer (A6) to deviate mesh nodes /
// feature points. Self-contained vector algebra (no external dependency); the
// helpers are `inline` so multiple TUs may include this header.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

#include "opendva/IToleranceSampler.h"
#include "opendva/Types.h"

namespace opendva {
namespace tolerance {

// --- Minimal vector algebra (clean-room, header-only) ---------------------

inline Vec3 vAdd(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 vSub(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 vScale(const Vec3& a, double s) { return {a.x * s, a.y * s, a.z * s}; }
inline double vDot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline double vNorm(const Vec3& a) {
    const double ax = std::fabs(a.x);
    const double ay = std::fabs(a.y);
    const double az = std::fabs(a.z);
    const double scale = std::max(ax, std::max(ay, az));
    if (scale <= 0.0 || !std::isfinite(scale)) return scale;
    const double sx = a.x / scale;
    const double sy = a.y / scale;
    const double sz = a.z / scale;
    return scale * std::sqrt(sx * sx + sy * sy + sz * sz);
}

inline Vec3 vCross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

// Normalise; returns +Z for a degenerate (near-zero) input so callers always
// get a usable unit direction.
inline Vec3 vNormalize(const Vec3& v) {
    const double len = vNorm(v);
    if (len < 1e-12 || !std::isfinite(len)) return {0.0, 0.0, 1.0};
    return vScale(v, 1.0 / len);
}

// --- GeomRule geometric application ---------------------------------------

// Inputs shared by every rule. Not every field is used by every rule; the
// unused ones are ignored (documented per branch in GeomRule.cpp).
struct GeomRuleInput {
    GeomRule rule{GeomRule::TranslateAlongVector};
    double magnitude{0.0};            // sampled scalar deviation (signed)
    Vec3 dir{0.0, 0.0, 1.0};          // tolerance/node direction (need not be unit)
    Vec3 featureLocatorPoint{0, 0, 0};// FLP / axis point / section center
    Vec3 axis{0.0, 0.0, 1.0};         // feature axis (radial/orientation rules)
    Vec3 target{0, 0, 0};             // the point that receives the offset
    double nominalSize{1.0};          // nominal size for DiameterScale (avoid /0)
    // Reference span for RotateAboutLocatorPoint: the magnitude is the zone
    // displacement reached at this distance from the FLP (the feature's far
    // end). Offset scales linearly with target distance / referenceSpan.
    double referenceSpan{1.0};
};

// Compute the offset vector to apply to GeomRuleInput::target for one rule.
// See GeomRule.cpp for the per-rule geometry.
Vec3 applyGeomRule(const GeomRuleInput& in);

// --- RSS combination (README §5.4) ----------------------------------------

// Split a Position frame's total range into the location (translation) range
// once the refinement frames (orientation/form) have taken their share:
//   location_range = sqrt(total^2 - sum(refine_i^2)).
// Clamped at 0 if the refinements over-subscribe the total. This keeps the RSS
// of the location and refinement normals equal to the parent total range.
double rssCombine(double total_range, const std::vector<double>& refine_ranges);

// --- Bonus tolerance (README §5.6) ----------------------------------------

// Bonus magnitude granted by departure of the actual size from MMC/LMC:
//   bonus = |actual_size - mmc_size|.
double bonus(double actual_size, double mmc_size);

// Expand a base range by the size-derived bonus:
//   expanded = base_range + |actual_size - mmc_size|.
double applyBonus(double base_range, double actual_size, double mmc_size);

}  // namespace tolerance
}  // namespace opendva
