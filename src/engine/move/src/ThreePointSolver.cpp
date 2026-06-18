// A3 Three-Point Move (README §4.3). Classic 3-2-1 closed-form locator.
// Builds a rigid transform [R|t] from three object/target point pairs in three
// geometric steps:
//   (1) translate so O1 -> T1 (T1 becomes the rotation pivot);
//   (2) rotate about T1 so the O1-O2 line is collinear with the T1-T2 line;
//   (3) rotate about the O1-O2 axis so O3 lands on the T1-T2-T3 plane.
// All rotations use the Rodrigues formula; "rotate about point P" is realised as
// translate(-P) -> rotate -> translate(+P).
//
// Original clean-room implementation using plain vector algebra (no external
// linear-algebra dependency).
#include <algorithm>
#include <cmath>

#include "opendva/IMoveSolver.h"

namespace opendva {
namespace {

constexpr double kPi = 3.14159265358979323846;

Vec3 add(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 sub(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 scale(const Vec3& a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
double norm(const Vec3& a) { return std::hypot(a.x, a.y, a.z); }

bool finiteVec(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

Vec3 normalise(const Vec3& v) {
    const double len = norm(v);
    if (!std::isfinite(len) || len < 1e-12) return {0, 0, 1};
    return scale(v, 1.0 / len);
}

double componentScale(const Vec3& v) {
    return std::max(std::fabs(v.x), std::max(std::fabs(v.y), std::fabs(v.z)));
}

Vec3 perpendicularToAxis(const Vec3& v, const Vec3& axis) {
    const double s = componentScale(v);
    if (s <= 0.0 || !std::isfinite(s)) return {0, 0, 0};
    const Vec3 sv{v.x / s, v.y / s, v.z / s};
    const double axial = dot(sv, axis);
    return scale(sub(sv, scale(axis, axial)), s);
}

// Rodrigues rotation of v about unit axis k by angle theta (radians).
Vec3 rotateAxis(const Vec3& v, const Vec3& k, double theta) {
    const double c = std::cos(theta);
    const double s = std::sin(theta);
    const Vec3 kxv = cross(k, v);
    const double kdv = dot(k, v);
    return add(add(scale(v, c), scale(kxv, s)), scale(k, kdv * (1.0 - c)));
}

// Rotate point p about an axis through pivot P with unit direction k by theta.
Vec3 rotateAboutPoint(const Vec3& p, const Vec3& pivot, const Vec3& k, double theta) {
    return add(rotateAxis(sub(p, pivot), k, theta), pivot);
}

// Compose the three rigid steps into a single Mat34 by tracking the images of the
// canonical basis points: t = f(0), and column j = f(e_j) - f(0).
struct RigidOp {
    // Steps are applied in order via this captured-state functor.
    Vec3 t1{};       // target pivot (translation target for O1)
    Vec3 o1{};       // original O1 (translation source)
    Vec3 axis2{};    // step-2 rotation axis (through T1)
    double theta2{}; // step-2 angle
    Vec3 axis3{};    // step-3 rotation axis (T1->T2, through T1)
    double theta3{}; // step-3 angle

    Vec3 apply(const Vec3& p) const {
        // Step 1: translation O1 -> T1 (delta = T1 - O1).
        Vec3 q = add(p, sub(t1, o1));
        // Step 2: rotate about T1.
        q = rotateAboutPoint(q, t1, axis2, theta2);
        // Step 3: rotate about the (already-moved) O1-O2 axis, which passes
        // through T1 with direction axis3.
        q = rotateAboutPoint(q, t1, axis3, theta3);
        return q;
    }
};

Mat34 toMat34(const RigidOp& op) {
    const Vec3 t = op.apply({0, 0, 0});
    const Vec3 cx = sub(op.apply({1, 0, 0}), t);
    const Vec3 cy = sub(op.apply({0, 1, 0}), t);
    const Vec3 cz = sub(op.apply({0, 0, 1}), t);
    Mat34 out{};
    out.m[0][0] = cx.x; out.m[0][1] = cy.x; out.m[0][2] = cz.x; out.m[0][3] = t.x;
    out.m[1][0] = cx.y; out.m[1][1] = cy.y; out.m[1][2] = cz.y; out.m[1][3] = t.y;
    out.m[2][0] = cx.z; out.m[2][1] = cy.z; out.m[2][2] = cz.z; out.m[2][3] = t.z;
    return out;
}

bool finiteMat34(const Mat34& T) {
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (!std::isfinite(T.m[i][j])) return false;
        }
    }
    return true;
}

class ThreePointSolver final : public IMoveSolver {
public:
    MoveType type() const override { return MoveType::ThreePoint; }

    MoveResult solve(const MoveInputs& in, IRng& /*rng*/) override {
        MoveResult res{};
        if (in.pairs.size() < 3) {
            res.log = "No Solution: Three-Point requires 3 point pairs";
            return res;
        }
        const Vec3 o1 = in.pairs[0].objectPoint;
        const Vec3 o2 = in.pairs[1].objectPoint;
        const Vec3 o3 = in.pairs[2].objectPoint;
        const Vec3 t1 = in.pairs[0].targetPoint;
        const Vec3 t2 = in.pairs[1].targetPoint;
        const Vec3 t3 = in.pairs[2].targetPoint;
        if (!finiteVec(o1) || !finiteVec(o2) || !finiteVec(o3) ||
            !finiteVec(t1) || !finiteVec(t2) || !finiteVec(t3)) {
            res.log = "No Solution: Three-Point input point is not finite";
            return res;
        }

        RigidOp op{};
        op.o1 = o1;
        op.t1 = t1;

        // Step 2: align O1-O2 with T1-T2 by rotating about an axis through T1.
        // The object line has already been translated so O1 sits at T1; its
        // direction is unchanged by translation, so we use the original O1-O2.
        const Vec3 baseO12 = sub(o2, o1);
        const Vec3 baseT12 = sub(t2, t1);
        const double baseO12Len = norm(baseO12);
        const double baseT12Len = norm(baseT12);
        if (!std::isfinite(baseO12Len) || !std::isfinite(baseT12Len)) {
            res.log = "No Solution: Three-Point baseline is not finite";
            return res;
        }
        if (baseO12Len < 1e-12 || baseT12Len < 1e-12) {
            res.log = "No Solution: Three-Point baseline is zero";
            return res;
        }
        const Vec3 dirO12 = normalise(baseO12);
        const Vec3 dirT12 = normalise(baseT12);
        Vec3 axis2 = cross(dirO12, dirT12);
        if (norm(axis2) < 1e-9) {
            // Already (anti)parallel: pick any axis perpendicular to dirO12 for a
            // possible 180-degree flip; if parallel, no rotation needed.
            if (dot(dirO12, dirT12) > 0.0) {
                axis2 = {0, 0, 1};
                op.theta2 = 0.0;
            } else {
                Vec3 ref = (std::fabs(dirO12.x) < 0.9) ? Vec3{1, 0, 0} : Vec3{0, 1, 0};
                axis2 = normalise(cross(dirO12, ref));
                op.theta2 = kPi;
            }
        } else {
            axis2 = normalise(axis2);
            const double c = std::max(-1.0, std::min(1.0, dot(dirO12, dirT12)));
            op.theta2 = std::acos(c);
        }
        op.axis2 = axis2;

        // After step 2 the O1-O2 line lies along T1-T2; step 3 spins about it so
        // O3 reaches the T1-T2-T3 plane. Compute O3's position after steps 1-2.
        RigidOp upto2 = op;     // theta3 = 0 still
        upto2.theta3 = 0.0;
        const Vec3 o3_after2 = upto2.apply(o3);
        // Axis of step 3 is the T1-T2 direction (through pivot T1).
        const Vec3 axis3 = dirT12;
        op.axis3 = axis3;

        // Target plane normal from the three target points.
        const Vec3 planeNormal = cross(sub(t2, t1), sub(t3, t1));
        const double planeNormalLen = norm(planeNormal);
        if (!std::isfinite(planeNormalLen)) {
            res.log = "No Solution: Three-Point target plane normal is not finite";
            return res;
        }
        if (planeNormalLen < 1e-12) {
            // Degenerate target (collinear T1,T2,T3): no well-defined plane, leave
            // O3 where steps 1-2 put it (theta3 = 0).
            op.theta3 = 0.0;
        } else {
            const Vec3 n = normalise(planeNormal);
            // Project the radial vector (O3 about the axis) and the in-plane target
            // direction onto the plane perpendicular to axis3, then find the angle
            // bringing O3's radial onto the plane.
            const Vec3 r = sub(o3_after2, t1);              // radius vector
            const Vec3 rPerp = perpendicularToAxis(r, axis3);  // component perp axis
            if (norm(rPerp) < 1e-12) {
                op.theta3 = 0.0;  // O3 on the axis: already in plane.
            } else {
                // We need rotated rPerp to be perpendicular to n (i.e. lie in plane).
                // Build an orthonormal in-plane basis {u, w} with u = rPerp dir.
                const Vec3 u = normalise(rPerp);
                const Vec3 w = cross(axis3, u);  // axis3 unit, u unit, => w unit
                // After rotating by phi: rPerp' = |rPerp|(cos phi * u + sin phi * w).
                // Plane condition: dot(rPerp', n) = 0 -> a cos phi + b sin phi = 0.
                const double a = dot(u, n);
                const double b = dot(w, n);
                if (std::fabs(a) < 1e-15 && std::fabs(b) < 1e-15) {
                    op.theta3 = 0.0;  // radius already in plane for all phi.
                } else {
                    op.theta3 = std::atan2(-a, b);  // one of two roots (pi apart).
                }
            }
        }

        res.transform = toMat34(op);
        if (!finiteMat34(res.transform)) {
            res.transform = Mat34{};
            res.log = "No Solution: Three-Point transform is not finite";
            return res;
        }
        res.ok = true;
        return res;
    }
};

}  // namespace

std::unique_ptr<IMoveSolver> makeThreePointSolver() {
    return std::make_unique<ThreePointSolver>();
}

}  // namespace opendva
