// A4 distribution tests (README §5.5). For each distribution we draw a large
// sample and check that the empirical mean, variance, and support match the
// intended shape. Tolerances are loose enough to absorb Monte Carlo noise at
// N = 100000 but tight enough to catch a wrong shape.
#include <cmath>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <vector>

#include "dva_test.h"
#include "opendva/IToleranceSampler.h"
#include "opendva/Mt19937Rng.h"
#include "opendva/tolerance/Distributions.h"

using namespace opendva;

namespace {

constexpr int kN = 100000;

struct Stats {
    double mean{0.0};
    double var{0.0};
    double min{0.0};
    double max{0.0};
};

// Draw kN samples from a distribution and summarise them.
Stats sampleStats(DistributionType type, double range, double offset, double sigmaNum) {
    auto dist = DistributionFactory::create(type);
    Mt19937Rng rng(12345);
    std::vector<double> v;
    v.reserve(kN);
    double sum = 0.0;
    for (int i = 0; i < kN; ++i) {
        const double x = dist->sample(range, offset, sigmaNum, rng);
        v.push_back(x);
        sum += x;
    }
    Stats s;
    s.mean = sum / kN;
    s.min = v.front();
    s.max = v.front();
    double sq = 0.0;
    for (double x : v) {
        const double d = x - s.mean;
        sq += d * d;
        if (x < s.min) s.min = x;
        if (x > s.max) s.max = x;
    }
    s.var = sq / kN;  // population variance (README §7: sigma uses /n)
    return s;
}

// All 1D bounded draws must stay inside [offset - range/2, offset + range/2].
void checkInRange(const Stats& s, double range, double offset, const std::string& name) {
    const double half = range / 2.0;
    const double eps = 1e-9;
    dvatest::check(s.min >= offset - half - eps, name + ": min within lower bound");
    dvatest::check(s.max <= offset + half + eps, name + ": max within upper bound");
}

}  // namespace

// Uniform: mean = offset, variance = range^2 / 12, fills the band.
TEST("dist_uniform") {
    Stats s = sampleStats(DistributionType::Uniform, 2.0, 5.0, 3.0);
    checkInRange(s, 2.0, 5.0, "uniform");
    dvatest::checkNear(s.mean, 5.0, 0.02, "uniform mean");
    dvatest::checkNear(s.var, 4.0 / 12.0, 0.02, "uniform variance");
}

// Triangular: symmetric, mean = offset, variance = range^2 / 24.
TEST("dist_triangular") {
    Stats s = sampleStats(DistributionType::Triangular, 2.0, 5.0, 3.0);
    checkInRange(s, 2.0, 5.0, "triangular");
    dvatest::checkNear(s.mean, 5.0, 0.02, "triangular mean");
    dvatest::checkNear(s.var, 4.0 / 24.0, 0.02, "triangular variance");
}

// Normal: mean = offset, sigma = range / (2*sigmaNum) -> var = (range/6)^2.
TEST("dist_normal") {
    Stats s = sampleStats(DistributionType::Normal, 2.0, 5.0, 3.0);
    const double sigma = 2.0 / 6.0;
    dvatest::checkNear(s.mean, 5.0, 0.02, "normal mean");
    dvatest::checkNear(s.var, sigma * sigma, 0.01, "normal variance");
}

// Constant: every draw equals offset; zero variance.
TEST("dist_constant") {
    Stats s = sampleStats(DistributionType::Constant, 2.0, 5.0, 3.0);
    dvatest::checkNear(s.mean, 5.0, 1e-9, "constant mean");
    dvatest::checkNear(s.var, 0.0, 1e-12, "constant variance");
}

// BiMode: symmetric two-hump -> mean = offset, variance larger than Normal.
TEST("dist_bimode") {
    Stats s = sampleStats(DistributionType::BiMode, 2.0, 5.0, 3.0);
    checkInRange(s, 2.0, 5.0, "bimode");
    dvatest::checkNear(s.mean, 5.0, 0.03, "bimode mean centred");
    dvatest::check(s.var > 0.05, "bimode should spread toward the edges");
}

