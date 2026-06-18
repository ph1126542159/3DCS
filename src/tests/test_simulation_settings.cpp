// Run Analysis settings used by desktop UI and batch entry points.
#include "dva_test.h"
#include "opendva/domain/SimulationSettings.h"

using namespace opendva;

TEST("simulation settings: defaults map to Monte Carlo run config") {
    const SimulationSettings settings;
    const RunConfig config = toRunConfig(settings);

    dvatest::check(settings.monteCarloEnabled, "monte carlo enabled by default");
    dvatest::check(config.totalRuns == 10000, "default desktop runs");
    dvatest::check(config.initialSeed == 12345, "default desktop seed");
    dvatest::check(config.threads == 1, "default desktop threads");
}

TEST("simulation settings: normalization clamps invalid numeric values") {
    SimulationSettings settings;
    settings.totalRuns = -20;
    settings.initialSeed = 0;
    settings.threads = 99;

    const SimulationSettings normalized = normalizeSimulationSettings(settings, 8);

    dvatest::check(normalized.totalRuns == 1, "runs clamp to minimum");
    dvatest::check(normalized.initialSeed == 1, "seed clamp to non-zero");
    dvatest::check(normalized.threads == 8, "threads clamp to configured maximum");
}

TEST("simulation settings: zero threads remains automatic mode") {
    SimulationSettings settings;
    settings.threads = 0;

    const SimulationSettings normalized = normalizeSimulationSettings(settings, 8);

    dvatest::check(normalized.threads == 0, "thread auto mode preserved");
}

TEST("simulation settings: disabled monte carlo is not runnable") {
    SimulationSettings settings;
    settings.monteCarloEnabled = false;

    dvatest::check(!shouldRunMonteCarlo(settings), "disabled MC does not run");
}

TEST("simulation settings: contributor can run without monte carlo") {
    SimulationSettings settings;
    settings.monteCarloEnabled = false;
    settings.contributorEnabled = true;

    dvatest::check(!shouldRunMonteCarlo(settings), "MC remains disabled");
    dvatest::check(shouldRunContributor(settings), "contributor runs");
    dvatest::check(shouldRunAnyAnalysis(settings), "some analysis selected");
}
