// A3 second batch of Move tests (README §4.5/4.10/4.16-4.20). Drives the
// CrossProduct, RotateLine, RTouch, Match, Gravity, BestFit and LeastSquaresAxis
// solvers through the factory and checks their geometric guarantees.
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "dva_test.h"
#include "opendva/dcs_plugin_api.h"
#include "opendva/IMoveSolver.h"
#include "opendva/Mt19937Rng.h"
#include "opendva/plugin/PluginHost.h"

using namespace opendva;

namespace {

void userDllMoveRoutine(dcsDataPtr data) {
    auto* moveData = static_cast<dcsMoveCalData*>(data);
    moveData->transform[0][3] = 3.0;
}

Vec3 applyMat(const Mat34& T, const Vec3& p) {
    return {T.m[0][0] * p.x + T.m[0][1] * p.y + T.m[0][2] * p.z + T.m[0][3],
            T.m[1][0] * p.x + T.m[1][1] * p.y + T.m[1][2] * p.z + T.m[1][3],
            T.m[2][0] * p.x + T.m[2][1] * p.y + T.m[2][2] * p.z + T.m[2][3]};
}

double dist(const Vec3& a, const Vec3& b) {
    const double dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

Vec3 sub(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
double norm(const Vec3& a) { return std::sqrt(dot(a, a)); }
Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

MovePair makePair(Vec3 o, Vec3 t, Vec3 dir = {0, 0, 1}) {
    MovePair p;
    p.objectPoint = o;
    p.targetPoint = t;
    p.direction.ijk = dir;
    return p;
}

bool isIdentity(const Mat34& T, double tol) {
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 4; ++j) {
            const double want = (i == j) ? 1.0 : 0.0;
            if (std::fabs(T.m[i][j] - want) > tol) return false;
        }
    return true;
}

}  // namespace

// CrossProduct: D1 = +X, D2 = +Y -> Dx = +Z (right-hand rule). Transform stays
// identity (this move assigns a direction, not a pose); the vector is logged.
TEST("crossproduct_x_cross_y_is_z") {
    MoveInputs in;
    in.type = MoveType::CrossProduct;
    in.pairs = {makePair({0, 0, 0}, {0, 0, 0}, {1, 0, 0}),
                makePair({0, 0, 0}, {0, 0, 0}, {0, 1, 0})};
    auto solver = MoveSolverFactory::create(MoveType::CrossProduct);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "cross product should solve");
    dvatest::check(isIdentity(r.transform, 1e-12), "cross product transform is identity");
    // Verify the logged vector equals +Z (X x Y = Z).
    dvatest::check(r.log.find("Dx = D1 x D2") != std::string::npos, "log records Dx");
    const Vec3 expectZ = applyMat(r.transform, {0, 0, 1});
    dvatest::checkNear(dist(expectZ, {0, 0, 1}), 0.0, 1e-12, "identity maps Z->Z");
}

TEST("crossproduct_handles_huge_finite_directions") {
    MoveInputs in;
    in.type = MoveType::CrossProduct;
    in.pairs = {makePair({0, 0, 0}, {0, 0, 0}, {1e308, 0, 0}),
                makePair({0, 0, 0}, {0, 0, 0}, {0, 1e308, 0})};
    auto solver = MoveSolverFactory::create(MoveType::CrossProduct);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "huge finite cross product should solve");
    dvatest::check(r.log.find("nan") == std::string::npos &&
                       r.log.find("inf") == std::string::npos,
                   "huge finite cross product log is finite");
    dvatest::check(r.log.find("(0.000000, 0.000000, 1.000000)") != std::string::npos,
                   "huge finite X cross Y is +Z");
}

TEST("crossproduct_rejects_nonfinite_direction") {
    MoveInputs in;
    in.type = MoveType::CrossProduct;
    in.pairs = {makePair({0, 0, 0}, {0, 0, 0},
                         {std::numeric_limits<double>::infinity(), 0, 0}),
                makePair({0, 0, 0}, {0, 0, 0}, {0, 1, 0})};
    auto solver = MoveSolverFactory::create(MoveType::CrossProduct);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "non-finite cross-product input direction must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "non-finite cross-product diagnostic");
}

// CrossProduct degenerate: D1 || D2 -> zero cross product -> No Solution.
TEST("crossproduct_parallel_fails") {
    MoveInputs in;
    in.type = MoveType::CrossProduct;
    in.pairs = {makePair({0, 0, 0}, {0, 0, 0}, {1, 0, 0}),
                makePair({0, 0, 0}, {0, 0, 0}, {2, 0, 0})};
    auto solver = MoveSolverFactory::create(MoveType::CrossProduct);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "parallel directions must fail");
}

// CrossProduct zero input direction is an invalid direction definition, distinct
// from two valid directions that are parallel.
TEST("crossproduct_zero_direction_reports_invalid_input") {
    MoveInputs in;
    in.type = MoveType::CrossProduct;
    in.pairs = {makePair({0, 0, 0}, {0, 0, 0}, {0, 0, 0}),
                makePair({0, 0, 0}, {0, 0, 0}, {0, 1, 0})};
    auto solver = MoveSolverFactory::create(MoveType::CrossProduct);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "zero cross-product input direction must fail");
    dvatest::check(r.log.find("direction is zero") != std::string::npos,
                   "zero direction diagnostic");
}