// RightSkew: positive skew -> mode left of centre, mean below the midpoint.
TEST("dist_rightskew") {
    Stats s = sampleStats(DistributionType::RightSkew, 2.0, 5.0, 3.0);
    checkInRange(s, 2.0, 5.0, "rightskew");
    dvatest::check(s.mean < 5.0, "rightskew mean below centre (tail to the right)");
}

// LeftSkew: negative skew -> mean above the midpoint (mirror of RightSkew).
TEST("dist_leftskew") {
    Stats s = sampleStats(DistributionType::LeftSkew, 2.0, 5.0, 3.0);
    checkInRange(s, 2.0, 5.0, "leftskew");
    dvatest::check(s.mean > 5.0, "leftskew mean above centre (tail to the left)");
}

// OpenUp: U-shaped (arcsine) -> mean = offset, variance = range^2/8 (largest
// of the symmetric bounded shapes since mass sits at the edges).
TEST("dist_openup") {
    Stats s = sampleStats(DistributionType::OpenUp, 2.0, 5.0, 3.0);
    checkInRange(s, 2.0, 5.0, "openup");
    dvatest::checkNear(s.mean, 5.0, 0.02, "openup mean centred");
    dvatest::checkNear(s.var, 4.0 / 8.0, 0.02, "openup variance (arcsine)");
}

// OpenDown: inverted-U central hump -> mean = offset, variance = range^2/20.
TEST("dist_opendown") {
    Stats s = sampleStats(DistributionType::OpenDown, 2.0, 5.0, 3.0);
    checkInRange(s, 2.0, 5.0, "opendown");
    dvatest::checkNear(s.mean, 5.0, 0.02, "opendown mean centred");
    dvatest::checkNear(s.var, 4.0 / 20.0, 0.02, "opendown variance (Beta(2,2))");
    // Inverted-U has smaller variance than the U-shaped OpenUp.
    dvatest::check(s.var < 4.0 / 8.0, "opendown tighter than openup");
}

// Step: symmetric two-lot mean shift -> mean = offset, spreads across range.
TEST("dist_step") {
    Stats s = sampleStats(DistributionType::Step, 2.0, 5.0, 3.0);
    checkInRange(s, 2.0, 5.0, "step");
    dvatest::checkNear(s.mean, 5.0, 0.03, "step mean centred");
    dvatest::check(s.var > 0.05, "step should occupy both sub-bands");
}

// Weibull4: right-skewed life shape -> mean below the midpoint, within range.
TEST("dist_weibull4") {
    Stats s = sampleStats(DistributionType::Weibull4, 2.0, 5.0, 3.0);
    checkInRange(s, 2.0, 5.0, "weibull4");
    dvatest::check(s.mean < 5.0, "weibull4 mean below centre (right skew)");
}

// Pearson4: mildly skewed, peaked -> mean stays near offset, bounded.
TEST("dist_pearson4") {
    Stats s = sampleStats(DistributionType::Pearson4, 2.0, 5.0, 3.0);
    checkInRange(s, 2.0, 5.0, "pearson4");
    dvatest::checkNear(s.mean, 5.0, 0.06, "pearson4 mean near offset");
    dvatest::check(s.var > 0.0, "pearson4 has spread");
}

// Modal: sharp central spike -> mean = offset, variance below Normal's.
TEST("dist_modal") {
    Stats s = sampleStats(DistributionType::Modal, 2.0, 5.0, 3.0);
    checkInRange(s, 2.0, 5.0, "modal");
    const double normalVar = (2.0 / 6.0) * (2.0 / 6.0);
    dvatest::checkNear(s.mean, 5.0, 0.02, "modal mean centred");
    dvatest::check(s.var < normalVar, "modal tighter than Normal");
}

