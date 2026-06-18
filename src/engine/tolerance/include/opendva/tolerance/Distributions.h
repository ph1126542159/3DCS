// A4 distribution module public API (README §5.5). The 1D/2D shapes are built
// through DistributionFactory::create() (declared in IToleranceSampler.h); this
// header adds UserDefined helpers for injecting empirical samples or loading
// them from 3DCS-style .SMP text files.
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "opendva/IToleranceSampler.h"

namespace opendva {
namespace tolerance {

// Build a UserDefined distribution that replays the supplied empirical samples.
// Draws by bootstrap resampling, rescaled from the recorded sample range onto
// {range, offset}.
// An empty sample vector degrades to Normal.
std::unique_ptr<IDistribution> makeUserDefinedDistribution(std::vector<double> samples);

// Load a UserDefined distribution directly from a .SMP file.
std::unique_ptr<IDistribution> makeUserDefinedDistributionFromSmp(
    const std::string& path);

// Load numeric samples from a 3DCS-style User Defined distribution .SMP text
// file. Empty lines and '#' comments are ignored; commas and whitespace both
// separate values. Non-decimal, non-numeric, and non-finite tokens are skipped.
// Throws std::invalid_argument when the file cannot be opened or contains no
// finite numeric samples.
std::vector<double> loadSmpSamples(const std::string& path);

}  // namespace tolerance
}  // namespace opendva
