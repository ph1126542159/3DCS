// A4 tolerance sampler contract (README §5).
// A tolerance is compiled into a unified IR: {Rand[], Distribution, Range,
// Offset, SigmaNum, Truncation, RangeScale, GeomRule}.
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "opendva/Rng.h"
#include "opendva/Types.h"

namespace opendva {

enum class DistributionType {
    Normal, Uniform, Triangular, BiMode, RightSkew, LeftSkew, OpenUp, OpenDown,
    UserDefined, Step, Constant, Weibull4, Pearson4, Modal, Trapezoid, PowerFunction,
    Normal2D, Uniform2D, Triangular2D, Trapezoid2D
};

// How a sampled scalar deviation is applied to geometry (README §5 复刻要点).
enum class GeomRule {
    TranslateAlongVector,       // linear
    RotateAboutLocatorPoint,    // orientation: max at far end
    NodeNormalOffset,           // surface profile (no DRF)
    SectionRadialOffset,        // circularity: section independent, axis collinear
    DiameterScale               // size
};

struct Truncation {
    double minTrunc{0.0};
    double maxTrunc{0.0};
    bool active{false};
};

struct RandSpec {
    DistributionType distribution{DistributionType::Normal};
    double range{0.0};      // = Max - Min
    double offset{0.0};     // = (Max + Min) / 2
    double sigmaNum{3.0};   // = (Max - Min) / 6
    std::string userDefinedSamplePath;  // Optional .SMP file for UserDefined
};

struct ToleranceIR {
    std::vector<RandSpec> rands;     // Rand#1..#4
    Truncation truncation{};
    double rangeScale{1.0};
    GeomRule geomRule{GeomRule::TranslateAlongVector};
    Direction direction{};
};

// One sampled distribution draw.
class IDistribution {
public:
    virtual ~IDistribution() = default;
    virtual double sample(double range, double offset, double sigmaNum, IRng& rng) = 0;
};

class DistributionFactory {
public:
    static std::unique_ptr<IDistribution> create(DistributionType type);
};

class IToleranceSampler {
public:
    virtual ~IToleranceSampler() = default;
    // Sample and apply the deviation to mesh/points (not CAD).
    // Returns the applied scalar magnitude (for contributor bookkeeping).
    virtual double applyDeviation(const ToleranceIR& tol, MeshHandle mesh, IRng& rng) = 0;
};

}  // namespace opendva
