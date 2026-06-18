// A4 GeomRule applier implementation (README §5.1 公差→几何偏差映射, §5.4 RSS,
// §5.6 Bonus). Each rule turns a sampled scalar magnitude into a 3D offset for
// one point so the sim layer can deviate mesh nodes / feature points. Vector
// norms are kept overflow-stable by the shared header helpers.
#include "opendva/tolerance/GeomRule.h"

#include <algorithm>
#include <cmath>

namespace opendva {
namespace tolerance {
namespace {

// Component of (P - FLP) that is perpendicular to the axis: the radial vector
// from the axis to the target point. Used by the radial and orientation rules.
Vec3 radialFromAxis(const Vec3& target, const Vec3& axisPoint, const Vec3& axisDir) {
    const Vec3 u = vNormalize(axisDir);
    const Vec3 d = vSub(target, axisPoint);      // point relative to axis point
    const double along = vDot(d, u);             // projection onto the axis
    return vSub(d, vScale(u, along));            // strip the axial component
}

}  // namespace

Vec3 applyGeomRule(const GeomRuleInput& in) {
    switch (in.rule) {
        case GeomRule::TranslateAlongVector: {
            // Linear / Position location band: the whole feature shifts rigidly
            // along the tolerance direction. Offset = magnitude * unit(dir);
            // identical for every point (no shape change). FLP/axis unused.
            return vScale(vNormalize(in.dir), in.magnitude);
        }

        case GeomRule::RotateAboutLocatorPoint: {
            // Orientation (Perpendicularity / Parallelism / Angularity): the
            // feature tilts about the Feature Locator Point. The displacement is
            // zero at the FLP and grows linearly to `magnitude` at the far end
            // (referenceSpan away). For a small tilt the displacement is along
            // `dir` (the zone normal), scaled by the lever arm:
            //   offset = magnitude * (armLength / referenceSpan) * unit(dir),
            // where armLength is the distance from the FLP measured across the
            // feature (perpendicular to the rotation/zone normal).
            const Vec3 zoneNormal = vNormalize(in.dir);
            const Vec3 arm = vSub(in.target, in.featureLocatorPoint);
            // Lever arm = span of the point away from the FLP within the feature
            // plane (component perpendicular to the zone normal).
            const Vec3 armPerp = vSub(arm, vScale(zoneNormal, vDot(arm, zoneNormal)));
            const double armLen = vNorm(armPerp);
            const double span = (std::fabs(in.referenceSpan) > 1e-12) ? in.referenceSpan : 1.0;
            const double factor = armLen / span;  // 0 at FLP, 1 at the far end
            return vScale(zoneNormal, in.magnitude * factor);
        }

        case GeomRule::NodeNormalOffset: {
            // Form (Flatness / no-DRF Surface Profile): every node moves
            // independently along its own surface normal by the sampled
            // magnitude. Here `dir` is that node normal. FLP/axis unused.
            return vScale(vNormalize(in.dir), in.magnitude);
        }

        case GeomRule::SectionRadialOffset: {
            // Form on a round feature (Circularity / Cylindricity): each section
            // grows/shrinks radially while the axis (section centers) stays
            // collinear. The point moves along its radial direction (point ->
            // axis perpendicular) by `magnitude`. Degenerate (on-axis) points
            // get no offset (radial direction undefined).
            const Vec3 radial = radialFromAxis(in.target, in.featureLocatorPoint, in.axis);
            if (vNorm(radial) < 1e-12) return {0.0, 0.0, 0.0};
            return vScale(vNormalize(radial), in.magnitude);
        }

        case GeomRule::DiameterScale: {
            // Size: the whole feature scales uniformly about its center. A point
            // at radius r moves to r * (1 + magnitude / nominal), i.e. a radial
            // offset of magnitude * (r / nominal). Simplified to a unit-radial
            // offset of `magnitude` when nominalSize ~ the local radius; here we
            // scale by the actual radial distance so larger radii move more.
            const double nominal = (std::fabs(in.nominalSize) > 1e-12) ? in.nominalSize : 1.0;
            const Vec3 radial = radialFromAxis(in.target, in.featureLocatorPoint, in.axis);
            const double r = vNorm(radial);
            if (r < 1e-12) return {0.0, 0.0, 0.0};
            const double scaleOffset = in.magnitude * (r / nominal);
            return vScale(vNormalize(radial), scaleOffset);
        }
    }
    return {0.0, 0.0, 0.0};
}

double rssCombine(double total_range, const std::vector<double>& refine_ranges) {
    double scale = std::fabs(total_range);
    for (double r : refine_ranges) {
        scale = std::max(scale, std::fabs(r));
    }
    if (scale <= 0.0 || !std::isfinite(scale)) return 0.0;
    const double total = total_range / scale;
    double sumSq = total * total;
    for (double r : refine_ranges) {
        const double scaled = r / scale;
        sumSq -= scaled * scaled;
    }
    if (sumSq <= 0.0) return 0.0;  // refinements over-subscribe the total
    return scale * std::sqrt(sumSq);
}

double bonus(double actual_size, double mmc_size) {
    return std::fabs(actual_size - mmc_size);
}

double applyBonus(double base_range, double actual_size, double mmc_size) {
    return base_range + bonus(actual_size, mmc_size);
}

}  // namespace tolerance
}  // namespace opendva
