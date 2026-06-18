// A4 distribution library (README §5.5). Reference implementations of the
// 1D distributions used by Monte Carlo sampling and the radial 2D draws.
// sigmaNum relates range to sigma: sigma = range / (2 * sigmaNum)
// (README: SigmaNum = (Max-Min)/6, i.e. half-range / sigmaNum = sigma).
//
// All bounded 1D distributions draw onto [offset - range/2, offset + range/2].
// Sampling uses only IRng::uniform01() / IRng::normal01(); the shapes are built
// from inverse-transform, rejection, and composition methods so the library has
// no external statistical dependency.
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "opendva/IToleranceSampler.h"
#include "opendva/tolerance/Distributions.h"

namespace opendva {
namespace {

constexpr double kPi = 3.14159265358979323846;

// Clamp a value into the symmetric range about offset.
double clampToRange(double value, double offset, double range) {
    const double half = range / 2.0;
    return std::min(std::max(value, offset - half), offset + half);
}

bool isDigit(char ch) {
    return ch >= '0' && ch <= '9';
}

bool isDecimalFloatLiteral(const std::string& text) {
    std::size_t pos = 0;
    if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) {
        ++pos;
    }

    bool sawDigit = false;
    while (pos < text.size() && isDigit(text[pos])) {
        sawDigit = true;
        ++pos;
    }

    if (pos < text.size() && text[pos] == '.') {
        ++pos;
        while (pos < text.size() && isDigit(text[pos])) {
            sawDigit = true;
            ++pos;
        }
    }

    if (!sawDigit) {
        return false;
    }

    if (pos < text.size() && (text[pos] == 'e' || text[pos] == 'E')) {
        ++pos;
        if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) {
            ++pos;
        }
        const std::size_t exponentStart = pos;
        while (pos < text.size() && isDigit(text[pos])) {
            ++pos;
        }
        if (pos == exponentStart) {
            return false;
        }
    }

    return pos == text.size();
}

// --- Symmetric base shapes ------------------------------------------------

class NormalDist final : public IDistribution {
public:
    double sample(double range, double offset, double sigmaNum, IRng& rng) override {
        const double sigma = sigmaNum > 0 ? range / (2.0 * sigmaNum) : 0.0;
        return offset + sigma * rng.normal01();
    }
};

class UniformDist final : public IDistribution {
public:
    double sample(double range, double offset, double /*sigmaNum*/, IRng& rng) override {
        // Uniform over [offset - range/2, offset + range/2].
        return offset + (rng.uniform01() - 0.5) * range;
    }
};

class TriangularDist final : public IDistribution {
public:
    double sample(double range, double offset, double /*sigmaNum*/, IRng& rng) override {
        // Symmetric triangular: sum of two uniforms.
        const double u = (rng.uniform01() + rng.uniform01()) - 1.0;  // [-1,1], triangular
        return offset + u * (range / 2.0);
    }
};

class ConstantDist final : public IDistribution {
public:
    double sample(double /*range*/, double offset, double /*sigmaNum*/, IRng& /*rng*/) override {
        return offset;
    }
};

// --- Multi-modal / shaped distributions -----------------------------------

// BiMode: two clusters near the edges of the range (a "double hump"). Models a
// process with two competing setups. Implemented as a 50/50 mixture of two
// normals centred at +-half/2 with a narrow spread, clamped into range.
class BiModeDist final : public IDistribution {
public:
    double sample(double range, double offset, double sigmaNum, IRng& rng) override {
        const double half = range / 2.0;
        const double sigma = (sigmaNum > 0 ? range / (2.0 * sigmaNum) : 0.0) * 0.5;
        const double center = (rng.uniform01() < 0.5) ? -half / 2.0 : half / 2.0;
        const double v = offset + center + sigma * rng.normal01();
        return clampToRange(v, offset, range);
    }
};

// RightSkew: peak shifted toward the low end with a tail toward the high end
// (positive skew). Built from a Weibull-like inverse transform mapped into the
// range. Mean lies above the mode but stays inside the range.
class RightSkewDist final : public IDistribution {
public:
    double sample(double range, double offset, double /*sigmaNum*/, IRng& rng) override {
        const double u = std::max(rng.uniform01(), 1e-12);
        // x in [0,1) with positive skew: -ln(u) compressed and normalised.
        const double k = 1.6;  // shape: controls skew strength
        double x = std::pow(-std::log(u), 1.0 / k);  // Weibull(shape=k), mean ~ O(1)
        x = std::min(x / 3.0, 1.0);                   // map bulk into [0,1], clip tail
        return offset - range / 2.0 + x * range;
    }
};