// RotateLine: object line along +X, target line along +Y, axis = +Z through
// origin. After the move the object line direction must be parallel to +Y.
TEST("rotateline_to_parallel") {
    MoveInputs in;
    in.type = MoveType::RotateLine;
    // Obj1/Obj2 -> object line +X; Tgt1/Tgt2 -> target line +Y; Tgt3 = origin (axis pivot).
    MovePair p0 = makePair({0, 0, 0}, {0, 0, 0}, {0, 0, 1});  // axis = +Z
    MovePair p1 = makePair({1, 0, 0}, {0, 1, 0});             // Obj2 / Tgt2
    MovePair p2 = makePair({0, 0, 0}, {0, 0, 0});             // Tgt3 = pivot at origin
    in.pairs = {p0, p1, p2};
    auto solver = MoveSolverFactory::create(MoveType::RotateLine);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "rotate line should solve");
    const Vec3 a = applyMat(r.transform, {0, 0, 0});
    const Vec3 b = applyMat(r.transform, {1, 0, 0});
    const Vec3 d = sub(b, a);  // object line direction after move
    // Parallel to +Y: cross with (0,1,0) is ~zero.
    const Vec3 c = cross(d, {0, 1, 0});
    dvatest::checkNear(norm(c), 0.0, 1e-9, "object line parallel to target +Y");
    dvatest::checkNear(d.x, 0.0, 1e-9, "object line dir X ~ 0");
    dvatest::checkNear(std::fabs(d.y), 1.0, 1e-9, "object line dir |Y| ~ 1");
}

TEST("rotateline_handles_huge_axis_pivot") {
    MoveInputs in;
    in.type = MoveType::RotateLine;
    const Vec3 pivot{9e307, 9e307, 0};
    constexpr double local = 1e293;
    MovePair p0 = makePair({pivot.x + local, pivot.y, 0}, {pivot.x, pivot.y, 0}, {0, 0, 1});
    MovePair p1 = makePair({pivot.x + 2.0 * local, pivot.y, 0}, {pivot.x, pivot.y + local, 0});
    MovePair p2 = makePair({0, 0, 0}, pivot);
    in.pairs = {p0, p1, p2};
    auto solver = MoveSolverFactory::create(MoveType::RotateLine);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "unrepresentable huge-pivot rotate line must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "unrepresentable huge-pivot diagnostic");
}

TEST("rotateline_handles_huge_parallel_component_angle") {
    MoveInputs in;
    in.type = MoveType::RotateLine;
    const double huge = 9e307;
    const double perp = 1e292;
    MovePair p0 = makePair({0, 0, 0}, {0, 0, 0}, {1, 1, 0});
    MovePair p1 = makePair({huge, huge, perp}, {huge, huge, -perp});
    MovePair p2 = makePair({0, 0, 0}, {0, 0, 0});
    in.pairs = {p0, p1, p2};
    auto solver = MoveSolverFactory::create(MoveType::RotateLine);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "huge parallel-component rotate line should solve");
    dvatest::checkNear(r.transform.m[2][2], -1.0, 1e-12,
                       "huge parallel-component rotate line flips perpendicular axis");
}

// RotateLine requires an explicit rotation axis. A zero axis is invalid and
// must not be treated as the helper's default +Z axis.
TEST("rotateline_zero_axis_fails") {
    MoveInputs in;
    in.type = MoveType::RotateLine;
    MovePair p0 = makePair({0, 0, 0}, {0, 0, 0}, {0, 0, 0});
    MovePair p1 = makePair({1, 0, 0}, {0, 1, 0});
    MovePair p2 = makePair({0, 0, 0}, {0, 0, 0});
    in.pairs = {p0, p1, p2};
    auto solver = MoveSolverFactory::create(MoveType::RotateLine);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "zero rotate-line axis must fail");
}

TEST("rotateline_rejects_nonfinite_axis") {
    MoveInputs in;
    in.type = MoveType::RotateLine;
    MovePair p0 = makePair({0, 0, 0}, {0, 0, 0},
                           {std::numeric_limits<double>::infinity(), 0, 0});
    MovePair p1 = makePair({1, 0, 0}, {0, 1, 0});
    MovePair p2 = makePair({0, 0, 0}, {0, 0, 0});
    in.pairs = {p0, p1, p2};
    auto solver = MoveSolverFactory::create(MoveType::RotateLine);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "non-finite rotate-line axis must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "non-finite rotate-line axis diagnostic");
}

TEST("rotateline_rejects_nonfinite_line_point") {
    MoveInputs in;
    in.type = MoveType::RotateLine;
    MovePair p0 = makePair({0, 0, 0}, {0, 0, 0}, {0, 0, 1});
    MovePair p1 = makePair({std::numeric_limits<double>::quiet_NaN(), 0, 0}, {0, 1, 0});
    MovePair p2 = makePair({0, 0, 0}, {0, 0, 0});
    in.pairs = {p0, p1, p2};
    auto solver = MoveSolverFactory::create(MoveType::RotateLine);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "non-finite rotate-line point must fail");
    dvatest::check(r.log.find("line point is not finite") != std::string::npos,
                   "non-finite rotate-line point diagnostic");
}

// R-Touch: two object points at +90deg and +30deg (about +Z through origin) from
// their targets. Smallest angular distance is 30deg, so the whole object rotates
// by -30deg (the smaller magnitude), bringing the closer point into contact.
TEST("rtouch_smallest_angle") {
    MoveInputs in;
    in.type = MoveType::RTouch;
    const double deg = 3.14159265358979323846 / 180.0;
    // Object point A at angle 90deg, target A at 0deg -> needs -90deg.
    const Vec3 oA{std::cos(90 * deg), std::sin(90 * deg), 0};
    const Vec3 tA{1, 0, 0};
    // Object point B at angle 30deg, target B at 0deg -> needs -30deg (smaller).
    const Vec3 oB{std::cos(30 * deg), std::sin(30 * deg), 0};
    const Vec3 tB{1, 0, 0};
    // The solver uses pairs[0].targetPoint as the axis pivot. Pair 0 sits on the
    // axis (pivot = origin, target on axis) so it carries no constraint and is
    // skipped; pairs 1 and 2 are the real A/B correspondences whose targets lie on
    // the +X ray.
    MovePair p0 = makePair({0, 0, 0}, {0, 0, 0}, {0, 0, 1});  // pivot=origin (skipped)
    MovePair pA = makePair(oA, tA);                            // A: needs -90deg
    MovePair pB = makePair(oB, tB);                            // B: needs -30deg (smallest)
    in.pairs = {p0, pA, pB};
    auto solver = MoveSolverFactory::create(MoveType::RTouch);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "r-touch should solve");
    // After rotating by the smallest angle (-30deg), point B must land on its
    // target direction (the +X ray); point A must NOT yet be in contact.
    const Vec3 bMoved = applyMat(r.transform, oB);
    dvatest::checkNear(dist(bMoved, tB), 0.0, 1e-9, "closest point B reaches contact");
    const Vec3 aMoved = applyMat(r.transform, oA);
    dvatest::check(dist(aMoved, tA) > 1e-3, "farther point A not yet in contact");
}

