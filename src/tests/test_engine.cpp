// Engine tests: reproducibility (the §7 red line), statistics correctness,
// and the Six-Plane move solver.
#include <cmath>
#include <limits>
#include <map>

#include "dva_test.h"
#include "opendva/dcs_plugin_api.h"
#include "opendva/Mt19937Rng.h"
#include "opendva/domain/Model.h"
#include "opendva/move/MoveSolver.h"
#include "opendva/plugin/PluginHost.h"
#include "opendva/sim/SimulationEngine.h"

using namespace opendva;

namespace {

void userDllMeasureRoutine(dcsDataPtr data) {
    auto* measureData = static_cast<dcsMeasureCalData*>(data);
    measureData->value = 12.25;
}

Model makeSimpleModel() {
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

}  // namespace

// Same seed + same runs -> identical results (README §7 reproducibility).
TEST("monte_carlo_userdll_measure_dispatches_active_plugin_routine") {
    opendva::plugin::PluginHost host;
    host.registerRoutine("calcGap", &userDllMeasureRoutine, dcsCalTypeMeas);
    opendva::plugin::PluginHost::ActiveScope scope(&host);

    Model m;
    Part part;
    part.id = 1;
    part.dcsName = "P";
    Point p;
    p.id = 101;
    part.points = {p};
    m.parts = {part};

    MeasureRecord mr;
    mr.id = 401;
    mr.name = "userdll_gap";
    mr.def.type = MeasureType::UserDll;
    mr.def.equation = "calcGap";
    mr.def.scale = 2.0;
    m.measures = {mr};

    auto e = makeMonteCarloEngine();
    RunConfig cfg;
    cfg.totalRuns = 2;
    cfg.initialSeed = 123;
    const auto detailed = e->runMonteCarloDetailed(m, cfg);

    dvatest::check(detailed.samples.size() == 2, "User-DLL sample rows exist");
    dvatest::check(detailed.samples[0].measureValues.count(401) == 1,
                   "User-DLL sample value exists");
    dvatest::check(detailed.stats.count(401) == 1, "User-DLL stats exist");
    if (detailed.samples.size() >= 1 &&
        detailed.samples[0].measureValues.count(401) == 1 &&
        detailed.stats.count(401) == 1) {
        dvatest::checkNear(detailed.samples[0].measureValues.at(401), 24.5, 1e-12,
                           "User-DLL sample value comes from active plugin routine");
        dvatest::checkNear(detailed.stats.at(401).nominal, 24.5, 1e-12,
                           "User-DLL nominal value comes from active plugin routine");
        dvatest::checkNear(detailed.stats.at(401).mean, 24.5, 1e-12,
                           "User-DLL mean comes from active plugin routine");
    }
}

TEST("monte_carlo_reproducible") {
    Model m = makeSimpleModel();
    auto e1 = makeMonteCarloEngine();
    auto e2 = makeMonteCarloEngine();
    RunConfig cfg; cfg.totalRuns = 5000; cfg.initialSeed = 42;
    auto s1 = e1->runMonteCarlo(m, cfg);
    auto s2 = e2->runMonteCarlo(m, cfg);
    dvatest::checkNear(s1[401].mean, s2[401].mean, 0.0, "mean must match exactly");
    dvatest::checkNear(s1[401].sigma, s2[401].sigma, 0.0, "sigma must match exactly");
}

// Different seed -> (almost surely) different mean.
TEST("monte_carlo_seed_changes_result") {
    Model m = makeSimpleModel();
    auto e = makeMonteCarloEngine();
    RunConfig a; a.totalRuns = 5000; a.initialSeed = 1;
    RunConfig b; b.totalRuns = 5000; b.initialSeed = 2;
    auto sa = e->runMonteCarlo(m, a);
    auto sb = e->runMonteCarlo(m, b);
    dvatest::check(std::fabs(sa[401].mean - sb[401].mean) > 1e-12,
                   "different seeds should give different means");
}

// README §7.6 Show Samples: keep the per-build measure values so the Simulation
// Window can inspect individual Monte Carlo builds, not only aggregate stats.
TEST("monte_carlo_detailed_returns_reproducible_samples") {
    Model m = makeSimpleModel();
    auto e = makeMonteCarloEngine();
    RunConfig cfg;
    cfg.totalRuns = 5;
    cfg.initialSeed = 99;

    auto detailed = e->runMonteCarloDetailed(m, cfg);
    auto again = e->runMonteCarloDetailed(m, cfg);

    dvatest::check(detailed.stats.count(401) == 1, "stats include measure 401");
    dvatest::check(detailed.samples.size() == 5, "one sample row per build");
    dvatest::check(detailed.samples[0].buildIndex == 0, "first build index");
    dvatest::check(detailed.samples[4].buildIndex == 4, "last build index");
    dvatest::check(detailed.samples[0].measureValues.count(401) == 1,
                   "sample row has measure value");
    dvatest::checkNear(detailed.samples[0].measureValues.at(401),
                       again.samples[0].measureValues.at(401),
                       0.0, "sample value reproducible");
    dvatest::checkNear(detailed.stats.at(401).mean,
                       e->runMonteCarlo(m, cfg).at(401).mean,
                       0.0, "detailed stats match legacy stats");
}

// Measures run in model order, so a Combination can reference earlier measures
// and use their current build values.
TEST("monte_carlo_combination_references_earlier_measures") {
    Model m;
    Part part;
    part.id = 1;
    part.dcsName = "P";
    Point p0; p0.id = 101; p0.position = {0, 0, 0};
    Point p1; p1.id = 102; p1.position = {0, 0, 3};
    Point p2; p2.id = 103; p2.position = {0, 0, 7};
    part.points = {p0, p1, p2};
    m.parts = {part};

    MeasureRecord a; a.id = 401; a.name = "gap_a";
    a.def.type = MeasureType::PointPoint;
    a.def.inputPoints = {101, 102};
    a.def.direction.ijk = {0, 0, 1};
    a.def.dirMode = DirectionMode::ProjectedOnVector;

    MeasureRecord b; b.id = 402; b.name = "gap_b";
    b.def.type = MeasureType::PointPoint;
    b.def.inputPoints = {102, 103};
    b.def.direction.ijk = {0, 0, 1};
    b.def.dirMode = DirectionMode::ProjectedOnVector;

    MeasureRecord combo; combo.id = 403; combo.name = "stack";
    combo.def.type = MeasureType::Combination;
    combo.def.inputFeatures = {401, 402};
    m.measures = {a, b, combo};

    auto e = makeMonteCarloEngine();
    RunConfig cfg;
    cfg.totalRuns = 3;
    cfg.initialSeed = 123;
    auto detailed = e->runMonteCarloDetailed(m, cfg);

    dvatest::checkNear(detailed.stats.at(403).mean, 7.0, 1e-12,
                       "combination mean sums referenced measures");
    dvatest::checkNear(detailed.samples[0].measureValues.at(403), 7.0, 1e-12,
                       "combination sample sums referenced measures");
}

TEST("monte_carlo_feature_angle_resolves_input_features") {
    Model m;
    Part part;
    part.id = 1;
    part.dcsName = "P";
    Point x0;
    x0.id = 101;
    x0.position = {0, 0, 0};
    Point x1;
    x1.id = 102;
    x1.position = {1, 0, 0};
    Point y0;
    y0.id = 103;
    y0.position = {0, 0, 0};
    Point y1;
    y1.id = 104;
    y1.position = {0, 1, 0};
    part.points = {x0, x1, y0, y1};
    Feature featureX;
    featureX.id = 201;
    featureX.kind = FeatureKind::Edge;
    featureX.definingPoints = {101, 102};
    Feature featureY;
    featureY.id = 202;
    featureY.kind = FeatureKind::Edge;
    featureY.definingPoints = {103, 104};
    part.features = {featureX, featureY};
    m.parts = {part};

    MeasureRecord angle;
    angle.id = 401;
    angle.name = "feature_angle";
    angle.def.type = MeasureType::FeatureAngle;
    angle.def.inputFeatures = {201, 202};
    angle.def.direction.ijk = {0, 0, 1};
    m.measures = {angle};

    auto e = makeMonteCarloEngine();
    RunConfig cfg;
    cfg.totalRuns = 1;
    cfg.initialSeed = 123;
    auto detailed = e->runMonteCarloDetailed(m, cfg);

    dvatest::checkNear(detailed.stats.at(401).nominal, 90.0, 1e-12,
                       "feature angle nominal from feature inputs");
    dvatest::checkNear(detailed.samples[0].measureValues.at(401), 90.0, 1e-12,
                       "feature angle sample from feature inputs");
}

TEST("monte_carlo_tolerance_direction_two_points_uses_reference_vector") {
    Model m;
    Part part;
    part.id = 1;
    part.dcsName = "P";
    Point anchor;
    anchor.id = 100;
    anchor.position = {0, 0, 0};
    Point moving;
    moving.id = 101;
    moving.position = {0, 0, 0};
    Point dir0;
    dir0.id = 102;
    dir0.position = {0, 0, 0};
    Point dir1;
    dir1.id = 103;
    dir1.position = {0, 5, 0};
    part.points = {anchor, moving, dir0, dir1};

    Feature movedFeature;
    movedFeature.id = 201;
    movedFeature.kind = FeatureKind::PointBased;
    movedFeature.definingPoints = {101};
    part.features = {movedFeature};

    ToleranceDef tol;
    tol.id = 301;
    tol.active = true;
    tol.features = {201};
    RandSpec rand;
    rand.distribution = DistributionType::Constant;
    rand.offset = 2.0;
    tol.ir.rands = {rand};
    tol.ir.direction.type = DirectionType::TwoPoints;
    tol.ir.direction.refPoints = {102, 103};
    tol.ir.direction.ijk = {0, 0, 1};
    part.tolerances = {tol};
    m.parts = {part};

    MeasureRecord gap;
    gap.id = 401;
    gap.name = "y_gap";
    gap.def.type = MeasureType::PointPoint;
    gap.def.inputPoints = {100, 101};
    gap.def.direction.ijk = {0, 1, 0};
    gap.def.dirMode = DirectionMode::ProjectedOnVector;
    m.measures = {gap};

    auto e = makeMonteCarloEngine();
    RunConfig cfg;
    cfg.totalRuns = 1;
    cfg.initialSeed = 123;
    const auto detailed = e->runMonteCarloDetailed(m, cfg);

    dvatest::checkNear(detailed.stats.at(401).nominal, 0.0, 1e-12,
                       "two-point direction nominal remains unshifted");
    dvatest::checkNear(detailed.samples[0].measureValues.at(401), 2.0, 1e-12,
                       "two-point tolerance direction follows reference vector");
}

TEST("monte_carlo_node_normal_geomrule_uses_point_direction") {
    Model m;
    Part part;
    part.id = 1;
    part.dcsName = "P";
    Point anchor;
    anchor.id = 100;
    anchor.position = {0, 0, 0};
    Point moving;
    moving.id = 101;
    moving.position = {0, 0, 0};
    moving.ijk = {0, 1, 0};
    part.points = {anchor, moving};

    Feature movedFeature;
    movedFeature.id = 201;
    movedFeature.kind = FeatureKind::PointBased;
    movedFeature.definingPoints = {101};
    part.features = {movedFeature};

    ToleranceDef tol;
    tol.id = 301;
    tol.active = true;
    tol.features = {201};
    RandSpec rand;
    rand.distribution = DistributionType::Constant;
    rand.offset = 2.0;
    tol.ir.rands = {rand};
    tol.ir.geomRule = GeomRule::NodeNormalOffset;
    tol.ir.direction.ijk = {0, 0, 1};
    part.tolerances = {tol};
    m.parts = {part};

    MeasureRecord gap;
    gap.id = 401;
    gap.name = "node_normal_y";
    gap.def.type = MeasureType::PointPoint;
    gap.def.inputPoints = {100, 101};
    gap.def.direction.ijk = {0, 1, 0};
    gap.def.dirMode = DirectionMode::ProjectedOnVector;
    m.measures = {gap};

    auto e = makeMonteCarloEngine();
    RunConfig cfg;
    cfg.totalRuns = 1;
    cfg.initialSeed = 123;
    const auto detailed = e->runMonteCarloDetailed(m, cfg);

    dvatest::checkNear(detailed.samples[0].measureValues.at(401), 2.0, 1e-12,
                       "node-normal geom rule follows point direction");
}

// Normal tolerance range 1.0 @ sigmaNum 3 -> sigma ~= 1/6 ~= 0.1667.
TEST("normal_sigma_from_range") {
    Model m = makeSimpleModel();
    auto e = makeMonteCarloEngine();
    RunConfig cfg; cfg.totalRuns = 200000; cfg.initialSeed = 7;
    auto s = e->runMonteCarlo(m, cfg);
    dvatest::checkNear(s[401].sigma, 1.0 / 6.0, 0.005, "sigma should be range/6");
    dvatest::checkNear(s[401].mean, 10.0, 0.01, "mean should center on nominal gap 10");
}

// Six-Plane solver: identity case (object already on target planes).
TEST("sixplane_identity") {
    MoveInputs in; in.type = MoveType::SixPlane;
    // 6 axis-aligned point-to-plane constraints already satisfied.
    auto add = [&](Vec3 o, Vec3 t, Vec3 n) {
        MovePair p; p.objectPoint = o; p.targetPoint = t; p.direction.ijk = n; in.pairs.push_back(p);
    };
    add({1,0,0},{1,0,0},{1,0,0}); add({0,1,0},{0,1,0},{0,1,0}); add({0,0,1},{0,0,1},{0,0,1});
    add({2,0,0},{2,0,0},{1,0,0}); add({0,2,0},{0,2,0},{0,1,0}); add({0,0,2},{0,0,2},{0,0,1});
    Mt19937Rng rng(1);
    MoveResult r = solveSixPlane(in);
    dvatest::check(r.ok, "identity six-plane should solve");
    dvatest::checkNear(r.transform.m[0][3], 0.0, 1e-6, "no translation X");
    dvatest::checkNear(r.transform.m[2][3], 0.0, 1e-6, "no translation Z");
}

// Six-Plane requires every target plane to have a real normal. A zero normal is
// an invalid constraint and must not fall back to +Z.
TEST("sixplane_zero_normal_fails") {
    MoveInputs in;
    in.type = MoveType::SixPlane;
    auto add = [&](Vec3 o, Vec3 t, Vec3 n) {
        MovePair p;
        p.objectPoint = o;
        p.targetPoint = t;
        p.direction.ijk = n;
        in.pairs.push_back(p);
    };
    add({1, 0, 0}, {1, 0, 0}, {1, 0, 0});
    add({0, 1, 0}, {0, 1, 0}, {0, 1, 0});
    add({0, 0, 1}, {0, 0, 1}, {0, 0, 0});
    add({2, 0, 0}, {2, 0, 0}, {1, 0, 0});
    add({0, 2, 0}, {0, 2, 0}, {0, 1, 0});
    add({0, 0, 2}, {0, 0, 2}, {0, 0, 1});

    MoveResult r = solveSixPlane(in);
    dvatest::check(!r.ok, "zero six-plane normal must fail");
    dvatest::check(r.log.find("normal is zero") != std::string::npos,
                   "zero normal diagnostic");
}

TEST("sixplane_rejects_nonfinite_normal") {
    MoveInputs in;
    in.type = MoveType::SixPlane;
    auto add = [&](Vec3 o, Vec3 t, Vec3 n) {
        MovePair p;
        p.objectPoint = o;
        p.targetPoint = t;
        p.direction.ijk = n;
        in.pairs.push_back(p);
    };
    add({1, 0, 0}, {1, 0, 0}, {1, 0, 0});
    add({0, 1, 0}, {0, 1, 0}, {0, 1, 0});
    add({0, 0, 1}, {0, 0, 1}, {std::numeric_limits<double>::infinity(), 0, 0});
    add({2, 0, 0}, {2, 0, 0}, {1, 0, 0});
    add({0, 2, 0}, {0, 2, 0}, {0, 1, 0});
    add({0, 0, 2}, {0, 0, 2}, {0, 0, 1});

    MoveResult r = solveSixPlane(in);
    dvatest::check(!r.ok, "non-finite six-plane normal must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "non-finite normal diagnostic");
}

TEST("sixplane_rejects_nonfinite_constraint_point") {
    MoveInputs in;
    in.type = MoveType::SixPlane;
    auto add = [&](Vec3 o, Vec3 t, Vec3 n) {
        MovePair p;
        p.objectPoint = o;
        p.targetPoint = t;
        p.direction.ijk = n;
        in.pairs.push_back(p);
    };
    add({1, 0, 0}, {1, 0, 0}, {1, 0, 0});
    add({0, 1, 0}, {0, 1, 0}, {0, 1, 0});
    add({0, 0, 1}, {0, 0, std::numeric_limits<double>::quiet_NaN()}, {0, 0, 1});
    add({2, 0, 0}, {2, 0, 0}, {1, 0, 0});
    add({0, 2, 0}, {0, 2, 0}, {0, 1, 0});
    add({0, 0, 2}, {0, 0, 2}, {0, 0, 1});

    MoveResult r = solveSixPlane(in);
    dvatest::check(!r.ok, "non-finite six-plane point must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "non-finite six-plane point diagnostic");
}

// Six-Plane solver: pure Z translation recovered. A consistent point-to-plane
// system: only the Z-normal constraints demand +5; X/Y-normal constraints are
// satisfied by any Z translation (their residual is unaffected), so the unique
// least-squares solution is a clean +5 shift in Z.
TEST("sixplane_translation") {
    MoveInputs in; in.type = MoveType::SixPlane;
    auto add = [&](Vec3 o, Vec3 t, Vec3 n) {
        MovePair p; p.objectPoint = o; p.targetPoint = t; p.direction.ijk = n; in.pairs.push_back(p);
    };
    // X/Y-normal constraints already satisfied (no demand along X or Y).
    add({0,0,0},{0,0,0},{1,0,0}); add({0,0,0},{0,0,0},{0,1,0});
    add({1,0,0},{1,0,0},{1,0,0}); add({0,1,0},{0,1,0},{0,1,0});
    // Z-normal constraints both demand +5: consistent -> pure +5 Z translation.
    add({0,0,0},{0,0,5},{0,0,1}); add({1,1,0},{1,1,5},{0,0,1});
    MoveResult r = solveSixPlane(in);
    dvatest::check(r.ok, "should solve");
    dvatest::checkNear(r.transform.m[2][3], 5.0, 1e-3, "Z translation should be ~5");
}

TEST("sixplane_translation_handles_huge_finite_normals") {
    MoveInputs in;
    in.type = MoveType::SixPlane;
    auto add = [&](Vec3 o, Vec3 t, Vec3 n) {
        MovePair p;
        p.objectPoint = o;
        p.targetPoint = t;
        p.direction.ijk = n;
        in.pairs.push_back(p);
    };
    add({0, 0, 0}, {0, 0, 0}, {1, 0, 0});
    add({0, 0, 0}, {0, 0, 0}, {0, 1, 0});
    add({1, 0, 0}, {1, 0, 0}, {1, 0, 0});
    add({0, 1, 0}, {0, 1, 0}, {0, 1, 0});
    add({0, 0, 0}, {0, 0, 5}, {0, 0, 1e308});
    add({1, 1, 0}, {1, 1, 5}, {0, 0, 1e308});
    MoveResult r = solveSixPlane(in);
    dvatest::check(r.ok, "huge-normal six-plane should solve");
    dvatest::checkNear(r.transform.m[2][3], 5.0, 1e-3,
                       "huge-normal Z translation should be ~5");
}

TEST("sixplane_rejects_nonfinite_transform") {
    MoveInputs in;
    in.type = MoveType::SixPlane;
    const double inf = std::numeric_limits<double>::infinity();
    auto add = [&](Vec3 o, Vec3 t, Vec3 n) {
        MovePair p;
        p.objectPoint = o;
        p.targetPoint = t;
        p.direction.ijk = n;
        in.pairs.push_back(p);
    };
    add({0, 0, 0}, {0, 0, 0}, {1, 0, 0});
    add({0, 0, 0}, {0, 0, 0}, {0, 1, 0});
    add({1, 0, 0}, {1, 0, 0}, {1, 0, 0});
    add({0, 1, 0}, {0, 1, 0}, {0, 1, 0});
    add({0, 0, 0}, {0, 0, inf}, {0, 0, 1});
    add({1, 1, 0}, {1, 1, 5}, {0, 0, 1});
    MoveResult r = solveSixPlane(in);
    dvatest::check(!r.ok, "non-finite six-plane transform must fail");
    dvatest::check(r.log.find("not finite") != std::string::npos,
                   "non-finite six-plane transform diagnostic");
}