// LeftSkew: mirror of RightSkew (negative skew, tail toward the low end).
class LeftSkewDist final : public IDistribution {
public:
    double sample(double range, double offset, double /*sigmaNum*/, IRng& rng) override {
        const double u = std::max(rng.uniform01(), 1e-12);
        const double k = 1.6;
        double x = std::pow(-std::log(u), 1.0 / k);
        x = std::min(x / 3.0, 1.0);
        return offset + range / 2.0 - x * range;  // mirror about the high edge
    }
};

// OpenUp: U-shaped (bathtub) density - mass concentrated at the two edges,
// thin in the middle. The arcsine distribution has exactly this shape:
// x = (1 - cos(pi*u)) / 2 maps a uniform into an arcsine on [0,1].
class OpenUpDist final : public IDistribution {
public:
    double sample(double range, double offset, double /*sigmaNum*/, IRng& rng) override {
        const double x = (1.0 - std::cos(kPi * rng.uniform01())) / 2.0;  // arcsine [0,1]
        return offset - range / 2.0 + x * range;
    }
};

// OpenDown: inverted-U (single broad central hump that falls to zero at the
// edges) - the complement of OpenUp. A scaled Beta(2,2) gives this profile;
// here built as the mean of two uniforms (triangular) softened toward the
// parabolic shape via a third uniform average.
class OpenDownDist final : public IDistribution {
public:
    double sample(double range, double offset, double /*sigmaNum*/, IRng& rng) override {
        // Beta(2,2)-like central hump on [0,1] via order-statistic median of 3.
        const double a = rng.uniform01();
        const double b = rng.uniform01();
        const double c = rng.uniform01();
        const double x = std::max(std::min(a, b),
                                  std::min(std::max(a, b), c));  // median of 3
        return offset - range / 2.0 + x * range;
    }
};

// Step: models a mean shift between two production lots. Pick one of two
// uniform sub-bands offset to either side of centre, each spanning a quarter
// of the range. Produces a flat two-step histogram.
class StepDist final : public IDistribution {
public:
    double sample(double range, double offset, double /*sigmaNum*/, IRng& rng) override {
        const double quarter = range / 4.0;
        const double shift = (rng.uniform01() < 0.5) ? -quarter : quarter;
        const double within = (rng.uniform01() - 0.5) * (range / 2.0);  // band width = half
        return clampToRange(offset + shift + within, offset, range);
    }
};

// Weibull4: four-parameter Weibull (location, scale, shape, range), positively
// skewed life-data shape. Drawn by inverse transform x = (-ln u)^(1/k), scaled
// so the bulk lands inside the range; the long tail is clamped to the edge.
class Weibull4Dist final : public IDistribution {
public:
    double sample(double range, double offset, double /*sigmaNum*/, IRng& rng) override {
        const double u = std::max(rng.uniform01(), 1e-12);
        const double k = 2.0;  // shape ~ 2 (Rayleigh-like), gentle right skew
        double x = std::pow(-std::log(u), 1.0 / k);  // mean ~ 0.886
        x = std::min(x / 2.5, 1.0);                   // normalise bulk into [0,1]
        return offset - range / 2.0 + x * range;
    }
};

// Pearson4: the Pearson type IV family - a skewed, heavy-ish unimodal curve.
// Approximated here by a normal core warped by a cubic skew term, then clamped
// into the range. Captures the asymmetric, peaked character without the full
// Pearson normalisation integral.
class Pearson4Dist final : public IDistribution {
public:
    double sample(double range, double offset, double sigmaNum, IRng& rng) override {
        const double sigma = sigmaNum > 0 ? range / (2.0 * sigmaNum) : 0.0;
        const double z = rng.normal01();
        const double skew = 0.25;                       // mild positive skew
        const double warped = z + skew * (z * z - 1.0);  // skew-normal-like warp
        return clampToRange(offset + sigma * warped, offset, range);
    }
};