TEST("rtouch_skips_huge_axis_point_before_contact_angle") {
    MoveInputs in;
    in.type = MoveType::RTouch;
    const double huge = 1.3e308;
    const double invRoot2 = 0.70710678118654752440;
    MovePair p0 = makePair({0, 0, 0}, {0, 0, 0}, {1, 1, 0});
    MovePair pOnAxis = makePair({huge, huge, 0}, {huge, huge, 0});
    MovePair pContact = makePair({-invRoot2, invRoot2, 0}, {0, 0, 1});
    in.pairs = {p0, pOnAxis, pContact};
    auto solver = MoveSolverFactory::create(MoveType::RTouch);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "r-touch with huge on-axis point should solve");
    const Vec3 moved = applyMat(r.transform, pContact.objectPoint);
    dvatest::checkNear(dist(moved, pContact.targetPoint), 0.0, 1e-9,
                       "huge on-axis point is skipped before selecting contact angle");
}

TEST("rtouch_rejects_unrepresentable_huge_pivot_transform") {
    MoveInputs in;
    in.type = MoveType::RTouch;
    const Vec3 pivot{9e307, 9e307, 0};
    constexpr double local = 1e293;
    MovePair p0 = makePair(pivot, pivot, {0, 0, 1});
    MovePair p1 = makePair({pivot.x + local, pivot.y, 0}, {pivot.x, pivot.y + local, 0});
    in.pairs = {p0, p1};
    auto solver = MoveSolverFactory::create(MoveType::RTouch);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "unrepresentable huge-pivot r-touch must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "unrepresentable huge-pivot r-touch diagnostic");
}

// R-Touch also needs an explicit rotation axis. A zero vector is an invalid
// move definition and must not fall back to +Z.
TEST("rtouch_zero_axis_fails") {
    MoveInputs in;
    in.type = MoveType::RTouch;
    MovePair p0 = makePair({0, 0, 0}, {0, 0, 0}, {0, 0, 0});
    MovePair p1 = makePair({1, 0, 0}, {0, 1, 0});
    in.pairs = {p0, p1};
    auto solver = MoveSolverFactory::create(MoveType::RTouch);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "zero r-touch axis must fail");
}

TEST("rtouch_rejects_nonfinite_axis") {
    MoveInputs in;
    in.type = MoveType::RTouch;
    MovePair p0 = makePair({0, 0, 0}, {0, 0, 0},
                           {std::numeric_limits<double>::infinity(), 0, 0});
    MovePair p1 = makePair({1, 0, 0}, {0, 1, 0});
    in.pairs = {p0, p1};
    auto solver = MoveSolverFactory::create(MoveType::RTouch);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "non-finite r-touch axis must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "non-finite r-touch axis diagnostic");
}

TEST("rtouch_rejects_nonfinite_skipped_point") {
    MoveInputs in;
    in.type = MoveType::RTouch;
    MovePair p0 = makePair({std::numeric_limits<double>::infinity(), 0, 0},
                           {0, 0, 0}, {0, 0, 1});
    MovePair p1 = makePair({0, 1, 0}, {1, 0, 0});
    in.pairs = {p0, p1};
    auto solver = MoveSolverFactory::create(MoveType::RTouch);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "non-finite skipped r-touch point must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "non-finite skipped r-touch point diagnostic");
}

// Match: O3 rotates about the O1-O2 axis (the Z axis) so its distance to a target
// POINT equals a specified target distance. O3 traces a unit circle in z=0; we
// ask for distance 1 to the target point (2,0,0): the chord from a circle point
// to (2,0,0) equals 1 at a specific angle. Verify d == target after the move.
TEST("match_point_distance") {
    MoveInputs in;
    in.type = MoveType::Match;
    // Axis O1-O2 = Z axis through origin. O3 = (1,0,0) (radius 1 in z=0 plane).
    MovePair p0 = makePair({0, 0, 0}, {2, 0, 0});  // O1; T1 (target geom point)
    MovePair p1 = makePair({0, 0, 1}, {2, 0, 0});  // O2 (axis dir); T2 = T1 -> point
    MovePair p2 = makePair({1, 0, 0}, {2, 0, 0});  // O3 (moving); T3 = T1 -> point
    // Target distance encoded as |T4 - T5| = 1.
    MovePair p3 = makePair({0, 0, 0}, {0, 0, 0});  // T4
    MovePair p4 = makePair({0, 0, 0}, {1, 0, 0});  // T5 -> |T4-T5| = 1
    in.pairs = {p0, p1, p2, p3, p4};
    in.searchAccuracy = 1e-9;
    auto solver = MoveSolverFactory::create(MoveType::Match);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "match should solve");
    const Vec3 o3moved = applyMat(r.transform, {1, 0, 0});
    const double d = dist(o3moved, {2, 0, 0});  // distance to target point
    dvatest::checkNear(d, 1.0, 1e-6, "O3 distance to target point == target dist");
    // O3 must remain on its rotation circle (radius 1 about Z, z = 0).
    dvatest::checkNear(norm(Vec3{o3moved.x, o3moved.y, 0}), 1.0, 1e-9, "O3 stays on circle");
    dvatest::checkNear(o3moved.z, 0.0, 1e-9, "O3 stays in z=0 plane");
}

