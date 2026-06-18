// A7 report service (README §8.4). Minimal HTML writer for simulation results.
#pragma once
#include <map>
#include <string>
#include <vector>

#include "opendva/ISimulationEngine.h"

namespace opendva {

struct ReportImage {
    std::string title;
    std::string source;
    std::string caption;
};

// Writes a self-contained HTML page with a statistics table for each measure.
// Returns true on success.
bool writeHtmlReport(const std::string& path,
                     const std::string& title,
                     const std::map<MeasureId, MeasureStats>& stats,
                     const std::map<MeasureId, std::string>& measureNames);
bool writeHtmlReport(const std::string& path,
                     const std::string& title,
                     const std::map<MeasureId, MeasureStats>& stats,
                     const std::map<MeasureId, std::string>& measureNames,
                     const std::vector<ReportImage>& images);
bool writeHtmlReport(const std::string& path,
                     const std::string& title,
                     const std::map<MeasureId, MeasureStats>& stats,
                     const std::map<MeasureId, std::string>& measureNames,
                     const std::vector<ReportImage>& images,
                     const std::vector<SimulationSampleRow>& samples,
                     const std::vector<ContributorRow>& contributors,
                     const std::map<ToleranceId, std::string>& toleranceNames);

}  // namespace opendva
