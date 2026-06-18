// A5 measure evaluator tests (README §6.2-6.6). Each test builds a known
// geometry through a stub PointResolver that returns fixed coordinates, then
// checks the evaluated scalar against the analytic value. Angles are in degrees.
#include <limits>
#include <map>

#include "dva_test.h"
#include "opendva/measure/MeasureEvaluator.h"

using namespace opendva;

namespace {

// Stub resolver: current() and nominal() both read from a fixed point table.
// Tests that need a nominal/current split can override nominalPositions.
struct StubResolver : PointResolver {
    std::map<PointId, Vec3> positions;
    std::map<PointId, Vec3> nominalPositions;
    std::map<PointId, double> diameters;

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
    double diameter(PointId id) const override {
        auto it = diameters.find(id);
        return it == diameters.end() ? 0.0 : it->second;
    }
};

// Bind a resolver into the opaque BuildState the evaluator reads.
BuildState makeState(const StubResolver& r) {
    BuildState s;
    s.impl = &r;
    return s;
}

}  // namespace

// Point-Point true distance: (0,0,0) -> (0,0,10) = 10.
TEST("measure_point_point_true") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {0, 0, 10}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PointPoint;
    d.inputPoints = {1, 2};
    d.dirMode = DirectionMode::TrueDistance;
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 10.0, 1e-9, "point-point distance");
}

TEST("measure_point_point_true_handles_huge_finite_distance") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {1e308, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PointPoint;
    d.inputPoints = {1, 2};
    d.dirMode = DirectionMode::TrueDistance;
    const double v = ev->evaluate(d, makeState(r));
    dvatest::check(std::isfinite(v), "huge finite point-point distance is finite");
    dvatest::checkNear(v, 1e308, 1e292, "huge finite point-point distance");
}

// Point-Point projected on +Z is signed: reverse direction -> negative.
TEST("measure_point_point_projected_signed") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {0, 0, 10}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PointPoint;
    d.inputPoints = {1, 2};
    d.dirMode = DirectionMode::ProjectedOnVector;
    d.direction.ijk = {0, 0, -1};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), -10.0, 1e-9, "projected against vector");
}

TEST("measure_point_point_projected_huge_direction_normalizes") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {10, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PointPoint;
    d.inputPoints = {1, 2};
    d.dirMode = DirectionMode::ProjectedOnVector;
    d.direction.ijk = {1.3e308, 1.3e308, 0};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 10.0 * std::sqrt(0.5), 1e-9,
                       "huge projected direction normalizes");
}

TEST("measure_point_point_projected_vector_handles_huge_parallel_component") {
    StubResolver r;
    const double huge = 9e307;
    const double local = 6e292;
    const double invRoot2 = 0.70710678118654752440;
    r.positions = {{1, {0, 0, 0}},
                   {2, {huge - local * invRoot2, huge + local * invRoot2, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PointPoint;
    d.inputPoints = {1, 2};
    d.dirMode = DirectionMode::ProjectedOnVector;
    d.direction.ijk = {-1, 1, 0};
    const Vec3 p = r.positions[2];
    const double expected = (p.y - p.x) * invRoot2;
    dvatest::checkNear(ev->evaluate(d, makeState(r)), expected, expected * 1e-12,
                       "huge parallel component preserves projected-vector distance");
}

TEST("measure_projected_zero_direction_returns_zero") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {0, 0, 10}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PointPoint;
    d.inputPoints = {1, 2};
    d.dirMode = DirectionMode::ProjectedOnVector;
    d.direction.ijk = {0, 0, 0};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "zero projected direction -> invalid measure value");
}

TEST("measure_projected_plane_zero_direction_returns_zero") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {0, 0, 10}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PointPoint;
    d.inputPoints = {1, 2};
    d.dirMode = DirectionMode::ProjectedOnPlane;
    d.direction.ijk = {0, 0, 0};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "zero projected-plane direction -> invalid measure value");
}