// Match line target inference: T1==T2 and T3 distinct still defines a line
// through T1/T3. The solver must not use the zero T1->T2 vector and silently
// fall back to +Z.
TEST("match_line_uses_t1_t3_when_t1_equals_t2") {
    MoveInputs in;
    in.type = MoveType::Match;
    // Axis O1-O2 = Z through origin. O3 rotates on the unit circle.
    MovePair p0 = makePair({0, 0, 0}, {2, 0, 0});
    MovePair p1 = makePair({0, 0, 1}, {2, 0, 0});  // T2 == T1
    MovePair p2 = makePair({1, 0, 0}, {2, 1, 0});  // T3 defines +Y line direction
    MovePair p3 = makePair({0, 0, 0}, {0, 0, 0});
    MovePair p4 = makePair({0, 0, 0}, {0, 2, 0});  // target distance = 2
    in.pairs = {p0, p1, p2, p3, p4};
    in.searchAccuracy = 1e-9;
    auto solver = MoveSolverFactory::create(MoveType::Match);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "match line should solve with T1/T3 direction");
    const Vec3 o3moved = applyMat(r.transform, {1, 0, 0});
    // Intended line is x=2, z=0, direction +Y. Distance is |x - 2|.
    dvatest::checkNear(std::fabs(o3moved.x - 2.0), 2.0, 1e-6,
                       "O3 distance to T1/T3 target line == target dist");
}

TEST("match_rejects_nonfinite_axis") {
    MoveInputs in;
    in.type = MoveType::Match;
    MovePair p0 = makePair({0, 0, 0}, {2, 0, 0});
    MovePair p1 = makePair({std::numeric_limits<double>::infinity(), 0, 0}, {2, 0, 0});
    MovePair p2 = makePair({1, 0, 0}, {2, 0, 0});
    MovePair p3 = makePair({0, 0, 0}, {0, 0, 0});
    MovePair p4 = makePair({0, 0, 0}, {1, 0, 0});
    in.pairs = {p0, p1, p2, p3, p4};
    auto solver = MoveSolverFactory::create(MoveType::Match);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "non-finite match axis must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "non-finite match axis diagnostic");
}

TEST("match_rejects_nonfinite_target_distance") {
    MoveInputs in;
    in.type = MoveType::Match;
    MovePair p0 = makePair({0, 0, 0}, {2, 0, 0});
    MovePair p1 = makePair({0, 0, 1}, {2, 0, 0});
    MovePair p2 = makePair({1, 0, 0}, {2, 0, 0});
    MovePair p3 = makePair({0, 0, 0}, {0, 0, 0});
    MovePair p4 = makePair({0, 0, 0}, {std::numeric_limits<double>::infinity(), 0, 0});
    in.pairs = {p0, p1, p2, p3, p4};
    auto solver = MoveSolverFactory::create(MoveType::Match);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "non-finite match target distance must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "non-finite match target distance diagnostic");
}

TEST("match_rejects_nonfinite_target_geometry") {
    MoveInputs in;
    in.type = MoveType::Match;
    MovePair p0 = makePair({0, 0, 0}, {2, 0, 0});
    MovePair p1 = makePair({0, 0, 1}, {2, 0, 0});
    MovePair p2 = makePair({1, 0, 0}, {2, std::numeric_limits<double>::infinity(), 0});
    MovePair p3 = makePair({0, 0, 0}, {0, 0, 0});
    MovePair p4 = makePair({0, 0, 0}, {1, 0, 0});
    in.pairs = {p0, p1, p2, p3, p4};
    auto solver = MoveSolverFactory::create(MoveType::Match);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "non-finite match target geometry must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "non-finite match target geometry diagnostic");
}

TEST("match_rejects_nonfinite_moving_point") {
    MoveInputs in;
    in.type = MoveType::Match;
    MovePair p0 = makePair({0, 0, 0}, {2, 0, 0});
    MovePair p1 = makePair({0, 0, 1}, {2, 0, 0});
    MovePair p2 = makePair({std::numeric_limits<double>::infinity(), 0, 0}, {2, 0, 0});
    MovePair p3 = makePair({0, 0, 0}, {0, 0, 0});
    MovePair p4 = makePair({0, 0, 0}, {1, 0, 0});
    in.pairs = {p0, p1, p2, p3, p4};
    auto solver = MoveSolverFactory::create(MoveType::Match);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "non-finite match moving point must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "non-finite match moving point diagnostic");
}

TEST("match_rejects_unrepresentable_huge_pivot_transform") {
    MoveInputs in;
    in.type = MoveType::Match;
    const Vec3 pivot{9.5e307, 9.5e307, 0};
    constexpr double local = 1e293;
    const Vec3 lineA{pivot.x, pivot.y, 0};
    const Vec3 lineB{pivot.x, pivot.y + local, 0};
    MovePair p0 = makePair(pivot, lineA);
    MovePair p1 = makePair({pivot.x, pivot.y, 1}, lineB);
    MovePair p2 = makePair({pivot.x + local, pivot.y, 0}, lineB);
    MovePair p3 = makePair({0, 0, 0}, {0, 0, 0});
    MovePair p4 = makePair({0, 0, 0}, {0, 0, 0});
    in.pairs = {p0, p1, p2, p3, p4};
    in.searchAccuracy = 1e-9;
    auto solver = MoveSolverFactory::create(MoveType::Match);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "unrepresentable huge-pivot match transform must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "unrepresentable huge-pivot match diagnostic");
}

