#include "opendva/report/CsvReport.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace opendva {

namespace {

// Resolves a measure's display name, falling back to "M<id>".
std::string measureName(MeasureId id,
                        const std::map<MeasureId, std::string>& names) {
    if (auto it = names.find(id); it != names.end()) return it->second;
    return "M" + std::to_string(id);
}

// Escapes a CSV field: quote when it contains a comma, quote or newline,
// doubling any embedded quotes (RFC 4180).
std::string csvField(const std::string& s) {
    const bool needsQuote =
        s.find_first_of(",\"\n\r") != std::string::npos;
    if (!needsQuote) return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += '"';
        out += c;
    }
    out += '"';
    return out;
}

// Escapes XML text content for SpreadsheetML cells.
std::string xmlEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out += c; break;
        }
    }
    return out;
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

}  // namespace

bool writeCsvReport(const std::string& path,
                    const std::map<MeasureId, MeasureStats>& stats,
                    const std::map<MeasureId, std::string>& names) {
    if (!statsAreFinite(stats)) return false;

    std::ofstream out(path);
    if (!out) return false;

    out << "Measure,Nominal,Mean,Sigma,6Sigma,Min,Max,Cp,Cpk,TotOUT%,DPMO\n";
    out << std::fixed << std::setprecision(6);
    for (const auto& [id, s] : stats) {
        out << csvField(measureName(id, names)) << ','
            << s.nominal << ',' << s.mean << ',' << s.sigma << ','
            << s.sixSigma << ',' << s.minVal << ',' << s.maxVal << ','
            << s.cp << ',' << s.cpk << ',' << s.totOutPct << ',' << s.dpmo
            << '\n';
    }
    return static_cast<bool>(out);
}

bool writeExcelXmlReport(const std::string& path,
                         const std::map<MeasureId, MeasureStats>& stats,
                         const std::map<MeasureId, std::string>& names) {
    if (!statsAreFinite(stats)) return false;

    std::ofstream out(path);
    if (!out) return false;

    out << "<?xml version=\"1.0\"?>\n"
        << "<Workbook xmlns=\"urn:schemas-microsoft-com:office:spreadsheet\" "
        << "xmlns:ss=\"urn:schemas-microsoft-com:office:spreadsheet\">\n"
        << "<Worksheet ss:Name=\"Statistics\">\n<Table>\n";

    // Header row (string cells).
    const char* headers[] = {"Measure", "Nominal", "Mean",   "Sigma",
                             "6Sigma",  "Min",     "Max",    "Cp",
                             "Cpk",     "TotOUT%", "DPMO"};
    out << "<Row>";
    for (const char* h : headers)
        out << "<Cell><Data ss:Type=\"String\">" << h << "</Data></Cell>";
    out << "</Row>\n";

    out << std::fixed << std::setprecision(6);
    for (const auto& [id, s] : stats) {
        out << "<Row>";
        out << "<Cell><Data ss:Type=\"String\">"
            << xmlEscape(measureName(id, names)) << "</Data></Cell>";
        const double cols[] = {s.nominal, s.mean,      s.sigma, s.sixSigma,
                               s.minVal,  s.maxVal,    s.cp,    s.cpk,
                               s.totOutPct, s.dpmo};
        for (double v : cols)
            out << "<Cell><Data ss:Type=\"Number\">" << v << "</Data></Cell>";
        out << "</Row>\n";
    }

    out << "</Table>\n</Worksheet>\n</Workbook>\n";
    return static_cast<bool>(out);
}

}  // namespace opendva
