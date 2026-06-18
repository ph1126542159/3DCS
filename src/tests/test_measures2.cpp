// A5 second batch of measure tests (README §6.3-6.6). Covers DimensionalDistance
// (center/min/max), CircleInterference (clearance sign), VirtualClearance
// (inner/outer diameter), FeatureMeasure (minimum gap), Combination (referenced
// measure values plus compatibility fallback), and Equation (minimal expression
// evaluator). Each test drives a stub PointResolver and checks the scalar
// against the analytic value.
#include <map>
#include <string>

#include "dva_test.h"
#include "opendva/measure/MeasureEvaluator.h"

using namespace opendva;

namespace {

// Stub resolver: current() and nominal() both read from a fixed point table.
struct StubResolver : PointResolver {
    std::map<PointId, Vec3> positions;
    std::map<PointId, Vec3> nominalPositions;
    std::map<PointId, Vec3> directions;
    std::map<PointId, double> diameters;
    std::map<MeasureId, double> measureValues;
    std::map<MeasureId, SpecLimits> measureSpecs;
    double userDllValue{0.0};
    mutable int userDllCalls{0};

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
    Vec3 direction(PointId id) const override {
        auto it = directions.find(id);
        return it == directions.end() ? Vec3{0, 0, 1} : it->second;
    }
    double measureValue(MeasureId id) const override {
        auto it = measureValues.find(id);
        return it == measureValues.end() ? 0.0 : it->second;
    }
    SpecLimits measureSpec(MeasureId id) const override {
        auto it = measureSpecs.find(id);
        return it == measureSpecs.end() ? SpecLimits{} : it->second;
    }
    double userDllMeasure(const MeasureDef&) const override {
        ++userDllCalls;
        return userDllValue;
    }
};

BuildState makeState(const StubResolver& r) {
    BuildState s;
    s.impl = &r;
    return s;
}

}  // namespace

TEST("measure_userdll_dispatches_to_resolver_callback") {
    StubResolver r;
    r.userDllValue = 21.25;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::UserDll;
    d.scale = 2.0;
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 42.5, 1e-12,
                       "UserDll measure callback result is scaled");
    dvatest::check(r.userDllCalls == 1,
                   "UserDll measure dispatches exactly once");
}

// Dimensional Distance, Center: group1 centroid (0,0,0), group2 centroid
// (10,0,0) -> centre distance 10.
TEST("measure_dim_distance_center") {
    StubResolver r;
    r.positions = {{1, {-1, 0, 0}}, {2, {1, 0, 0}},     // group1 centroid (0,0,0)
                   {3, {9, 0, 0}}, {4, {11, 0, 0}}};    // group2 centroid (10,0,0)
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::DimensionalDistance;
    d.inputPoints = {1, 2, 3, 4};
    d.dirMode = DirectionMode::TrueDistance;
    d.equation = "center";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 10.0, 1e-9, "dim distance center");
}