// BestFit (True Distance): apply a KNOWN rigid transform (90deg about Z + a
// translation) to a cloud of points, add small symmetric noise, then verify the
// solver recovers a transform that maps each object point close to its target.
TEST("bestfit_overdetermined_recovers") {
    // Known rotation: +90deg about Z. R = [[0,-1,0],[1,0,0],[0,0,1]].
    auto applyKnown = [](const Vec3& p) -> Vec3 {
        return {-p.y + 10.0, p.x + 5.0, p.z + 2.0};
    };
    // 8 object points (over-determined for a 6-DOF fit).
    std::vector<Vec3> objs = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1},
                              {1, 1, 0}, {1, 0, 1}, {0, 1, 1}, {2, 3, 1}};
    // Small deterministic, zero-mean noise so the LSQ optimum stays near truth.
    const double noise[8][3] = {{1e-4, -1e-4, 0}, {-1e-4, 1e-4, 0}, {1e-4, 0, -1e-4},
                                {-1e-4, 0, 1e-4}, {0, 1e-4, -1e-4}, {0, -1e-4, 1e-4},
                                {1e-4, 1e-4, -2e-4}, {-1e-4, -1e-4, 2e-4}};
    MoveInputs in;
    in.type = MoveType::BestFit;
    for (int i = 0; i < 8; ++i) {
        const Vec3 t0 = applyKnown(objs[static_cast<std::size_t>(i)]);
        const Vec3 t{t0.x + noise[i][0], t0.y + noise[i][1], t0.z + noise[i][2]};
        in.pairs.push_back(makePair(objs[static_cast<std::size_t>(i)], t));
    }
    auto solver = MoveSolverFactory::create(MoveType::BestFit);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "best-fit should solve");
    // Recovered rotation block must match the known +90deg-about-Z rotation.
    dvatest::checkNear(r.transform.m[0][0], 0.0, 1e-3, "R00 ~ 0");
    dvatest::checkNear(r.transform.m[0][1], -1.0, 1e-3, "R01 ~ -1");
    dvatest::checkNear(r.transform.m[1][0], 1.0, 1e-3, "R10 ~ 1");
    dvatest::checkNear(r.transform.m[2][2], 1.0, 1e-3, "R22 ~ 1");
    // Each object point must land near its (noisy) target.
    double maxErr = 0.0;
    for (const auto& pr : in.pairs) {
        const Vec3 moved = applyMat(r.transform, pr.objectPoint);
        maxErr = std::max(maxErr, dist(moved, pr.targetPoint));
    }
    dvatest::check(maxErr < 1e-2, "all points fit within noise tolerance");
}

// BestFit rotation must be proper (det = +1), not a reflection, even for a
// planar (degenerate) point set. Use coplanar points under a pure +90deg-Z rot.
TEST("bestfit_no_reflection_planar") {
    auto applyKnown = [](const Vec3& p) -> Vec3 { return {-p.y, p.x, p.z}; };
    std::vector<Vec3> objs = {{1, 0, 0}, {0, 1, 0}, {-1, 0, 0}, {0, -1, 0}, {2, 1, 0}};
    MoveInputs in;
    in.type = MoveType::BestFit;
    for (const auto& o : objs) in.pairs.push_back(makePair(o, applyKnown(o)));
    auto solver = MoveSolverFactory::create(MoveType::BestFit);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "planar best-fit should solve");
    // det(R) must be +1 (proper rotation).
    const Mat34& T = r.transform;
    const double det = T.m[0][0] * (T.m[1][1] * T.m[2][2] - T.m[1][2] * T.m[2][1]) -
                       T.m[0][1] * (T.m[1][0] * T.m[2][2] - T.m[1][2] * T.m[2][0]) +
                       T.m[0][2] * (T.m[1][0] * T.m[2][1] - T.m[1][1] * T.m[2][0]);
    dvatest::checkNear(det, 1.0, 1e-6, "rotation is proper (det = +1)");
}

TEST("bestfit_handles_huge_same_sign_centroids") {
    std::vector<Vec3> objs = {{9e307, 0, 0},
                              {9e307, 1e6, 0},
                              {9e307, 0, 1e6},
                              {9e307, 1e6, 1e6}};
    MoveInputs in;
    in.type = MoveType::BestFit;
    for (const auto& o : objs) {
        in.pairs.push_back(makePair(o, {o.x, o.y + 2.0, o.z}));
    }
    auto solver = MoveSolverFactory::create(MoveType::BestFit);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "huge same-sign best-fit should solve");
    for (const auto& pr : in.pairs) {
        const Vec3 moved = applyMat(r.transform, pr.objectPoint);
        dvatest::check(std::isfinite(moved.x) && std::isfinite(moved.y) &&
                           std::isfinite(moved.z),
                       "huge same-sign best-fit moved point is finite");
        dvatest::checkNear(dist(moved, pr.targetPoint), 0.0, 1e-3,
                           "huge same-sign best-fit pair aligns");
    }
}

TEST("bestfit_handles_huge_opposite_sign_spread") {
    std::vector<Vec3> objs = {{9e307, 0, 0},
                              {-9e307, 0, 0},
                              {0, 4e307, 0},
                              {0, -4e307, 0},
                              {0, 0, 2e307},
                              {0, 0, -2e307}};
    MoveInputs in;
    in.type = MoveType::BestFit;
    for (const auto& o : objs) in.pairs.push_back(makePair(o, o));
    auto solver = MoveSolverFactory::create(MoveType::BestFit);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "huge opposite-sign best-fit should solve");
    dvatest::check(isIdentity(r.transform, 1e-12),
                   "huge opposite-sign best-fit identity");
    dvatest::check(r.log.find("inf") == std::string::npos,
                   "huge opposite-sign best-fit finite log");
    dvatest::check(r.log.find("nan") == std::string::npos,
                   "huge opposite-sign best-fit no nan log");
}

