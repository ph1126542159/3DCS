// A3 Cross Product Move (README §4.20). Computes Dx = D1 x D2 (right-hand rule),
// normalised, and assigns it as the associated DIRECTION of a set of object
// points. This move assigns a direction to points rather than producing a rigid
// pose, so the returned transform is the IDENTITY (no part motion). The computed
// cross-product vector is reported in the result log for downstream consumers
// (the A4 geometry layer applies it as the points' direction).
//
// Degenerate case (D1 || D2 or D1 anti-parallel D2): the cross product is the
// zero vector; per README this is flagged as a failure (No Solution).
//
// Inputs: D1 = pairs[0].direction.ijk, D2 = pairs[1].direction.ijk.
//
// Original clean-room implementation.
#include <string>

#include "MoveMath.h"
#include "opendva/IMoveSolver.h"

namespace opendva {
namespace {

using namespace movemath;

class CrossProductSolver final : public IMoveSolver {
public:
    MoveType type() const override { return MoveType::CrossProduct; }

    MoveResult solve(const MoveInputs& in, IRng& /*rng*/) override {
        MoveResult res{};
        if (in.pairs.size() < 2) {
            res.log = "No Solution: Cross Product requires 2 direction inputs";
            return res;
        }
        const Vec3 d1 = in.pairs[0].direction.ijk;
        const Vec3 d2 = in.pairs[1].direction.ijk;
        const double len1 = norm(d1);
        const double len2 = norm(d2);
        if (!std::isfinite(len1) || !std::isfinite(len2)) {
            res.log = "No Solution: Cross Product input direction is not finite";
            return res;
        }
        if (len1 < 1e-12 || len2 < 1e-12) {
            res.log = "No Solution: Cross Product input direction is zero";
            return res;
        }
        const Vec3 dx = cross(normalise(d1), normalise(d2));
        if (norm(dx) < 1e-12) {
            // D1 and D2 (anti-)parallel -> zero cross product (degenerate).
            res.log = "No Solution: input directions are parallel (zero cross product)";
            return res;
        }
        const Vec3 dxu = normalise(dx);
        // Transform stays identity: this move only assigns a direction to points.
        res.transform = Mat34{};
        res.ok = true;
        res.log = "Dx = D1 x D2 = (" + std::to_string(dxu.x) + ", " +
                  std::to_string(dxu.y) + ", " + std::to_string(dxu.z) + ")";
        return res;
    }
};

}  // namespace

std::unique_ptr<IMoveSolver> makeCrossProductSolver() {
    return std::make_unique<CrossProductSolver>();
}

}  // namespace opendva