TEST("measure_dim_distance_center_handles_huge_same_sign_centroids") {
    StubResolver r;
    r.positions = {{1, {9e307, 0, 0}},
                   {2, {9e307, 2, 0}},
                   {3, {9e307, 10, 0}},
                   {4, {9e307, 12, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::DimensionalDistance;
    d.inputPoints = {1, 2, 3, 4};
    d.dirMode = DirectionMode::TrueDistance;
    d.equation = "center";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 10.0, 1e-9,
                       "huge same-sign dimensional center distance");
}

// Dimensional Distance, Minimum: closest cross-pair is (1,0,0)-(9,0,0) = 8.
TEST("measure_dim_distance_min") {
    StubResolver r;
    r.positions = {{1, {-1, 0, 0}}, {2, {1, 0, 0}},
                   {3, {9, 0, 0}}, {4, {11, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::DimensionalDistance;
    d.inputPoints = {1, 2, 3, 4};
    d.dirMode = DirectionMode::TrueDistance;
    d.equation = "min";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 8.0, 1e-9, "dim distance min");
}

// Dimensional Distance, Maximum: farthest cross-pair is (-1,0,0)-(11,0,0) = 12.
TEST("measure_dim_distance_max") {
    StubResolver r;
    r.positions = {{1, {-1, 0, 0}}, {2, {1, 0, 0}},
                   {3, {9, 0, 0}}, {4, {11, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::DimensionalDistance;
    d.inputPoints = {1, 2, 3, 4};
    d.dirMode = DirectionMode::TrueDistance;
    d.equation = "max";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 12.0, 1e-9, "dim distance max");
}

// Default sub-mode (empty equation) falls back to centre distance.
TEST("measure_dim_distance_default_center") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {7, 0, 0}}};  // single-point groups
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::DimensionalDistance;
    d.inputPoints = {1, 2};
    d.dirMode = DirectionMode::TrueDistance;
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 7.0, 1e-9, "dim distance default center");
}

TEST("measure_dim_distance_odd_input_count_returns_zero") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {6, 0, 0}}, {3, {8, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::DimensionalDistance;
    d.inputPoints = {1, 2, 3};
    d.dirMode = DirectionMode::TrueDistance;
    d.equation = "center";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "odd dimensional-distance input count -> invalid measure value");
}

// Circle Interference, positive clearance: with no size data, the radius sum is
// zero and a pair whose centres are 5 apart reports clearance 5.
TEST("measure_circle_interference_clearance") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {5, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::CircleInterference;
    d.inputPoints = {1, 2};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 5.0, 1e-9, "circle interference clearance");
}

// Circle Interference subtracts the two feature radii when size data exists:
// centre distance 5 - (2 + 2) = 1 clearance.
TEST("measure_circle_interference_uses_point_sizes") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {5, 0, 0}}};
    r.diameters = {{1, 4.0}, {2, 4.0}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::CircleInterference;
    d.inputPoints = {1, 2};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 1.0, 1e-9,
                       "circle interference clearance with radii");
}

// Circle Interference, min over pairs: pair A gap 5, pair B gap 2 -> min 2.
TEST("measure_circle_interference_min_pair") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {5, 0, 0}},   // pair A: 5
                   {3, {0, 0, 0}}, {4, {2, 0, 0}}};  // pair B: 2
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::CircleInterference;
    d.inputPoints = {1, 2, 3, 4};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 2.0, 1e-9, "circle interference min pair");
}

TEST("measure_circle_interference_odd_input_count_returns_zero") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {5, 0, 0}}, {3, {99, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::CircleInterference;
    d.inputPoints = {1, 2, 3};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "odd circle-interference input count -> invalid measure value");
}

TEST("measure_circle_interference_output_modes") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {5, 0, 0}},   // pair A clearance 5
                   {3, {0, 0, 0}}, {4, {2, 0, 0}}};  // pair B clearance 2
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::CircleInterference;
    d.inputPoints = {1, 2, 3, 4};
    d.equation = "max";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 5.0, 1e-9,
                       "circle interference max clearance mode");

    r.diameters = {{1, 8.0}, {2, 8.0}, {3, 1.0}, {4, 1.0}};
    d.equation = "interference";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 1.0, 1e-9,
                       "circle interference count mode");
}

// Negative clearance models interference. With the scale applied we can flip
// the sign to confirm negative results propagate (interference convention).
TEST("measure_circle_interference_negative_via_scale") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {3, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::CircleInterference;
    d.inputPoints = {1, 2};
    d.scale = -1.0;  // emulate interference (negative) for the assertion
    dvatest::checkNear(ev->evaluate(d, makeState(r)), -3.0, 1e-9, "circle interference negative");
}