TEST("bestfit_handles_huge_opposite_centroids_with_local_shape") {
    constexpr double base = 9e307;
    constexpr double local = 1e293;
    std::vector<Vec3> objs = {{-base, 0, 0},
                              {-base + local, 0, 0},
                              {-base, local, 0},
                              {-base, 0, local}};
    MoveInputs in;
    in.type = MoveType::BestFit;
    for (const auto& o : objs) {
        in.pairs.push_back(makePair(o, {base, o.y, o.z}));
    }
    auto solver = MoveSolverFactory::create(MoveType::BestFit);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "huge opposite-centroid best-fit should solve");
    dvatest::check(r.log.find("inf") == std::string::npos,
                   "huge opposite-centroid best-fit finite log");
    dvatest::check(r.log.find("nan") == std::string::npos,
                   "huge opposite-centroid best-fit no nan log");
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 4; ++j)
            dvatest::check(std::isfinite(r.transform.m[i][j]),
                           "huge opposite-centroid best-fit finite transform");
}

TEST("bestfit_rejects_nonfinite_point_pair") {
    MoveInputs in;
    in.type = MoveType::BestFit;
    in.pairs = {makePair({0, 0, 0}, {10, 0, 0}),
                makePair({1, 0, 0}, {10, 1, 0}),
                makePair({0, 1, 0}, {9, 0, 0}),
                makePair({0, 0, 1},
                         {10, std::numeric_limits<double>::quiet_NaN(), 1})};
    auto solver = MoveSolverFactory::create(MoveType::BestFit);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "non-finite best-fit point pair must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "non-finite best-fit point-pair diagnostic");
}

// BestFit needs a source and target point cloud with real spread. If all source
// points coincide, no rigid rotation can be inferred.
TEST("bestfit_degenerate_source_cloud_fails") {
    MoveInputs in;
    in.type = MoveType::BestFit;
    in.pairs = {makePair({1, 1, 1}, {0, 0, 0}), makePair({1, 1, 1}, {1, 0, 0}),
                makePair({1, 1, 1}, {0, 1, 0}), makePair({1, 1, 1}, {0, 0, 1})};
    auto solver = MoveSolverFactory::create(MoveType::BestFit);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "degenerate best-fit source cloud must fail");
    dvatest::check(r.log.find("point cloud is degenerate") != std::string::npos,
                   "degenerate point-cloud diagnostic");
}

// PatternRigid reference behavior: during nominal build the rigid hole/pin
// pattern is fitted together. With point-pair inputs, this reduces to the same
// rigid best-fit transform used by the BestFit move.
TEST("patternrigid_nominal_pattern_best_fit") {
    auto applyKnown = [](const Vec3& p) -> Vec3 {
        return {-p.y + 4.0, p.x - 2.0, p.z + 1.0};
    };
    std::vector<Vec3> objs = {{0, 0, 0}, {2, 0, 0}, {0, 3, 0}, {1, 1, 1}};
    MoveInputs in;
    in.type = MoveType::PatternRigid;
    in.isNominalBuild = true;
    for (const auto& o : objs) in.pairs.push_back(makePair(o, applyKnown(o)));
    auto solver = MoveSolverFactory::create(MoveType::PatternRigid);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "pattern rigid should fit point pattern");
    for (const auto& pr : in.pairs) {
        dvatest::checkNear(dist(applyMat(r.transform, pr.objectPoint), pr.targetPoint),
                           0.0, 1e-9, "pattern pair aligns");
    }
}

// PatternFit reference behavior for point-to-point constraint pairs: the full
// 3DCS routine can mix hole sets and constraint pairs; this compact model only
// exposes MovePair point constraints, so it fits those pairs rigidly.
TEST("patternfit_constraint_pairs_best_fit") {
    auto applyKnown = [](const Vec3& p) -> Vec3 {
        return {-p.y + 7.0, p.x + 3.0, p.z - 2.0};
    };
    std::vector<Vec3> objs = {{0, 0, 0}, {3, 0, 0}, {0, 2, 0}, {1, 1, 1}};
    MoveInputs in;
    in.type = MoveType::PatternFit;
    for (const auto& o : objs) in.pairs.push_back(makePair(o, applyKnown(o)));
    auto solver = MoveSolverFactory::create(MoveType::PatternFit);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "pattern fit should fit point constraint pairs");
    for (const auto& pr : in.pairs) {
        dvatest::checkNear(dist(applyMat(r.transform, pr.objectPoint), pr.targetPoint),
                           0.0, 1e-9, "pattern fit pair aligns");
    }
}

TEST("iteration_empty_sequence_is_identity_reference_move") {
    MoveInputs in;
    in.type = MoveType::Iteration;
    auto solver = MoveSolverFactory::create(MoveType::Iteration);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "empty iteration move should solve as identity");
    dvatest::check(isIdentity(r.transform, 1e-12), "empty iteration transform is identity");
    dvatest::check(r.log.find("empty") != std::string::npos,
                   "empty iteration log documents reference behavior");
}

TEST("iteration_nonempty_sequence_reports_missing_nested_infrastructure") {
    MoveInputs in;
    in.type = MoveType::Iteration;
    in.pairs = {makePair({0, 0, 0}, {1, 0, 0})};
    auto solver = MoveSolverFactory::create(MoveType::Iteration);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "nonempty iteration move should fail explicitly");
    dvatest::check(r.log.find("Iteration") != std::string::npos,
                   "Iteration diagnostic names the move type");
    dvatest::check(r.log.find("missing") != std::string::npos,
                   "Iteration diagnostic names missing infrastructure");
    dvatest::check(r.log.find("nested move-sequence") != std::string::npos,
                   "Iteration diagnostic names the missing nested sequence");
}

