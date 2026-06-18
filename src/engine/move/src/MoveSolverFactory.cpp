// A3 factory: maps MoveType -> solver. Six-Plane is the general kernel; this
// reference build routes the point-to-plane family through that kernel and
// implements specialised closed-form, plugin-dispatch, or guarded reference
// behaviour for the remaining supported move types.
#include "opendva/IMoveSolver.h"

#include <cmath>

#include "opendva/dcs_plugin_api.h"
#include "opendva/plugin/PluginHost.h"

namespace opendva {

// Defined in SixPlaneSolver.cpp.
MoveResult solveSixPlane(const MoveInputs& in);

// Closed-form specialised solvers (each in its own TU).
std::unique_ptr<IMoveSolver> makeThreePointSolver();
std::unique_ptr<IMoveSolver> makeTwoPointSolver();
std::unique_ptr<IMoveSolver> makeTransformSolver();
std::unique_ptr<IMoveSolver> makeThermalScalingSolver();
std::unique_ptr<IMoveSolver> makeCrossProductSolver();
std::unique_ptr<IMoveSolver> makeRotateLineSolver();
std::unique_ptr<IMoveSolver> makeRTouchSolver();
std::unique_ptr<IMoveSolver> makeMatchSolver();
std::unique_ptr<IMoveSolver> makeGravitySolver();
std::unique_ptr<IMoveSolver> makeBestFitSolver();
std::unique_ptr<IMoveSolver> makeLeastSquaresAxisSolver();

namespace {

class GenericPlaneSolver final : public IMoveSolver {
public:
    explicit GenericPlaneSolver(MoveType t) : type_(t) {}
    MoveType type() const override { return type_; }
    MoveResult solve(const MoveInputs& in, IRng& /*rng*/) override {
        // SixPlane / StepPlane / LinePlane / FeatureMove all reduce to a
        // point-to-plane least-squares fit in this reference build.
        return solveSixPlane(in);
    }

private:
    MoveType type_;
};

class NotImplementedSolver final : public IMoveSolver {
public:
    explicit NotImplementedSolver(MoveType t) : type_(t) {}
    MoveType type() const override { return type_; }
    MoveResult solve(const MoveInputs& /*in*/, IRng& /*rng*/) override {
        MoveResult r{};
        r.ok = false;
        if (type_ == MoveType::UserDll) {
            r.log = "No Solution: UserDll move requires a bound plugin host";
        } else if (type_ == MoveType::AutoBend) {
            r.log = "No Solution: AutoBend move is missing bend/FEA infrastructure";
        } else {
            r.log = "Move type not yet implemented in reference build (A3 TODO)";
        }
        return r;
    }

private:
    MoveType type_;
};

class UserDllSolver final : public IMoveSolver {
public:
    MoveType type() const override { return MoveType::UserDll; }
    MoveResult solve(const MoveInputs& in, IRng& /*rng*/) override {
        MoveResult r{};
        if (in.userDllRoutine.empty()) {
            r.log = "No Solution: UserDll move requires a bound plugin host";
            return r;
        }
        auto* host = plugin::PluginHost::active();
        if (!host) {
            r.log = "No Solution: UserDll move requires a bound plugin host";
            return r;
        }

        dcsMoveCalData data{};
        for (int row = 0; row < 3; ++row) {
            data.transform[row][row] = 1.0;
        }
        if (!host->invokeRoutine(in.userDllRoutine, dcsCalTypeMove, &data)) {
            r.log = "No Solution: UserDll move requires a bound plugin host";
            return r;
        }
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 4; ++col) {
                const double value = data.transform[row][col];
                if (!std::isfinite(value)) {
                    r.log = "No Solution: UserDll move returned a non-finite transform";
                    return r;
                }
                r.transform.m[row][col] = value;
            }
        }
        r.ok = true;
        r.log = "UserDll move dispatched through plugin host";
        return r;
    }
};

class IterationSolver final : public IMoveSolver {
public:
    MoveType type() const override { return MoveType::Iteration; }
    MoveResult solve(const MoveInputs& in, IRng& /*rng*/) override {
        MoveResult r{};
        if (!in.pairs.empty()) {
            r.log = "No Solution: Iteration move is missing nested move-sequence infrastructure";
            return r;
        }
        r.ok = true;
        r.log = "Iteration reference move: empty sequence -> identity";
        return r;
    }
};

class DelegatingSolver final : public IMoveSolver {
public:
    DelegatingSolver(MoveType type, std::unique_ptr<IMoveSolver> inner)
        : type_(type), inner_(std::move(inner)) {}
    MoveType type() const override { return type_; }
    MoveResult solve(const MoveInputs& in, IRng& rng) override {
        return inner_ ? inner_->solve(in, rng) : MoveResult{};
    }

private:
    MoveType type_;
    std::unique_ptr<IMoveSolver> inner_;
};

}  // namespace

std::unique_ptr<IMoveSolver> MoveSolverFactory::create(MoveType type) {
    switch (type) {
        // Point-to-plane family: shared Six-Plane least-squares kernel.
        case MoveType::SixPlane:
        case MoveType::StepPlane:
        case MoveType::LinePlane:
        case MoveType::FeatureMove:
            return std::make_unique<GenericPlaneSolver>(type);
        // Closed-form specialised moves (A3, README §4.3/4.4/4.12/4.13).
        case MoveType::ThreePoint:
            return makeThreePointSolver();
        case MoveType::TwoPoint:
            return makeTwoPointSolver();
        case MoveType::Transform:
            return makeTransformSolver();
        case MoveType::ThermalScaling:
            return makeThermalScalingSolver();
        // Closed-form / iterative specialised moves (A3, README §4.5/4.10/4.16-4.20).
        case MoveType::CrossProduct:
            return makeCrossProductSolver();
        case MoveType::RotateLine:
            return makeRotateLineSolver();
        case MoveType::RTouch:
            return makeRTouchSolver();
        case MoveType::Match:
            return makeMatchSolver();
        case MoveType::Gravity:
            return makeGravitySolver();
        case MoveType::PatternRigid:
            return std::make_unique<DelegatingSolver>(type, makeBestFitSolver());
        case MoveType::PatternFit:
            return std::make_unique<DelegatingSolver>(type, makeBestFitSolver());
        case MoveType::BestFit:
            return makeBestFitSolver();
        case MoveType::LeastSquaresAxis:
            return makeLeastSquaresAxisSolver();
        case MoveType::Iteration:
            return std::make_unique<IterationSolver>();
        case MoveType::UserDll:
            return std::make_unique<UserDllSolver>();
        // Still TODO (need pattern/iteration/bend infrastructure):
        //   PatternFit hole-set solving (§4.9): multi-body hole/pin constraint
        //     satisfaction needs the pattern model. Point-to-point constraint
        //     pairs are supported above through the rigid BestFit path.
        //   Iteration (§4.11): non-empty nested move sequences need the
        //     move-sequence/iteration engine; empty reference move is supported.
        //   AutoBend (§4.15): flexible-body bending; needs the FEA/bend infrastructure.
        default:
            return std::make_unique<NotImplementedSolver>(type);
    }
}

}  // namespace opendva