// Virtual Clearance, Inner Diameter (default): four hole centres on a radius-5
// circle -> max inscribed diameter = 2 * min radius = 10.
TEST("measure_virtual_clearance_inner") {
    StubResolver r;
    r.positions = {{1, {5, 0, 0}}, {2, {-5, 0, 0}}, {3, {0, 5, 0}}, {4, {0, -5, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::VirtualClearance;
    d.inputPoints = {1, 2, 3, 4};
    d.direction.ijk = {0, 0, 1};  // axis perpendicular to the circle plane
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 10.0, 1e-9, "virtual clearance inner");
}

// Virtual Clearance, Outer Diameter: min circumscribed diameter = 2 * max radius.
// Push one point out to radius 7 -> centroid shifts; max radius near 6 -> ~12.
TEST("measure_virtual_clearance_outer") {
    StubResolver r;
    r.positions = {{1, {5, 0, 0}}, {2, {-5, 0, 0}}, {3, {0, 5, 0}}, {4, {0, -5, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::VirtualClearance;
    d.inputPoints = {1, 2, 3, 4};
    d.direction.ijk = {0, 0, 1};
    d.equation = "outer";
    // Symmetric centres -> centroid at origin -> outer diameter = 2*5 = 10.
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 10.0, 1e-9, "virtual clearance outer");
}

TEST("measure_virtual_clearance_zero_direction_returns_zero") {
    StubResolver r;
    r.positions = {{1, {5, 0, 0}}, {2, {-5, 0, 0}}, {3, {0, 5, 0}}, {4, {0, -5, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::VirtualClearance;
    d.inputPoints = {1, 2, 3, 4};
    d.direction.ijk = {0, 0, 0};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "zero virtual-clearance direction -> invalid measure value");
}

TEST("measure_virtual_clearance_uses_point_sizes") {
    StubResolver r;
    r.positions = {{1, {-1, 0, 0}}, {2, {1, 0, 0}}};
    r.diameters = {{1, 10.0}, {2, 10.0}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::VirtualClearance;
    d.inputPoints = {1, 2};
    d.direction.ijk = {0, 0, 1};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 8.0, 1e-9,
                       "virtual clearance inner with point sizes");

    r.diameters = {{1, 4.0}, {2, 4.0}};
    d.equation = "outer";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 6.0, 1e-9,
                       "virtual clearance outer with point sizes");
}

// Feature Measure, Feature-Feature minimum gap: group1 {(0,0,0),(0,1,0)},
// group2 {(2,0,0),(0,4,0)} -> closest pair (0,0,0)-(2,0,0) = 2.
TEST("measure_feature_measure_min_gap") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {0, 1, 0}},    // group1
                   {3, {2, 0, 0}}, {4, {0, 4, 0}}};   // group2
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::FeatureMeasure;
    d.inputPoints = {1, 2, 3, 4};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 2.0, 1e-9, "feature measure min gap");
}

TEST("measure_feature_measure_odd_input_count_returns_zero") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {5, 0, 0}}, {3, {99, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::FeatureMeasure;
    d.inputPoints = {1, 2, 3};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "odd feature-measure input count -> invalid measure value");
}

TEST("measure_feature_angle_between_two_feature_vectors") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {1, 0, 0}},
                   {3, {0, 0, 0}}, {4, {0, 1, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::FeatureAngle;
    d.inputPoints = {1, 2, 3, 4};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 90.0, 1e-9,
                       "feature angle between orthogonal vectors");
}

TEST("measure_feature_angle_honors_custom_direction_sign") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {1, 0, 0}},
                   {3, {0, 0, 0}}, {4, {0, 1, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::FeatureAngle;
    d.inputPoints = {1, 2, 3, 4};
    d.direction.ijk = {0, 0, -1};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), -90.0, 1e-9,
                       "feature angle sign follows custom direction");
}

TEST("measure_feature_angle_zero_direction_returns_zero") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {1, 0, 0}},
                   {3, {0, 0, 0}}, {4, {0, 1, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::FeatureAngle;
    d.inputPoints = {1, 2, 3, 4};
    d.direction.ijk = {0, 0, 0};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "zero feature-angle direction -> invalid measure value");
}

// Combination over referenced active measures: inputFeatures carries the
// referenced MeasureIds for the reference evaluator.
TEST("measure_combination_uses_referenced_measure_values") {
    StubResolver r;
    r.measureValues = {{101, 2.0}, {102, 5.0}, {103, 9.0}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Combination;
    d.inputFeatures = {101, 102, 103};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 16.0, 1e-9,
                       "combination referenced sum");

    d.equation = "subtract";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), -12.0, 1e-9,
                       "combination referenced subtract");

    d.equation = "max";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 9.0, 1e-9,
                       "combination referenced max");
}