// Modal: a single sharp central spike - tighter than Normal, mass piled at the
// centre. Implemented as a Normal whose sigma is halved so the peak is more
// pronounced while staying within range.
class ModalDist final : public IDistribution {
public:
    double sample(double range, double offset, double sigmaNum, IRng& rng) override {
        const double sigma = (sigmaNum > 0 ? range / (2.0 * sigmaNum) : 0.0) * 0.5;
        return clampToRange(offset + sigma * rng.normal01(), offset, range);
    }
};

// Trapezoid: flat top with linear ramps at both ends - between Uniform and
// Triangular. The sum of two uniforms of unequal half-width yields a true
// trapezoid; widths 0.7h and 0.3h sum to the half-range h so the support is
// exactly the range while the variance falls between Uniform and Triangular.
class TrapezoidDist final : public IDistribution {
public:
    double sample(double range, double offset, double /*sigmaNum*/, IRng& rng) override {
        const double half = range / 2.0;
        const double a = (rng.uniform01() - 0.5) * (2.0 * 0.7 * half);  // U(-0.7h, 0.7h)
        const double b = (rng.uniform01() - 0.5) * (2.0 * 0.3 * half);  // U(-0.3h, 0.3h)
        return offset + a + b;  // trapezoid on [-h, h]
    }
};

// PowerFunction: density proportional to a power of the position - rises (or
// falls) monotonically across the range. Inverse transform x = u^(1/p) gives a
// Beta(p,1) shape on [0,1] with the mass piled toward the high edge for p>1.
class PowerFunctionDist final : public IDistribution {
public:
    double sample(double range, double offset, double /*sigmaNum*/, IRng& rng) override {
        const double p = 2.0;  // power exponent: density ~ x^(p-1)
        const double x = std::pow(rng.uniform01(), 1.0 / p);  // Beta(p,1) on [0,1]
        return offset - range / 2.0 + x * range;
    }
};

// --- 2D radial distributions ----------------------------------------------
// 2D distributions model circular/diameter bands with no offset; sample()
// returns the radial magnitude (always >= 0). The maximum radius is range/2.

// Normal2D: bivariate normal -> the radius follows a Rayleigh distribution
// (README §5.5: Rayleigh is the internal radial mechanism, x/y both normal).
// Drawn directly from two independent normal01 components.
class Normal2DDist final : public IDistribution {
public:
    double sample(double range, double /*offset*/, double sigmaNum, IRng& rng) override {
        const double sigma = sigmaNum > 0 ? range / (2.0 * sigmaNum) : 0.0;
        const double x = sigma * rng.normal01();
        const double y = sigma * rng.normal01();
        return std::hypot(x, y);  // Rayleigh radius
    }
};

// Uniform2D: points spread uniformly over a disc of radius range/2 ("hockey
// puck"). For an areal-uniform disc the radius CDF is r^2, so r = R*sqrt(u).
class Uniform2DDist final : public IDistribution {
public:
    double sample(double range, double /*offset*/, double /*sigmaNum*/, IRng& rng) override {
        const double radius = range / 2.0;
        return radius * std::sqrt(rng.uniform01());  // area-uniform radial draw
    }
};

// Triangular2D: "cone" density over a disc of radius R = range/2 — areal density
// falls linearly from the center to zero at the rim. The radius then has pdf
// f(r) ∝ r*(R - r); its CDF is monotone, so an inverse-transform on the cubic
// F(r) = 3(r/R)^2 - 2(r/R)^3 maps a uniform u into a radius. Solving the cubic
// is avoided with a Newton step seeded by sqrt(u) (exact at the endpoints,
// converges in a couple of iterations across [0,1]).
class Triangular2DDist final : public IDistribution {
public:
    double sample(double range, double /*offset*/, double /*sigmaNum*/, IRng& rng) override {
        const double R = range / 2.0;
        const double u = rng.uniform01();
        // Solve 3 t^2 - 2 t^3 = u for t = r/R in [0,1] (smoothstep inverse).
        double t = std::sqrt(u);  // good initial guess
        for (int i = 0; i < 6; ++i) {
            const double f = 3.0 * t * t - 2.0 * t * t * t - u;
            const double df = 6.0 * t - 6.0 * t * t;  // 6 t (1 - t)
            if (std::fabs(df) < 1e-12) break;
            t -= f / df;
            t = std::min(std::max(t, 0.0), 1.0);
        }
        return R * t;
    }
};

