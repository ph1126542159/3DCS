// Headless CLI (Batch Processor analogue, README §7.7). Builds the golden
// model in code, runs Monte Carlo, prints stats, and writes an HTML report.
// Proves the L1 engine runs end-to-end without UI (A0 smoke test).
#include <charconv>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "opendva/app/DesktopSmokeWorkflow.h"
#include "opendva/domain/Model.h"

namespace {

using namespace opendva;

void printValidationIssues(const opendva::AnalysisWorkflowResult& analysis) {
    for (const opendva::ModelIssue& issue : analysis.validationIssues) {
        std::cerr << "  [" << opendva::issueSeverityName(issue.severity) << "] "
                  << opendva::issueCategoryName(issue.category) << " "
                  << issue.code << ": " << issue.message << "\n";
    }
}

bool parseIntArg(const std::string& text, int& value) {
    int parsed = 0;
    const char* first = text.data();
    const char* last = first + text.size();
    const auto result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc{} || result.ptr != last) {
        return false;
    }
    value = parsed;
    return true;
}

bool parseUInt64Arg(const std::string& text, std::uint64_t& value) {
    if (!text.empty() && text.front() == '-') {
        return false;
    }
    std::uint64_t parsed = 0;
    const char* first = text.data();
    const char* last = first + text.size();
    const auto result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc{} || result.ptr != last) {
        return false;
    }
    value = parsed;
    return true;
}

bool readOptionValue(int argc, char** argv, int& index, const std::string& option,
                     std::string& value) {
    if (index + 1 >= argc) {
        std::cerr << "Missing value for " << option << "\n";
        return false;
    }
    const std::string next = argv[index + 1];
    if (next.rfind("--", 0) == 0) {
        std::cerr << "Missing value for " << option << "\n";
        return false;
    }
    value = next;
    ++index;
    return true;
}

// Golden model: two coordinate points; a Point-Point measure between them.
// One linear tolerance perturbs the second point along +Z (Normal, range 1.0).
Model buildGoldenModel() {
    Model m;
    m.assemblyName = "GoldenModel";

    Part part;
    part.id = 1;
    part.cadName = "Block";
    part.dcsName = "Block";

    Point p1;
    p1.id = 101;
    p1.position = {0, 0, 0};
    Point p2;
    p2.id = 102;
    p2.position = {0, 0, 10};
    part.points = {p1, p2};

    Feature f;
    f.id = 201;
    f.kind = FeatureKind::Plane;
    f.definingPoints = {102};  // tolerance acts on p2
    part.features = {f};

    ToleranceDef tol;
    tol.id = 301;
    tol.name = "Tol_P2_Z";
    tol.active = true;
    tol.features = {201};
    RandSpec rand;
    rand.distribution = DistributionType::Normal;
    rand.range = 1.0;     // +/-0.5
    rand.offset = 0.0;
    rand.sigmaNum = 3.0;  // +/-3 sigma in range
    tol.ir.rands = {rand};
    tol.ir.direction.ijk = {0, 0, 1};
    part.tolerances = {tol};

    m.parts = {part};

    MeasureRecord mr;
    mr.id = 401;
    mr.name = "Gap_P1P2_Z";
    mr.def.type = MeasureType::PointPoint;
    mr.def.inputPoints = {101, 102};
    mr.def.direction.ijk = {0, 0, 1};
    mr.def.dirMode = DirectionMode::ProjectedOnVector;
    mr.def.spec.usl = 11.5;
    mr.def.spec.lsl = 8.5;
    mr.def.spec.uslActive = true;
    mr.def.spec.lslActive = true;
    m.measures = {mr};
    return m;
}

}  // namespace