TEST("measure_combination_in_spec_returns_referenced_pass_rate") {
    StubResolver r;
    r.measureValues = {{101, 2.0}, {102, 5.0}, {103, 12.0}};
    SpecLimits spec;
    spec.lslActive = true;
    spec.uslActive = true;
    spec.lsl = 0.0;
    spec.usl = 10.0;
    r.measureSpecs = {{101, spec}, {102, spec}, {103, spec}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Combination;
    d.inputFeatures = {101, 102, 103};
    d.equation = "in_spec";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 2.0 / 3.0, 1e-12,
                       "combination in-spec pass rate");
}

TEST("measure_combination_in_spec_without_refs_returns_zero") {
    StubResolver r;
    r.positions = {{1, {4, 0, 0}}, {2, {5, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Combination;
    d.inputPoints = {1, 2};
    d.equation = "in_spec";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "combination in-spec without measure refs -> invalid measure value");
}

// Combination, degraded Sum over point X coordinates: 1 + 2 + 4 = 7.
TEST("measure_combination_sum") {
    StubResolver r;
    r.positions = {{1, {1, 0, 0}}, {2, {2, 0, 0}}, {3, {4, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Combination;
    d.inputPoints = {1, 2, 3};
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 7.0, 1e-9, "combination sum");
}

// Combination, degraded Subtract: 10 - 3 - 2 = 5.
TEST("measure_combination_subtract") {
    StubResolver r;
    r.positions = {{1, {10, 0, 0}}, {2, {3, 0, 0}}, {3, {2, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Combination;
    d.inputPoints = {1, 2, 3};
    d.equation = "subtract";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 5.0, 1e-9, "combination subtract");
}

// Combination, Max over point X coordinates: max(1,9,4) = 9.
TEST("measure_combination_max") {
    StubResolver r;
    r.positions = {{1, {1, 0, 0}}, {2, {9, 0, 0}}, {3, {4, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Combination;
    d.inputPoints = {1, 2, 3};
    d.equation = "max";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 9.0, 1e-9, "combination max");
}

// Equation, point coordinate sum: [P1X:1] + [P2X:1] = 3 + 4 = 7.
TEST("measure_equation_point_sum") {
    StubResolver r;
    r.positions = {{1, {3, 0, 0}}, {2, {4, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.inputPoints = {1, 2};
    d.equation = "[P1X:1]+[P2X:1]";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 7.0, 1e-9, "equation point sum");
}

TEST("measure_equation_p1_p2_without_explicit_index") {
    StubResolver r;
    r.positions = {{1, {3, 0, 0}}, {2, {4, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.inputPoints = {1, 2};
    d.equation = "[P1X]+[P2X]";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 7.0, 1e-9,
                       "equation P1/P2 implicit point variables");
}

// Equation, Euclidean distance via sqrt: sqrt(3^2 + 4^2) = 5. Uses two point
// coordinates so the [Keyword:Index] + sqrt + ^ path is exercised together.
TEST("measure_equation_sqrt_distance") {
    StubResolver r;
    r.positions = {{1, {3, 4, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.inputPoints = {1};
    d.equation = "sqrt([P1X:1]^2+[P1Y:1]^2)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 5.0, 1e-9, "equation sqrt distance");
}

// Equation, P1C/P2C expose feature-of-size radius values from point diameters.
TEST("measure_equation_point_radius_variables") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {0, 0, 0}}};
    r.diameters = {{1, 8.0}, {2, 10.0}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.inputPoints = {1, 2};
    d.equation = "[P1C:1]+[P2C:1]";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 9.0, 1e-9,
                       "equation point radius variables");
}

// Equation, P1I/J/K and P2I/J/K expose point direction components.
TEST("measure_equation_point_direction_variables") {
    StubResolver r;
    r.positions = {{1, {0, 0, 0}}, {2, {0, 0, 0}}};
    r.directions = {{1, {0.25, 0.5, 0.75}}, {2, {1.0, 2.0, 3.0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.inputPoints = {1, 2};
    d.equation = "[P1I:1]+[P1J:1]+[P2K:1]";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 3.75, 1e-9,
                       "equation point direction variables");
}

// Equation, DRI/DRJ/DRK expose the measure direction vector components.
TEST("measure_equation_measure_direction_variables") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.direction.ijk = {0.2, 0.3, 0.4};
    d.equation = "[DRI:1]+[DRJ:1]+[DRK:1]";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.9, 1e-9,
                       "equation measure direction variables");
}

// Equation, precedence and parentheses: 2 + 3 * 4 = 14, (2 + 3) * 4 = 20.
TEST("measure_equation_precedence") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "2+3*4";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 14.0, 1e-9, "equation precedence");
    d.equation = "(2+3)*4";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 20.0, 1e-9, "equation parentheses");
}

// Equation, VAL without an index keeps the legacy def.scale compatibility path:
// [VAL] * 2 = 5 * 2 = 10.
TEST("measure_equation_val_constant") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.scale = 5.0;
    d.equation = "[VAL]*2";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 10.0, 1e-9, "equation VAL constant");
}

// Equation, indexed VAL entries read from the measure value list.
TEST("measure_equation_val_indexed_value_list") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.scale = 99.0;
    d.values = {2.5, 4.0};
    dvatest::check(d.values.size() == 2, "equation value list assigned");
    d.equation = "[VAL:1]*2+[VAL:2]";
    const double actual = ev->evaluate(d, makeState(r));
    dvatest::checkNear(actual, 9.0, 1e-9,
                       "equation indexed VAL value list");
}

// Equation, [MS:n] reads another active measure from the Equation measure list.
// inputFeatures carries the referenced MeasureIds in list order for this
// reference build.
TEST("measure_equation_referenced_measure_values") {
    StubResolver r;
    r.measureValues = {{101, 2.5}, {102, 4.0}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.inputFeatures = {101, 102};
    d.equation = "[MS:1]*2+[MS:2]";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 9.0, 1e-9,
                       "equation referenced measure values");
}

// Equation, a multi-line string list returns the last string result and [STR:n]
// references an earlier string result.
TEST("measure_equation_string_list_references") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "2+3\n[STR:1]*4";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 20.0, 1e-9,
                       "equation string list references");
}

// Equation, REM lines are comments in the string list and do not occupy a STR
// result slot.
TEST("measure_equation_string_list_rem_comments") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "2+3\nREM ignored string\n[STR:1]*4";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 20.0, 1e-9,
                       "equation string list rem comments");
}

// Equation, 3DCS supports REM comments but does not allow a REM line as the
// last string in the string list.
TEST("measure_equation_string_list_rem_comment_cannot_be_last") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "2+3\nrem trailing comment";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "equation string list trailing REM -> 0");
}

