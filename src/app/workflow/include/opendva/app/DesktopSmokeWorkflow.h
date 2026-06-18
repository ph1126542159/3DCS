#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "opendva/ISimulationEngine.h"
#include "opendva/domain/Model.h"
#include "opendva/domain/ModelValidation.h"

namespace opendva {

struct ReportExportPaths {
    std::string htmlPath;
    std::string csvPath;
    std::string excelXmlPath;
    std::string hstPath;
    std::string hlmPath;
};

struct AnalysisWorkflowResult {
    bool ok{false};
    std::string message;
    std::string assemblyName;
    std::vector<ModelIssue> validationIssues;
    std::map<MeasureId, MeasureStats> stats;
    std::map<MeasureId, std::string> measureNames;
    std::vector<SimulationSampleRow> samples;
    std::vector<ContributorRow> contributors;
    RunConfig config;
};

struct DesktopSmokeResult {
    bool ok{false};
    std::string message;
    std::map<MeasureId, MeasureStats> stats;
    RunConfig config;
};

struct BatchAnalysisJob {
    std::string modelPath;
    ReportExportPaths reports;
    int runs{0};
    std::uint64_t seed{0};
    int threads{-1};
};

struct BatchAnalysisJobInput {
    std::string modelPath;
    int runs{0};
    std::uint64_t seed{0};
    int threads{-1};
};

struct BatchAnalysisItemResult {
    bool ok{false};
    std::string message;
    std::string modelPath;
    ReportExportPaths reports;
    RunConfig config;
    std::vector<ModelIssue> validationIssues;
};

struct BatchAnalysisResult {
    int succeeded{0};
    int failed{0};
    std::vector<BatchAnalysisItemResult> items;
};

ReportExportPaths makeBatchReportPaths(const std::string& outDir,
                                       std::size_t oneBasedIndex);
std::vector<BatchAnalysisJob> makeBatchAnalysisJobs(
    const std::vector<std::string>& modelPaths,
    const std::string& outDir);
std::vector<BatchAnalysisJob> makeBatchAnalysisJobs(
    const std::vector<BatchAnalysisJobInput>& inputs,
    const std::string& outDir);
std::vector<BatchAnalysisJob> readBatchAnalysisJobs(
    const std::string& manifestPath,
    const std::string& outDir);
std::vector<BatchAnalysisJob> makeFailedBatchRetryJobs(
    const BatchAnalysisResult& result);
BatchAnalysisResult mergeBatchRetryResult(
    const BatchAnalysisResult& original,
    const BatchAnalysisResult& retry);

BatchAnalysisResult runBatchModelFileAnalysis(
    const std::vector<BatchAnalysisJob>& jobs,
    int runs,
    std::uint64_t seed,
    int threads = 1);

AnalysisWorkflowResult runModelAnalysis(const Model& model,
                                        const ReportExportPaths& reports,
                                        int runs,
                                        std::uint64_t seed,
                                        int threads = 1);

AnalysisWorkflowResult runModelFileAnalysis(const std::string& modelPath,
                                            const ReportExportPaths& reports,
                                            int runs,
                                            std::uint64_t seed,
                                            int threads = 1);

DesktopSmokeResult runStarterDesktopSmoke(const std::string& modelPath,
                                          const std::string& reportPath,
                                          int runs,
                                          std::uint64_t seed,
                                          int threads = 1);

}  // namespace opendva
