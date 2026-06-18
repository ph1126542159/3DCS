// Desktop application preferences shared by UI and future batch tooling.
#pragma once

#include <string>
#include <vector>

#include "opendva/domain/SimulationSettings.h"

namespace opendva {

enum class LengthUnit {
    Millimeter,
    Inch
};

struct AppPreferences {
    LengthUnit lengthUnit{LengthUnit::Millimeter};
    SimulationSettings analysisDefaults{};
    std::string defaultReportPath{"report.html"};
    std::vector<std::string> recentModelPaths{};
};

AppPreferences normalizeAppPreferences(const AppPreferences& preferences,
                                       int maxThreads = 8);
void rememberRecentModelPath(AppPreferences& preferences, const std::string& path,
                             std::size_t maxItems = 8);
bool saveAppPreferences(const AppPreferences& preferences, const std::string& path);
AppPreferences loadAppPreferences(const std::string& path);

}  // namespace opendva