// Equation, SETCFG output-unit directives are configuration lines and do not
// occupy a STR result slot in the equation string list.
TEST("measure_equation_string_list_setcfg_directives") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "SETCFG=ANGLEDATA\n2+3\n[STR:1]*4";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 20.0, 1e-9,
                       "equation string list SETCFG directives");
}

TEST("measure_equation_setcfg_after_rem_keeps_previous_result") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "2+3\nREM ignored string\nSETCFG=LENGTHDATA";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 5.0, 1e-12,
                       "SETCFG after REM keeps previous result");
}

TEST("measure_equation_string_list_invalid_setcfg_returns_zero") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "SETCFG=BOGUSDATA\n2+3";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "invalid SETCFG directive -> 0");
}

TEST("measure_equation_setcfg_keyword_and_value_must_be_uppercase") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "SETCFG=LENGTHDATA\n2+3";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 5.0, 1e-12,
                       "uppercase SETCFG directive is accepted");

    d.equation = "setcfg=lengthdata\n2+3";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "lowercase SETCFG directive -> 0");
}

// Equation, common scalar functions from the 3DCS operator list. Degree inputs
// for trig functions must be converted explicitly.
TEST("measure_equation_common_scalar_functions") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "abs(-3)+round(2.6)+tan(deg2rad(45))+log(1)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 7.0, 1e-9,
                       "equation common scalar functions");

    d.equation = "sqrt(1,2)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "invalid unary function arity -> 0");

    d.equation = "sqrt()";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "empty unary function arguments -> 0");

    d.equation = "sqrt((-1))";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "parenthesized negative sqrt input -> 0");

    d.equation = "pow(,2)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "empty function argument slot -> 0");

    d.equation = "if_then_else(1,2)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "invalid conditional arity -> 0");

    d.equation = "MIN()";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "empty MIN arguments -> 0");

    d.equation = "MAX()";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "empty MAX arguments -> 0");
}