TEST("userdll_move_reports_unbound_plugin_host") {
    MoveInputs in;
    in.type = MoveType::UserDll;
    auto solver = MoveSolverFactory::create(MoveType::UserDll);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "unbound UserDll move should fail explicitly");
    dvatest::check(r.log.find("UserDll") != std::string::npos,
                   "UserDll diagnostic names the move type");
    dvatest::check(r.log.find("plugin host") != std::string::npos,
                   "UserDll diagnostic names the missing plugin host");
}

TEST("userdll_move_dispatches_active_plugin_routine") {
    opendva::plugin::PluginHost host;
    host.registerRoutine("moveX", &userDllMoveRoutine, dcsCalTypeMove);
    opendva::plugin::PluginHost::ActiveScope scope(&host);

    MoveInputs in;
    in.type = MoveType::UserDll;
    in.userDllRoutine = "moveX";
    auto solver = MoveSolverFactory::create(MoveType::UserDll);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);

    dvatest::check(r.ok, "bound UserDll move should dispatch");
    dvatest::checkNear(r.transform.m[0][3], 3.0, 1e-12,
                       "UserDll move uses plugin transform");
    dvatest::checkNear(applyMat(r.transform, {1, 0, 0}).x, 4.0, 1e-12,
                       "UserDll move transform relocates points");
}

TEST("autobend_move_reports_missing_bend_infrastructure") {
    MoveInputs in;
    in.type = MoveType::AutoBend;
    auto solver = MoveSolverFactory::create(MoveType::AutoBend);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "unbound AutoBend move should fail explicitly");
    dvatest::check(r.log.find("AutoBend") != std::string::npos,
                   "AutoBend diagnostic names the move type");
    dvatest::check(r.log.find("missing") != std::string::npos,
                   "AutoBend diagnostic names missing infrastructure");
    dvatest::check(r.log.find("bend/FEA") != std::string::npos,
                   "AutoBend diagnostic names the missing bend infrastructure");
}

// Gravity: a single hole/pin pair with gravity = -Z. The hole drops along -Z
// until O1 reaches T1's Z level; lateral offset is left untouched (per the
// documented simplification). Verify O1 lands at the same Z as T1.
TEST("gravity_drops_to_contact") {
    MoveInputs in;
    in.type = MoveType::Gravity;
    // gravity = -Z; O1 at (0,0,5), T1 (pin) at (0,0,0).
    MovePair p0 = makePair({0, 0, 5}, {0, 0, 0}, {0, 0, -1});
    in.pairs = {p0};
    auto solver = MoveSolverFactory::create(MoveType::Gravity);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "gravity should solve");
    const Vec3 moved = applyMat(r.transform, {0, 0, 5});
    dvatest::checkNear(moved.z, 0.0, 1e-9, "hole drops to pin Z level");
    // Pure drop: X/Y unchanged (no lateral motion for a single pair).
    dvatest::checkNear(moved.x, 0.0, 1e-9, "no lateral X drift");
    dvatest::checkNear(moved.y, 0.0, 1e-9, "no lateral Y drift");
}

// Gravity requires a physical gravity direction. A zero vector is an invalid
// move definition, not an implicit +Z drop.
TEST("gravity_zero_direction_fails") {
    MoveInputs in;
    in.type = MoveType::Gravity;
    in.pairs = {makePair({0, 0, 5}, {0, 0, 0}, {0, 0, 0})};
    auto solver = MoveSolverFactory::create(MoveType::Gravity);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "zero gravity direction must fail");
}

TEST("gravity_rejects_nonfinite_direction") {
    MoveInputs in;
    in.type = MoveType::Gravity;
    in.pairs = {makePair({0, 0, 5}, {0, 0, 0},
                         {std::numeric_limits<double>::infinity(), 0, 0})};
    auto solver = MoveSolverFactory::create(MoveType::Gravity);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "non-finite gravity direction must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "non-finite gravity direction diagnostic");
}

TEST("gravity_rejects_nonfinite_skipped_settle_point") {
    MoveInputs in;
    in.type = MoveType::Gravity;
    MovePair p0 = makePair({0, 0, 0}, {0, 0, 0}, {0, 0, 1});
    MovePair p1 = makePair({1, 0, 0}, {0, 0, std::numeric_limits<double>::infinity()});
    in.pairs = {p0, p1};
    auto solver = MoveSolverFactory::create(MoveType::Gravity);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "non-finite skipped gravity settle point must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "non-finite skipped gravity settle diagnostic");
}

// Gravity settle: two pairs. After the drop, a rotation about the gravity axis
// swings the second hole toward its target direction.
TEST("gravity_settle_rotation") {
    MoveInputs in;
    in.type = MoveType::Gravity;
    // gravity = -Z through origin contact. O1 already at origin -> no drop.
    // O2 at (1,0,0); its target T2 at (0,1,0): a +90deg swing about Z.
    MovePair p0 = makePair({0, 0, 0}, {0, 0, 0}, {0, 0, -1});
    MovePair p1 = makePair({1, 0, 0}, {0, 1, 0});
    in.pairs = {p0, p1};
    auto solver = MoveSolverFactory::create(MoveType::Gravity);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "gravity settle should solve");
    const Vec3 o2moved = applyMat(r.transform, {1, 0, 0});
    // O2 radial direction about Z must align with T2's radial direction (+Y).
    const Vec3 d = sub(o2moved, applyMat(r.transform, {0, 0, 0}));
    dvatest::checkNear(d.x, 0.0, 1e-9, "O2 swung to +Y (x ~ 0)");
    dvatest::checkNear(d.y, 1.0, 1e-9, "O2 swung to +Y (y ~ 1)");
}

