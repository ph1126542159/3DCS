// A3 move solver contract (README §4).
// Core abstraction: solve a rigid transform (R,t) that satisfies N scalar
// constraints (point-to-plane / point-to-line / point-to-point distances).
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "opendva/Rng.h"
#include "opendva/Types.h"

namespace opendva {

enum class MoveType {
    StepPlane, SixPlane, ThreePoint, TwoPoint, BestFit, FeatureMove, PatternRigid,
    PatternFit, Match, Iteration, Transform, ThermalScaling, UserDll, AutoBend,
    RTouch, RotateLine, Gravity, LeastSquaresAxis, CrossProduct, LinePlane
};

// One object/target correspondence with an associated direction.
struct MovePair {
    Vec3 objectPoint{};
    Vec3 targetPoint{};
    Direction direction{};
};

struct FloatSpec {
    bool active{false};
    int sigmaNumber{3};      // 1..8
    double rangeScale{1.0};
    double angleRangeDeg{360.0};
    double angleOffsetDeg{0.0};
};

struct MoveInputs {
    MoveType type{MoveType::SixPlane};
    std::vector<MovePair> pairs;     // primary/secondary/tertiary in order
    FloatSpec hole_pin_float{};
    double searchAccuracy{1e-5};     // README §4.5
    int maxIterations{500};
    bool isNominalBuild{false};
    std::string userDllRoutine;      // routine name registered through PluginHost
};

struct MoveResult {
    Mat34 transform{};               // identity if !ok
    bool ok{false};
    std::string log;                 // "No Solution" etc. on failure (README §4)
};

class IMoveSolver {
public:
    virtual ~IMoveSolver() = default;
    virtual MoveType type() const = 0;
    virtual MoveResult solve(const MoveInputs& in, IRng& rng) = 0;
};

class MoveSolverFactory {
public:
    // Six-Plane is the general kernel; others are degenerate/ordered specialisations.
    static std::unique_ptr<IMoveSolver> create(MoveType type);
};

}  // namespace opendva
