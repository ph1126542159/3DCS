// A6 simulation-engine tests: Move integration into the Monte Carlo loop
// (README §7.2), convergence sample size (README §7.3) and worst-case synthesis
// (README §7.4). Uses the shared lightweight harness.
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>

#include "dva_test.h"
#include "opendva/domain/Model.h"
#include "opendva/sim/SimulationEngine.h"
#include "opendva/sim/WorstCase.h"

using namespace opendva;

namespace {

// Single-part golden model (mirrors test_sensitivity.cpp): P1(0,0,0)-P2(0,0,10),
// a Normal +Z tolerance (range 1, sigmaNum 3) on P2, gap measure P1->P2 on +Z.
Model makeGoldenModel() {
    Model m;
    Part part;
    part.id = 1;
    part.dcsName = "P";
    Point p1; p1.id = 101; p1.position = {0, 0, 0};
    Point p2; p2.id = 102; p2.position = {0, 0, 10};
    part.points = {p1, p2};
    Feature f; f.id = 201; f.kind = FeatureKind::Plane; f.definingPoints = {102};
    part.features = {f};
    ToleranceDef tol; tol.id = 301; tol.active = true; tol.features = {201};
    RandSpec r; r.distribution = DistributionType::Normal; r.range = 1.0; r.sigmaNum = 3.0;
    tol.ir.rands = {r}; tol.ir.direction.ijk = {0, 0, 1};
    part.tolerances = {tol};
    m.parts = {part};
    MeasureRecord mr; mr.id = 401; mr.name = "gap";
    mr.def.type = MeasureType::PointPoint; mr.def.inputPoints = {101, 102};
    mr.def.direction.ijk = {0, 0, 1}; mr.def.dirMode = DirectionMode::ProjectedOnVector;
    m.measures = {mr};
    return m;
}

// Two-part model. Part A is fixed (A1 at origin). Part B starts with B1 at the
// origin and carries a +X tolerance (range 2). A Transform Move translates B by
// +10 in X (object O1=(0,0,0) -> target T1=(10,0,0), direction +X). The measure
// is the +X gap A1->B1, which must read ~10 (the relocation) with the tolerance
// riding along through the rigid move.
Model makeTwoPartMoveModel() {
    Model m;
    // Part A: fixed reference point at origin.
    Part a;
    a.id = 1; a.dcsName = "A";
    Point a1; a1.id = 101; a1.position = {0, 0, 0};
    a.points = {a1};

    // Part B: relocated by the Move; B1 starts at origin with a +X tolerance.
    Part b;
    b.id = 2; b.dcsName = "B";
    Point b1; b1.id = 201; b1.position = {0, 0, 0};
    b.points = {b1};
    Feature bf; bf.id = 301; bf.kind = FeatureKind::PointBased; bf.definingPoints = {201};
    b.features = {bf};
    ToleranceDef tol; tol.id = 401; tol.active = true; tol.features = {301};
    RandSpec r; r.distribution = DistributionType::Normal; r.range = 2.0; r.sigmaNum = 3.0;
    tol.ir.rands = {r}; tol.ir.direction.ijk = {1, 0, 0};
    b.tolerances = {tol};

    m.parts = {b, a};  // leaf-first (B) then reference (A), per A2 convention.

    // Transform Move: translate part B by +10 along +X.
    MoveDef mv;
    mv.id = 501; mv.name = "locate_B"; mv.active = true;
    mv.inputs.type = MoveType::Transform;
    MovePair pair;
    pair.objectPoint = {0, 0, 0};
    pair.targetPoint = {10, 0, 0};
    pair.direction.ijk = {1, 0, 0};  // refPoints empty -> TRANSLATE branch.
    mv.inputs.pairs = {pair};
    mv.moveParts = {2};  // moves part B only.
    m.moves = {mv};

    // Measure: +X gap from A1 (fixed origin) to B1 (relocated).
    MeasureRecord mr; mr.id = 601; mr.name = "ax_gap";
    mr.def.type = MeasureType::PointPoint; mr.def.inputPoints = {101, 201};
    mr.def.direction.ijk = {1, 0, 0}; mr.def.dirMode = DirectionMode::ProjectedOnVector;
    m.measures = {mr};
    return m;
}

}  // namespace

// README §7.2: the Move must relocate part B so the measure picks up the +10
// translation, and the +X tolerance deviation must ride along the rigid move.
TEST("move_integration_translation_propagates") {
    Model m = makeTwoPartMoveModel();
    auto e = makeMonteCarloEngine();
    RunConfig cfg; cfg.totalRuns = 4000; cfg.initialSeed = 7;
    auto stats = e->runMonteCarlo(m, cfg);
    dvatest::check(stats.count(601) == 1, "measure present");
    const MeasureStats& s = stats.at(601);
    // Nominal build (tolerance zero, move applied) => exactly +10.
    dvatest::checkNear(s.nominal, 10.0, 1e-9, "nominal reflects +10 move");
    // Mean is centred on the relocated nominal (tolerance offset 0).
    dvatest::checkNear(s.mean, 10.0, 0.05, "mean ~ 10 after move");
    // Tolerance rides through the rigid move: sigma ~ range/6 = 2/6 = 0.333.
    dvatest::checkNear(s.sigma, 2.0 / 6.0, 0.02, "tolerance propagated through move");
}