TEST("measure_equation_exp_function") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "exp(log(5))";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 5.0, 1e-9,
                       "equation exp function");

    d.equation = "log(0)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "log zero input -> 0");

    d.equation = "exp(1000)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "non-finite exp result -> 0");

    d.equation = "inch2mm(1e308)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "non-finite inch conversion result -> 0");
}

TEST("measure_equation_pow_function") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "pow(2,3)+pow(4,0.5)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 10.0, 1e-9,
                       "equation pow function");

    d.equation = "pow(1e308,2)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "non-finite pow result -> 0");

    d.equation = "pow((-1),0.5)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "parenthesized non-finite pow result -> 0");
}

TEST("measure_equation_mod_function") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "mod(10,3)+mod(5.5,2)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 2.5, 1e-9,
                       "equation mod function");

    d.equation = "mod(10,0)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "zero mod divisor -> 0");
}

TEST("measure_equation_math_functions_must_be_lowercase") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "SIN(1.5707963267948966)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "uppercase math function -> 0");

    d.equation = "sin(1.5707963267948966)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 1.0, 1e-9,
                       "lowercase math function");
}

// Equation, sin/cos/tan consume radians; use deg2rad when writing degree inputs.
TEST("measure_equation_trig_functions_use_radians") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "sin(1.5707963267948966)+cos(0)+tan(0.7853981633974483)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 3.0, 1e-9,
                       "equation trig functions use radians");
}

TEST("measure_equation_trig_functions_validate_input_range") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "sin(7)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "out-of-range sin input -> 0");

    d.equation = "tan(7)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "out-of-range tan input -> 0");

    d.equation = "tan(2)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "tan outside half-pi range -> 0");
}

TEST("measure_equation_cos_function_validates_input_range") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "cos(2)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "out-of-range cos input -> 0");

    d.equation = "cos(1.5707963267948966)+1";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "cos boundary input -> 0");
}

// Equation, scientific notation supports signed exponents.
TEST("measure_equation_scientific_notation_signed_exponents") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "1e-3+2E+2";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 200.001, 1e-12,
                       "equation scientific notation signed exponents");
}

// Equation, inverse trig functions return radians.
TEST("measure_equation_inverse_trig_functions_return_radians") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "arcsin(0.5)+arccos(0.5)+arctan(1)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)),
                       0.5235987755982988 + 1.0471975511965976 + 0.7853981633974483,
                       1e-9, "equation inverse trig functions return radians");
}

TEST("measure_equation_inverse_trig_short_name_aliases") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "asin(0.5)+acos(0.5)+atan(1)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)),
                       0.5235987755982988 + 1.0471975511965976 + 0.7853981633974483,
                       1e-9, "equation inverse trig short-name aliases");

    d.equation = "asin(2)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "out-of-range inverse trig short-name input -> 0");
}

TEST("measure_equation_unknown_function_returns_zero") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "foo(1)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "unknown equation function returns zero");
}

TEST("measure_equation_inverse_trig_long_name_aliases") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "arcsine(0.5)+arccosine(0.5)+arctangent(1)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)),
                       0.5235987755982988 + 1.0471975511965976 + 0.7853981633974483,
                       1e-9, "equation inverse trig long-name aliases");
}

