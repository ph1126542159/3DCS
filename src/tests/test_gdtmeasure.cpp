// A5 GD&T measure tests (README §6.7). Each GD&T measure degrades to a set of
// Point Distances; the evaluator reports the largest (Recommended GD&T Value).
// MMC/LMC/datum-shift are out of scope; results may be negative to stay linear.
// Geometry is fed through a stub PointResolver with a nominal/current split.
#include <cmath>
#include <map>

#include "dva_test.h"
#include "opendva/measure/MeasureEvaluator.h"

using namespace opendva;

namespace {

// Stub resolver: current() and nominal() read from independent point tables so
// GD&T deviation (current - nominal) can be set directly. nominal() falls back
// to the current table when no nominal override is present.
struct StubResolver : PointResolver {
    std::map<PointId, Vec3> positions;
    std::map<PointId, Vec3> nominalPositions;

    Vec3 current(PointId id) const override {
        auto it = positions.find(id);
        return it == positions.end() ? Vec3{} : it->second;
    }
    Vec3 nominal(PointId id) const override {
        auto it = nominalPositions.find(id);
        if (it != nominalPositions.end()) return it->second;
        auto jt = positions.find(id);
        return jt == positions.end() ? Vec3{} : jt->second;
    }
};

BuildState makeState(const StubResolver& r) {
    BuildState s;
    s.impl = &r;
    return s;
}

}  // namespace

