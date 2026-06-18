// A3 Rotate Line Move (README §4.17). Rotates the object about a fixed axis until
// the object line (Obj1 -> Obj2) becomes parallel to the target line
// (Tgt1 -> Tgt2), measured in the plane perpendicular to the rotation axis.
//
// Inputs (per README):
//   - Obj1/Obj2 : pairs[0].objectPoint / pairs[1].objectPoint  (object line)
//   - Tgt1/Tgt2 : pairs[0].targetPoint / pairs[1].targetPoint  (target line)
//   - Tgt3      : pairs[2].targetPoint                         (point ON the axis)
//   - axis dir  : pairs[0].direction.ijk                       (rotation axis)
//
// Algorithm: project both lines onto the plane perpendicular to the axis, then
// rotate by the signed in-plane angle that brings the object line parallel to the
// target line. Rotation realised with the Rodrigues formula about the axis
// through Tgt3.
//
// Original clean-room implementation. Keep this TU rebuilt when shared move math
// projection, scale, and signed-angle helpers change.
#include "MoveMath.h"
#include "opendva/IMoveSolver.h"

namespace opendva {
namespace {

using namespace movemath;

bool finiteVec(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

class RotateLineSolver final : public IMoveSolver {
public:
    MoveType type() const override { return MoveType::RotateLine; }

    MoveResult solve(const MoveInputs& in, IRng& /*rng*/) override {
        MoveResult res{};
        if (in.pairs.size() < 3) {
            res.log = "No Solution: Rotate Line requires Obj1/Obj2, Tgt1/Tgt2, Tgt3";
            return res;
        }
        const Vec3 obj1 = in.pairs[0].objectPoint;
        const Vec3 obj2 = in.pairs[1].objectPoint;
        const Vec3 tgt1 = in.pairs[0].targetPoint;
        const Vec3 tgt2 = in.pairs[1].targetPoint;
        const Vec3 tgt3 = in.pairs[2].targetPoint;  // axis pivot
        if (!finiteVec(obj1) || !finiteVec(obj2) || !finiteVec(tgt1) ||
            !finiteVec(tgt2) || !finiteVec(tgt3)) {
            res.log = "No Solution: Rotate Line line point is not finite";
            return res;
        }
        const double axisLen = norm(in.pairs[0].direction.ijk);
        if (!std::isfinite(axisLen)) {
            res.log = "No Solution: Rotate Line axis direction is not finite";
            return res;
        }
        if (axisLen < 1e-12) {
            res.log = "No Solution: Rotate Line axis direction is zero";
            return res;
        }
        const Vec3 axis = normalise(in.pairs[0].direction.ijk);

        const Vec3 objLine = sub(obj2, obj1);
        const Vec3 tgtLine = sub(tgt2, tgt1);
        if (norm(objLine) < 1e-12 || norm(tgtLine) < 1e-12) {
            res.log = "No Solution: degenerate (zero-length) object or target line";
            return res;
        }

        // Signed in-plane angle that rotates the object line onto the target line.
        const double theta = signedAngleAboutAxis(objLine, tgtLine, axis);
        res.transform = rotationMat34(tgt3, axis, theta);
        if (!finiteMat34(res.transform)) {
            res.transform = Mat34{};
            res.log = "No Solution: Rotate Line transform is not finite";
            return res;
        }
        res.ok = true;
        return res;
    }
};

}  // namespace

std::unique_ptr<IMoveSolver> makeRotateLineSolver() {
    return std::make_unique<RotateLineSolver>();
}

}  // namespace opendva