// Equation, advanced functions from the 3DCS operator list: inverse trig,
// degree/radian conversion, unit conversion, and multi-argument MIN/MAX.
TEST("measure_equation_advanced_scalar_functions") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "arcsin(0.5)+arccos(0.5)+arctan(1)+deg2rad(180)"
                 "+rad2deg(1.5707963267948966)+mm2inch(25.4)+inch2mm(2)"
                 "+MIN(3,1,2)+MAX(3,1,2)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)),
                       0.5235987755982988 + 1.0471975511965976 +
                           0.7853981633974483 + 3.14159265358979323846 +
                           90.0 + 1.0 + 50.8 + 1.0 + 3.0,
                       1e-9, "equation advanced scalar functions");

    d.equation = "deg2rad(1e308)";
    const double hugeDeg2Rad = ev->evaluate(d, makeState(r));
    const double expectedHugeDeg2Rad =
        1e308 / 180.0 * 3.14159265358979323846;
    dvatest::check(std::isfinite(hugeDeg2Rad) &&
                       std::fabs((hugeDeg2Rad - expectedHugeDeg2Rad) /
                                 expectedHugeDeg2Rad) < 1e-12,
                   "huge finite deg2rad result stays finite");

    d.equation = "rad2deg(1e306)";
    const double hugeRad2Deg = ev->evaluate(d, makeState(r));
    const double expectedHugeRad2Deg =
        1e306 / 3.14159265358979323846 * 180.0;
    dvatest::check(std::isfinite(hugeRad2Deg) &&
                       std::fabs((hugeRad2Deg - expectedHugeRad2Deg) /
                                 expectedHugeRad2Deg) < 1e-12,
                   "huge finite rad2deg result stays finite");
}

TEST("measure_equation_min_max_cannot_nest_multi_entry_operators") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "MIN(MIN(1,2),3)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "nested MIN -> 0");

    d.equation = "MAX(1,MIN(2,3))";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "nested MAX/MIN -> 0");
}

TEST("measure_equation_conditional_operators_cannot_nest") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "if_then_else(if_then_else(1,1,0),2,3)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "nested if_then_else -> 0");

    d.equation = "if (if_then_else(1,1,0)) == (1) then (2) else (3)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "nested conditional in if token -> 0");
}

// Equation, roundup/rounddown mirror the 3DCS basic operator list.
TEST("measure_equation_roundup_rounddown_functions") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "roundup(2.1)+rounddown(2.9)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 5.0, 1e-9,
                       "equation roundup rounddown functions");
}

// Equation, ang2pos maps negative angles into [0,360] and ang2neg maps angles
// above 180 into the signed range.
TEST("measure_equation_angle_conversion_functions") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "ang2pos(-30)+ang2pos(45)+ang2neg(270)+ang2neg(90)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 375.0, 1e-9,
                       "equation angle conversion functions");

    d.equation = "ang2pos(-450)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 270.0, 1e-9,
                       "ang2pos normalizes multiple negative turns");

    d.equation = "ang2neg(900)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 180.0, 1e-9,
                       "ang2neg normalizes multiple positive turns");
}

// Equation, if_then_else uses numeric comparison conditions and returns the
// selected branch expression.
TEST("measure_equation_if_then_else_conditions") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "if_then_else(3>2,10,20)+if_then_else(2>=2,1,0)"
                 "+if_then_else(1==2,100,5)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 16.0, 1e-9,
                       "equation if_then_else conditions");
}

TEST("measure_equation_uppercase_if_then_else_function") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "IF_THEN_ELSE(3>2,10,20)+IF_THEN_ELSE(1==2,100,5)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 15.0, 1e-9,
                       "equation uppercase IF_THEN_ELSE function");
}

TEST("measure_equation_conditional_keywords_are_case_insensitive") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "If_Then_Else(3>2,10,20)+If (2) >= (2) Then (1) Else (0)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 11.0, 1e-9,
                       "equation conditional keywords are case-insensitive");
}

// Equation, official 3DCS conditional syntax accepts parenthesized tokens with
// then/else branch values.
TEST("measure_equation_if_then_else_statement") {
    StubResolver r;
    r.positions[1] = Vec3{1.0, 0.0, 0.0};
    r.positions[2] = Vec3{4.0, 0.0, 0.0};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.inputPoints = {1, 2};
    d.equation = "if ([P1X:1]) < ([P2X:1]) then (2.0) else (1.0)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 2.0, 1e-9,
                       "equation if then else statement");
}

