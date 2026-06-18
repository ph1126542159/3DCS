// A3 closed-form Move tests (README §4.3/4.4/4.12/4.13). Drives the specialised
// solvers through the factory and checks their geometric guarantees.
#include <cmath>
#include <limits>

#include "dva_test.h"
#include "opendva/IMoveSolver.h"
#include "opendva/Mt19937Rng.h"

using namespace opendva;

namespace {

Vec3 applyMat(const Mat34& T, const Vec3& p) {
    return {T.m[0][0] * p.x + T.m[0][1] * p.y + T.m[0][2] * p.z + T.m[0][3],
            T.m[1][0] * p.x + T.m[1][1] * p.y + T.m[1][2] * p.z + T.m[1][3],
            T.m[2][0] * p.x + T.m[2][1] * p.y + T.m[2][2] * p.z + T.m[2][3]};
}

double dist(const Vec3& a, const Vec3& b) {
    const double dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

MovePair makePair(Vec3 o, Vec3 t, Vec3 dir = {0, 0, 1}) {
    MovePair p;
    p.objectPoint = o;
    p.targetPoint = t;
    p.direction.ijk = dir;
    return p;
}

}  // namespace

// Three-Point: object frame == target frame -> identity (no translation/rotation).
TEST("threepoint_identity") {
    MoveInputs in;
    in.type = MoveType::ThreePoint;
    in.pairs = {makePair({0, 0, 0}, {0, 0, 0}), makePair({1, 0, 0}, {1, 0, 0}),
                makePair({0, 1, 0}, {0, 1, 0})};
    auto solver = MoveSolverFactory::create(MoveType::ThreePoint);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "identity three-point should solve");
    dvatest::checkNear(r.transform.m[0][3], 0.0, 1e-9, "no translation X");
    dvatest::checkNear(r.transform.m[1][3], 0.0, 1e-9, "no translation Y");
    dvatest::checkNear(r.transform.m[2][3], 0.0, 1e-9, "no translation Z");
    // All three object points must map exactly onto their targets.
    dvatest::checkNear(dist(applyMat(r.transform, {0, 0, 0}), {0, 0, 0}), 0.0, 1e-9, "O1->T1");
    dvatest::checkNear(dist(applyMat(r.transform, {1, 0, 0}), {1, 0, 0}), 0.0, 1e-9, "O2->T2");
    dvatest::checkNear(dist(applyMat(r.transform, {0, 1, 0}), {0, 1, 0}), 0.0, 1e-9, "O3->T3");
}

// Three-Point: pure translation (+5 in X, +2 in Z) recovered, O1/O2/O3 land on T.
TEST("threepoint_pure_translation") {
    const Vec3 shift{5, 0, 2};
    MoveInputs in;
    in.type = MoveType::ThreePoint;
    in.pairs = {makePair({0, 0, 0}, {0 + shift.x, 0 + shift.y, 0 + shift.z}),
                makePair({1, 0, 0}, {1 + shift.x, 0 + shift.y, 0 + shift.z}),
                makePair({0, 1, 0}, {0 + shift.x, 1 + shift.y, 0 + shift.z})};
    auto solver = MoveSolverFactory::create(MoveType::ThreePoint);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "should solve");
    dvatest::checkNear(r.transform.m[0][3], 5.0, 1e-9, "translation X = 5");
    dvatest::checkNear(r.transform.m[1][3], 0.0, 1e-9, "translation Y = 0");
    dvatest::checkNear(r.transform.m[2][3], 2.0, 1e-9, "translation Z = 2");
    dvatest::checkNear(dist(applyMat(r.transform, {0, 0, 0}), {5, 0, 2}), 0.0, 1e-9, "O1->T1");
    dvatest::checkNear(dist(applyMat(r.transform, {0, 1, 0}), {5, 1, 2}), 0.0, 1e-9, "O3->T3");
}

