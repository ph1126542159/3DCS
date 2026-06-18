// Sensitivity tests (README §7.4 GeoFactor, §7.5 Contributor). The golden model
// is a single segment P1(0,0,0)-P2(0,0,10) with a +Z tolerance on P2 and a
// Point-Point gap measure along +Z: the tolerance maps 1:1 into the measure, so
// GeoFactor == 1 and a single contributor owns 100%.
#include <cmath>

#include "dva_test.h"
#include "opendva/domain/Model.h"
#include "opendva/sim/SimulationEngine.h"

using namespace opendva;

namespace {

// One part, P1 fixed, P2 carries a Normal tolerance (range 1, sigmaNum 3) along
// +Z; the gap measure P1->P2 projects on +Z.
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

// Two tolerances on P2 (range 1 and range 2 along +Z), one measure. Both map
// 1:1, so GeoFactor == 1 each; variance shares are 1 : 4 -> 20% / 80%.
Model makeTwoToleranceModel() {
    Model m;
    Part part;
    part.id = 1;
    part.dcsName = "P";
    Point p1; p1.id = 101; p1.position = {0, 0, 0};
    Point p2; p2.id = 102; p2.position = {0, 0, 10};
    part.points = {p1, p2};
    Feature f; f.id = 201; f.kind = FeatureKind::Plane; f.definingPoints = {102};
    part.features = {f};
    ToleranceDef t1; t1.id = 301; t1.active = true; t1.features = {201};
    RandSpec r1; r1.distribution = DistributionType::Normal; r1.range = 1.0; r1.sigmaNum = 3.0;
    t1.ir.rands = {r1}; t1.ir.direction.ijk = {0, 0, 1};
    ToleranceDef t2; t2.id = 302; t2.active = true; t2.features = {201};
    RandSpec r2; r2.distribution = DistributionType::Normal; r2.range = 2.0; r2.sigmaNum = 3.0;
    t2.ir.rands = {r2}; t2.ir.direction.ijk = {0, 0, 1};
    part.tolerances = {t1, t2};
    m.parts = {part};
    MeasureRecord mr; mr.id = 401; mr.name = "gap";
    mr.def.type = MeasureType::PointPoint; mr.def.inputPoints = {101, 102};
    mr.def.direction.ijk = {0, 0, 1}; mr.def.dirMode = DirectionMode::ProjectedOnVector;
    m.measures = {mr};
    return m;
}

Model makeTwoHugeToleranceModel() {
    Model m = makeTwoToleranceModel();
    m.parts[0].tolerances[0].ir.rands[0].range = 1e308;
    m.parts[0].tolerances[1].ir.rands[0].range = 1e308;
    return m;
}

}  // namespace

// GeoFactor of the +Z tolerance on the +Z gap is exactly 1 (1:1 linear map).
TEST("geofactor_unity") {
    Model m = makeGoldenModel();
    auto e = makeMonteCarloEngine();
    SensitivityMatrix sm = e->runGeoFactor(m);
    dvatest::check(sm.contributors.size() == 1, "one contributor");
    dvatest::check(sm.measures.size() == 1, "one measure");
    dvatest::check(sm.geoFactor.size() == 1 && sm.geoFactor[0].size() == 1, "1x1 matrix");
    dvatest::checkNear(sm.geoFactor[0][0], 1.0, 1e-9, "GeoFactor should be 1.0");
}

// Single contributor -> 100% contribution; Six-Sigma = sigma*GF*6 = (1/6)*1*6 = 1.
TEST("contributor_single_100pct") {
    Model m = makeGoldenModel();
    auto e = makeMonteCarloEngine();
    auto rows = e->runContributor(m);
    dvatest::check(rows.size() == 1, "one (measure, contributor) row");
    dvatest::checkNear(rows[0].geoFactor, 1.0, 1e-9, "GeoFactor 1.0");
    dvatest::checkNear(rows[0].contributionPct, 100.0, 1e-9, "single contributor is 100%");
    dvatest::checkNear(rows[0].sixSigma, 1.0, 1e-9, "Six-Sigma = sigma*GF*6 = 1");
}

// Two contributors -> percentages sum to 100% and split 20/80 by variance.
TEST("contributor_two_sum_100pct") {
    Model m = makeTwoToleranceModel();
    auto e = makeMonteCarloEngine();
    auto rows = e->runContributor(m);
    dvatest::check(rows.size() == 2, "two rows for one measure");
    double sum = 0.0;
    for (const auto& row : rows) {
        dvatest::checkNear(row.geoFactor, 1.0, 1e-9, "each GeoFactor 1.0");
        sum += row.contributionPct;
    }
    dvatest::checkNear(sum, 100.0, 1e-9, "percentages sum to 100");
    // range 1 -> sigma 1/6, range 2 -> sigma 2/6; variance ratio 1:4.
    dvatest::checkNear(rows[0].contributionPct, 20.0, 1e-9, "range-1 tol owns 20%");
    dvatest::checkNear(rows[1].contributionPct, 80.0, 1e-9, "range-2 tol owns 80%");
}

TEST("contributor_two_huge_finite_ranges_split_evenly") {
    Model m = makeTwoHugeToleranceModel();
    auto e = makeMonteCarloEngine();
    auto rows = e->runContributor(m);
    dvatest::check(rows.size() == 2, "two huge rows for one measure");
    double sum = 0.0;
    for (const auto& row : rows) {
        dvatest::check(std::isfinite(row.contributionPct),
                       "huge contribution percent remains finite");
        dvatest::check(std::isfinite(row.sixSigma),
                       "huge contributor six sigma remains finite");
        sum += row.contributionPct;
    }
    dvatest::checkNear(sum, 100.0, 1e-9, "huge percentages sum to 100");
    dvatest::checkNear(rows[0].contributionPct, 50.0, 1e-9,
                       "first huge range owns half");
    dvatest::checkNear(rows[1].contributionPct, 50.0, 1e-9,
                       "second huge range owns half");
}
