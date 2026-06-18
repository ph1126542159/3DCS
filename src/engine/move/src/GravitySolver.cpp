// A3 Gravity Move (README §4.18). Simulates a hole/pin assembly settling under a
// gravity vector. The full README algorithm is a three-step contact sequence
// (nominal alignment -> smallest-clearance drop -> rotation to a second contact).
//
// SIMPLIFICATION (documented): this reference build implements the dominant two
// steps that determine the resulting rigid pose:
//   (1) DROP: translate the object so its first hole O1 reaches its target pin T1
//       along the gravity direction. Only the component of (T1 - O1) along gravity
//       is applied, modelling the part falling until first contact (lateral play
//       is left to the A4 clearance layer).
//   (2) SETTLE: rotate about the first-contact axis (gravity direction through the
//       now-coincident O1/T1) so the second hole O2 swings toward its target T2.
// Weight and per-pair clearance magnitudes (which only scale GeoFactor terms, not
// the nominal pose) are intentionally omitted here.
//
// Inputs: gravity = pairs[0].direction.ijk; O1..On / T1..Tn = pairs[i].
//
// Original clean-room implementation (Rodrigues for the settle rotation).
#include "MoveMath.h"
#include "opendva/IMoveSolver.h"

namespace opendva {
namespace {

using namespace movemath;

bool finiteVec(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

// Compose B after A (apply A first, then B) for Mat34 [R|t]: out = B * A.
Mat34 composeMat(const Mat34& b, const Mat34& a) {
    Mat34 out{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            double s = 0.0;
            for (int k = 0; k < 3; ++k) s += b.m[i][k] * a.m[k][j];
            out.m[i][j] = s;
        }
        out.m[i][3] = b.m[i][0] * a.m[0][3] + b.m[i][1] * a.m[1][3] + b.m[i][2] * a.m[2][3] +
                      b.m[i][3];
    }
    return out;
}

class GravitySolver final : public IMoveSolver {
public:
    MoveType type() const override { return MoveType::Gravity; }

    MoveResult solve(const MoveInputs& in, IRng& /*rng*/) override {
        MoveResult res{};
        auto failNonFiniteTransform = [&res]() {
            res.transform = Mat34{};
            res.log = "No Solution: Gravity transform is not finite";
            return res;
        };
        if (in.pairs.empty()) {
            res.log = "No Solution: Gravity requires at least 1 hole/pin pair";
            return res;
        }
        const double gravityLen = norm(in.pairs[0].direction.ijk);
        if (!std::isfinite(gravityLen)) {
            res.log = "No Solution: Gravity direction is not finite";
            return res;
        }
        if (gravityLen < 1e-12) {
            res.log = "No Solution: Gravity direction is zero";
            return res;
        }
        for (const auto& pr : in.pairs) {
            if (!finiteVec(pr.objectPoint) || !finiteVec(pr.targetPoint)) {
                res.log = "No Solution: Gravity point pair is not finite";
                return res;
            }
        }
        const Vec3 gravity = normalise(in.pairs[0].direction.ijk);
        const Vec3 o1 = in.pairs[0].objectPoint;
        const Vec3 t1 = in.pairs[0].targetPoint;

        // Step 1: DROP along gravity until O1 reaches T1 (projected component).
        const double drop = dot(sub(t1, o1), gravity);
        const Mat34 mDrop = translationMat34(scale(gravity, drop));
        if (!finiteMat34(mDrop)) return failNonFiniteTransform();
        const Vec3 o1Dropped = applyMat(mDrop, o1);  // contact point (== o1 + drop*g)

        Mat34 total = mDrop;

        // Step 2: SETTLE — rotate about the gravity axis through the contact point
        // so the second hole O2 swings toward its target T2. Skipped if no second
        // pair or if its radial component about the axis is degenerate.
        if (in.pairs.size() >= 2) {
            const Vec3 o2 = applyMat(mDrop, in.pairs[1].objectPoint);
            const Vec3 t2 = in.pairs[1].targetPoint;
            const Vec3 ro = sub(o2, o1Dropped);
            const Vec3 rt = sub(t2, o1Dropped);
            const Vec3 roPerp = sub(ro, scale(gravity, dot(ro, gravity)));
            const Vec3 rtPerp = sub(rt, scale(gravity, dot(rt, gravity)));
            if (norm(roPerp) >= 1e-12 && norm(rtPerp) >= 1e-12) {
                const double theta = signedAngleAboutAxis(ro, rt, gravity);
                const Mat34 mSettle = rotationMat34(o1Dropped, gravity, theta);
                if (!finiteMat34(mSettle)) return failNonFiniteTransform();
                total = composeMat(mSettle, mDrop);
                if (!finiteMat34(total)) return failNonFiniteTransform();
            }
        }

        res.transform = total;
        res.ok = true;
        return res;
    }
};

}  // namespace

std::unique_ptr<IMoveSolver> makeGravitySolver() {
    return std::make_unique<GravitySolver>();
}

}  // namespace opendva