// Three-Point: a 90-degree rotation about Z (O1 at origin fixed). Object frame
// X/Y axes map to target Y/-X; verify all three pairs align.
TEST("threepoint_rotation") {
    // Targets: rotate object basis by +90deg about Z. O1=(0,0,0) stays.
    MoveInputs in;
    in.type = MoveType::ThreePoint;
    in.pairs = {makePair({0, 0, 0}, {0, 0, 0}),   // O1 -> T1
                makePair({1, 0, 0}, {0, 1, 0}),   // +X -> +Y
                makePair({0, 1, 0}, {-1, 0, 0})};  // +Y -> -X
    auto solver = MoveSolverFactory::create(MoveType::ThreePoint);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "should solve");
    dvatest::checkNear(dist(applyMat(r.transform, {1, 0, 0}), {0, 1, 0}), 0.0, 1e-9, "O2->T2");
    dvatest::checkNear(dist(applyMat(r.transform, {0, 1, 0}), {-1, 0, 0}), 0.0, 1e-9, "O3->T3");
}

TEST("threepoint_handles_huge_finite_baselines") {
    MoveInputs in;
    in.type = MoveType::ThreePoint;
    in.pairs = {makePair({0, 0, 0}, {0, 0, 0}),
                makePair({1e308, 0, 0}, {0, 1e308, 0}),
                makePair({0, 0, 1}, {0, 0, 1})};
    auto solver = MoveSolverFactory::create(MoveType::ThreePoint);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "huge finite three-point baseline should solve");
    const Vec3 o2p = applyMat(r.transform, {1, 0, 0});
    const Vec3 o1p = applyMat(r.transform, {0, 0, 0});
    const Vec3 dir{o2p.x - o1p.x, o2p.y - o1p.y, o2p.z - o1p.z};
    dvatest::checkNear(dir.x, 0.0, 1e-9, "huge three-point aligned dir X");
    dvatest::checkNear(dir.y, 1.0, 1e-9, "huge three-point aligned dir Y");
    dvatest::checkNear(dir.z, 0.0, 1e-9, "huge three-point aligned dir Z");
}

TEST("threepoint_handles_huge_parallel_component_plane_spin") {
    MoveInputs in;
    in.type = MoveType::ThreePoint;
    const double huge = 1.3e308;
    const double local = 6e292;
    const double invRoot2 = 0.70710678118654752440;
    const Vec3 radial{invRoot2, -invRoot2, 0};
    in.pairs = {makePair({0, 0, 0}, {0, 0, 0}),
                makePair({1, 1, 0}, {1, 1, 0}),
                makePair({huge + local * invRoot2, huge - local * invRoot2, 0},
                         {0, 0, 1})};
    auto solver = MoveSolverFactory::create(MoveType::ThreePoint);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "huge parallel-component three-point should solve");
    const Vec3 movedProbe = applyMat(r.transform, radial);
    dvatest::checkNear(std::fabs(movedProbe.z), 1.0, 1e-9,
                       "third-point spin moves radial component into target plane");
}

// Three-Point needs a real O1-O2 baseline for the 3-2-1 locator. A zero-length
// baseline is invalid and must not fall back to the helper's default +Z axis.
TEST("threepoint_degenerate_baseline_fails") {
    MoveInputs in;
    in.type = MoveType::ThreePoint;
    in.pairs = {makePair({0, 0, 0}, {0, 0, 0}),
                makePair({0, 0, 0}, {1, 0, 0}),
                makePair({0, 1, 0}, {0, 1, 0})};
    auto solver = MoveSolverFactory::create(MoveType::ThreePoint);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "degenerate three-point object baseline must fail");
    dvatest::check(r.log.find("baseline is zero") != std::string::npos,
                   "degenerate baseline diagnostic");
}