// Same seed + same runs must be bit-for-bit reproducible with Moves active.
TEST("move_integration_reproducible") {
    Model m = makeTwoPartMoveModel();
    auto e = makeMonteCarloEngine();
    RunConfig cfg; cfg.totalRuns = 1000; cfg.initialSeed = 42;
    auto a = e->runMonteCarlo(m, cfg);
    auto b = e->runMonteCarlo(m, cfg);
    dvatest::checkNear(a.at(601).mean, b.at(601).mean, 0.0, "reproducible mean");
    dvatest::checkNear(a.at(601).sigma, b.at(601).sigma, 0.0, "reproducible sigma");
}

TEST("monte_carlo_huge_tolerance_direction_normalizes") {
    Model m;
    Part part;
    part.id = 1;
    Point p1; p1.id = 101; p1.position = {0, 0, 0};
    Point p2; p2.id = 102; p2.position = {0, 0, 10};
    part.points = {p1, p2};
    Feature f; f.id = 201; f.kind = FeatureKind::PointBased; f.definingPoints = {102};
    part.features = {f};
    ToleranceDef tol; tol.id = 301; tol.active = true; tol.features = {201};
    RandSpec r; r.distribution = DistributionType::Normal; r.range = 2.0; r.sigmaNum = 3.0;
    tol.ir.rands = {r};
    tol.ir.direction.ijk = {1e308, 0, 0};
    part.tolerances = {tol};
    m.parts = {part};

    MeasureRecord mr;
    mr.id = 401;
    mr.name = "x_gap";
    mr.def.type = MeasureType::PointPoint;
    mr.def.inputPoints = {101, 102};
    mr.def.direction.ijk = {1, 0, 0};
    mr.def.dirMode = DirectionMode::ProjectedOnVector;
    m.measures = {mr};

    auto e = makeMonteCarloEngine();
    RunConfig cfg;
    cfg.totalRuns = 4000;
    cfg.initialSeed = 17;
    const auto stats = e->runMonteCarlo(m, cfg);
    dvatest::checkNear(stats.at(401).sigma, 2.0 / 6.0, 0.02,
                       "huge tolerance direction propagates as unit +X");
}

// README §7.3 convergence anchors: 3%@95% ~ 2137, 1%@99% ~ 33178.
TEST("monte_carlo_rotate_geomrule_handles_huge_reference_span") {
    Model m;
    Part part;
    part.id = 1;
    Point p1; p1.id = 101; p1.position = {0, 0, 0};
    Point p2; p2.id = 102; p2.position = {0, 0, 0};
    Point p3; p3.id = 103; p3.position = {1e308, 0, 0};
    part.points = {p1, p2, p3};
    Feature f; f.id = 201; f.kind = FeatureKind::PointBased; f.definingPoints = {102, 103};
    part.features = {f};
    ToleranceDef tol; tol.id = 301; tol.active = true; tol.features = {201};
    RandSpec r; r.distribution = DistributionType::Constant; r.offset = 0.5; r.sigmaNum = 3.0;
    tol.ir.rands = {r};
    tol.ir.geomRule = GeomRule::RotateAboutLocatorPoint;
    tol.ir.direction.ijk = {0, 0, 1};
    part.tolerances = {tol};
    m.parts = {part};

    MeasureRecord mr;
    mr.id = 401;
    mr.name = "far_z";
    mr.def.type = MeasureType::PointPoint;
    mr.def.inputPoints = {101, 103};
    mr.def.direction.ijk = {0, 0, 1};
    mr.def.dirMode = DirectionMode::ProjectedOnVector;
    m.measures = {mr};

    auto e = makeMonteCarloEngine();
    RunConfig cfg;
    cfg.totalRuns = 1;
    cfg.initialSeed = 1;
    const auto stats = e->runMonteCarlo(m, cfg);
    dvatest::checkNear(stats.at(401).mean, 0.5, 1e-12,
                       "huge rotate reference span reaches full magnitude");
}

TEST("required_runs_anchors") {
    const int n95 = requiredRuns(0.03, 0.95);
    const int n99 = requiredRuns(0.01, 0.99);
    dvatest::check(std::abs(n95 - 2137) <= 50,
                   "3%@95% ~ 2137 (got " + std::to_string(n95) + ")");
    dvatest::check(std::abs(n99 - 33178) <= 500,
                   "1%@99% ~ 33178 (got " + std::to_string(n99) + ")");
    dvatest::check(requiredRuns(0.0, 0.95) == 0, "zero error -> 0 runs");
}

