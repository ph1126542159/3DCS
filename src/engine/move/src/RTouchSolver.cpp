// A3 R-Touch Move (README §4.16). Rotates the object about a fixed axis until the
// first object point "touches" its target, i.e. by the SMALLEST angular distance
// among all object/target pairs. This is the rotational analogue of a contact:
// the object spins just enough so that some surface point reaches its target.
//
// Inputs (per README):
//   - axis dir : pairs[0].direction.ijk
//   - T1       : pairs[0].targetPoint  (a point ON the rotation axis / pivot)
//   - O1..On   : pairs[i].objectPoint
//   - T1..Tn   : pairs[i].targetPoint
//
// Algorithm: for each pair compute the signed angle (about the axis through T1)
// that carries Oi onto the radial direction of Ti; take the one with the smallest
// magnitude and rotate the whole object by it. Pairs whose points lie on the axis
// (no in-plane component) contribute no constraint and are skipped.
//
// Original clean-room implementation (Rodrigues rotation).
#include <cmath>

#include "MoveMath.h"
#include "opendva/IMoveSolver.h"

namespace opendva {
namespace {

using namespace movemath;

bool finiteVec(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

class RTouchSolver final : public IMoveSolver {
public:
    MoveType type() const override { return MoveType::RTouch; }

    MoveResult solve(const MoveInputs& in, IRng& /*rng*/) override {
        MoveResult res{};
        if (in.pairs.empty()) {
            res.log = "No Solution: R-Touch requires at least 1 point pair";
            return res;
        }
        const double axisLen = norm(in.pairs[0].direction.ijk);
        if (!std::isfinite(axisLen)) {
            res.log = "No Solution: R-Touch axis direction is not finite";
            return res;
        }
        if (axisLen < 1e-12) {
            res.log = "No Solution: R-Touch axis direction is zero";
            return res;
        }
        for (const auto& pr : in.pairs) {
            if (!finiteVec(pr.objectPoint) || !finiteVec(pr.targetPoint)) {
                res.log = "No Solution: R-Touch point pair is not finite";
                return res;
            }
        }
        const Vec3 axis = normalise(in.pairs[0].direction.ijk);
        const Vec3 pivot = in.pairs[0].targetPoint;  // T1 on the axis

        double bestAbs = -1.0;
        double bestAngle = 0.0;
        bool found = false;
        for (const auto& pr : in.pairs) {
            // Radial vectors of object/target points about the axis through pivot.
            const Vec3 ro = sub(pr.objectPoint, pivot);
            const Vec3 rt = sub(pr.targetPoint, pivot);
            const double angle = signedAngleAboutAxis(ro, rt, axis);
            // Skip pairs with no in-plane component (point on the axis): they yield
            // an exact 0 from signedAngleAboutAxis but carry no real constraint.
            if (perpendicularMagnitudeAboutAxis(ro, axis) < 1e-12 ||
                perpendicularMagnitudeAboutAxis(rt, axis) < 1e-12) {
                continue;
            }
            const double a = std::fabs(angle);
            if (!found || a < bestAbs) {
                bestAbs = a;
                bestAngle = angle;
                found = true;
            }
        }
        if (!found) {
            res.log = "No Solution: all R-Touch points lie on the rotation axis";
            return res;
        }
        res.transform = rotationMat34(pivot, axis, bestAngle);
        if (!finiteMat34(res.transform)) {
            res.transform = Mat34{};
            res.log = "No Solution: R-Touch transform is not finite";
            return res;
        }
        res.ok = true;
        return res;
    }
};

}  // namespace

std::unique_ptr<IMoveSolver> makeRTouchSolver() {
    return std::make_unique<RTouchSolver>();
}

}  // namespace opendva