TEST("measure_point_point_projected_plane_handles_huge_parallel_component") {
    StubResolver r;
    const double huge = 9e307;
    const double local = 6e292;
    const double invRoot2 = 0.70710678118654752440;
    r.positions = {{1, {0, 0, 0}},
                   {2, {huge - local * invRoot2, huge + local * invRoot2, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PointPoint;
    d.inputPoints = {1, 2};
    d.dirMode = DirectionMode::ProjectedOnPlane;
    d.direction.ijk = {1, 1, 0};
    const Vec3 p = r.positions[2];
    const double expected = std::fabs((p.y - p.x) * invRoot2);
    dvatest::checkNear(ev->evaluate(d, makeState(r)), expected, expected * 1e-12,
                       "huge parallel component preserves projected-plane distance");
}

// Nominal-Point: current (0,0,5) vs nominal (0,0,0) = 5.
TEST("measure_nominal_point") {
    StubResolver r;
    r.positions = {{1, {0, 0, 5}}};
    r.nominalPositions = {{1, {0, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::NominalPoint;
    d.inputPoints = {1};
    d.dirMode = DirectionMode::TrueDistance;
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 5.0, 1e-9, "nominal-point deviation");
}

// Point-Line: point (0,5,0), line along X axis through origin -> perp dist 5.
TEST("measure_point_line_true") {
    StubResolver r;
    r.positions = {{1, {0, 5, 0}}, {2, {0, 0, 0}}, {3, {10, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PointLine;
    d.inputPoints = {1, 2, 3};
    d.dirMode = DirectionMode::TrueDistance;
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 5.0, 1e-9, "point-line perpendicular");
}

TEST("measure_point_line_handles_huge_parallel_component_distance") {
    StubResolver r;
    const double huge = 9e307;
    const double local = 6e292;
    const double invRoot2 = 0.70710678118654752440;
    r.positions = {{1, {huge - local * invRoot2, huge + local * invRoot2, 0}},
                   {2, {0, 0, 0}},
                   {3, {1, 1, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PointLine;
    d.inputPoints = {1, 2, 3};
    d.dirMode = DirectionMode::TrueDistance;
    const Vec3 p = r.positions[1];
    const double expected = std::fabs((p.y - p.x) * invRoot2);
    dvatest::checkNear(ev->evaluate(d, makeState(r)), expected, expected * 1e-12,
                       "huge parallel component preserves point-line distance");
}

TEST("measure_point_line_zero_length_line_returns_zero") {
    StubResolver r;
    r.positions = {{1, {3, 4, 5}}, {2, {0, 0, 0}}, {3, {0, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PointLine;
    d.inputPoints = {1, 2, 3};
    d.dirMode = DirectionMode::TrueDistance;
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "zero-length point-line direction -> invalid measure value");
}

TEST("measure_point_line_projected_plane_zero_direction_returns_zero") {
    StubResolver r;
    r.positions = {{1, {0, 5, 0}}, {2, {0, 0, 0}}, {3, {10, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PointLine;
    d.inputPoints = {1, 2, 3};
    d.dirMode = DirectionMode::ProjectedOnPlane;
    d.direction.ijk = {0, 0, 0};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "zero point-line projected-plane direction -> invalid measure value");
}

TEST("measure_point_line_projected_plane_handles_huge_parallel_component") {
    StubResolver r;
    const double huge = 9e307;
    const double local = 6e292;
    const double invRoot2 = 0.70710678118654752440;
    r.positions = {{1, {huge - local * invRoot2, huge + local * invRoot2, 0}},
                   {2, {0, 0, 0}},
                   {3, {0, 0, 1}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PointLine;
    d.inputPoints = {1, 2, 3};
    d.dirMode = DirectionMode::ProjectedOnPlane;
    d.direction.ijk = {1, 1, 0};
    const Vec3 p = r.positions[1];
    const double expected = std::fabs((p.y - p.x) * invRoot2);
    dvatest::checkNear(ev->evaluate(d, makeState(r)), expected, expected * 1e-12,
                       "huge parallel component preserves projected point-line distance");
}

// Point-Plane true: point (0,0,5) over the XY plane -> perpendicular dist 5.
TEST("measure_point_plane_true") {
    StubResolver r;
    r.positions = {{1, {0, 0, 5}}, {2, {0, 0, 0}}, {3, {1, 0, 0}}, {4, {0, 1, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PointPlane;
    d.inputPoints = {1, 2, 3, 4};
    d.dirMode = DirectionMode::TrueDistance;
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 5.0, 1e-9, "point-plane perpendicular");
}

TEST("measure_point_plane_handles_huge_finite_plane_edges") {
    StubResolver r;
    r.positions = {{1, {0, 0, 5}},
                   {2, {0, 0, 0}},
                   {3, {1e308, 0, 0}},
                   {4, {0, 1e308, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PointPlane;
    d.inputPoints = {1, 2, 3, 4};
    d.dirMode = DirectionMode::TrueDistance;
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 5.0, 1e-9,
                       "huge finite point-plane edges");
}

TEST("measure_point_plane_degenerate_plane_returns_zero") {
    StubResolver r;
    r.positions = {{1, {0, 0, 5}}, {2, {0, 0, 0}}, {3, {1, 0, 0}}, {4, {2, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PointPlane;
    d.inputPoints = {1, 2, 3, 4};
    d.dirMode = DirectionMode::TrueDistance;
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "degenerate point-plane normal -> invalid measure value");
}

// Point-Plane projected on -Z: point above XY plane along -Z gives -5.
TEST("measure_point_plane_projected_signed") {
    StubResolver r;
    r.positions = {{1, {0, 0, 5}}, {2, {0, 0, 0}}, {3, {1, 0, 0}}, {4, {0, 1, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PointPlane;
    d.inputPoints = {1, 2, 3, 4};
    d.dirMode = DirectionMode::ProjectedOnVector;
    d.direction.ijk = {0, 0, -1};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), -5.0, 1e-9, "point-plane signed");
}

TEST("measure_point_plane_projected_vector_handles_huge_parallel_component") {
    StubResolver r;
    const double huge = 9e307;
    const double local = 6e292;
    const double invRoot2 = 0.70710678118654752440;
    r.positions = {{1, {huge - local * invRoot2, huge + local * invRoot2, 5}},
                   {2, {0, 0, 0}},
                   {3, {1, 0, 0}},
                   {4, {0, 1, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PointPlane;
    d.inputPoints = {1, 2, 3, 4};
    d.dirMode = DirectionMode::ProjectedOnVector;
    d.direction.ijk = {-1, 1, 0};
    const Vec3 p = r.positions[1];
    const double expected = (p.y - p.x) * invRoot2;
    dvatest::checkNear(ev->evaluate(d, makeState(r)), expected, expected * 1e-12,
                       "huge parallel component preserves point-plane vector projection");
}

// Line-Nominal: line along X vs reference +Y -> 90 degrees.
TEST("measure_line_nominal_orthogonal") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {10, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::LineNominal;
    d.inputPoints = {1, 2};
    d.direction.ijk = {0, 1, 0};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 90.0, 1e-9, "line-nominal angle");
}

// Line-Line: X-axis line and Y-axis line -> 90 degrees.
TEST("measure_line_line_orthogonal") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {10, 0, 0}}, {3, {0, 0, 0}}, {4, {0, 10, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::LineLine;
    d.inputPoints = {1, 2, 3, 4};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 90.0, 1e-9, "line-line angle");
}

// Line-Line at 45 degrees: X axis vs the (1,1,0) line.
TEST("measure_line_line_45") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {1, 0, 0}}, {3, {0, 0, 0}}, {4, {1, 1, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::LineLine;
    d.inputPoints = {1, 2, 3, 4};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 45.0, 1e-6, "line-line 45 deg");
}

TEST("measure_line_line_handles_huge_finite_45_degree_vectors") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}},
                   {2, {1e308, 0, 0}},
                   {3, {0, 0, 0}},
                   {4, {1e308, 1e308, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::LineLine;
    d.inputPoints = {1, 2, 3, 4};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 45.0, 1e-6,
                       "huge finite line-line 45 deg");
}

TEST("measure_line_line_projected_on_plane_is_signed_by_view_direction") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {1, 0, 0}},
                   {3, {0, 0, 0}}, {4, {0, 1, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::LineLine;
    d.inputPoints = {1, 2, 3, 4};
    d.dirMode = DirectionMode::ProjectedOnPlane;

    d.direction.ijk = {0, 0, 1};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 90.0, 1e-9,
                       "line-line projected angle follows positive view");

    d.direction.ijk = {0, 0, -1};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), -90.0, 1e-9,
                       "line-line projected angle follows negative view");
}

TEST("measure_line_line_projected_handles_huge_finite_45_degree_vectors") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}},
                   {2, {1e308, 0, 0}},
                   {3, {0, 0, 0}},
                   {4, {1e308, 1e308, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::LineLine;
    d.inputPoints = {1, 2, 3, 4};
    d.dirMode = DirectionMode::ProjectedOnPlane;
    d.direction.ijk = {0, 0, 1};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 45.0, 1e-6,
                       "huge finite projected line-line 45 deg");
}

TEST("measure_line_line_projected_handles_huge_parallel_component_angle") {
    StubResolver r;
    const double huge = 1.3e308;
    const double local = 1e293;
    const double invRoot2 = 0.70710678118654752440;
    r.positions = {{1, {0, 0, 0}},
                   {2, {huge - local * invRoot2, huge + local * invRoot2, 0}},
                   {3, {0, 0, 0}},
                   {4, {huge, huge, local}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::LineLine;
    d.inputPoints = {1, 2, 3, 4};
    d.dirMode = DirectionMode::ProjectedOnPlane;
    d.direction.ijk = {1, 1, 0};
    dvatest::checkNear(std::fabs(ev->evaluate(d, makeState(r))), 90.0, 1e-9,
                       "huge parallel component projected line-line angle");
}

TEST("measure_line_line_projected_zero_view_direction_returns_zero") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {1, 0, 0}},
                   {3, {0, 0, 0}}, {4, {0, 1, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::LineLine;
    d.inputPoints = {1, 2, 3, 4};
    d.dirMode = DirectionMode::ProjectedOnPlane;
    d.direction.ijk = {0, 0, 0};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "zero projected-angle view direction -> invalid measure value");
}

// Line-Plane: line along Z, plane = XY -> line perpendicular to plane -> 90.
TEST("measure_line_plane_perpendicular") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {0, 0, 5}},          // line along Z
                   {3, {0, 0, 0}}, {4, {1, 0, 0}}, {5, {0, 1, 0}}};  // XY plane
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::LinePlane;
    d.inputPoints = {1, 2, 3, 4, 5};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 90.0, 1e-9, "line-plane perpendicular");
}

// Line-Plane: line in the XY plane (along X) -> angle to plane is 0.
TEST("measure_line_plane_parallel") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {5, 0, 0}},          // line along X (in plane)
                   {3, {0, 0, 0}}, {4, {1, 0, 0}}, {5, {0, 1, 0}}};  // XY plane
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::LinePlane;
    d.inputPoints = {1, 2, 3, 4, 5};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-9, "line-plane parallel");
}

TEST("measure_line_plane_degenerate_plane_returns_zero") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {0, 0, 5}},
                   {3, {0, 0, 0}}, {4, {1, 0, 0}}, {5, {2, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::LinePlane;
    d.inputPoints = {1, 2, 3, 4, 5};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "degenerate line-plane normal -> invalid measure value");
}

// Plane-Nominal: XY plane (normal +Z) vs reference normal +Z -> 0 degrees.
TEST("measure_plane_nominal_aligned") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {1, 0, 0}}, {3, {0, 1, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PlaneNominal;
    d.inputPoints = {1, 2, 3};
    d.direction.ijk = {0, 0, 1};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-9, "plane-nominal aligned");
}

// Plane-Plane: XY plane (normal +Z) and XZ plane (normal +/-Y) -> 90 degrees.
TEST("measure_plane_plane_orthogonal") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {1, 0, 0}}, {3, {0, 1, 0}},   // XY plane
                   {4, {0, 0, 0}}, {5, {1, 0, 0}}, {6, {0, 0, 1}}};  // XZ plane
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PlanePlane;
    d.inputPoints = {1, 2, 3, 4, 5, 6};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 90.0, 1e-9, "plane-plane orthogonal");
}

TEST("measure_plane_plane_opposite_normals_are_aligned") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {1, 0, 0}}, {3, {0, 1, 0}},
                   {4, {0, 0, 0}}, {5, {0, 1, 0}}, {6, {1, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PlanePlane;
    d.inputPoints = {1, 2, 3, 4, 5, 6};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-9,
                       "plane-plane opposite normals are aligned");
}

TEST("measure_plane_plane_projected_on_plane_is_signed_by_view_direction") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {0, 1, 0}}, {3, {0, 0, 1}},
                   {4, {0, 0, 0}}, {5, {0, 0, 1}}, {6, {1, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PlanePlane;
    d.inputPoints = {1, 2, 3, 4, 5, 6};
    d.dirMode = DirectionMode::ProjectedOnPlane;

    d.direction.ijk = {0, 0, 1};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 90.0, 1e-9,
                       "plane-plane projected angle follows positive view");

    d.direction.ijk = {0, 0, -1};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), -90.0, 1e-9,
                       "plane-plane projected angle follows negative view");
}