TEST("threepoint_rejects_nonfinite_baseline") {
    MoveInputs in;
    in.type = MoveType::ThreePoint;
    in.pairs = {makePair({0, 0, 0}, {0, 0, 0}),
                makePair({std::numeric_limits<double>::infinity(), 0, 0}, {1, 0, 0}),
                makePair({0, 1, 0}, {0, 1, 0})};
    auto solver = MoveSolverFactory::create(MoveType::ThreePoint);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "non-finite three-point baseline must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "non-finite three-point baseline diagnostic");
}

TEST("threepoint_rejects_nonfinite_target_plane") {
    MoveInputs in;
    in.type = MoveType::ThreePoint;
    in.pairs = {makePair({0, 0, 0}, {0, 0, 0}),
                makePair({1, 0, 0}, {1, 0, 0}),
                makePair({0, 1, 0}, {0, std::numeric_limits<double>::infinity(), 0})};
    auto solver = MoveSolverFactory::create(MoveType::ThreePoint);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "non-finite three-point target plane must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "non-finite three-point target plane diagnostic");
}

TEST("threepoint_rejects_nonfinite_third_object_point") {
    MoveInputs in;
    in.type = MoveType::ThreePoint;
    in.pairs = {makePair({0, 0, 0}, {0, 0, 0}),
                makePair({1, 0, 0}, {1, 0, 0}),
                makePair({0, std::numeric_limits<double>::quiet_NaN(), 0}, {0, 1, 0})};
    auto solver = MoveSolverFactory::create(MoveType::ThreePoint);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "non-finite three-point third object point must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "non-finite three-point third object point diagnostic");
}

TEST("threepoint_rejects_nonfinite_transform") {
    MoveInputs in;
    in.type = MoveType::ThreePoint;
    in.pairs = {makePair({-9e307, 0, 0}, {9e307, 0, 0}),
                makePair({-8e307, 0, 0}, {1e308, 0, 0}),
                makePair({-9e307, 1, 0}, {9e307, 1, 0})};
    auto solver = MoveSolverFactory::create(MoveType::ThreePoint);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "non-finite three-point transform must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "non-finite three-point diagnostic");
}

// Two-Point: O1-O2 along X, T1-T2 along Y -> 90-degree rotation aligning the
// lines. After the move the O1->O2 direction must match T1->T2 direction.
TEST("twopoint_90deg_align") {
    MoveInputs in;
    in.type = MoveType::TwoPoint;
    // O1=origin, O2 along +X; T1=origin, T2 along +Y.
    in.pairs = {makePair({0, 0, 0}, {0, 0, 0}), makePair({1, 0, 0}, {0, 1, 0})};
    auto solver = MoveSolverFactory::create(MoveType::TwoPoint);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "two-point should solve");
    // O1 lands on T1.
    dvatest::checkNear(dist(applyMat(r.transform, {0, 0, 0}), {0, 0, 0}), 0.0, 1e-9, "O1->T1");
    // Transformed O1->O2 direction must equal the T1->T2 direction (0,1,0).
    const Vec3 o2p = applyMat(r.transform, {1, 0, 0});
    const Vec3 o1p = applyMat(r.transform, {0, 0, 0});
    const Vec3 dir{o2p.x - o1p.x, o2p.y - o1p.y, o2p.z - o1p.z};
    dvatest::checkNear(dir.x, 0.0, 1e-9, "aligned dir X");
    dvatest::checkNear(dir.y, 1.0, 1e-9, "aligned dir Y (collinear with T1-T2)");
    dvatest::checkNear(dir.z, 0.0, 1e-9, "aligned dir Z");
}

