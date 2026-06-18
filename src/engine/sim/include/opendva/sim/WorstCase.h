// A6 worst-case synthesis & convergence helpers (README §7.3 / §7.4).
//
// These are pure analytic post-processors over the same High-Low sensitivity
// the Monte Carlo / Contributor analyses use; they add no interface surface to
// ISimulationEngine, so the engine contract stays fixed.
#pragma once
#include <map>

#include "opendva/Types.h"

namespace opendva {

struct Model;  // domain handle (defined in opendva/domain/Model.h)

// README §7.4 — arithmetic-sum worst case.
//   Contributor WC Range = tolerance Range * GeoFactor
//   Measure     WC Range = Sum_i |Contributor_i WC Range|   (arithmetic sum)
// The sum is taken over absolute contributor ranges so that contributors with
// opposing signs still widen (never cancel) the worst-case envelope, matching
// the "宽于 RSS" guarantee in the reference tool. Returns one entry per active
// output measure (id -> worst-case range).
std::map<MeasureId, double> computeWorstCase(const Model& model);

// README §7.3 — sample size required for a target standard-deviation estimate.
//   n ≈ ( z / (sqrt(2) * percentError) )^2 ,  z = Phi^-1((1 + confidence)/2)
// percentError is a fraction (0.03 == 3%), confidence is a fraction (0.95).
// Verified anchors: 3%@95% -> ~2137, 1%@99% -> ~33178.
// Returns 0 for non-positive percentError.
int requiredRuns(double percentError, double confidence);

// Inverse standard-normal CDF (quantile) via the Acklam rational approximation.
// Exposed for testing; valid for p in (0,1).
double invNormalCdf(double p);

}  // namespace opendva