// Trapezoid: flat top with ramps -> symmetric, variance between Uniform and
// Triangular (range^2/24 < var < range^2/12).
TEST("dist_trapezoid") {
    Stats s = sampleStats(DistributionType::Trapezoid, 2.0, 5.0, 3.0);
    checkInRange(s, 2.0, 5.0, "trapezoid");
    dvatest::checkNear(s.mean, 5.0, 0.02, "trapezoid mean centred");
    dvatest::check(s.var > 4.0 / 24.0 - 0.02, "trapezoid wider than triangular");
    dvatest::check(s.var < 4.0 / 12.0 + 0.02, "trapezoid narrower than uniform");
}

// PowerFunction: Beta(2,1) -> mass toward high edge, mean = offset + range/6.
TEST("dist_powerfunction") {
    Stats s = sampleStats(DistributionType::PowerFunction, 2.0, 5.0, 3.0);
    checkInRange(s, 2.0, 5.0, "powerfunction");
    dvatest::checkNear(s.mean, 5.0 + 2.0 / 6.0, 0.03, "powerfunction mean = offset + range/6");
}

TEST("dist_userdefined_smp_loader") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "opendva_user_defined_dist.smp";
    {
        std::ofstream out(path);
        out << "# sampled production offsets\n";
        out << "-2.0, -1.0\n";
        out << "0.0 1.0\n";
        out << "bad-token 2.0 # inline comment\n";
        out << "\n";
    }

    const std::vector<double> samples =
        tolerance::loadSmpSamples(path.string());

    dvatest::check(samples.size() == 5, "smp loader keeps numeric values");
    dvatest::checkNear(samples.front(), -2.0, 1e-12, "smp loader first sample");
    dvatest::checkNear(samples.back(), 2.0, 1e-12, "smp loader last sample");
    std::remove(path.string().c_str());
}

TEST("dist_userdefined_smp_loader_ignores_non_finite_samples") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "opendva_nonfinite_user_defined.smp";
    {
        std::ofstream out(path);
        out << "-1\n";
        out << "nan inf -inf 1e309\n";
        out << "2\n";
    }

    const std::vector<double> samples =
        tolerance::loadSmpSamples(path.string());

    dvatest::check(samples.size() == 2, "smp loader skips non-finite values");
    dvatest::checkNear(samples[0], -1.0, 1e-12, "smp finite first sample");
    dvatest::checkNear(samples[1], 2.0, 1e-12, "smp finite last sample");
    std::remove(path.string().c_str());
}

TEST("dist_userdefined_smp_loader_ignores_hexadecimal_samples") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "opendva_hex_user_defined.smp";
    {
        std::ofstream out(path);
        out << "-1\n";
        out << "0x1p1\n";
        out << "2\n";
    }

    const std::vector<double> samples =
        tolerance::loadSmpSamples(path.string());

    dvatest::check(samples.size() == 2, "smp loader skips hexadecimal values");
    dvatest::checkNear(samples[0], -1.0, 1e-12, "smp decimal first sample");
    dvatest::checkNear(samples[1], 2.0, 1e-12, "smp decimal last sample");
    std::remove(path.string().c_str());
}

TEST("dist_userdefined_smp_loader_keeps_bom_prefixed_first_sample") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "opendva_bom_user_defined.smp";
    {
        std::ofstream out(path, std::ios::binary);
        out << "\xEF\xBB\xBF";
        out << "-1, 2\n";
    }

    const std::vector<double> samples =
        tolerance::loadSmpSamples(path.string());

    dvatest::check(samples.size() == 2, "smp loader keeps BOM-prefixed sample");
    dvatest::checkNear(samples[0], -1.0, 1e-12, "smp BOM first sample");
    dvatest::checkNear(samples[1], 2.0, 1e-12, "smp BOM second sample");
    std::remove(path.string().c_str());
}

