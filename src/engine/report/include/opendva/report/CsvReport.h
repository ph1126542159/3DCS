// A7 report extensions (README §8.4). CSV and Excel SpreadsheetML writers for
// the per-measure statistics. Plain text, no external dependencies.
#pragma once

#include <map>
#include <string>

#include "opendva/ISimulationEngine.h"

namespace opendva {

// Writes the statistics as a CSV file with the header
//   Measure,Nominal,Mean,Sigma,6Sigma,Min,Max,Cp,Cpk,TotOUT%,DPMO
// and one row per measure. Returns true on success.
bool writeCsvReport(const std::string& path,
                    const std::map<MeasureId, MeasureStats>& stats,
                    const std::map<MeasureId, std::string>& names);

// Writes the statistics as Excel-readable SpreadsheetML XML (urn:schemas-
// microsoft-com:office:spreadsheet). Same columns as the CSV writer.
// Returns true on success.
bool writeExcelXmlReport(const std::string& path,
                         const std::map<MeasureId, MeasureStats>& stats,
                         const std::map<MeasureId, std::string>& names);

}  // namespace opendva