TEST("twopoint_handles_huge_finite_baselines") {
    MoveInputs in;
    in.type = MoveType::TwoPoint;
    in.pairs = {makePair({0, 0, 0}, {0, 0, 0}),
                makePair({1e308, 0, 0}, {0, 1e308, 0})};
    auto solver = MoveSolverFactory::create(MoveType::TwoPoint);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "huge finite two-point baseline should solve");
    const Vec3 o2p = applyMat(r.transform, {1, 0, 0});
    const Vec3 o1p = applyMat(r.transform, {0, 0, 0});
    const Vec3 dir{o2p.x - o1p.x, o2p.y - o1p.y, o2p.z - o1p.z};
    dvatest::checkNear(dir.x, 0.0, 1e-9, "huge two-point aligned dir X");
    dvatest::checkNear(dir.y, 1.0, 1e-9, "huge two-point aligned dir Y");
    dvatest::checkNear(dir.z, 0.0, 1e-9, "huge two-point aligned dir Z");
}

TEST("twopoint_rejects_nonfinite_baseline") {
    MoveInputs in;
    in.type = MoveType::TwoPoint;
    in.pairs = {makePair({0, 0, 0}, {0, 0, 0}),
                makePair({std::numeric_limits<double>::infinity(), 0, 0}, {0, 1, 0})};
    auto solver = MoveSolverFactory::create(MoveType::TwoPoint);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "non-finite two-point baseline must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "non-finite two-point baseline diagnostic");
}

TEST("twopoint_rejects_nonfinite_transform") {
    MoveInputs in;
    in.type = MoveType::TwoPoint;
    in.pairs = {makePair({-9e307, 0, 0}, {9e307, 0, 0}),
                makePair({-9e307, 0, 0}, {9e307, 0, 0})};
    auto solver = MoveSolverFactory::create(MoveType::TwoPoint);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "non-finite two-point transform must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "non-finite two-point diagnostic");
}

// Transform (TRANSLATE branch): direction = +X, amount = |T1-O1| = 3 -> t = (3,0,0).
TEST("transform_translate") {
    MoveInputs in;
    in.type = MoveType::Transform;
    // direction.ijk = +X (default refPoints empty -> translate branch).
    MovePair p = makePair({0, 0, 0}, {3, 0, 0}, {1, 0, 0});
    in.pairs = {p};
    auto solver = MoveSolverFactory::create(MoveType::Transform);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "transform should solve");
    // Pure translation: R = I.
    dvatest::checkNear(r.transform.m[0][0], 1.0, 1e-12, "R = identity (00)");
    dvatest::checkNear(r.transform.m[1][1], 1.0, 1e-12, "R = identity (11)");
    // t = amount(3) * dir(+X).
    dvatest::checkNear(r.transform.m[0][3], 3.0, 1e-12, "tx = 3");
    dvatest::checkNear(r.transform.m[1][3], 0.0, 1e-12, "ty = 0");
    dvatest::checkNear(r.transform.m[2][3], 0.0, 1e-12, "tz = 0");
}

TEST("transform_translate_handles_huge_finite_direction") {
    MoveInputs in;
    in.type = MoveType::Transform;
    MovePair p = makePair({0, 0, 0}, {3, 0, 0}, {1e308, 0, 0});
    in.pairs = {p};
    auto solver = MoveSolverFactory::create(MoveType::Transform);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "huge finite transform direction should solve");
    dvatest::checkNear(r.transform.m[0][3], 3.0, 1e-12, "huge finite dir tx = 3");
    dvatest::checkNear(r.transform.m[1][3], 0.0, 1e-12, "huge finite dir ty = 0");
    dvatest::checkNear(r.transform.m[2][3], 0.0, 1e-12, "huge finite dir tz = 0");
}

TEST("transform_translate_rejects_nonfinite_transform") {
    MoveInputs in;
    in.type = MoveType::Transform;
    MovePair p = makePair({-9e307, 0, 0}, {9e307, 0, 0}, {1, 0, 0});
    in.pairs = {p};
    auto solver = MoveSolverFactory::create(MoveType::Transform);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "non-finite transform translate must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "non-finite transform translate diagnostic");
}

