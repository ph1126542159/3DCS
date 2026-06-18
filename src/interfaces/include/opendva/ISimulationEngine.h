// A6 simulation engine contract (README §7).
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "opendva/Types.h"

namespace opendva {

// Forward-declared model handle; concrete type lives in the domain layer.
struct Model;

// README §7.3 — full statistic set per measure.
struct MeasureStats {
    double nominal{0};
    double mean{0};
    double median{0};
    double sigma{0};        // population stddev, /n (README §7.3)
    double sixSigma{0};     // sigmaNum * sigma
    double minVal{0};
    double maxVal{0};
    double range{0};
    double skewness{0};
    double kurtosis{0};
    double cp{0}, cpk{0}, pp{0}, ppk{0};
    double lOutPct{0}, hOutPct{0}, totOutPct{0};
    double dpmo{0};
    std::vector<std::uint32_t> histogram;  // frequency bins
};

// README §7.4 — sensitivity matrix (rows=contributors, cols=measures).
struct SimulationSampleRow {
    int buildIndex{0};
    std::map<MeasureId, double> measureValues;
};

struct MonteCarloResult {
    std::map<MeasureId, MeasureStats> stats;
    std::vector<SimulationSampleRow> samples;
};

struct SensitivityMatrix {
    std::vector<ToleranceId> contributors;
    std::vector<MeasureId> measures;
    std::vector<std::vector<double>> geoFactor;  // [contributor][measure]
};

// README §7.5 — contributor rows.
struct ContributorRow {
    MeasureId measure{kInvalidId};
    ToleranceId contributor{kInvalidId};
    double geoFactor{0};
    double sixSigma{0};
    double contributionPct{0};  // GF^2 * sigma^2 proportion
};

struct RunConfig {
    int totalRuns{2137};       // README §7.3 default (3% @ 95%)
    std::uint64_t initialSeed{1};
    int threads{1};            // 0..8
};

class ISimulationEngine {
public:
    virtual ~ISimulationEngine() = default;

    // Monte Carlo (README §7.2/7.3). Fixed traversal order -> reproducible.
    virtual std::map<MeasureId, MeasureStats> runMonteCarlo(const Model& model,
                                                            const RunConfig& cfg) = 0;
    virtual MonteCarloResult runMonteCarloDetailed(const Model& model,
                                                   const RunConfig& cfg) = 0;
    // GeoFactor sensitivity (README §7.4) — High-Low difference quotient.
    virtual SensitivityMatrix runGeoFactor(const Model& model) = 0;
    // Contributor HLM (README §7.5).
    virtual std::vector<ContributorRow> runContributor(const Model& model) = 0;
};

}  // namespace opendva
