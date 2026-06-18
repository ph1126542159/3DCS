#include "opendva/report/AnalysisDataFiles.h"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <utility>

namespace opendva {
namespace {

std::vector<std::string> splitCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    std::istringstream in(line);
    while (std::getline(in, field, ',')) {
        fields.push_back(field);
    }
    if (!line.empty() && line.back() == ',') fields.emplace_back();
    return fields;
}

void stripUtf8Bom(std::string& line) {
    constexpr char bom[] = "\xEF\xBB\xBF";
    if (line.rfind(bom, 0) == 0) {
        line.erase(0, 3);
    }
}

bool parseInt(const std::string& text, int& value) {
    int parsed = 0;
    const char* first = text.data();
    const char* last = first + text.size();
    const auto result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc{} || result.ptr != last) return false;
    value = static_cast<int>(parsed);
    return true;
}

bool parseUnsigned(const std::string& text, std::uint64_t& value) {
    std::uint64_t parsed = 0;
    const char* first = text.data();
    const char* last = first + text.size();
    const auto result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc{} || result.ptr != last) return false;
    value = static_cast<std::uint64_t>(parsed);
    return true;
}

bool isDecimalFloatLiteral(const std::string& text) {
    std::size_t pos = 0;
    if (pos < text.size() && text[pos] == '-') ++pos;

    bool sawDigit = false;
    while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
        sawDigit = true;
        ++pos;
    }

    if (pos < text.size() && text[pos] == '.') {
        ++pos;
        while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
            sawDigit = true;
            ++pos;
        }
    }
    if (!sawDigit) return false;

    if (pos < text.size() && (text[pos] == 'e' || text[pos] == 'E')) {
        ++pos;
        if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) ++pos;
        bool sawExponentDigit = false;
        while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
            sawExponentDigit = true;
            ++pos;
        }
        if (!sawExponentDigit) return false;
    }

    return pos == text.size();
}

bool parseDouble(const std::string& text, double& value) {
    if (!isDecimalFloatLiteral(text)) return false;
    char* end = nullptr;
    const double parsed = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0') return false;
    if (!std::isfinite(parsed)) return false;
    value = parsed;
    return true;
}

bool parseMeasureHeader(const std::string& text, MeasureId& id) {
    if (text.size() < 2 || text[0] != 'M') return false;
    return parseUnsigned(text.substr(1), id);
}

bool measureOrderIsDistinct(const std::vector<MeasureId>& measureOrder) {
    std::set<MeasureId> seen;
    for (MeasureId measure : measureOrder) {
        if (measure == kInvalidId) return false;
        if (!seen.insert(measure).second) return false;
    }
    return true;
}

bool hstSamplesAreFinite(const std::vector<SimulationSampleRow>& samples,
                         const std::vector<MeasureId>& measureOrder) {
    for (const SimulationSampleRow& sample : samples) {
        if (sample.buildIndex < 0) return false;
        for (const auto& [measure, value] : sample.measureValues) {
            if (measure == kInvalidId) return false;
            if (!std::isfinite(value)) return false;
        }
        for (MeasureId measure : measureOrder) {
            if (auto it = sample.measureValues.find(measure);
                it != sample.measureValues.end() &&
                !std::isfinite(it->second)) {
                return false;
            }
        }
    }
    return true;
}

bool hstBuildIndicesAreDistinct(const std::vector<SimulationSampleRow>& samples) {
    std::set<int> seen;
    for (const SimulationSampleRow& sample : samples) {
        if (!seen.insert(sample.buildIndex).second) {
            return false;
        }
    }
    return true;
}

bool hlmContributorsAreFinite(const std::vector<ContributorRow>& contributors) {
    for (const ContributorRow& contributor : contributors) {
        if (contributor.measure == kInvalidId ||
            contributor.contributor == kInvalidId) {
            return false;
        }
        if (!std::isfinite(contributor.geoFactor) ||
            !std::isfinite(contributor.sixSigma) ||
            !std::isfinite(contributor.contributionPct)) {
            return false;
        }
        if (contributor.sixSigma < 0.0) return false;
        if (contributor.contributionPct < 0.0 ||
            contributor.contributionPct > 100.0) {
            return false;
        }
    }
    return true;
}