TEST("dist_userdefined_distribution_from_smp") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "opendva_user_defined_factory.smp";
    {
        std::ofstream out(path);
        out << "10\n";
        out << "20\n";
        out << "30\n";
    }

    auto dist = tolerance::makeUserDefinedDistributionFromSmp(path.string());
    Mt19937Rng rng(24680);
    bool sawLow = false;
    bool sawHigh = false;
    for (int i = 0; i < 200; ++i) {
        const double value = dist->sample(4.0, 100.0, 3.0, rng);
        dvatest::check(value >= 98.0 - 1e-12, "smp distribution lower bound");
        dvatest::check(value <= 102.0 + 1e-12, "smp distribution upper bound");
        sawLow = sawLow || value < 99.0;
        sawHigh = sawHigh || value > 101.0;
    }
    dvatest::check(sawLow, "smp distribution samples low empirical value");
    dvatest::check(sawHigh, "smp distribution samples high empirical value");
    std::remove(path.string().c_str());
}

TEST("dist_userdefined_smp_loader_rejects_missing_file") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "opendva_missing_user_defined.smp";
    std::error_code ec;
    std::filesystem::remove(path, ec);

    bool threw = false;
    try {
        (void)tolerance::loadSmpSamples(path.string());
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    dvatest::check(threw, "missing .SMP file is rejected");
}

TEST("dist_userdefined_smp_loader_rejects_empty_numeric_content") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "opendva_empty_user_defined.smp";
    {
        std::ofstream out(path);
        out << "# comments only\n";
        out << "not-a-number , nope\n";
    }

    bool threw = false;
    try {
        (void)tolerance::loadSmpSamples(path.string());
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    dvatest::check(threw, ".SMP file with no numeric samples is rejected");
    std::remove(path.string().c_str());
}

// Normal2D: Rayleigh radius -> non-negative, mean = sigma*sqrt(pi/2),
// variance = sigma^2 * (4 - pi)/2, with sigma = range/(2*sigmaNum).
TEST("dist_normal2d") {
    const double range = 2.0, sigmaNum = 3.0;
    Stats s = sampleStats(DistributionType::Normal2D, range, 0.0, sigmaNum);
    const double sigma = range / (2.0 * sigmaNum);
    const double pi = 3.14159265358979323846;
    dvatest::check(s.min >= 0.0, "normal2d radius non-negative");
    dvatest::checkNear(s.mean, sigma * std::sqrt(pi / 2.0), 0.01, "normal2d Rayleigh mean");
    dvatest::checkNear(s.var, sigma * sigma * (4.0 - pi) / 2.0, 0.01, "normal2d Rayleigh variance");
}

TEST("dist_normal2d_keeps_huge_finite_radius") {
    auto dist = DistributionFactory::create(DistributionType::Normal2D);
    Mt19937Rng rng(12345);
    for (int i = 0; i < 32; ++i) {
        const double r = dist->sample(1e308, 0.0, 3.0, rng);
        dvatest::check(std::isfinite(r), "huge normal2d radius remains finite");
        dvatest::check(r >= 0.0, "huge normal2d radius remains non-negative");
    }
}

// Uniform2D: area-uniform disc of radius range/2 -> radius in [0, R],
// mean = (2/3) R = range/3, max approaches R.
TEST("dist_uniform2d") {
    const double range = 2.0;
    Stats s = sampleStats(DistributionType::Uniform2D, range, 0.0, 3.0);
    const double R = range / 2.0;
    dvatest::check(s.min >= 0.0, "uniform2d radius non-negative");
    dvatest::check(s.max <= R + 1e-9, "uniform2d radius within disc");
    dvatest::checkNear(s.mean, 2.0 * R / 3.0, 0.02, "uniform2d mean = 2R/3");
    dvatest::check(s.max > 0.95 * R, "uniform2d should reach near the rim");
}
