#include "opendva/app/DesktopSmokeWorkflow.h"

#include <charconv>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>

#include "opendva/domain/ModelEditing.h"
#include "opendva/domain/ModelSerializer.h"
#include "opendva/domain/ModelValidation.h"
#include "opendva/domain/SimulationSettings.h"
#include "opendva/report/AnalysisDataFiles.h"
#include "opendva/report/CsvReport.h"
#include "opendva/report/HtmlReport.h"
#include "opendva/sim/SimulationEngine.h"

namespace opendva {
namespace {

std::map<MeasureId, std::string> measureNames(const Model& model) {
    std::map<MeasureId, std::string> names;
    for (const MeasureRecord& measure : model.measures) {
        names[measure.id] = measure.name;
    }
    return names;
}

std::map<ToleranceId, std::string> toleranceNames(const Model& model) {
    std::map<ToleranceId, std::string> names;
    for (const Part& part : model.parts) {
        for (const ToleranceDef& tolerance : part.tolerances) {
            names[tolerance.id] = tolerance.name;
        }
    }
    return names;
}

std::string trim(std::string value) {
    const std::string whitespace = " \t";
    const std::size_t first = value.find_first_not_of(whitespace);
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

std::string stripInlineComment(const std::string& value) {
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '#') {
            continue;
        }
        if (index == 0 || value[index - 1] == ' ' ||
            value[index - 1] == '\t') {
            return value.substr(0, index);
        }
    }
    return value;
}

std::string stripUtf8Bom(const std::string& value) {
    constexpr char bom[] = "\xEF\xBB\xBF";
    if (value.rfind(bom, 0) == 0) {
        return value.substr(3);
    }
    return value;
}

std::vector<std::string> splitCommaLine(const std::string& line) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= line.size()) {
        const std::size_t comma = line.find(',', start);
        if (comma == std::string::npos) {
            parts.push_back(trim(line.substr(start)));
            break;
        }
        parts.push_back(trim(line.substr(start, comma - start)));
        start = comma + 1;
    }
    return parts;
}

