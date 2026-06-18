// A3 Transform Move (README §4.12). Directly applies a user-specified rigid
// translation or rotation. The interface carries no explicit "mode" or scalar
// amount field, so this reference build adopts the following CONVENTION:
//
//   - Direction / axis: pairs[0].direction.ijk (auto-normalised).
//   - Amount: the signed distance |T1 - O1| from pairs[0], i.e. how far the first
//     object point should move toward its target. This gives a concrete, testable
//     magnitude derived solely from MoveInputs.
//   - Mode selection: TRANSLATE by default. If pairs[0].direction.refPoints is
//     non-empty (a rotation needs a center, supplied via reference points per
//     README §3.3), we ROTATE by 'amount' radians about the axis (ijk) through the
//     center pairs[0].objectPoint. This keeps the contract header untouched while
//     still exercising both branches.
//
// Original clean-room implementation (Rodrigues for the rotation branch).
#include <cmath>

#include "MoveMath.h"
#include "opendva/IMoveSolver.h"

namespace opendva {
namespace {

Vec3 sub(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
double norm(const Vec3& a) { return std::hypot(a.x, a.y, a.z); }

bool finiteVec(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

Vec3 normalise(const Vec3& v) {
    const double len = norm(v);
    if (!std::isfinite(len) || len < 1e-12) return {0, 0, 1};
    return {v.x / len, v.y / len, v.z / len};
}

class TransformSolver final : public IMoveSolver {
public:
    MoveType type() const override { return MoveType::Transform; }

    MoveResult solve(const MoveInputs& in, IRng& /*rng*/) override {
        MoveResult res{};
        if (in.pairs.empty()) {
            res.log = "No Solution: Transform requires at least 1 pair";
            return res;
        }
        const MovePair& p = in.pairs[0];
        if (!finiteVec(p.objectPoint) || !finiteVec(p.targetPoint)) {
            res.log = "No Solution: Transform point pair is not finite";
            return res;
        }
        const double dirLen = norm(p.direction.ijk);
        if (!std::isfinite(dirLen)) {
            res.log = "No Solution: Transform direction is not finite";
            return res;
        }
        if (dirLen < 1e-12) {
            res.log = "No Solution: Transform direction is zero";
            return res;
        }
        const Vec3 dir = normalise(p.direction.ijk);
        // Amount = how far O1 must travel to reach T1 (a magnitude from inputs).
        const double amount = norm(sub(p.targetPoint, p.objectPoint));

        Mat34 T{};  // identity
        if (p.direction.refPoints.empty()) {
            // TRANSLATE branch: t = amount * dir, R = I.
            T.m[0][3] = dir.x * amount;
            T.m[1][3] = dir.y * amount;
            T.m[2][3] = dir.z * amount;
        } else {
            // ROTATE branch: rotate by 'amount' radians about axis 'dir' through
            // center c = p.objectPoint. Build R via Rodrigues, then set
            // t = c - R*c so the center is fixed.
            const Vec3 k = dir;
            const double theta = amount;
            const double c = std::cos(theta);
            const double s = std::sin(theta);
            // R = I cos + [k]_x sin + (k k^T)(1 - cos).
            const double kx = k.x, ky = k.y, kz = k.z;
            const double one_c = 1.0 - c;
            const double R[3][3] = {
                {c + kx * kx * one_c, kx * ky * one_c - kz * s, kx * kz * one_c + ky * s},
                {ky * kx * one_c + kz * s, c + ky * ky * one_c, ky * kz * one_c - kx * s},
                {kz * kx * one_c - ky * s, kz * ky * one_c + kx * s, c + kz * kz * one_c}};
            const Vec3 ctr = p.objectPoint;
            const Vec3 Rc = {R[0][0] * ctr.x + R[0][1] * ctr.y + R[0][2] * ctr.z,
                             R[1][0] * ctr.x + R[1][1] * ctr.y + R[1][2] * ctr.z,
                             R[2][0] * ctr.x + R[2][1] * ctr.y + R[2][2] * ctr.z};
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) T.m[i][j] = R[i][j];
            T.m[0][3] = ctr.x - Rc.x;
            T.m[1][3] = ctr.y - Rc.y;
            T.m[2][3] = ctr.z - Rc.z;
        }

        res.transform = T;
        if (!movemath::finiteMat34(res.transform)) {
            res.transform = Mat34{};
            res.log = "No Solution: Transform transform is not finite";
            return res;
        }
        res.ok = true;
        return res;
    }
};

}  // namespace

std::unique_ptr<IMoveSolver> makeTransformSolver() {
    return std::make_unique<TransformSolver>();
}

}  // namespace opendva
