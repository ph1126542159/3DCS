// Desktop/batch run-analysis settings independent of Qt widgets.
#pragma once

#include <cstdint>

#include "opendva/ISimulationEngine.h"

namespace opendva {

struct SimulationSettings {
    bool monteCarloEnabled{true};
    bool contributorEnabled{false};
    int totalRuns{10000};
    std::uint64_t initialSeed{12345};
    int threads{1};  // 0 = automatic/single-stream compatibility mode.
};

SimulationSettings normalizeSimulationSettings(const SimulationSettings& settings,
                                                int maxThreads = 8);
bool shouldRunMonteCarlo(const SimulationSettings& settings);
bool shouldRunContributor(const SimulationSettings& settings);
bool shouldRunAnyAnalysis(const SimulationSettings& settings);
RunConfig toRunConfig(const SimulationSettings& settings, int maxThreads = 8);

}  // namespace opendva