// Two Point List, Pair Points Max: group1 {A,B}, group2 {C,D}, true distances
// |A-C| = 1, |B-D| = 3 -> max = 3.
TEST("measure_two_point_list_pair_max") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {0, 0, 0}},   // group1
                   {3, {1, 0, 0}}, {4, {3, 0, 0}}};  // group2
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::TwoPointList;
    d.inputPoints = {1, 2, 3, 4};
    d.dirMode = DirectionMode::TrueDistance;
    d.equation = "pair_max";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 3.0, 1e-9, "pair points max");
}

TEST("measure_two_point_list_pair_odd_input_count_returns_zero") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {5, 0, 0}}, {3, {99, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::TwoPointList;
    d.inputPoints = {1, 2, 3};
    d.dirMode = DirectionMode::TrueDistance;
    d.equation = "pair_max";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "odd two-point-list pair input count -> invalid measure value");
}

// Two Point List, Pair Points Min: same data -> min pair distance = 1.
TEST("measure_two_point_list_pair_min") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {0, 0, 0}},
                   {3, {1, 0, 0}}, {4, {3, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::TwoPointList;
    d.inputPoints = {1, 2, 3, 4};
    d.dirMode = DirectionMode::TrueDistance;
    d.equation = "pair_min";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 1.0, 1e-9, "pair points min");
}