TEST("required_runs_rejects_invalid_confidence") {
    dvatest::check(requiredRuns(0.03, 0.0) == 0,
                   "zero confidence -> 0 runs");
    dvatest::check(requiredRuns(0.03, 1.0) == 0,
                   "unit confidence -> 0 runs");
    dvatest::check(requiredRuns(0.03, 1.5) == 0,
                   "over-unit confidence -> 0 runs");
}

// invNormalCdf sanity: median 0, symmetric, matches the 97.5% quantile 1.96.
TEST("inv_normal_cdf_quantiles") {
    dvatest::checkNear(invNormalCdf(0.5), 0.0, 1e-9, "median quantile is 0");
    dvatest::checkNear(invNormalCdf(0.975), 1.959964, 1e-4, "97.5% quantile ~ 1.96");
    dvatest::checkNear(invNormalCdf(0.995), 2.575829, 1e-4, "99.5% quantile ~ 2.576");
}

TEST("monte_carlo_stats_keep_huge_constant_samples_finite") {
    Model m;
    Part part;
    part.id = 1;
    Point p1; p1.id = 101; p1.position = {0, 0, 0};
    Point p2; p2.id = 102; p2.position = {1e308, 0, 0};
    part.points = {p1, p2};
    m.parts = {part};

    MeasureRecord mr;
    mr.id = 401;
    mr.name = "huge_gap";
    mr.def.type = MeasureType::PointPoint;
    mr.def.inputPoints = {101, 102};
    mr.def.direction.ijk = {1, 0, 0};
    mr.def.dirMode = DirectionMode::ProjectedOnVector;
    m.measures = {mr};

    auto e = makeMonteCarloEngine();
    RunConfig cfg;
    cfg.totalRuns = 2;
    const auto stats = e->runMonteCarlo(m, cfg);
    const MeasureStats& s = stats.at(401);
    dvatest::check(std::isfinite(s.mean), "huge constant mean remains finite");
    dvatest::checkNear(s.mean, 1e308, 0.0, "huge constant mean");
    dvatest::checkNear(s.sigma, 0.0, 0.0, "huge constant sigma");
    dvatest::checkNear(s.range, 0.0, 0.0, "huge constant range");
}

TEST("monte_carlo_stats_keep_huge_userdefined_sigma_finite") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "opendva_huge_mc_sigma.smp";
    {
        std::ofstream out(path);
        out << "0\n";
        out << "1\n";
    }

    Model m;
    Part part;
    part.id = 1;
    Point p1; p1.id = 101; p1.position = {0, 0, 0};
    Point p2; p2.id = 102; p2.position = {0, 0, 0};
    part.points = {p1, p2};
    Feature f; f.id = 201; f.kind = FeatureKind::PointBased; f.definingPoints = {102};
    part.features = {f};
    ToleranceDef tol; tol.id = 301; tol.active = true; tol.features = {201};
    RandSpec r;
    r.distribution = DistributionType::UserDefined;
    r.range = 1e308;
    r.sigmaNum = 3.0;
    r.userDefinedSamplePath = path.string();
    tol.ir.rands = {r};
    tol.ir.direction.ijk = {1, 0, 0};
    part.tolerances = {tol};
    m.parts = {part};

    MeasureRecord mr;
    mr.id = 401;
    mr.name = "huge_user_gap";
    mr.def.type = MeasureType::PointPoint;
    mr.def.inputPoints = {101, 102};
    mr.def.direction.ijk = {1, 0, 0};
    mr.def.dirMode = DirectionMode::ProjectedOnVector;
    m.measures = {mr};

    auto e = makeMonteCarloEngine();
    RunConfig cfg;
    cfg.totalRuns = 20;
    cfg.initialSeed = 123;
    const MonteCarloResult result = e->runMonteCarloDetailed(m, cfg);
    std::remove(path.string().c_str());

    bool sawNegative = false;
    bool sawPositive = false;
    for (const auto& row : result.samples) {
        const double value = row.measureValues.at(401);
        sawNegative = sawNegative || value < 0.0;
        sawPositive = sawPositive || value > 0.0;
    }
    dvatest::check(sawNegative && sawPositive, "huge user-defined samples vary");
    const MeasureStats& s = result.stats.at(401);
    dvatest::check(std::isfinite(s.sigma), "huge user-defined sigma remains finite");
    dvatest::check(s.sigma > 0.0, "huge user-defined sigma is positive");
}

// README §7.4: golden single-tolerance worst case = Range * GeoFactor.
// Range 1, GeoFactor 1 -> WC Range 1.
TEST("worst_case_golden") {
    Model m = makeGoldenModel();
    auto wc = computeWorstCase(m);
    dvatest::check(wc.count(401) == 1, "worst case for the gap measure");
    dvatest::checkNear(wc.at(401), 1.0, 1e-9, "WC = Range(1) * GeoFactor(1) = 1");
}
