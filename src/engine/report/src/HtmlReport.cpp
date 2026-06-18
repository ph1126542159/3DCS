#include "opendva/report/HtmlReport.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>

namespace opendva {

namespace {

std::string escapeHtml(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (const char ch : text) {
        switch (ch) {
            case '&': escaped += "&amp;"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '"': escaped += "&quot;"; break;
            case '\'': escaped += "&#39;"; break;
            default: escaped += ch; break;
        }
    }
    return escaped;
}

std::string measureName(MeasureId id,
                        const std::map<MeasureId, std::string>& names) {
    if (auto it = names.find(id); it != names.end()) return it->second;
    return "M" + std::to_string(id);
}

std::string toleranceName(ToleranceId id,
                          const std::map<ToleranceId, std::string>& names) {
    if (auto it = names.find(id); it != names.end()) return it->second;
    return "T" + std::to_string(id);
}

bool statsAreFinite(const std::map<MeasureId, MeasureStats>& stats) {
    for (const auto& [id, s] : stats) {
        if (id == kInvalidId) return false;
        const double values[] = {s.nominal, s.mean,      s.sigma, s.sixSigma,
                                 s.minVal,  s.maxVal,    s.cp,    s.cpk,
                                 s.totOutPct, s.dpmo};
        for (const double value : values) {
            if (!std::isfinite(value)) return false;
        }
        if (s.sigma < 0.0 || s.sixSigma < 0.0) return false;
        if (s.minVal > s.maxVal) return false;
        if (s.cp < 0.0) return false;
        if (s.totOutPct < 0.0 || s.totOutPct > 100.0) return false;
        if (s.dpmo < 0.0 || s.dpmo > 1000000.0) return false;
    }
    return true;
}

bool samplesAreFinite(const std::vector<SimulationSampleRow>& samples) {
    for (const SimulationSampleRow& sample : samples) {
        for (const auto& [measure, value] : sample.measureValues) {
            if (measure == kInvalidId) return false;
            if (!std::isfinite(value)) return false;
        }
    }
    return true;
}

bool sampleBuildsAreValid(const std::vector<SimulationSampleRow>& samples) {
    std::set<int> seen;
    for (const SimulationSampleRow& sample : samples) {
        if (sample.buildIndex < 0) return false;
        if (!seen.insert(sample.buildIndex).second) {
            return false;
        }
    }
    return true;
}

bool samplesReferenceStats(const std::vector<SimulationSampleRow>& samples,
                           const std::map<MeasureId, MeasureStats>& stats) {
    for (const SimulationSampleRow& sample : samples) {
        for (const auto& [measure, value] : sample.measureValues) {
            (void)value;
            if (stats.find(measure) == stats.end()) return false;
        }
    }
    return true;
}

bool contributorsAreFinite(const std::vector<ContributorRow>& contributors) {
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

bool contributorsAreDistinct(const std::vector<ContributorRow>& contributors) {
    std::set<std::pair<MeasureId, ToleranceId>> seen;
    for (const ContributorRow& contributor : contributors) {
        if (!seen.insert({contributor.measure, contributor.contributor}).second) {
            return false;
        }
    }
    return true;
}

bool contributorsReferenceStats(
    const std::vector<ContributorRow>& contributors,
    const std::map<MeasureId, MeasureStats>& stats) {
    for (const ContributorRow& contributor : contributors) {
        if (stats.find(contributor.measure) == stats.end()) return false;
    }
    return true;
}

}  // namespace

bool writeHtmlReport(const std::string& path,
                     const std::string& title,
                     const std::map<MeasureId, MeasureStats>& stats,
                     const std::map<MeasureId, std::string>& names) {
    return writeHtmlReport(path, title, stats, names, {}, {}, {}, {});
}

bool writeHtmlReport(const std::string& path, const std::string& title,
                     const std::map<MeasureId, MeasureStats>& stats,
                     const std::map<MeasureId, std::string>& names,
                     const std::vector<ReportImage>& images) {
    return writeHtmlReport(path, title, stats, names, images, {}, {}, {});
}

bool writeHtmlReport(const std::string& path,
                     const std::string& title,
                     const std::map<MeasureId, MeasureStats>& stats,
                     const std::map<MeasureId, std::string>& names,
                     const std::vector<ReportImage>& images,
                     const std::vector<SimulationSampleRow>& samples,
                     const std::vector<ContributorRow>& contributors,
                     const std::map<ToleranceId, std::string>& toleranceNames) {
    if (!statsAreFinite(stats)) return false;
    if (!samplesAreFinite(samples)) return false;
    if (!sampleBuildsAreValid(samples)) return false;
    if (!samplesReferenceStats(samples, stats)) return false;
    if (!contributorsAreFinite(contributors)) return false;
    if (!contributorsAreDistinct(contributors)) return false;
    if (!contributorsReferenceStats(contributors, stats)) return false;

    std::ofstream out(path);
    if (!out) return false;

    out << "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
        << "<title>" << escapeHtml(title) << "</title>"
        << "<style>body{font-family:sans-serif;margin:24px}"
        << "section{margin:18px 0}img{border:1px solid #bbb}"
        << "table{border-collapse:collapse}th,td{border:1px solid #999;padding:4px 8px;"
        << "text-align:right}th{background:#eee}td:first-child{text-align:left}</style>"
        << "</head><body>";
    out << "<h1>" << escapeHtml(title) << "</h1>";

    for (const ReportImage& image : images) {
        out << "<section><h2>" << escapeHtml(image.title) << "</h2>"
            << "<img src=\"" << escapeHtml(image.source) << "\""
            << " alt=\"" << escapeHtml(image.title) << "\""
            << " style=\"max-width:100%;height:auto\">";
        if (!image.caption.empty()) {
            out << "<p>" << escapeHtml(image.caption) << "</p>";
        }
        out << "</section>";
    }

    out << "<table><tr><th>Measure</th><th>Nominal</th><th>Mean</th><th>Sigma</th>"
        << "<th>6-Sigma</th><th>Min</th><th>Max</th><th>Cp</th><th>Cpk</th>"
        << "<th>Tot-OUT%</th><th>DPMO</th></tr>";

    out << std::fixed << std::setprecision(4);
    for (const auto& [id, s] : stats) {
        const std::string name = measureName(id, names);
        out << "<tr><td>" << escapeHtml(name) << "</td>"
            << "<td>" << s.nominal << "</td>"
            << "<td>" << s.mean << "</td>"
            << "<td>" << s.sigma << "</td>"
            << "<td>" << s.sixSigma << "</td>"
            << "<td>" << s.minVal << "</td>"
            << "<td>" << s.maxVal << "</td>"
            << "<td>" << s.cp << "</td>"
            << "<td>" << s.cpk << "</td>"
            << "<td>" << s.totOutPct << "</td>"
            << "<td>" << s.dpmo << "</td></tr>";
    }
    out << "</table>";

    out << "<h2>Histogram</h2>"
        << "<table><tr><th>Measure Bin</th><th>Count</th></tr>";
    for (const auto& [id, s] : stats) {
        const std::string name = measureName(id, names);
        for (std::size_t bin = 0; bin < s.histogram.size(); ++bin) {
            out << "<tr><td>" << escapeHtml(name) << " bin " << bin << "</td>"
                << "<td>" << s.histogram[bin] << "</td></tr>";
        }
    }
    out << "</table>";

    if (!samples.empty()) {
        out << "<h2>Samples</h2><table><tr><th>Build</th>";
        for (const auto& [id, s] : stats) {
            (void)s;
            out << "<th>" << escapeHtml(measureName(id, names)) << "</th>";
        }
        out << "</tr>";
        for (const SimulationSampleRow& sample : samples) {
            out << "<tr><td>" << sample.buildIndex << "</td>";
            for (const auto& [id, s] : stats) {
                (void)s;
                if (auto it = sample.measureValues.find(id);
                    it != sample.measureValues.end()) {
                    out << "<td>" << it->second << "</td>";
                } else {
                    out << "<td></td>";
                }
            }
            out << "</tr>";
        }
        out << "</table>";
    }

    if (!contributors.empty()) {
        out << "<h2>Contributor</h2>"
            << "<table><tr><th>Measure</th><th>Contributor</th><th>GeoFactor</th>"
            << "<th>6-Sigma</th><th>Contribution %</th></tr>";
        for (const ContributorRow& contributor : contributors) {
            out << "<tr><td>" << escapeHtml(measureName(contributor.measure, names))
                << "</td><td>"
                << escapeHtml(toleranceName(contributor.contributor, toleranceNames))
                << "</td><td>" << contributor.geoFactor
                << "</td><td>" << contributor.sixSigma
                << "</td><td>" << contributor.contributionPct << "</td></tr>";
        }
        out << "</table>";
    }

    out << "</body></html>";
    return static_cast<bool>(out);
}

}  // namespace opendva
