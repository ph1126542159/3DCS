// A6 worst-case synthesis & convergence helpers (README §7.3 / §7.4).
#include "opendva/sim/WorstCase.h"

#include <cmath>
#include <limits>
#include <unordered_map>

#include "opendva/domain/Model.h"
#include "opendva/sim/SimulationEngine.h"

namespace opendva {
namespace {

// Full High-Low span of a tolerance (= Max - Min), scaled by rangeScale. This is
// the "Range" used by the worst-case arithmetic sum (README §7.4). Mirrors the
// Monte Carlo / Contributor convention: the first Rand drives the geometry.
double toleranceRange(const ToleranceDef& tol) {
    if (tol.ir.rands.empty()) return 0.0;
    return tol.ir.rands.front().range * tol.ir.rangeScale;
}

}  // namespace

std::map<MeasureId, double> computeWorstCase(const Model& model) {
    // Reuse the published GeoFactor sensitivity so the worst case shares exactly
    // the same High-Low geometry as the contributor analysis.
    auto engine = makeMonteCarloEngine();
    const SensitivityMatrix sm = engine->runGeoFactor(model);

    // Map contributor id -> tolerance range for quick lookup.
    std::unordered_map<ToleranceId, double> rangeOf;
    for (const auto* tol : model.activeTolerances()) {
        rangeOf[tol->id] = toleranceRange(*tol);
    }

    std::map<MeasureId, double> out;
    for (std::size_t j = 0; j < sm.measures.size(); ++j) {
        double wc = 0.0;
        for (std::size_t i = 0; i < sm.contributors.size(); ++i) {
            const double range = rangeOf.count(sm.contributors[i])
                                     ? rangeOf[sm.contributors[i]]
                                     : 0.0;
            // Contributor WC Range = Range * GeoFactor; arithmetic sum of the
            // magnitudes (opposing signs widen, never cancel).
            wc += std::fabs(range * sm.geoFactor[i][j]);
        }
        out[sm.measures[j]] = wc;
    }
    return out;
}

double invNormalCdf(double p) {
    // Acklam's rational approximation (max relative error ~1.15e-9), good enough
    // for the convergence sample-size estimate. Clamps the open interval ends.
    if (p <= 0.0) return -INFINITY;
    if (p >= 1.0) return INFINITY;

    // Coefficients for the central and tail regions.
    static const double a[6] = {-3.969683028665376e+01, 2.209460984245205e+02,
                                -2.759285104469687e+02, 1.383577518672690e+02,
                                -3.066479806614716e+01, 2.506628277459239e+00};
    static const double b[5] = {-5.447609879822406e+01, 1.615858368580409e+02,
                                -1.556989798598866e+02, 6.680131188771972e+01,
                                -1.328068155288572e+01};
    static const double c[6] = {-7.784894002430293e-03, -3.223964580411365e-01,
                                -2.400758277161838e+00, -2.549732539343734e+00,
                                4.374664141464968e+00,  2.938163982698783e+00};
    static const double d[4] = {7.784695709041462e-03, 3.224671290700398e-01,
                                2.445134137142996e+00, 3.754408661907416e+00};

    const double pLow = 0.02425;
    const double pHigh = 1.0 - pLow;
    double x;
    if (p < pLow) {
        const double q = std::sqrt(-2.0 * std::log(p));
        x = (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
            ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0);
    } else if (p <= pHigh) {
        const double q = p - 0.5;
        const double r = q * q;
        x = (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) *
            q /
            (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1.0);
    } else {
        const double q = std::sqrt(-2.0 * std::log(1.0 - p));
        x = -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
            ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0);
    }
    return x;
}

int requiredRuns(double percentError, double confidence) {
    if (percentError <= 0.0) return 0;
    if (!std::isfinite(percentError) || !std::isfinite(confidence) ||
        confidence <= 0.0 || confidence >= 1.0) {
        return 0;
    }
    // z for a two-sided confidence interval on the standard-deviation estimate.
    const double z = invNormalCdf((1.0 + confidence) / 2.0);
    // n ≈ ( z / (sqrt(2) * percentError) )^2 (README §7.3).
    const double n = (z * z) / (2.0 * percentError * percentError);
    if (!std::isfinite(n) ||
        n > static_cast<double>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(std::ceil(n));
}

}  // namespace opendva