// Two Point List, Pair Points Max-Min: same-index distances 1, 3, 8 -> range 7.
TEST("measure_two_point_list_pair_max_min") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {0, 0, 0}}, {3, {0, 0, 0}},
                   {4, {1, 0, 0}}, {5, {3, 0, 0}}, {6, {8, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::TwoPointList;
    d.inputPoints = {1, 2, 3, 4, 5, 6};
    d.dirMode = DirectionMode::TrueDistance;
    d.equation = "pair_max_min";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 7.0, 1e-9,
                       "pair points max-min range");
}

// Two Point List, Center Deviate: the current pair midpoint shifts +3 along X
// from the nominal pair midpoint.
TEST("measure_two_point_list_center_deviate_single_pair") {
    StubResolver r;
    r.nominalPositions = {{1, {0, 0, 0}}, {2, {10, 0, 0}}};
    r.positions = {{1, {2, 0, 0}}, {2, {14, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::TwoPointList;
    d.inputPoints = {1, 2};
    d.direction.ijk = {1, 0, 0};
    d.dirMode = DirectionMode::ProjectedOnVector;
    d.equation = "center_deviate";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 3.0, 1e-9,
                       "center deviate follows midpoint shift");
}

TEST("measure_two_point_list_center_deviate_handles_huge_same_sign_midpoints") {
    StubResolver r;
    r.nominalPositions = {{1, {1.5e308, 0, 0}}, {2, {1.5e308, 0, 0}}};
    r.positions = {{1, {1.6e308, 0, 0}}, {2, {1.6e308, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::TwoPointList;
    d.inputPoints = {1, 2};
    d.direction.ijk = {1, 0, 0};
    d.dirMode = DirectionMode::ProjectedOnVector;
    d.equation = "center_deviate";
    const double v = ev->evaluate(d, makeState(r));
    dvatest::check(std::isfinite(v), "huge same-sign center deviate remains finite");
    dvatest::checkNear(v, 1e307, 1e292, "huge same-sign center deviate");
}

TEST("measure_two_point_list_center_deviate_zero_direction_returns_zero") {
    StubResolver r;
    r.nominalPositions = {{1, {0, 0, 0}}, {2, {10, 0, 0}}};
    r.positions = {{1, {0, 0, 3}}, {2, {10, 0, 3}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::TwoPointList;
    d.inputPoints = {1, 2};
    d.direction.ijk = {0, 0, 0};
    d.dirMode = DirectionMode::ProjectedOnVector;
    d.equation = "center_deviate";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "zero center-deviate direction -> invalid measure value");
}

// Two Point List, All Points Min: cross pairs A-C=1, A-D=3, B-C=1, B-D=3 -> 1.
TEST("measure_two_point_list_all_min") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {0, 0, 0}},
                   {3, {1, 0, 0}}, {4, {3, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::TwoPointList;
    d.inputPoints = {1, 2, 3, 4};
    d.dirMode = DirectionMode::TrueDistance;
    d.equation = "all_min";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 1.0, 1e-9, "all points min");
}

// Two Point List, All Points Max: cross pairs -> max = 3.
TEST("measure_two_point_list_all_max") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {0, 0, 0}},
                   {3, {1, 0, 0}}, {4, {3, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::TwoPointList;
    d.inputPoints = {1, 2, 3, 4};
    d.dirMode = DirectionMode::TrueDistance;
    d.equation = "all_max";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 3.0, 1e-9, "all points max");
}

// Circularity: 4 points on a perfect circle of radius 5 in the XY plane
// (normal +Z) -> outer radius == inner radius -> circularity 0.
TEST("measure_circularity_perfect") {
    StubResolver r;
    r.positions = {{1, {5, 0, 0}}, {2, {-5, 0, 0}}, {3, {0, 5, 0}}, {4, {0, -5, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Circularity;
    d.inputPoints = {1, 2, 3, 4};
    d.direction.ijk = {0, 0, 1};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-9, "perfect circle circularity");
}

// Circularity: one point pushed out to radius 6 -> band = 6 - 5 = 1.
TEST("measure_circularity_band") {
    StubResolver r;
    r.positions = {{1, {6, 0, 0}}, {2, {-5, 0, 0}}, {3, {0, 5, 0}}, {4, {0, -5, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Circularity;
    d.inputPoints = {1, 2, 3, 4};
    d.direction.ijk = {0, 0, 1};
    // Centroid shifts slightly toward +X (0.25,0,0); inner/outer radii change
    // accordingly. The band is positive and near 1.
    const double v = ev->evaluate(d, makeState(r));
    dvatest::check(v > 0.5 && v < 1.5, "circularity band positive and near 1");
}

TEST("measure_circularity_handles_huge_same_sign_points") {
    StubResolver r;
    r.positions = {{1, {8e307, 0, 0}},
                   {2, {5e307, 0, 0}},
                   {3, {6e307, 1e307, 0}},
                   {4, {6e307, -1e307, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Circularity;
    d.inputPoints = {1, 2, 3, 4};
    d.direction.ijk = {0, 0, 1};
    const double v = ev->evaluate(d, makeState(r));
    dvatest::check(std::isfinite(v), "huge same-sign circularity remains finite");
    dvatest::check(v > 5e306, "huge same-sign circularity keeps nonzero band");
}

TEST("measure_circularity_zero_direction_returns_zero") {
    StubResolver r;
    r.positions = {{1, {6, 0, 0}}, {2, {-5, 0, 0}}, {3, {0, 5, 0}}, {4, {0, -5, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Circularity;
    d.inputPoints = {1, 2, 3, 4};
    d.direction.ijk = {0, 0, 0};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "zero circularity direction -> invalid measure value");
}

// CircleDiameter reads the feature-of-size diameter attached to its point.
TEST("measure_circle_diameter_uses_point_size") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}};
    r.diameters = {{1, 12.5}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::CircleDiameter;
    d.inputPoints = {1};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 12.5, 1e-12, "circle diameter");
}

// Scale factor is applied to the final value.
TEST("measure_scale_applied") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {0, 0, 10}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PointPoint;
    d.inputPoints = {1, 2};
    d.dirMode = DirectionMode::TrueDistance;
    d.scale = 2.0;
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 20.0, 1e-9, "scale applied");
}

TEST("measure_non_finite_scale_returns_zero") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {0, 0, 10}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PointPoint;
    d.inputPoints = {1, 2};
    d.dirMode = DirectionMode::TrueDistance;
    d.scale = std::numeric_limits<double>::infinity();
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "non-finite measure scale -> invalid measure value");
}

TEST("measure_non_finite_result_returns_zero") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}},
                   {2, {std::numeric_limits<double>::infinity(), 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::PointPoint;
    d.inputPoints = {1, 2};
    d.dirMode = DirectionMode::TrueDistance;
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "non-finite measure result -> invalid measure value");
}
