// A5 measure evaluator contract (README §6).
// A measure is a build-stage-bound scalar evaluator: each build produces one
// double; nominal/current dual-track defines signed deviation.
#pragma once
#include <string>
#include <vector>

#include "opendva/Types.h"

namespace opendva {

enum class MeasureType {
    NominalPoint, PointPoint, PointLine, PointPlane, DimensionalDistance,
    CircleInterference, VirtualClearance, CircleDiameter, Circularity,
    FeatureMeasure, FeatureAngle, LineNominal, LineLine, LinePlane,
    PlaneNominal, PlanePlane, TwoPointList, Combination, Equation,
    GdtPosition, GdtSurfaceProfile, GdtPerpendicularity, GdtAngularity,
    GdtParallelism, GdtConcentricity, UserDll
};

// README §6.1.3 — direction strategy shared by all distance/angle measures.
enum class DirectionMode { TrueDistance, ProjectedOnVector, ProjectedOnPlane };

enum class SpecMode { Absolute, RelativeToNominal };

struct SpecLimits {
    double usl{0.0};
    double lsl{0.0};
    bool uslActive{false};
    bool lslActive{false};
    SpecMode mode{SpecMode::Absolute};
};

struct MeasureDef {
    MeasureType type{MeasureType::PointPoint};
    std::vector<PointId> inputPoints;
    std::vector<FeatureId> inputFeatures;
    Direction direction{};
    DirectionMode dirMode{DirectionMode::TrueDistance};
    SpecLimits spec{};
    double scale{1.0};
    bool active{true};
    bool asOutput{true};
    std::string equation;  // for MeasureType::Equation (<=400 chars)
    std::vector<double> values;  // for MeasureType::Equation [VAL:n] constants
};

// Per-build geometry state the evaluator reads (resolved point/feature positions).
struct BuildState {
    // Implementation-defined accessor for current vs nominal positions.
    // Kept opaque here; concrete types live in the domain/engine layer.
    const void* impl{nullptr};
};

class IMeasureEvaluator {
public:
    virtual ~IMeasureEvaluator() = default;
    virtual double evaluate(const MeasureDef& def, const BuildState& state) = 0;
};

}  // namespace opendva
