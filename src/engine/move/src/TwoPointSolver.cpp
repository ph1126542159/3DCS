// A3 Two-Point Move (README §4.4). Constrains 5 DOF (free spin about the line):
//   (1) translate so O1 -> T1;
//   (2) rotate about T1 so the O1-O2 line is collinear with the T1-T2 line,
//       with axis = O1O2 x T1T2 and angle = the angle between them.
// Degenerate case O1==O2 and T1==T2 collapses to a pure translation.
//
// Original clean-room implementation; rotation via the Rodrigues formula,
// "rotate about point P" realised as translate(-P) -> rotate -> translate(+P).
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

Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

Vec3 normalise(const Vec3& v) {
    const double len = norm(v);
    if (!std::isfinite(len) || len < 1e-12) return {0, 0, 1};
    return scale(v, 1.0 / len);
}

Vec3 rotateAxis(const Vec3& v, const Vec3& k, double theta) {
    const double c = std::cos(theta);
    const double s = std::sin(theta);
    const Vec3 kxv = cross(k, v);
    const double kdv = dot(k, v);
    return add(add(scale(v, c), scale(kxv, s)), scale(k, kdv * (1.0 - c)));
}

struct RigidOp {
    Vec3 o1{};        // translation source
    Vec3 t1{};        // translation target + rotation pivot
    Vec3 axis{};      // unit rotation axis
    double theta{};   // rotation angle

    Vec3 apply(const Vec3& p) const {
        Vec3 q = add(p, sub(t1, o1));              // step 1: O1 -> T1
        return add(rotateAxis(sub(q, t1), axis, theta), t1);  // step 2: rotate about T1
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

class TwoPointSolver final : public IMoveSolver {
public:
    MoveType type() const override { return MoveType::TwoPoint; }

    MoveResult solve(const MoveInputs& in, IRng& /*rng*/) override {
        MoveResult res{};
        if (in.pairs.size() < 2) {
            res.log = "No Solution: Two-Point requires 2 point pairs";
            return res;
        }
        const Vec3 o1 = in.pairs[0].objectPoint;
        const Vec3 o2 = in.pairs[1].objectPoint;
        const Vec3 t1 = in.pairs[0].targetPoint;
        const Vec3 t2 = in.pairs[1].targetPoint;

        RigidOp op{};
        op.o1 = o1;
        op.t1 = t1;

        const Vec3 vO = sub(o2, o1);
        const Vec3 vT = sub(t2, t1);
        const double vOLen = norm(vO);
        const double vTLen = norm(vT);
        if (!std::isfinite(vOLen) || !std::isfinite(vTLen)) {
            res.log = "No Solution: Two-Point baseline is not finite";
            return res;
        }
        if (vOLen < 1e-12 || vTLen < 1e-12) {
            // Degenerate: one line has zero length -> pure translation O1 -> T1.
            op.axis = {0, 0, 1};
            op.theta = 0.0;
            res.transform = toMat34(op);
            if (!finiteMat34(res.transform)) {
                res.transform = Mat34{};
                res.log = "No Solution: Two-Point transform is not finite";
                return res;
            }
            res.ok = true;
            return res;
        }

        const Vec3 dirO = normalise(vO);
        const Vec3 dirT = normalise(vT);
        Vec3 axis = cross(dirO, dirT);
        if (norm(axis) < 1e-9) {
            // (Anti)parallel directions.
            if (dot(dirO, dirT) > 0.0) {
                op.axis = {0, 0, 1};
                op.theta = 0.0;  // already aligned
            } else {
                // 180-degree flip about any axis perpendicular to dirO.
                Vec3 ref = (std::fabs(dirO.x) < 0.9) ? Vec3{1, 0, 0} : Vec3{0, 1, 0};
                op.axis = normalise(cross(dirO, ref));
                op.theta = kPi;
            }
        } else {
            op.axis = normalise(axis);
            const double c = std::max(-1.0, std::min(1.0, dot(dirO, dirT)));
            op.theta = std::acos(c);
        }

        res.transform = toMat34(op);
        if (!finiteMat34(res.transform)) {
            res.transform = Mat34{};
            res.log = "No Solution: Two-Point transform is not finite";
            return res;
        }
        res.ok = true;
        return res;
    }
};

}  // namespace

std::unique_ptr<IMoveSolver> makeTwoPointSolver() {
    return std::make_unique<TwoPointSolver>();
}

}  // namespace opendva
