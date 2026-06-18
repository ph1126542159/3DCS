#include "opendva/report/ReportAssets.h"

#include <filesystem>

namespace opendva {

ReportImageAsset planHtmlSnapshotAsset(const std::string& htmlReportPath,
                                       const std::string& title,
                                       const std::string& caption) {
    const std::filesystem::path reportPath(htmlReportPath);
    const std::string fileName = reportPath.stem().string() + "-view.png";
    const std::filesystem::path snapshotPath = reportPath.parent_path() / fileName;
    return ReportImageAsset{
        snapshotPath.string(),
        ReportImage{title, fileName, caption},
    };
}

}  // namespace opendva