TEST("transform_rejects_nonfinite_point_pair") {
    MoveInputs in;
    in.type = MoveType::Transform;
    MovePair p = makePair({0, 0, 0}, {std::numeric_limits<double>::quiet_NaN(), 0, 0},
                          {1, 0, 0});
    in.pairs = {p};
    auto solver = MoveSolverFactory::create(MoveType::Transform);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "non-finite transform point pair must fail");
    dvatest::check(r.log.find("point pair is not finite") != std::string::npos,
                   "non-finite transform point-pair diagnostic");
}

// Transform requires an explicit translation/rotation direction. A zero vector
// must not be treated as the helper's default +Z direction.
TEST("transform_zero_direction_fails") {
    MoveInputs in;
    in.type = MoveType::Transform;
    in.pairs = {makePair({0, 0, 0}, {3, 0, 0}, {0, 0, 0})};
    auto solver = MoveSolverFactory::create(MoveType::Transform);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "zero transform direction must fail");
}

TEST("transform_rejects_nonfinite_direction") {
    MoveInputs in;
    in.type = MoveType::Transform;
    in.pairs = {makePair({0, 0, 0}, {3, 0, 0},
                         {std::numeric_limits<double>::infinity(), 0, 0})};
    auto solver = MoveSolverFactory::create(MoveType::Transform);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "non-finite transform direction must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "non-finite transform direction diagnostic");
}

// Thermal Scaling: scale = 1 + alpha*deltaT with alpha = searchAccuracy,
// deltaT = 100 (fixed example). With searchAccuracy = 1e-3 -> scale = 1.1 on
// the rotation-block diagonal; translation zero.
TEST("thermal_scaling_diagonal") {
    MoveInputs in;
    in.type = MoveType::ThermalScaling;
    in.searchAccuracy = 1e-3;  // borrowed as alpha (CTE per degree)
    in.pairs = {makePair({0, 0, 0}, {0, 0, 0})};
    auto solver = MoveSolverFactory::create(MoveType::ThermalScaling);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "thermal scaling should solve");
    const double expected = 1.0 + 1e-3 * 100.0;  // 1.1
    dvatest::checkNear(r.transform.m[0][0], expected, 1e-12, "scale on diag X");
    dvatest::checkNear(r.transform.m[1][1], expected, 1e-12, "scale on diag Y");
    dvatest::checkNear(r.transform.m[2][2], expected, 1e-12, "scale on diag Z");
    // Off-diagonal and translation must be zero.
    dvatest::checkNear(r.transform.m[0][1], 0.0, 1e-12, "no shear");
    dvatest::checkNear(r.transform.m[0][3], 0.0, 1e-12, "no translation X");
}

TEST("thermal_scaling_rejects_nonfinite_scale") {
    MoveInputs in;
    in.type = MoveType::ThermalScaling;
    in.searchAccuracy = 1e308;
    in.pairs = {makePair({0, 0, 0}, {0, 0, 0})};
    auto solver = MoveSolverFactory::create(MoveType::ThermalScaling);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "non-finite thermal scale must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "non-finite thermal scale diagnostic");
}

TEST("thermal_scaling_rejects_nonfinite_scaling_point") {
    MoveInputs in;
    in.type = MoveType::ThermalScaling;
    in.searchAccuracy = 1e-3;
    in.pairs = {makePair({std::numeric_limits<double>::quiet_NaN(), 0, 0}, {0, 0, 0})};
    auto solver = MoveSolverFactory::create(MoveType::ThermalScaling);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "non-finite thermal scaling point must fail");
    dvatest::check(r.log.find("scaling point is not finite") != std::string::npos,
                   "non-finite thermal scaling point diagnostic");
}

TEST("thermal_scaling_requires_scaling_point") {
    MoveInputs in;
    in.type = MoveType::ThermalScaling;
    in.searchAccuracy = 1e-3;
    auto solver = MoveSolverFactory::create(MoveType::ThermalScaling);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "thermal scaling without scaling point must fail");
    dvatest::check(r.log.find("requires a scaling point") != std::string::npos,
                   "missing thermal scaling point diagnostic");
}