int main(int argc, char** argv) {
    int totalRuns = 10000;
    int threads = 1;
    std::uint64_t seed = 12345;
    std::string reportPath = "report.html";
    std::string csvReportPath;
    std::string excelReportPath;
    std::string hstReportPath;
    std::string hlmReportPath;
    std::string modelPath = "starter-model.xml";
    std::string modelInPath;
    std::string batchListPath;
    std::string batchOutDir;
    bool starterSmoke = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--runs") {
            std::string value;
            if (!readOptionValue(argc, argv, i, a, value)) {
                return 1;
            }
            if (!parseIntArg(value, totalRuns)) {
                std::cerr << "Invalid --runs value: " << value << "\n";
                return 1;
            }
        } else if (a == "--seed") {
            std::string value;
            if (!readOptionValue(argc, argv, i, a, value)) {
                return 1;
            }
            if (!parseUInt64Arg(value, seed)) {
                std::cerr << "Invalid --seed value: " << value << "\n";
                return 1;
            }
        } else if (a == "--threads") {
            std::string value;
            if (!readOptionValue(argc, argv, i, a, value)) {
                return 1;
            }
            if (!parseIntArg(value, threads)) {
                std::cerr << "Invalid --threads value: " << value << "\n";
                return 1;
            }
        } else if (a == "--out") {
            if (!readOptionValue(argc, argv, i, a, reportPath)) return 1;
        } else if (a == "--csv-out") {
            if (!readOptionValue(argc, argv, i, a, csvReportPath)) return 1;
        } else if (a == "--excel-out") {
            if (!readOptionValue(argc, argv, i, a, excelReportPath)) return 1;
        } else if (a == "--hst-out") {
            if (!readOptionValue(argc, argv, i, a, hstReportPath)) return 1;
        } else if (a == "--hlm-out") {
            if (!readOptionValue(argc, argv, i, a, hlmReportPath)) return 1;
        } else if (a == "--model-out") {
            if (!readOptionValue(argc, argv, i, a, modelPath)) return 1;
        } else if (a == "--model-in") {
            if (!readOptionValue(argc, argv, i, a, modelInPath)) return 1;
        } else if (a == "--batch-list") {
            if (!readOptionValue(argc, argv, i, a, batchListPath)) return 1;
        } else if (a == "--batch-out-dir") {
            if (!readOptionValue(argc, argv, i, a, batchOutDir)) return 1;
        }
        else if (a == "--starter-smoke") starterSmoke = true;
        else {
            std::cerr << "Unknown argument: " << a << "\n";
            return 1;
        }
    }

    if (starterSmoke) {
        const opendva::DesktopSmokeResult result =
            opendva::runStarterDesktopSmoke(modelPath, reportPath, totalRuns,
                                            seed, threads);
        if (!result.ok) {
            std::cerr << "Starter smoke failed: " << result.message << "\n";
            return 1;
        }
        std::cout << "=== OpenDVA starter desktop smoke ("
                  << result.config.totalRuns << " runs, seed "
                  << result.config.initialSeed << ", threads "
                  << result.config.threads << ") ===\n";
        std::cout << "Model written: " << modelPath << "\n";
        std::cout << "Report written: " << reportPath << "\n";
        for (const auto& [id, s] : result.stats) {
            std::cout << "Measure " << id << ": mean=" << s.mean
                      << " sigma=" << s.sigma << " Cpk=" << s.cpk << "\n";
        }
        return 0;
    }

    opendva::ReportExportPaths reports;
    reports.htmlPath = reportPath;
    reports.csvPath = csvReportPath;
    reports.excelXmlPath = excelReportPath;
    reports.hstPath = hstReportPath;
    reports.hlmPath = hlmReportPath;

    if (!batchListPath.empty()) {
        if (batchOutDir.empty()) {
            std::cerr << "Batch analysis failed: --batch-out-dir is required\n";
            return 1;
        }

        const std::vector<opendva::BatchAnalysisJob> batchJobs =
            opendva::readBatchAnalysisJobs(batchListPath, batchOutDir);
        if (batchJobs.empty()) {
            std::cerr << "Batch analysis failed: no model paths in "
                      << batchListPath << "\n";
            return 1;
        }

        const opendva::BatchAnalysisResult batchResult =
            opendva::runBatchModelFileAnalysis(
                batchJobs, totalRuns, seed, threads);
        for (std::size_t index = 0; index < batchResult.items.size(); ++index) {
            const opendva::BatchAnalysisItemResult& item =
                batchResult.items[index];
            std::cout << "Batch item " << (index + 1) << "/"
                      << batchResult.items.size() << ": " << item.modelPath
                      << " -> " << item.reports.htmlPath;
            if (item.ok) {
                std::cout << " (ok, " << item.config.totalRuns
                          << " runs, seed " << item.config.initialSeed
                          << ", threads " << item.config.threads
                          << ")\n";
            } else {
                std::cout << " (failed: " << item.message << ")\n";
                for (const opendva::ModelIssue& issue : item.validationIssues) {
                    std::cerr << "  [" << opendva::issueSeverityName(issue.severity)
                              << "] " << opendva::issueCategoryName(issue.category)
                              << " " << issue.code << ": " << issue.message
                              << "\n";
                }
            }
        }

        std::cout << "Batch completed: " << batchResult.succeeded
                  << " succeeded, " << batchResult.failed << " failed\n";
        return batchResult.failed == 0 ? 0 : 1;
    }

    opendva::AnalysisWorkflowResult analysis;
    if (!modelInPath.empty()) {
        analysis =
            opendva::runModelFileAnalysis(modelInPath, reports, totalRuns,
                                          seed, threads);
    } else {
        const opendva::Model model = buildGoldenModel();
        analysis = opendva::runModelAnalysis(model, reports, totalRuns, seed, threads);
    }
    if (!analysis.ok) {
        std::cerr << "Analysis failed: " << analysis.message << "\n";
        printValidationIssues(analysis);
        return 1;
    }

    const std::string assemblyName =
        analysis.assemblyName.empty() ? "Model" : analysis.assemblyName;
    std::cout << "=== OpenDVA Monte Carlo: " << assemblyName << " ("
              << analysis.config.totalRuns << " runs, seed "
              << analysis.config.initialSeed << ", threads "
              << analysis.config.threads << ") ===\n";
    for (const auto& [id, s] : analysis.stats) {
        const auto nameIt = analysis.measureNames.find(id);
        const std::string name = nameIt != analysis.measureNames.end()
                                     ? nameIt->second
                                     : std::to_string(id);
        std::cout << name << ": mean=" << s.mean << " sigma=" << s.sigma
                  << " 6sigma=" << s.sixSigma << " min=" << s.minVal << " max=" << s.maxVal
                  << " Cpk=" << s.cpk << " TotOUT%=" << s.totOutPct << "\n";
    }

    std::cout << "Report written: " << reportPath << "\n";
    if (!csvReportPath.empty()) {
        std::cout << "CSV report written: " << csvReportPath << "\n";
    }
    if (!excelReportPath.empty()) {
        std::cout << "Excel XML report written: " << excelReportPath << "\n";
    }
    if (!hstReportPath.empty()) {
        std::cout << "HST samples written: " << hstReportPath << "\n";
    }
    if (!hlmReportPath.empty()) {
        std::cout << "HLM contributors written: " << hlmReportPath << "\n";
    }
    return 0;
}
