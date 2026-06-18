// A7 analysis data files: minimal HST/HLM text round-trips.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "opendva/ISimulationEngine.h"

namespace opendva {

bool writeHstFile(const std::string& path,
                  const std::vector<SimulationSampleRow>& samples,
                  const std::vector<MeasureId>& measureOrder);
bool writeHstFile(const std::string& path,
                  const std::vector<SimulationSampleRow>& samples,
                  const std::map<MeasureId, MeasureStats>& stats);
bool readHstFile(const std::string& path,
                 std::vector<SimulationSampleRow>& samples);
bool readHstFile(const std::string& path,
                 std::vector<SimulationSampleRow>& samples,
                 std::vector<MeasureId>& measureOrder);

bool writeHlmFile(const std::string& path,
                  const std::vector<ContributorRow>& contributors);
bool readHlmFile(const std::string& path,
                 std::vector<ContributorRow>& contributors);

}  // namespace opendva
