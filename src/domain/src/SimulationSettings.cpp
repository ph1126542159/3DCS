#include "opendva/domain/SimulationSettings.h"

#include <algorithm>

namespace opendva {

SimulationSettings normalizeSimulationSettings(const SimulationSettings& settings,
                                                int maxThreads) {
    SimulationSettings out = settings;
    out.totalRuns = std::max(1, out.totalRuns);
    out.initialSeed = std::max<std::uint64_t>(1, out.initialSeed);

    const int safeMaxThreads = std::max(1, maxThreads);
    out.threads = std::max(0, std::min(out.threads, safeMaxThreads));
    return out;
}

bool shouldRunMonteCarlo(const SimulationSettings& settings) {
    return settings.monteCarloEnabled;
}

bool shouldRunContributor(const SimulationSettings& settings) {
    return settings.contributorEnabled;
}

bool shouldRunAnyAnalysis(const SimulationSettings& settings) {
    return shouldRunMonteCarlo(settings) || shouldRunContributor(settings);
}

RunConfig toRunConfig(const SimulationSettings& settings, int maxThreads) {
    const SimulationSettings normalized =
        normalizeSimulationSettings(settings, maxThreads);
    RunConfig config;
    config.totalRuns = normalized.totalRuns;
    config.initialSeed = normalized.initialSeed;
    config.threads = normalized.threads;
    return config;
}

}  // namespace opendva