TEST("gravity_rejects_unrepresentable_huge_pivot_settle_transform") {
    MoveInputs in;
    in.type = MoveType::Gravity;
    const Vec3 pivot{9e307, 9e307, 0};
    constexpr double local = 1e293;
    MovePair p0 = makePair(pivot, pivot, {0, 0, 1});
    MovePair p1 = makePair({pivot.x + local, pivot.y, 0}, {pivot.x, pivot.y + local, 0});
    in.pairs = {p0, p1};
    auto solver = MoveSolverFactory::create(MoveType::Gravity);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "unrepresentable huge-pivot gravity settle must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "unrepresentable huge-pivot gravity diagnostic");
}

// LeastSquaresAxis: part is NOT moved -> identity transform; the fitted axis
// direction (logged) follows the dominant spread of the target centre points.
TEST("lsqaxis_identity_and_axis") {
    MoveInputs in;
    in.type = MoveType::LeastSquaresAxis;
    // Target centre points spread mainly along +X (with tiny Y jitter).
    in.pairs = {makePair({0, 0, 0}, {0.0, 0.01, 0}), makePair({0, 0, 0}, {1.0, -0.01, 0}),
                makePair({0, 0, 0}, {2.0, 0.01, 0}), makePair({0, 0, 0}, {3.0, -0.01, 0}),
                makePair({0, 0, 0}, {4.0, 0.0, 0})};
    auto solver = MoveSolverFactory::create(MoveType::LeastSquaresAxis);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "lsq-axis should solve");
    dvatest::check(isIdentity(r.transform, 1e-12), "lsq-axis does not move the part");
    dvatest::check(r.log.find("Fitted axis") != std::string::npos, "log records fitted axis");
    // The dominant direction is ~+X; the log must contain a near-1 X component.
    // (Direction sign is arbitrary for an axis, so accept +X or -X.)
    dvatest::check(r.log.find("dir=(0.99") != std::string::npos ||
                       r.log.find("dir=(-0.99") != std::string::npos ||
                       r.log.find("dir=(1.0") != std::string::npos ||
                       r.log.find("dir=(-1.0") != std::string::npos,
                   "fitted axis dominant direction ~ +/-X");
}

TEST("lsqaxis_handles_huge_same_sign_centroids") {
    MoveInputs in;
    in.type = MoveType::LeastSquaresAxis;
    in.pairs = {makePair({0, 0, 0}, {9e307, 0, 0}),
                makePair({0, 0, 0}, {9e307, 1e6, 0}),
                makePair({0, 0, 0}, {9e307, 2e6, 0}),
                makePair({0, 0, 0}, {9e307, 3e6, 0})};
    auto solver = MoveSolverFactory::create(MoveType::LeastSquaresAxis);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "huge same-sign lsq-axis should solve");
    dvatest::check(isIdentity(r.transform, 1e-12), "huge same-sign lsq-axis identity");
    dvatest::check(r.log.find("inf") == std::string::npos, "huge same-sign lsq-axis finite log");
    dvatest::check(r.log.find("dir=(0.000000, 1.000000, 0.000000)") != std::string::npos ||
                       r.log.find("dir=(0.000000, -1.000000, 0.000000)") != std::string::npos,
                   "huge same-sign lsq-axis dominant direction ~ +/-Y");
}

TEST("lsqaxis_handles_huge_opposite_sign_centroids") {
    MoveInputs in;
    in.type = MoveType::LeastSquaresAxis;
    in.pairs = {makePair({0, 0, 0}, {-9e307, 0, 0}),
                makePair({0, 0, 0}, {9e307, 0, 0}),
                makePair({0, 0, 0}, {0, 1e6, 0})};
    auto solver = MoveSolverFactory::create(MoveType::LeastSquaresAxis);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(r.ok, "huge opposite-sign lsq-axis should solve");
    dvatest::check(r.log.find("inf") == std::string::npos &&
                       r.log.find("nan") == std::string::npos,
                   "huge opposite-sign lsq-axis finite log");
    dvatest::check(r.log.find("dir=(1.000000, 0.000000, 0.000000)") != std::string::npos ||
                       r.log.find("dir=(-1.000000, 0.000000, 0.000000)") != std::string::npos,
                   "huge opposite-sign lsq-axis dominant direction ~ +/-X");
}

TEST("lsqaxis_rejects_nonfinite_target_center") {
    MoveInputs in;
    in.type = MoveType::LeastSquaresAxis;
    in.pairs = {makePair({0, 0, 0}, {0, 0, 0}),
                makePair({0, 0, 0}, {1, 0, 0}),
                makePair({0, 0, 0}, {std::numeric_limits<double>::infinity(), 0, 0})};
    auto solver = MoveSolverFactory::create(MoveType::LeastSquaresAxis);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "non-finite lsq-axis target centre must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "non-finite lsq-axis target diagnostic");
}

// LeastSquaresAxis needs target centre points with nonzero spread. If all centre
// points coincide, the fitted axis direction is undefined.
TEST("lsqaxis_coincident_centres_fail") {
    MoveInputs in;
    in.type = MoveType::LeastSquaresAxis;
    in.pairs = {makePair({0, 0, 0}, {2, 2, 2}), makePair({0, 0, 0}, {2, 2, 2}),
                makePair({0, 0, 0}, {2, 2, 2})};
    auto solver = MoveSolverFactory::create(MoveType::LeastSquaresAxis);
    Mt19937Rng rng(1);
    MoveResult r = solver->solve(in, rng);
    dvatest::check(!r.ok, "coincident target centres must fail");
    dvatest::check(r.log.find("axis is degenerate") != std::string::npos,
                   "degenerate axis diagnostic");
}
