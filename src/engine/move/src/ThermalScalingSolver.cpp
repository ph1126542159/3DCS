// A3 Thermal Scaling Move (README §4.13). Real thermal scaling is NON-RIGID:
//   p' = SP + scale * (p - SP),  scale = 1 + alpha * delta_T
// where SP is the scaling point (default: first point). That node-level
// deformation belongs to the A4 geometry layer. The IMoveSolver contract only
// returns a rigid Mat34 [R|t], so this is an APPROXIMATION: an origin-centred
// isotropic scaling encoded in the R block (diagonal = scale). It is exact only
// when the scaling point is the origin; otherwise A4 must apply the SP-centred
// form. Documented here so callers do not mistake it for the full thermal model.
//
// CONVENTION for the missing alpha / delta_T fields (the contract has no place
// for them):
//   - alpha   = in.searchAccuracy, borrowed as the CTE (per-degree). README's
//               default CTE is 1e-5/degC, which is also searchAccuracy's default
//               (1e-5), so an unconfigured input yields a physically sensible CTE.
//   - delta_T = kExampleDeltaT (fixed example temperature rise, see below).
// Both are spelled out so the resulting scale is deterministic and testable.
//
// Original clean-room implementation.
#include <cmath>

#include "opendva/IMoveSolver.h"

namespace opendva {
namespace {

// Fixed example temperature rise (degC) used because the contract carries no
// per-move delta_T field. Real models supply this via the A4 thermal inputs.
constexpr double kExampleDeltaT = 100.0;

bool finiteVec(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

class ThermalScalingSolver final : public IMoveSolver {
public:
    MoveType type() const override { return MoveType::ThermalScaling; }

    MoveResult solve(const MoveInputs& in, IRng& /*rng*/) override {
        if (in.pairs.empty()) {
            MoveResult res{};
            res.log = "No Solution: Thermal Scaling requires a scaling point";
            return res;
        }
        if (!finiteVec(in.pairs[0].objectPoint)) {
            MoveResult res{};
            res.log = "No Solution: Thermal Scaling scaling point is not finite";
            return res;
        }
        // alpha borrowed from searchAccuracy (see file header); delta_T fixed.
        const double alpha = in.searchAccuracy;
        const double scale = 1.0 + alpha * kExampleDeltaT;
        if (!std::isfinite(scale)) {
            MoveResult res{};
            res.log = "No Solution: Thermal Scaling transform is not finite";
            return res;
        }

        Mat34 T{};  // identity
        // Origin-centred isotropic scaling: R = scale * I, t = 0.
        T.m[0][0] = scale;
        T.m[1][1] = scale;
        T.m[2][2] = scale;
        // Off-diagonals stay 0 (Mat34 default), translation stays 0.

        MoveResult res{};
        res.transform = T;
        res.ok = true;
        return res;
    }
};

}  // namespace

std::unique_ptr<IMoveSolver> makeThermalScalingSolver() {
    return std::make_unique<ThermalScalingSolver>();
}

}  // namespace opendva
