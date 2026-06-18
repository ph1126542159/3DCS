#include "opendva/domain/AppPreferences.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <utility>

namespace opendva {
namespace {

std::string lengthUnitText(LengthUnit unit) {
    return unit == LengthUnit::Inch ? "inch" : "millimeter";
}

LengthUnit parseLengthUnit(const std::string& text) {
    return text == "inch" ? LengthUnit::Inch : LengthUnit::Millimeter;
}

std::string boolText(bool value) {
    return value ? "1" : "0";
}

bool isLineSafeValue(const std::string& text) {
    return text.find_first_of("\r\n") == std::string::npos;
}

bool parseBool(const std::string& text, bool fallback) {
    if (text == "1" || text == "true") return true;
    if (text == "0" || text == "false") return false;
    return fallback;
}

void stripUtf8Bom(std::string& line) {
    constexpr char bom[] = "\xEF\xBB\xBF";
    if (line.rfind(bom, 0) == 0) {
        line.erase(0, 3);
    }
}

int parseInt(const std::string& text, int fallback) {
    int value = fallback;
    const char* first = text.data();
    const char* last = first + text.size();
    const auto result = std::from_chars(first, last, value);
    return result.ec == std::errc{} && result.ptr == last ? value : fallback;
}

std::uint64_t parseUInt64(const std::string& text, std::uint64_t fallback) {
    if (!text.empty() && text.front() == '-') return fallback;
    std::uint64_t value = fallback;
    const char* first = text.data();
    const char* last = first + text.size();
    const auto result = std::from_chars(first, last, value);
    return result.ec == std::errc{} && result.ptr == last ? value : fallback;
}

void appendRecentModelPath(AppPreferences& preferences, const std::string& path,
                           std::size_t maxItems = 8) {
    if (path.empty() || preferences.recentModelPaths.size() >= maxItems) return;
    if (std::find(preferences.recentModelPaths.begin(),
                  preferences.recentModelPaths.end(),
                  path) != preferences.recentModelPaths.end()) {
        return;
    }
    preferences.recentModelPaths.push_back(path);
}

}  // namespace

AppPreferences normalizeAppPreferences(const AppPreferences& preferences,
                                       int maxThreads) {
    AppPreferences out = preferences;
    out.analysisDefaults =
        normalizeSimulationSettings(out.analysisDefaults, maxThreads);
    if (out.defaultReportPath.empty() ||
        !isLineSafeValue(out.defaultReportPath)) {
        out.defaultReportPath = "report.html";
    }
    std::vector<std::string> recent;
    for (const std::string& path : out.recentModelPaths) {
        if (!isLineSafeValue(path)) continue;
        if (path.empty() ||
            std::find(recent.begin(), recent.end(), path) != recent.end()) {
            continue;
        }
        recent.push_back(path);
        if (recent.size() == 8) break;
    }
    out.recentModelPaths = std::move(recent);
    return out;
}

void rememberRecentModelPath(AppPreferences& preferences, const std::string& path,
                             std::size_t maxItems) {
    if (path.empty() || !isLineSafeValue(path) || maxItems == 0) return;

    auto& paths = preferences.recentModelPaths;
    paths.erase(std::remove(paths.begin(), paths.end(), path), paths.end());
    paths.insert(paths.begin(), path);
    if (paths.size() > maxItems) {
        paths.resize(maxItems);
    }
}

bool saveAppPreferences(const AppPreferences& preferences, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    const AppPreferences normalized = normalizeAppPreferences(preferences, 64);
    out << "version=1\n";
    out << "lengthUnit=" << lengthUnitText(normalized.lengthUnit) << "\n";
    out << "totalRuns=" << normalized.analysisDefaults.totalRuns << "\n";
    out << "initialSeed=" << normalized.analysisDefaults.initialSeed << "\n";
    out << "threads=" << normalized.analysisDefaults.threads << "\n";
    out << "monteCarloEnabled="
        << boolText(normalized.analysisDefaults.monteCarloEnabled) << "\n";
    out << "contributorEnabled="
        << boolText(normalized.analysisDefaults.contributorEnabled) << "\n";
    out << "defaultReportPath=" << normalized.defaultReportPath << "\n";
    for (const std::string& recentPath : normalized.recentModelPaths) {
        out << "recent=" << recentPath << "\n";
    }
    return static_cast<bool>(out);
}

AppPreferences loadAppPreferences(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return AppPreferences{};

    AppPreferences preferences;
    preferences.recentModelPaths.clear();

    std::string line;
    bool firstLine = true;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (firstLine) {
            stripUtf8Bom(line);
            firstLine = false;
        }
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) continue;

        const std::string key = line.substr(0, separator);
        const std::string value = line.substr(separator + 1);
        if (key == "lengthUnit") {
            preferences.lengthUnit = parseLengthUnit(value);
        } else if (key == "totalRuns") {
            preferences.analysisDefaults.totalRuns =
                parseInt(value, preferences.analysisDefaults.totalRuns);
        } else if (key == "initialSeed") {
            preferences.analysisDefaults.initialSeed =
                parseUInt64(value, preferences.analysisDefaults.initialSeed);
        } else if (key == "threads") {
            preferences.analysisDefaults.threads =
                parseInt(value, preferences.analysisDefaults.threads);
        } else if (key == "monteCarloEnabled") {
            preferences.analysisDefaults.monteCarloEnabled =
                parseBool(value, preferences.analysisDefaults.monteCarloEnabled);
        } else if (key == "contributorEnabled") {
            preferences.analysisDefaults.contributorEnabled =
                parseBool(value, preferences.analysisDefaults.contributorEnabled);
        } else if (key == "defaultReportPath") {
            preferences.defaultReportPath = value;
        } else if (key == "recent") {
            appendRecentModelPath(preferences, value);
        }
    }

    return normalizeAppPreferences(preferences, 64);
}

}  // namespace opendva