// Trapezoid2D: "truncated cone" — a flat-topped radial ramp set by a single
// B-Ratio b in (0,1]: the areal density is uniform for r <= b*R and ramps down
// linearly to zero at r = R. Sampled by composition: pick the flat core vs the
// ramp shoulder by their area weights, then draw within the chosen region
// (area-uniform core, cone shoulder).
class Trapezoid2DDist final : public IDistribution {
public:
    double sample(double range, double /*offset*/, double /*sigmaNum*/, IRng& rng) override {
        const double R = range / 2.0;
        const double b = 0.6;            // B-Ratio: flat-top fraction of the radius
        // Unnormalised areal mass of the flat core (uniform density 1 over a
        // disc of radius b*R) and of the linear ramp shoulder. Work in unit
        // radius so huge finite ranges preserve the intended mixture weights.
        const double coreMass = b * b;                         // ∝ area of core
        // Shoulder: density ramps 1 -> 0 over [rb, R]; mass = ∫ 2πr·w(r) dr with
        // w(r) = (R - r)/(R - rb). Compute the relative shoulder mass.
        const double span = 1.0 - b;
        const double shoulderMass =
            span > 1e-12
                ? ((1.0 - b * b) - (2.0 / 3.0) * (1.0 - b * b * b)) / span
                : 0.0;
        const double total = coreMass + shoulderMass;
        const double pick = rng.uniform01() * (total > 0 ? total : 1.0);
        if (pick < coreMass || span <= 1e-12) {
            // Area-uniform draw inside the flat core.
            return R * b * std::sqrt(rng.uniform01());
        }
        // Cone shoulder on [rb, R]: density f(r) ∝ r*(R - r). Inverse-transform
        // via a Newton solve on the normalised CDF, seeded at the inner edge.
        const double u = rng.uniform01();
        double t = b + u * span;  // unit-radius initial guess
        for (int i = 0; i < 8; ++i) {
            // CDF on [b, 1] (unnormalised), divided by its full mass at 1.
            const double cdf =
                ((t * t - b * b) / 2.0 - (t * t * t - b * b * b) / 3.0);
            const double full =
                ((1.0 - b * b) / 2.0 - (1.0 - b * b * b) / 3.0);
            const double f = (full > 1e-12 ? cdf / full : 0.0) - u;
            const double pdf = (full > 1e-12 ? (t * (1.0 - t)) / full : 0.0);
            if (std::fabs(pdf) < 1e-12) break;
            t -= f / pdf;
            t = std::min(std::max(t, b), 1.0);
        }
        return R * t;
    }
};

// UserDefined: replay user-supplied empirical samples (README §5.5 "User
// Defined external .SMP"). The IDistribution::sample() signature carries no
// file argument, so samples are injected via the constructor or .SMP helper.
// Drawn by uniform resampling (bootstrap) and rescaled from the recorded
// sample range onto the requested {range, offset}. With no samples it degrades
// to Normal so the factory default stays usable.
class UserDefinedDist final : public IDistribution {
public:
    UserDefinedDist() = default;
    explicit UserDefinedDist(std::vector<double> samples) : samples_(std::move(samples)) {
        if (!samples_.empty()) {
            sampleMin_ = sampleMax_ = samples_.front();
            for (double s : samples_) {
                sampleMin_ = std::min(sampleMin_, s);
                sampleMax_ = std::max(sampleMax_, s);
            }
        }
    }

    double sample(double range, double offset, double sigmaNum, IRng& rng) override {
        if (samples_.empty()) {
            // Fallback: behave as Normal until real samples are injected.
            const double sigma = sigmaNum > 0 ? range / (2.0 * sigmaNum) : 0.0;
            return offset + sigma * rng.normal01();
        }
        const std::size_t i =
            std::min(samples_.size() - 1,
                     static_cast<std::size_t>(rng.uniform01() * samples_.size()));
        const double raw = samples_[i];
        const double span = sampleMax_ - sampleMin_;
        if (span < 1e-12) return offset;  // all samples identical
        // Map the empirical value onto [offset - range/2, offset + range/2].
        const double t = (raw - sampleMin_) / span;  // [0,1]
        return offset - range / 2.0 + t * range;
    }

private:
    std::vector<double> samples_;
    double sampleMin_{0.0};
    double sampleMax_{0.0};
};

}  // namespace

