// A4 GeomRule / RSS / Bonus tests (README §5.1, §5.4, §5.6) plus the radial 2D
// (Triangular2D / Trapezoid2D) and injectable UserDefined distributions.
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

#include "dva_test.h"
#include "opendva/Mt19937Rng.h"
#include "opendva/tolerance/Distributions.h"
#include "opendva/tolerance/GeomRule.h"
#include "opendva/tolerance/ToleranceSampler.h"

using namespace opendva;
using namespace opendva::tolerance;

namespace {

double vlen(const Vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

}  // namespace

// TranslateAlongVector: rigid shift along the direction. magnitude 2 along
// (0,0,1) -> exactly (0,0,2), independent of the target point.
TEST("geomrule_translate") {
    GeomRuleInput in;
    in.rule = GeomRule::TranslateAlongVector;
    in.magnitude = 2.0;
    in.dir = {0.0, 0.0, 1.0};
    in.target = {7.0, -3.0, 5.0};  // arbitrary; must not matter
    Vec3 off = applyGeomRule(in);
    dvatest::checkNear(off.x, 0.0, 1e-12, "translate x");
    dvatest::checkNear(off.y, 0.0, 1e-12, "translate y");
    dvatest::checkNear(off.z, 2.0, 1e-12, "translate z");
}

// TranslateAlongVector normalises a non-unit direction before scaling.
TEST("geomrule_translate_nonunit_dir") {
    GeomRuleInput in;
    in.rule = GeomRule::TranslateAlongVector;
    in.magnitude = 3.0;
    in.dir = {0.0, 4.0, 0.0};  // length 4 -> unit (0,1,0)
    Vec3 off = applyGeomRule(in);
    dvatest::checkNear(off.y, 3.0, 1e-12, "translate normalised magnitude");
    dvatest::checkNear(vlen(off), 3.0, 1e-12, "translate offset length = magnitude");
}

// RotateAboutLocatorPoint: zero offset at the FLP, growing linearly to the far
// end. Zone normal +Z, feature extends along +X; the lever arm is the X span.
TEST("geomrule_rotate_locator") {
    const Vec3 flp = {0.0, 0.0, 0.0};
    const double span = 10.0;  // far end at x = 10

    GeomRuleInput in;
    in.rule = GeomRule::RotateAboutLocatorPoint;
    in.magnitude = 0.5;        // zone displacement at the far end
    in.dir = {0.0, 0.0, 1.0};  // zone normal
    in.featureLocatorPoint = flp;
    in.referenceSpan = span;

    // At the FLP itself -> ~0 offset.
    in.target = flp;
    Vec3 atFlp = applyGeomRule(in);
    dvatest::checkNear(vlen(atFlp), 0.0, 1e-12, "rotate at FLP is zero");

    // Halfway out -> half the far-end displacement.
    in.target = {5.0, 0.0, 0.0};
    Vec3 mid = applyGeomRule(in);
    dvatest::checkNear(mid.z, 0.25, 1e-9, "rotate halfway = half magnitude");

    // Far end -> full magnitude, along the zone normal.
    in.target = {10.0, 0.0, 0.0};
    Vec3 far = applyGeomRule(in);
    dvatest::checkNear(far.z, 0.5, 1e-9, "rotate far end = full magnitude");

    // Monotone with distance.
    dvatest::check(vlen(far) > vlen(mid) && vlen(mid) > vlen(atFlp),
                   "rotate offset grows with distance from FLP");
}

TEST("geomrule_rotate_locator_handles_huge_finite_arm") {
    GeomRuleInput in;
    in.rule = GeomRule::RotateAboutLocatorPoint;
    in.magnitude = 0.5;
    in.dir = {0.0, 0.0, 1.0};
    in.featureLocatorPoint = {0.0, 0.0, 0.0};
    in.referenceSpan = 1e308;
    in.target = {1e308, 0.0, 0.0};

    const Vec3 off = applyGeomRule(in);
    dvatest::check(std::isfinite(off.z), "huge rotate offset remains finite");
    dvatest::checkNear(off.x, 0.0, 0.0, "huge rotate no x");
    dvatest::checkNear(off.y, 0.0, 0.0, "huge rotate no y");
    dvatest::checkNear(off.z, 0.5, 1e-12,
                       "huge rotate far end reaches full magnitude");
}

// NodeNormalOffset: each node moves along its own normal by the magnitude.
TEST("geomrule_node_normal") {
    GeomRuleInput in;
    in.rule = GeomRule::NodeNormalOffset;
    in.magnitude = -1.5;
    in.dir = {0.0, 1.0, 0.0};
    in.target = {3.0, 3.0, 3.0};
    Vec3 off = applyGeomRule(in);
    dvatest::checkNear(off.y, -1.5, 1e-12, "node normal offset along normal");
    dvatest::checkNear(off.x, 0.0, 1e-12, "node normal no x");
    dvatest::checkNear(off.z, 0.0, 1e-12, "node normal no z");
}

// SectionRadialOffset: offset is radial (point -> axis perpendicular), axis
// stays collinear. Axis = Z through origin; target at (2,0,3) -> radial +X.
TEST("geomrule_section_radial") {
    GeomRuleInput in;
    in.rule = GeomRule::SectionRadialOffset;
    in.magnitude = 0.4;
    in.axis = {0.0, 0.0, 1.0};
    in.featureLocatorPoint = {0.0, 0.0, 0.0};  // axis point
    in.target = {2.0, 0.0, 3.0};               // radius 2 in +X, any Z
    Vec3 off = applyGeomRule(in);
    dvatest::checkNear(off.x, 0.4, 1e-12, "radial offset along +X");
    dvatest::checkNear(off.y, 0.0, 1e-12, "radial no y");
    dvatest::checkNear(off.z, 0.0, 1e-12, "radial has no axial component");

    // A point on the axis has no defined radial direction -> zero offset.
    in.target = {0.0, 0.0, 5.0};
    Vec3 onAxis = applyGeomRule(in);
    dvatest::checkNear(vlen(onAxis), 0.0, 1e-12, "on-axis point gets no radial offset");
}

// DiameterScale: radial offset scales with the point's radius / nominal.
TEST("geomrule_diameter_scale") {
    GeomRuleInput in;
    in.rule = GeomRule::DiameterScale;
    in.magnitude = 0.2;
    in.axis = {0.0, 0.0, 1.0};
    in.featureLocatorPoint = {0.0, 0.0, 0.0};
    in.nominalSize = 4.0;          // nominal radius
    in.target = {4.0, 0.0, 0.0};   // at r = nominal -> offset = magnitude
    Vec3 atNominal = applyGeomRule(in);
    dvatest::checkNear(atNominal.x, 0.2, 1e-12, "diameter scale at r=nominal -> magnitude");

    in.target = {2.0, 0.0, 0.0};   // half radius -> half offset
    Vec3 half = applyGeomRule(in);
    dvatest::checkNear(half.x, 0.1, 1e-12, "diameter scale at r=nominal/2 -> half");
}

// rssCombine: total = 1, one refine = 0.6 -> location = sqrt(1 - 0.36) = 0.8.
TEST("rss_combine_basic") {
    double loc = rssCombine(1.0, {0.6});
    dvatest::checkNear(loc, 0.8, 1e-12, "rss location = 0.8");
}

TEST("rss_combine_handles_huge_finite_ranges") {
    const double loc = rssCombine(1e308, {6e307});
    dvatest::check(std::isfinite(loc), "huge rss location remains finite");
    dvatest::checkNear(loc / 1e307, 8.0, 1e-9,
                       "huge rss location = sqrt(100 - 36) * 1e307");
}

// rssCombine over multiple refinements: sqrt(total^2 - sum refine^2).
TEST("rss_combine_multi") {
    double loc = rssCombine(1.0, {0.6, 0.0});           // extra zero refine
    dvatest::checkNear(loc, 0.8, 1e-12, "rss with zero refine unchanged");
    double loc2 = rssCombine(13.0, {3.0, 4.0});         // sqrt(169-9-16)=12
    dvatest::checkNear(loc2, 12.0, 1e-9, "rss multi refine");
}

// rssCombine clamps at zero when refinements over-subscribe the total.
TEST("rss_combine_oversubscribed") {
    double loc = rssCombine(0.5, {0.6});
    dvatest::checkNear(loc, 0.0, 1e-12, "rss clamps to 0 when over-subscribed");
}

// applyBonus: bonus = |actual - mmc|. actual 8.25, mmc 7.5 -> 0.75; range grows.
TEST("bonus_basic") {
    dvatest::checkNear(bonus(8.25, 7.5), 0.75, 1e-12, "bonus magnitude = 0.75");
    double expanded = applyBonus(1.0, 8.25, 7.5);
    dvatest::checkNear(expanded, 1.75, 1e-12, "expanded range = base + bonus");
    dvatest::check(expanded > 1.0, "bonus expands the range");
}

// Triangular2D radial: cone density over disc R = range/2. Radius in [0,R],
// mean = (1/2) R for f(r) ∝ r(R-r) (E[r] = R/2). Reaches near the rim.
TEST("dist_triangular2d") {
    const double range = 2.0, R = range / 2.0;
    auto dist = DistributionFactory::create(DistributionType::Triangular2D);
    Mt19937Rng rng(2024);
    const int N = 100000;
    double sum = 0.0, mx = 0.0, mn = R;
    for (int i = 0; i < N; ++i) {
        double r = dist->sample(range, 0.0, 3.0, rng);
        sum += r;
        mx = std::max(mx, r);
        mn = std::min(mn, r);
    }
    double mean = sum / N;
    dvatest::check(mn >= 0.0, "triangular2d radius non-negative");
    dvatest::check(mx <= R + 1e-9, "triangular2d radius within disc");
    dvatest::checkNear(mean, R / 2.0, 0.02, "triangular2d mean = R/2");
    dvatest::check(mx > 0.9 * R, "triangular2d should reach near the rim");
}

// Trapezoid2D radial: truncated cone. Radius bounded by R, non-negative, mean
// between the area-uniform disc (2R/3) and the full cone (R/2).
TEST("dist_trapezoid2d") {
    const double range = 2.0, R = range / 2.0;
    auto dist = DistributionFactory::create(DistributionType::Trapezoid2D);
    Mt19937Rng rng(777);
    const int N = 100000;
    double sum = 0.0, mx = 0.0, mn = R;
    for (int i = 0; i < N; ++i) {
        double r = dist->sample(range, 0.0, 3.0, rng);
        sum += r;
        mx = std::max(mx, r);
        mn = std::min(mn, r);
    }
    double mean = sum / N;
    dvatest::check(mn >= 0.0, "trapezoid2d radius non-negative");
    dvatest::check(mx <= R + 1e-9, "trapezoid2d radius within disc");
    // Flat core + ramp shoulder -> mean strictly between cone and uniform disc.
    dvatest::check(mean > R / 2.0 - 0.02, "trapezoid2d mean above cone");
    dvatest::check(mean < 2.0 * R / 3.0 + 0.02, "trapezoid2d mean below uniform disc");
    dvatest::check(mx > 0.9 * R, "trapezoid2d should reach near the rim");
}

TEST("dist_trapezoid2d_keeps_huge_finite_radius") {
    auto dist = DistributionFactory::create(DistributionType::Trapezoid2D);
    Mt19937Rng rng(777);
    const double R = 5e307;
    const int N = 1000;
    double sumUnit = 0.0;
    for (int i = 0; i < N; ++i) {
        const double r = dist->sample(1e308, 0.0, 3.0, rng);
        dvatest::check(std::isfinite(r), "huge trapezoid2d radius remains finite");
        dvatest::check(r >= 0.0, "huge trapezoid2d radius remains non-negative");
        dvatest::check(r <= R, "huge trapezoid2d radius stays inside disc");
        sumUnit += r / R;
    }
    const double meanUnit = sumUnit / static_cast<double>(N);
    dvatest::check(meanUnit > 0.5 - 0.02, "huge trapezoid2d mean above cone");
    dvatest::check(meanUnit < 2.0 / 3.0 + 0.02, "huge trapezoid2d mean below uniform disc");
}

// UserDefined: bootstrap-replays injected samples, rescaled onto {range,offset}.
// Samples drawn from [10,20] map onto [offset-range/2, offset+range/2]; the
// empirical mean should track the input mean position within the band.
TEST("dist_userdefined_injected") {
    std::vector<double> samples;
    for (int i = 0; i <= 100; ++i) samples.push_back(10.0 + 0.1 * i);  // 10..20 uniform
    auto dist = makeUserDefinedDistribution(samples);
    Mt19937Rng rng(99);
    const int N = 100000;
    const double range = 4.0, offset = 5.0;  // band [3,7]
    double sum = 0.0, mx = -1e9, mn = 1e9;
    for (int i = 0; i < N; ++i) {
        double x = dist->sample(range, offset, 3.0, rng);
        sum += x;
        mx = std::max(mx, x);
        mn = std::min(mn, x);
    }
    double mean = sum / N;
    dvatest::check(mn >= offset - range / 2.0 - 1e-9, "userdefined within lower bound");
    dvatest::check(mx <= offset + range / 2.0 + 1e-9, "userdefined within upper bound");
    // Uniform 10..20 -> centred sample -> mean near band centre (offset).
    dvatest::checkNear(mean, offset, 0.05, "userdefined mean tracks band centre");
}

// UserDefined with no samples degrades to Normal (mean = offset).
TEST("dist_userdefined_empty_fallback") {
    auto dist = makeUserDefinedDistribution({});
    Mt19937Rng rng(3);
    const int N = 100000;
    double sum = 0.0;
    for (int i = 0; i < N; ++i) sum += dist->sample(2.0, 5.0, 3.0, rng);
    dvatest::checkNear(sum / N, 5.0, 0.02, "empty userdefined falls back to Normal mean");
}

TEST("tolerance_sampler_userdefined_uses_smp_path") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "opendva_sampler_userdefined.smp";
    {
        std::ofstream out(path);
        out << "7\n7\n7\n";
    }

    ToleranceIR tol;
    RandSpec rand;
    rand.distribution = DistributionType::UserDefined;
    rand.range = 10.0;
    rand.offset = 42.0;
    rand.sigmaNum = 3.0;
    rand.userDefinedSamplePath = path.string();
    tol.rands.push_back(rand);

    auto sampler = makeReferenceToleranceSampler();
    Mt19937Rng rng(123);
    for (int i = 0; i < 20; ++i) {
        const double value = sampler->applyDeviation(tol, MeshHandle{}, rng);
        dvatest::checkNear(value, 42.0, 1e-12,
                           "sampler uses .SMP-backed UserDefined distribution");
    }
    std::remove(path.string().c_str());
}
