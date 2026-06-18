#pragma once

#include <string>

#include "opendva/report/HtmlReport.h"

namespace opendva {

struct ReportImageAsset {
    std::string filePath;
    ReportImage image;
};

ReportImageAsset planHtmlSnapshotAsset(const std::string& htmlReportPath,
                                       const std::string& title,
                                       const std::string& caption);

}  // namespace opendva