namespace tolerance {

std::unique_ptr<IDistribution> makeUserDefinedDistribution(std::vector<double> samples) {
    return std::make_unique<UserDefinedDist>(std::move(samples));
}

std::unique_ptr<IDistribution> makeUserDefinedDistributionFromSmp(
    const std::string& path) {
    return makeUserDefinedDistribution(loadSmpSamples(path));
}

void stripUtf8Bom(std::string& line) {
    constexpr char bom[] = "\xEF\xBB\xBF";
    if (line.rfind(bom, 0) == 0) {
        line.erase(0, 3);
    }
}

std::vector<double> loadSmpSamples(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::invalid_argument("Could not open UserDefined .SMP file: " + path);
    }
    std::vector<double> samples;
    std::string line;
    bool firstLine = true;
    while (std::getline(in, line)) {
        if (firstLine) {
            stripUtf8Bom(line);
            firstLine = false;
        }
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        std::replace(line.begin(), line.end(), ',', ' ');

        std::size_t pos = 0;
        while (pos < line.size()) {
            while (pos < line.size() &&
                   (line[pos] == ' ' || line[pos] == '\t' || line[pos] == '\r')) {
                ++pos;
            }
            if (pos >= line.size()) {
                break;
            }

            std::size_t tokenEnd = pos;
            while (tokenEnd < line.size() &&
                   line[tokenEnd] != ' ' && line[tokenEnd] != '\t' &&
                   line[tokenEnd] != '\r') {
                ++tokenEnd;
            }

            const std::string token = line.substr(pos, tokenEnd - pos);
            if (isDecimalFloatLiteral(token)) {
                char* end = nullptr;
                const double value = std::strtod(token.c_str(), &end);
                if (end != token.c_str() && *end == '\0' && std::isfinite(value)) {
                    samples.push_back(value);
                }
            }

            pos = tokenEnd;
        }
    }
    if (samples.empty()) {
        throw std::invalid_argument("UserDefined .SMP file contains no numeric samples: " +
                                    path);
    }
    return samples;
}

}  // namespace tolerance

std::unique_ptr<IDistribution> DistributionFactory::create(DistributionType type) {
    switch (type) {
        // Symmetric / base shapes.
        case DistributionType::Normal:        return std::make_unique<NormalDist>();
        case DistributionType::Uniform:       return std::make_unique<UniformDist>();
        case DistributionType::Triangular:    return std::make_unique<TriangularDist>();
        case DistributionType::Constant:      return std::make_unique<ConstantDist>();
        // Shaped 1D distributions.
        case DistributionType::BiMode:        return std::make_unique<BiModeDist>();
        case DistributionType::RightSkew:     return std::make_unique<RightSkewDist>();
        case DistributionType::LeftSkew:      return std::make_unique<LeftSkewDist>();
        case DistributionType::OpenUp:        return std::make_unique<OpenUpDist>();
        case DistributionType::OpenDown:      return std::make_unique<OpenDownDist>();
        case DistributionType::Step:          return std::make_unique<StepDist>();
        case DistributionType::Weibull4:      return std::make_unique<Weibull4Dist>();
        case DistributionType::Pearson4:      return std::make_unique<Pearson4Dist>();
        case DistributionType::Modal:         return std::make_unique<ModalDist>();
        case DistributionType::Trapezoid:     return std::make_unique<TrapezoidDist>();
        case DistributionType::PowerFunction: return std::make_unique<PowerFunctionDist>();
        // 2D radial distributions.
        case DistributionType::Normal2D:      return std::make_unique<Normal2DDist>();
        case DistributionType::Uniform2D:     return std::make_unique<Uniform2DDist>();
        case DistributionType::Triangular2D:  return std::make_unique<Triangular2DDist>();
        case DistributionType::Trapezoid2D:   return std::make_unique<Trapezoid2DDist>();
        // UserDefined from the factory has no samples to replay yet (the .SMP
        // loader injects them via makeUserDefinedDistribution); it falls back to
        // Normal until then.
        case DistributionType::UserDefined:   return std::make_unique<UserDefinedDist>();
        default:
            return std::make_unique<NormalDist>();
    }
}

}  // namespace opendva