bool hlmContributorsAreDistinct(const std::vector<ContributorRow>& contributors) {
    std::set<std::pair<MeasureId, ToleranceId>> seen;
    for (const ContributorRow& contributor : contributors) {
        if (!seen.insert({contributor.measure, contributor.contributor}).second) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool writeHstFile(const std::string& path,
                  const std::vector<SimulationSampleRow>& samples,
                  const std::vector<MeasureId>& measureOrder) {
    if (measureOrder.empty()) return false;
    if (!hstSamplesAreFinite(samples, measureOrder)) return false;
    if (!hstBuildIndicesAreDistinct(samples)) return false;
    if (!measureOrderIsDistinct(measureOrder)) return false;

    std::ofstream out(path);
    if (!out) return false;

    out << "Build";
    for (MeasureId measure : measureOrder) {
        out << ",M" << measure;
    }
    out << '\n' << std::fixed << std::setprecision(10);

    for (const SimulationSampleRow& sample : samples) {
        out << sample.buildIndex;
        for (MeasureId measure : measureOrder) {
            out << ',';
            if (auto it = sample.measureValues.find(measure);
                it != sample.measureValues.end()) {
                out << it->second;
            }
        }
        out << '\n';
    }
    return static_cast<bool>(out);
}

bool writeHstFile(const std::string& path,
                  const std::vector<SimulationSampleRow>& samples,
                  const std::map<MeasureId, MeasureStats>& stats) {
    std::vector<MeasureId> measureOrder;
    for (const auto& item : stats) {
        measureOrder.push_back(item.first);
    }
    return writeHstFile(path, samples, measureOrder);
}

bool readHstFile(const std::string& path,
                 std::vector<SimulationSampleRow>& samples) {
    std::vector<MeasureId> measureOrder;
    return readHstFile(path, samples, measureOrder);
}

bool readHstFile(const std::string& path,
                 std::vector<SimulationSampleRow>& samples,
                 std::vector<MeasureId>& measureOrder) {
    std::ifstream in(path);
    if (!in) return false;

    std::string line;
    if (!std::getline(in, line)) return false;
    stripUtf8Bom(line);
    const std::vector<std::string> header = splitCsvLine(line);
    if (header.size() < 2 || header[0] != "Build") return false;

    std::vector<MeasureId> measures;
    for (std::size_t i = 1; i < header.size(); ++i) {
        MeasureId id{kInvalidId};
        if (!parseMeasureHeader(header[i], id)) return false;
        measures.push_back(id);
    }
    if (!measureOrderIsDistinct(measures)) return false;

    std::vector<SimulationSampleRow> loaded;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        const std::vector<std::string> fields = splitCsvLine(line);
        if (fields.size() != header.size()) return false;

        SimulationSampleRow row;
        if (!parseInt(fields[0], row.buildIndex)) return false;
        if (row.buildIndex < 0) return false;
        for (std::size_t i = 1; i < fields.size(); ++i) {
            if (fields[i].empty()) continue;
            double value = 0.0;
            if (!parseDouble(fields[i], value)) return false;
            row.measureValues[measures[i - 1]] = value;
        }
        loaded.push_back(std::move(row));
    }

    if (!hstBuildIndicesAreDistinct(loaded)) return false;

    samples = std::move(loaded);
    measureOrder = std::move(measures);
    return true;
}

bool writeHlmFile(const std::string& path,
                  const std::vector<ContributorRow>& contributors) {
    if (!hlmContributorsAreFinite(contributors)) return false;
    if (!hlmContributorsAreDistinct(contributors)) return false;

    std::ofstream out(path);
    if (!out) return false;

    out << "Measure,Contributor,GeoFactor,6Sigma,ContributionPct\n"
        << std::fixed << std::setprecision(10);
    for (const ContributorRow& contributor : contributors) {
        out << contributor.measure << ',' << contributor.contributor << ','
            << contributor.geoFactor << ',' << contributor.sixSigma << ','
            << contributor.contributionPct << '\n';
    }
    return static_cast<bool>(out);
}

bool readHlmFile(const std::string& path,
                 std::vector<ContributorRow>& contributors) {
    std::ifstream in(path);
    if (!in) return false;

    std::string line;
    if (!std::getline(in, line)) return false;
    stripUtf8Bom(line);
    if (line != "Measure,Contributor,GeoFactor,6Sigma,ContributionPct") {
        return false;
    }

    std::vector<ContributorRow> loaded;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        const std::vector<std::string> fields = splitCsvLine(line);
        if (fields.size() != 5) return false;

        ContributorRow row;
        if (!parseUnsigned(fields[0], row.measure) ||
            !parseUnsigned(fields[1], row.contributor) ||
            !parseDouble(fields[2], row.geoFactor) ||
            !parseDouble(fields[3], row.sixSigma) ||
            !parseDouble(fields[4], row.contributionPct)) {
            return false;
        }
        loaded.push_back(row);
    }
    if (!hlmContributorsAreFinite(loaded)) return false;
    if (!hlmContributorsAreDistinct(loaded)) return false;

    contributors = std::move(loaded);
    return true;
}

}  // namespace opendva