TEST("measure_equation_uppercase_if_statement") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "IF (1) < (2) THEN (3) ELSE (4)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 3.0, 1e-12,
                       "equation uppercase IF statement");
}

TEST("measure_equation_if_statement_accepts_omitted_then_else_keywords") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;

    d.equation = "if (1) < (2) (3) else (4)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 3.0, 1e-12,
                       "if statement missing then keyword");

    d.equation = "if (1) < (2) then (3) (4)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 3.0, 1e-12,
                       "if statement missing else keyword");

    d.equation = "if (2) < (1) (3) (4)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 4.0, 1e-12,
                       "if statement missing then and else keywords");
}

TEST("measure_equation_conditionals_skip_unselected_branch_errors") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;

    d.equation = "if_then_else(1, 7, sqrt(-1))";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 7.0, 1e-12,
                       "if_then_else skips invalid false branch");

    d.equation = "if (1) < (2) then (7) else (sqrt(-1))";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 7.0, 1e-12,
                       "if statement skips invalid false branch");

    d.equation = "if (2) < (1) then (sqrt(-1)) else (11)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 11.0, 1e-12,
                       "if statement skips invalid true branch");
}

// Equation, malformed input returns 0 (invalid-measure convention).
TEST("measure_equation_invalid_zero") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "2+";  // dangling operator
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12, "equation invalid -> 0");
}

TEST("measure_equation_non_finite_numeric_results_zero") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "1e309";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "overflow numeric literal -> 0");

    d.equation = "1e308*1e308";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "non finite arithmetic result -> 0");

    d.equation = "1e308+1e308";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "non finite sum result -> 0");

    d.equation = "(1e308)*(1e308)";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "parenthesized non finite arithmetic result -> 0");
}

TEST("measure_equation_top_level_equality_operator_invalid") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "1==1";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "top-level equality operator -> 0");
}

TEST("measure_equation_string_over_400_chars_zero") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "1";
    for (int i = 0; i < 200; ++i) d.equation += "+1";
    dvatest::check(d.equation.size() == 401, "test equation is 401 characters");
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "equation over 400 chars -> 0");
}

TEST("measure_equation_negative_constants_must_be_parenthesized") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.equation = "-1+2";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "bare negative constant -> 0");

    d.equation = "(-1)+2";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 1.0, 1e-12,
                       "parenthesized negative constant");
}

TEST("measure_equation_invalid_variable_index_zero") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.values = {2.5};
    d.equation = "[VAL:abc]+1";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "equation invalid variable index -> 0");
}

TEST("measure_equation_point_variable_index_zero_invalid") {
    StubResolver r;
    r.positions = {{1, {3, 4, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.inputPoints = {1};
    d.equation = "[P1X:0]";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "P point zero index -> 0");
}

TEST("measure_equation_point_variable_only_p1_p2_groups_valid") {
    StubResolver r;
    r.positions = {{1, {3, 0, 0}}, {2, {4, 0, 0}}};
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.inputPoints = {1, 2};
    d.equation = "[P3X:1]";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "P3 group variable -> 0");
}

TEST("measure_equation_direction_variable_index_zero_invalid") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.direction.ijk = {0.2, 0.3, 0.4};
    d.equation = "[DRI:0]";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "DRI zero index -> 0");
}

TEST("measure_equation_direction_variable_only_index_one_valid") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.direction.ijk = {0.2, 0.3, 0.4};
    d.equation = "[DRJ:2]";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "DRJ index 2 -> 0");
}

TEST("measure_equation_direction_variable_requires_explicit_index") {
    StubResolver r;
    auto ev = makeReferenceMeasureEvaluator();
    MeasureDef d;
    d.type = MeasureType::Equation;
    d.direction.ijk = {0.2, 0.3, 0.4};
    d.equation = "[DRK]";
    dvatest::checkNear(ev->evaluate(d, makeState(r)), 0.0, 1e-12,
                       "DRK missing index -> 0");
}