// GdtPosition diametrical: measured point deviates (0.3,0.4,0) in the XY plane
// (|dev| = 0.5) with the zone axis +Z. The true diametrical band is 2*0.5 = 1.0;
// discrete 0/45/90/135 deg sampling reports the nearest axis (135 deg projects
// ~0.495), so 2x lands a touch under 1.0 but well inside the band envelope.
TEST("gdt_position_diametrical") {
    StubResolver r;
    r.positions = {{1, {0.3, 0.4, 0.0}}};
    r.nominalPositions = {{1, {0.0, 0.0, 0.0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::GdtPosition;
    d.inputPoints = {1};
    d.direction.ijk = {0, 0, 1};
    const double v = ev->evaluate(d, makeState(r));
    dvatest::check(v > 0.7 && v <= 1.0 + 1e-9, "position diametrical band near 1.0");
    // The 135 deg sample aligns best with (0.3,0.4): projection 0.495, x2 = 0.99.
    dvatest::checkNear(v, 0.99, 0.02, "position diametrical exact sample");
}

TEST("gdt_position_zero_direction_returns_zero") {
    StubResolver r;
    r.positions = {{1, {0.3, 0.4, 0.0}}};
    r.nominalPositions = {{1, {0.0, 0.0, 0.0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::GdtPosition;
    d.inputPoints = {1};
    d.direction.ijk = {0, 0, 0};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "zero position direction -> invalid measure value");
}

// GdtPosition non-diametrical: single Point Distance along the reference
// direction. Deviation (0,0,0.6) projected on +Z = 0.6 (signed).
TEST("gdt_position_non_diametrical") {
    StubResolver r;
    r.positions = {{1, {0.0, 0.0, 0.6}}};
    r.nominalPositions = {{1, {0.0, 0.0, 0.0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::GdtPosition;
    d.inputPoints = {1};
    d.direction.ijk = {0, 0, 1};
    d.equation = "non_diametrical";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.6, 1e-9, "position non-diametrical");
}

TEST("gdt_position_non_diametrical_handles_huge_parallel_component") {
    StubResolver r;
    const double huge = 9e307;
    const double local = 6e292;
    const double invRoot2 = 0.70710678118654752440;
    r.positions = {{1, {huge - local * invRoot2, huge + local * invRoot2, 0.0}}};
    r.nominalPositions = {{1, {0.0, 0.0, 0.0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::GdtPosition;
    d.inputPoints = {1};
    d.direction.ijk = {-1, 1, 0};
    d.equation = "non_diametrical";
    const Vec3 p = r.positions[1];
    const double expected = (p.y - p.x) * invRoot2;
    dvatest::checkNear(ev->evaluate(d, makeState(r)), expected, expected * 1e-12,
                       "huge parallel component preserves non-diametrical position");
}

// GdtPosition non-diametrical reverse: deviation opposite the reference axis is
// negative (linear, sign preserved).
TEST("gdt_position_non_diametrical_signed") {
    StubResolver r;
    r.positions = {{1, {0.0, 0.0, -0.4}}};
    r.nominalPositions = {{1, {0.0, 0.0, 0.0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::GdtPosition;
    d.inputPoints = {1};
    d.direction.ijk = {0, 0, 1};
    d.equation = "non_diametrical";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), -0.4, 1e-9, "position signed negative");
}

// GdtSurfaceProfile: single point deviates (0,0,0.25) along the surface normal
// +Z -> Point Distance 0.25.
TEST("gdt_surface_profile_single") {
    StubResolver r;
    r.positions = {{1, {0.0, 0.0, 0.25}}};
    r.nominalPositions = {{1, {0.0, 0.0, 0.0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::GdtSurfaceProfile;
    d.inputPoints = {1};
    d.direction.ijk = {0, 0, 1};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.25, 1e-9, "surface profile single point");
}

TEST("gdt_surface_profile_zero_direction_returns_zero") {
    StubResolver r;
    r.positions = {{1, {0.0, 0.0, 0.25}}};
    r.nominalPositions = {{1, {0.0, 0.0, 0.0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::GdtSurfaceProfile;
    d.inputPoints = {1};
    d.direction.ijk = {0, 0, 0};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "zero surface-profile direction -> invalid measure value");
}

// GdtSurfaceProfile multi-point: report the worst (largest absolute) point.
// Point 1 deviates +0.2, point 2 deviates -0.5 -> worst is -0.5.
TEST("gdt_surface_profile_worst_point") {
    StubResolver r;
    r.positions = {{1, {0.0, 0.0, 0.2}}, {2, {0.0, 0.0, -0.5}}};
    r.nominalPositions = {{1, {0.0, 0.0, 0.0}}, {2, {0.0, 0.0, 0.0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::GdtSurfaceProfile;
    d.inputPoints = {1, 2};
    d.direction.ijk = {0, 0, 1};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), -0.5, 1e-9, "surface profile worst point");
}

// GdtConcentricity: centre deviates (0.3,0.4,0) off the coaxial DRF axis +Z.
// Same diametrical sampling as Position -> ~0.99 (2 * largest in-plane sample).
TEST("gdt_concentricity_offset") {
    StubResolver r;
    r.positions = {{1, {0.3, 0.4, 0.0}}};
    r.nominalPositions = {{1, {0.0, 0.0, 0.0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::GdtConcentricity;
    d.inputPoints = {1};
    d.direction.ijk = {0, 0, 1};
    const double v = ev->evaluate(d, makeState(r));
    dvatest::check(v > 0.7 && v <= 1.0 + 1e-9, "concentricity diametrical band near 1.0");
}

// GdtConcentricity coaxial: zero off-axis deviation -> zero.
TEST("gdt_concentricity_coaxial_zero") {
    StubResolver r;
    r.positions = {{1, {0.0, 0.0, 5.0}}};       // pure on-axis shift
    r.nominalPositions = {{1, {0.0, 0.0, 0.0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::GdtConcentricity;
    d.inputPoints = {1};
    d.direction.ijk = {0, 0, 1};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-9, "concentricity coaxial zero");
}

// GdtPerpendicularity: the feature rotates about its Feature Locator Point (the
// nominal of inputPoints[0]). Axis ends tip off their nominal arms producing a
// non-zero orientation band. FLP at origin; one axis end at nominal (0,0,5)
// tips to (0.3,0,5) -> in-plane arm change (0.3,0,0), diametrical x2 ~ 0.6.
TEST("gdt_perpendicularity_tilt") {
    StubResolver r;
    r.positions = {{1, {0.0, 0.0, 0.0}}, {2, {0.3, 0.0, 5.0}}};
    r.nominalPositions = {{1, {0.0, 0.0, 0.0}}, {2, {0.0, 0.0, 5.0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::GdtPerpendicularity;
    d.inputPoints = {1, 2};
    d.direction.ijk = {0, 0, 1};
    const double v = ev->evaluate(d, makeState(r));
    dvatest::check(std::fabs(v) > 1e-6, "perpendicularity tilt non-zero");
    dvatest::check(v > 0.4 && v <= 0.6 + 1e-9, "perpendicularity tilt band ~0.6");
}

// GdtAngularity shares the orientation routine: an arm tip yields a non-zero
// band (translation of the whole feature cancels via the arm difference).
TEST("gdt_angularity_nonzero") {
    StubResolver r;
    r.positions = {{1, {1.0, 1.0, 0.0}}, {2, {1.4, 1.0, 5.0}}};  // whole feature shifted + tip
    r.nominalPositions = {{1, {1.0, 1.0, 0.0}}, {2, {1.0, 1.0, 5.0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::GdtAngularity;
    d.inputPoints = {1, 2};
    d.direction.ijk = {0, 0, 1};
    dvatest::check(std::fabs(ev->evaluate(d, makeState(r))) > 1e-6, "angularity non-zero");
}

// GdtParallelism non-diametrical: planar extreme point folds back to the FLP as
// a signed band along the reference direction.
TEST("gdt_parallelism_non_diametrical") {
    StubResolver r;
    r.positions = {{1, {0.0, 0.0, 0.0}}, {2, {2.0, 0.0, 0.3}}};
    r.nominalPositions = {{1, {0.0, 0.0, 0.0}}, {2, {2.0, 0.0, 0.0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::GdtParallelism;
    d.inputPoints = {1, 2};
    d.direction.ijk = {0, 0, 1};
    d.equation = "non_diametrical";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.3, 1e-9, "parallelism non-diametrical band");
}

// GD&T scale factor is applied to the final value like every other measure.
TEST("gdt_scale_applied") {
    StubResolver r;
    r.positions = {{1, {0.0, 0.0, 0.5}}};
    r.nominalPositions = {{1, {0.0, 0.0, 0.0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::GdtSurfaceProfile;
    d.inputPoints = {1};
    d.direction.ijk = {0, 0, 1};
    d.scale = 2.0;
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 1.0, 1e-9, "gdt scale applied");
}