bool parseInt(const std::string& text, int& value) {
    if (text.empty()) {
        return false;
    }
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

bool parseUint64(const std::string& text, std::uint64_t& value) {
    if (text.empty() || text.front() == '-') {
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

bool validPerItemBatchSettings(int runs, std::uint64_t seed, int threads) {
    return runs > 0 && seed > 0 && threads >= 0;
}

std::string resolveManifestModelPath(const std::string& manifestPath,
                                     const std::string& modelPath) {
    const std::filesystem::path model(modelPath);
    if (model.is_absolute()) {
        return model.string();
    }

    const std::filesystem::path manifestDir =
        std::filesystem::path(manifestPath).parent_path();
    if (manifestDir.empty()) {
        return model.string();
    }
    return (manifestDir / model).string();
}

void resolveUserDefinedSamplePaths(Model& model, const std::filesystem::path& baseDir) {
    if (baseDir.empty()) {
        return;
    }
    for (Part& part : model.parts) {
        for (ToleranceDef& tolerance : part.tolerances) {
            for (RandSpec& rand : tolerance.ir.rands) {
                if (rand.distribution != DistributionType::UserDefined ||
                    rand.userDefinedSamplePath.empty()) {
                    continue;
                }
                const std::filesystem::path samplePath(rand.userDefinedSamplePath);
                if (!samplePath.is_absolute()) {
                    rand.userDefinedSamplePath = (baseDir / samplePath).string();
                }
            }
        }
    }
}

}  // namespace

ReportExportPaths makeBatchReportPaths(const std::string& outDir,
                                       std::size_t oneBasedIndex) {
    const std::filesystem::path base =
        std::filesystem::path(outDir) /
        ("batch-" + std::to_string(oneBasedIndex));
    ReportExportPaths paths;
    paths.htmlPath = base.string() + ".html";
    paths.csvPath = base.string() + ".csv";
    paths.excelXmlPath = base.string() + ".xml.xls";
    paths.hstPath = base.string() + ".hst";
    paths.hlmPath = base.string() + ".hlm";
    return paths;
}

std::vector<BatchAnalysisJob> makeBatchAnalysisJobs(
    const std::vector<std::string>& modelPaths,
    const std::string& outDir) {
    std::vector<BatchAnalysisJob> jobs;
    jobs.reserve(modelPaths.size());
    for (std::size_t index = 0; index < modelPaths.size(); ++index) {
        jobs.push_back(BatchAnalysisJob{
            modelPaths[index],
            makeBatchReportPaths(outDir, index + 1)});
    }
    return jobs;
}

std::vector<BatchAnalysisJob> makeBatchAnalysisJobs(
    const std::vector<BatchAnalysisJobInput>& inputs,
    const std::string& outDir) {
    std::vector<BatchAnalysisJob> jobs;
    jobs.reserve(inputs.size());
    for (std::size_t index = 0; index < inputs.size(); ++index) {
        BatchAnalysisJob job;
        job.modelPath = inputs[index].modelPath;
        job.reports = makeBatchReportPaths(outDir, index + 1);
        job.runs = inputs[index].runs;
        job.seed = inputs[index].seed;
        job.threads = inputs[index].threads;
        jobs.push_back(std::move(job));
    }
    return jobs;
}

std::vector<BatchAnalysisJob> readBatchAnalysisJobs(
    const std::string& manifestPath,
    const std::string& outDir) {
    std::ifstream in(manifestPath);
    std::vector<BatchAnalysisJob> jobs;
    std::string line;
    bool firstLine = true;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (firstLine) {
            line = stripUtf8Bom(line);
            firstLine = false;
        }
        line = stripInlineComment(line);
        line = trim(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }

        const std::vector<std::string> parts = splitCommaLine(line);
        if (parts.size() != 1 && parts.size() != 4) {
            continue;
        }
        if (parts[0].empty()) {
            continue;
        }
        BatchAnalysisJob job;
        job.modelPath = resolveManifestModelPath(manifestPath, parts[0]);
        job.reports = makeBatchReportPaths(outDir, jobs.size() + 1);
        if (parts.size() == 4) {
            int runs = 0;
            std::uint64_t seed = 0;
            int threads = -1;
            if (!parseInt(parts[1], runs) ||
                !parseUint64(parts[2], seed) ||
                !parseInt(parts[3], threads) ||
                !validPerItemBatchSettings(runs, seed, threads)) {
                continue;
            }
            job.runs = runs;
            job.seed = seed;
            job.threads = threads;
        }
        jobs.push_back(std::move(job));
    }
    return jobs;
}

std::vector<BatchAnalysisJob> makeFailedBatchRetryJobs(
    const BatchAnalysisResult& result) {
    std::vector<BatchAnalysisJob> jobs;
    for (const BatchAnalysisItemResult& item : result.items) {
        if (item.ok) {
            continue;
        }
        BatchAnalysisJob job;
        job.modelPath = item.modelPath;
        job.reports = item.reports;
        job.runs = item.config.totalRuns;
        job.seed = item.config.initialSeed;
        job.threads = item.config.threads;
        jobs.push_back(std::move(job));
    }
    return jobs;
}

BatchAnalysisResult mergeBatchRetryResult(
    const BatchAnalysisResult& original,
    const BatchAnalysisResult& retry) {
    BatchAnalysisResult merged = original;
    std::vector<bool> consumed(retry.items.size(), false);

    for (BatchAnalysisItemResult& item : merged.items) {
        if (item.ok) {
            continue;
        }
        for (std::size_t index = 0; index < retry.items.size(); ++index) {
            const BatchAnalysisItemResult& retryItem = retry.items[index];
            if (consumed[index] || retryItem.modelPath != item.modelPath ||
                retryItem.reports.htmlPath != item.reports.htmlPath) {
                continue;
            }
            item = retryItem;
            consumed[index] = true;
            break;
        }
    }

    for (std::size_t index = 0; index < retry.items.size(); ++index) {
        if (!consumed[index]) {
            merged.items.push_back(retry.items[index]);
        }
    }

    merged.succeeded = 0;
    merged.failed = 0;
    for (const BatchAnalysisItemResult& item : merged.items) {
        if (item.ok) {
            ++merged.succeeded;
        } else {
            ++merged.failed;
        }
    }
    return merged;
}

BatchAnalysisResult runBatchModelFileAnalysis(
    const std::vector<BatchAnalysisJob>& jobs,
    int runs,
    std::uint64_t seed,
    int threads) {
    BatchAnalysisResult result;
    result.items.reserve(jobs.size());

    for (const BatchAnalysisJob& job : jobs) {
        if (!job.reports.htmlPath.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(
                std::filesystem::path(job.reports.htmlPath).parent_path(), ec);
            if (ec) {
                BatchAnalysisItemResult item;
                item.modelPath = job.modelPath;
                item.reports = job.reports;
                item.message = "Could not create batch report directory";
                ++result.failed;
                result.items.push_back(std::move(item));
                continue;
            }
        }

        const int jobRuns = job.runs > 0 ? job.runs : runs;
        const std::uint64_t jobSeed = job.seed != 0 ? job.seed : seed;
        const int jobThreads = job.threads >= 0 ? job.threads : threads;
        const AnalysisWorkflowResult analysis = runModelFileAnalysis(
            job.modelPath, job.reports, jobRuns, jobSeed, jobThreads);

        BatchAnalysisItemResult item;
        item.ok = analysis.ok;
        item.message = analysis.message;
        item.modelPath = job.modelPath;
        item.reports = job.reports;
        item.config = analysis.config;
        item.validationIssues = analysis.validationIssues;
        if (item.ok) {
            ++result.succeeded;
        } else {
            ++result.failed;
        }
        result.items.push_back(std::move(item));
    }

    return result;
}

AnalysisWorkflowResult runModelAnalysis(const Model& model,
                                        const ReportExportPaths& reports,
                                        int runs,
                                        std::uint64_t seed,
                                        int threads) {
    AnalysisWorkflowResult result;
    result.assemblyName = model.assemblyName;

    if (runs <= 0) {
        result.message = "Run count must be positive";
        return result;
    }

    const Model analysisModel = model.activeVariantApplied();

    result.validationIssues = validateModelForSimulation(analysisModel);
    if (hasBlockingIssues(result.validationIssues)) {
        result.message = "Model validation failed";
        return result;
    }

    SimulationSettings settings;
    settings.totalRuns = runs;
    settings.initialSeed = seed;
    settings.threads = threads;
    const RunConfig config = toRunConfig(settings, 64);
    result.config = config;

    auto engine = makeMonteCarloEngine();
    MonteCarloResult detailed;
    try {
        detailed = engine->runMonteCarloDetailed(analysisModel, config);
    } catch (const std::exception& e) {
        result.message = std::string("Simulation failed: ") + e.what();
        return result;
    }
    result.stats = std::move(detailed.stats);
    result.samples = std::move(detailed.samples);
    if (result.stats.empty()) {
        result.message = "Model produced no simulation statistics";
        return result;
    }
    try {
        result.contributors = engine->runContributor(analysisModel);
    } catch (const std::exception& e) {
        result.message = std::string("Contributor analysis failed: ") + e.what();
        return result;
    }

    result.measureNames = measureNames(analysisModel);
    if (!reports.htmlPath.empty() &&
        !writeHtmlReport(reports.htmlPath, result.assemblyName, result.stats,
                         result.measureNames, std::vector<ReportImage>{},
                         result.samples, result.contributors,
                         toleranceNames(analysisModel))) {
        result.message = "Could not write HTML report";
        return result;
    }
    if (!reports.csvPath.empty() &&
        !writeCsvReport(reports.csvPath, result.stats, result.measureNames)) {
        result.message = "Could not write CSV report";
        return result;
    }
    if (!reports.excelXmlPath.empty() &&
        !writeExcelXmlReport(reports.excelXmlPath, result.stats,
                             result.measureNames)) {
        result.message = "Could not write Excel XML report";
        return result;
    }
    if (!reports.hstPath.empty() &&
        !writeHstFile(reports.hstPath, result.samples, result.stats)) {
        result.message = "Could not write HST file";
        return result;
    }
    if (!reports.hlmPath.empty() &&
        !writeHlmFile(reports.hlmPath, result.contributors)) {
        result.message = "Could not write HLM file";
        return result;
    }

    result.ok = true;
    result.message = "Model analysis workflow completed";
    return result;
}

AnalysisWorkflowResult runModelFileAnalysis(const std::string& modelPath,
                                            const ReportExportPaths& reports,
                                            int runs,
                                            std::uint64_t seed,
                                            int threads) {
    Model model;
    if (!loadModel(model, modelPath)) {
        AnalysisWorkflowResult result;
        result.message = "Could not load model";
        return result;
    }
    resolveUserDefinedSamplePaths(model,
                                  std::filesystem::path(modelPath).parent_path());
    return runModelAnalysis(model, reports, runs, seed, threads);
}

DesktopSmokeResult runStarterDesktopSmoke(const std::string& modelPath,
                                          const std::string& reportPath,
                                          int runs,
                                          std::uint64_t seed,
                                          int threads) {
    DesktopSmokeResult result;

    if (runs <= 0) {
        result.message = "Run count must be positive";
        return result;
    }
    SimulationSettings settings;
    settings.totalRuns = runs;
    settings.initialSeed = seed;
    settings.threads = threads;
    result.config = toRunConfig(settings, 64);

    const Model starter = createStarterModel();
    if (!saveModel(starter, modelPath)) {
        result.message = "Could not save starter model";
        return result;
    }

    Model loaded;
    if (!loadModel(loaded, modelPath)) {
        result.message = "Could not reopen starter model";
        return result;
    }

    ReportExportPaths reports;
    reports.htmlPath = reportPath;
    const AnalysisWorkflowResult analysis =
        runModelAnalysis(loaded, reports, runs, seed, threads);
    if (!analysis.ok) {
        result.message = analysis.message;
        return result;
    }

    result.stats = analysis.stats;
    result.config = analysis.config;
    result.ok = true;
    result.message = "Starter desktop smoke workflow completed";
    return result;
}

}  // namespace opendva
