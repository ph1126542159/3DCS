// A4 tolerance sampler. Compiles a ToleranceIR draw and reports the magnitude.
// Geometric application to the mesh is delegated to the geometry layer in the
// full build; the reference sampler returns the scalar deviation.
#include "opendva/IToleranceSampler.h"
#include "opendva/tolerance/Distributions.h"

namespace opendva {

class ReferenceToleranceSampler final : public IToleranceSampler {
public:
    double applyDeviation(const ToleranceIR& tol, MeshHandle /*mesh*/, IRng& rng) override {
        if (tol.rands.empty()) return 0.0;
        const RandSpec& r = tol.rands.front();
        auto dist = (r.distribution == DistributionType::UserDefined &&
                     !r.userDefinedSamplePath.empty())
                        ? tolerance::makeUserDefinedDistributionFromSmp(
                              r.userDefinedSamplePath)
                        : DistributionFactory::create(r.distribution);
        double mag = dist->sample(r.range, r.offset, r.sigmaNum, rng);
        mag *= tol.rangeScale;
        // Truncation (README §5.6): clamp into [minTrunc, maxTrunc].
        if (tol.truncation.active) {
            if (mag < tol.truncation.minTrunc) mag = tol.truncation.minTrunc;
            if (mag > tol.truncation.maxTrunc) mag = tol.truncation.maxTrunc;
        }
        return mag;
    }
};

// Factory free function (the interface header keeps IToleranceSampler abstract).
std::unique_ptr<IToleranceSampler> makeReferenceToleranceSampler() {
    return std::make_unique<ReferenceToleranceSampler>();
}

}  // namespace opendva
