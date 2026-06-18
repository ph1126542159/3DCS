#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "dva_test.h"
#include "opendva/app/DesktopSmokeWorkflow.h"
#include "opendva/domain/ModelEditing.h"
#include "opendva/domain/ModelSerializer.h"
#include "opendva/domain/ModelValidation.h"
#include "opendva/report/AnalysisDataFiles.h"

using namespace opendva;

namespace {

std::string readText(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

bool hasValidationIssue(const std::vector<ModelIssue>& issues,
                        const std::string& code) {
    for (const ModelIssue& issue : issues) {
        if (issue.code == code) return true;
    }
    return false;
}

}  // namespace

TEST("desktop workflow: starter model can save open run and export HTML") {
    const auto base = std::filesystem::temp_directory_path();
    const std::string modelPath =
        (base / "opendva_desktop_starter_smoke.xml").string();
    const std::string reportPath =
        (base / "opendva_desktop_starter_smoke.html").string();
    std::remove(modelPath.c_str());
    std::remove(reportPath.c_str());

    DesktopSmokeResult result =
        runStarterDesktopSmoke(modelPath, reportPath, 64, 12345);

    dvatest::check(result.ok, result.message);
    dvatest::check(result.stats.count(401) == 1,
                   "starter smoke reports measure 401");
    dvatest::check(std::filesystem::exists(modelPath),
                   "starter smoke writes model file");
    dvatest::check(std::filesystem::exists(reportPath),
                   "starter smoke writes report file");

    const std::string report = readText(reportPath);
    dvatest::check(report.find("UntitledModel") != std::string::npos,
                   "report includes starter title");
    dvatest::check(report.find("PointPoint_401") != std::string::npos,
                   "report includes measure name");

    std::remove(modelPath.c_str());
    std::remove(reportPath.c_str());
}

TEST("desktop workflow: model analysis exports selected report formats") {
    const auto base = std::filesystem::temp_directory_path();
    const std::string htmlPath =
        (base / "opendva_workflow_report.html").string();
    const std::string csvPath =
        (base / "opendva_workflow_report.csv").string();
    const std::string excelPath =
        (base / "opendva_workflow_report.xml").string();
    const std::string hstPath =
        (base / "opendva_workflow_report.hst").string();
    const std::string hlmPath =
        (base / "opendva_workflow_report.hlm").string();
    std::remove(htmlPath.c_str());
    std::remove(csvPath.c_str());
    std::remove(excelPath.c_str());
    std::remove(hstPath.c_str());
    std::remove(hlmPath.c_str());

    ReportExportPaths paths;
    paths.htmlPath = htmlPath;
    paths.csvPath = csvPath;
    paths.excelXmlPath = excelPath;
    paths.hstPath = hstPath;
    paths.hlmPath = hlmPath;

    AnalysisWorkflowResult result =
        runModelAnalysis(createStarterModel(), paths, 64, 12345);

    dvatest::check(result.ok, result.message);
    dvatest::check(result.assemblyName == "UntitledModel",
                   "analysis workflow returns assembly name");
    dvatest::check(result.stats.count(401) == 1,
                   "analysis workflow reports measure 401");
    dvatest::check(result.measureNames.at(401) == "PointPoint_401",
                   "analysis workflow returns measure display name");
    dvatest::check(std::filesystem::exists(htmlPath),
                   "analysis workflow writes HTML report");
    dvatest::check(std::filesystem::exists(csvPath),
                   "analysis workflow writes CSV report");
    dvatest::check(std::filesystem::exists(excelPath),
                   "analysis workflow writes Excel XML report");
    dvatest::check(std::filesystem::exists(hstPath),
                   "analysis workflow writes HST samples");
    dvatest::check(std::filesystem::exists(hlmPath),
                   "analysis workflow writes HLM contributors");

    const std::string csv = readText(csvPath);
    const std::string excel = readText(excelPath);
    dvatest::check(csv.find("PointPoint_401") != std::string::npos,
                   "CSV includes measure name");
    dvatest::check(excel.find("<Workbook") != std::string::npos,
                   "Excel XML includes workbook root");
    std::vector<SimulationSampleRow> loadedSamples;
    std::vector<ContributorRow> loadedContributors;
    dvatest::check(readHstFile(hstPath, loadedSamples),
                   "workflow HST can be read");
    dvatest::check(readHlmFile(hlmPath, loadedContributors),
                   "workflow HLM can be read");
    dvatest::check(!loadedSamples.empty(), "workflow HST has sample rows");
    dvatest::check(!loadedContributors.empty(),
                   "workflow HLM has contributor rows");

    std::remove(htmlPath.c_str());
    std::remove(csvPath.c_str());
    std::remove(excelPath.c_str());
    std::remove(hstPath.c_str());
    std::remove(hlmPath.c_str());
}

TEST("desktop workflow: model analysis applies active model variant") {
    Model model = createStarterModel();
    const MeasureId baseMeasureId = model.measures[0].id;
    const MeasureId scenarioMeasureId = addPointPointMeasure(model, 9.0, 11.0);
    dvatest::check(scenarioMeasureId != kInvalidId,
                   "scenario measure added");

    ModelVariant scenario = captureActiveVariant(model, "Scenario Measure");
    scenario.active = true;
    scenario.measures = {scenarioMeasureId};
    model.variants = {scenario};

    AnalysisWorkflowResult result =
        runModelAnalysis(model, ReportExportPaths{}, 64, 12345);

    dvatest::check(result.ok, result.message);
    dvatest::check(result.stats.count(scenarioMeasureId) == 1,
                   "analysis uses variant-selected measure");
    dvatest::check(result.stats.count(baseMeasureId) == 0,
                   "analysis excludes measures outside active variant");
}

TEST("desktop workflow: model analysis rejects non-positive run count") {
    ReportExportPaths paths;
    paths.htmlPath = "unused.html";

    AnalysisWorkflowResult result =
        runModelAnalysis(createStarterModel(), paths, 0, 12345);

    dvatest::check(!result.ok, "zero runs rejected");
    dvatest::check(result.message.find("Run count") != std::string::npos,
                   "run count error message");
}

TEST("desktop workflow: model analysis returns validation issues before running") {
    const auto base = std::filesystem::temp_directory_path();
    const std::string htmlPath =
        (base / "opendva_invalid_workflow_report.html").string();
    std::remove(htmlPath.c_str());

    ReportExportPaths paths;
    paths.htmlPath = htmlPath;

    AnalysisWorkflowResult result = runModelAnalysis(Model{}, paths, 64, 12345);

    dvatest::check(!result.ok, "invalid model analysis rejected");
    dvatest::check(result.message.find("validation") != std::string::npos,
                   "validation failure message");
    dvatest::check(hasValidationIssue(result.validationIssues, "model.no_parts"),
                   "missing parts validation issue returned");
    dvatest::check(hasValidationIssue(result.validationIssues,
                                      "model.no_active_measures"),
                   "missing measures validation issue returned");
    dvatest::check(!std::filesystem::exists(htmlPath),
                   "invalid model does not write report");
}

TEST("desktop workflow: model analysis reports missing userdefined smp file") {
    const auto base = std::filesystem::temp_directory_path();
    const std::string htmlPath =
        (base / "opendva_missing_smp_workflow_report.html").string();
    const std::string smpPath =
        (base / "opendva_missing_userdefined_samples.smp").string();
    std::remove(htmlPath.c_str());
    std::remove(smpPath.c_str());

    Model model = createStarterModel();
    model.parts[0].tolerances[0].ir.rands[0].distribution =
        DistributionType::UserDefined;
    model.parts[0].tolerances[0].ir.rands[0].userDefinedSamplePath = smpPath;

    ReportExportPaths paths;
    paths.htmlPath = htmlPath;

    AnalysisWorkflowResult result = runModelAnalysis(model, paths, 64, 12345);

    dvatest::check(!result.ok, "missing userdefined smp file rejects analysis");
    dvatest::check(result.message.find(".SMP") != std::string::npos,
                   "missing smp failure message names .SMP");
    dvatest::check(!std::filesystem::exists(htmlPath),
                   "missing smp model does not write report");
}

TEST("desktop workflow: saved XML model can be loaded analyzed and exported") {
    const auto base = std::filesystem::temp_directory_path();
    const std::string modelPath =
        (base / "opendva_workflow_model_in.xml").string();
    const std::string htmlPath =
        (base / "opendva_workflow_model_in.html").string();
    std::remove(modelPath.c_str());
    std::remove(htmlPath.c_str());

    dvatest::check(saveModel(createStarterModel(), modelPath),
                   "starter model saved for model-in workflow");

    ReportExportPaths paths;
    paths.htmlPath = htmlPath;

    AnalysisWorkflowResult result =
        runModelFileAnalysis(modelPath, paths, 64, 12345);

    dvatest::check(result.ok, result.message);
    dvatest::check(result.assemblyName == "UntitledModel",
                   "model-in workflow returns assembly name");
    dvatest::check(result.stats.count(401) == 1,
                   "model-in workflow reports measure 401");
    dvatest::check(result.measureNames.at(401) == "PointPoint_401",
                   "model-in workflow returns measure display name");
    dvatest::check(std::filesystem::exists(htmlPath),
                   "model-in workflow writes HTML report");

    const std::string html = readText(htmlPath);
    dvatest::check(html.find("UntitledModel") != std::string::npos,
                   "model-in report includes model title");

    std::remove(modelPath.c_str());
    std::remove(htmlPath.c_str());
}

TEST("desktop workflow: model-in resolves relative userdefined smp paths") {
    const auto base =
        std::filesystem::temp_directory_path() / "opendva_relative_smp_workflow";
    const std::filesystem::path samplesDir = base / "samples";
    const std::string modelPath = (base / "relative-smp-model.xml").string();
    const std::string smpPath = (samplesDir / "offsets.smp").string();
    const std::string htmlPath = (base / "relative-smp-report.html").string();
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(samplesDir);
    {
        std::ofstream out(smpPath);
        out << "7\n";
        out << "7\n";
    }

    Model model = createStarterModel();
    model.parts[0].tolerances[0].ir.rands[0].distribution =
        DistributionType::UserDefined;
    model.parts[0].tolerances[0].ir.rands[0].userDefinedSamplePath =
        "samples/offsets.smp";
    dvatest::check(saveModel(model, modelPath),
                   "relative smp workflow model saved");

    ReportExportPaths paths;
    paths.htmlPath = htmlPath;

    AnalysisWorkflowResult result =
        runModelFileAnalysis(modelPath, paths, 32, 12345);

    dvatest::check(result.ok, result.message);
    dvatest::check(std::filesystem::exists(htmlPath),
                   "relative smp model-in writes report");

    std::filesystem::remove_all(base);
}

TEST("desktop workflow: model-in analysis rejects unreadable XML") {
    ReportExportPaths paths;
    paths.htmlPath = "unused.html";

    AnalysisWorkflowResult result =
        runModelFileAnalysis("missing-opendva-model.xml", paths, 64, 12345);

    dvatest::check(!result.ok, "missing model path rejected");
    dvatest::check(result.message.find("load model") != std::string::npos,
                   "load failure message");
}

TEST("desktop workflow: batch model files write per-item reports") {
    const auto base = std::filesystem::temp_directory_path();
    const std::string modelAPath =
        (base / "opendva_batch_workflow_a.xml").string();
    const std::string modelBPath =
        (base / "opendva_batch_workflow_b.xml").string();
    const std::string outDir =
        (base / "opendva_batch_workflow_reports").string();
    std::filesystem::remove_all(outDir);
    std::remove(modelAPath.c_str());
    std::remove(modelBPath.c_str());

    dvatest::check(saveModel(createStarterModel(), modelAPath),
                   "first batch model saved");
    dvatest::check(saveModel(createStarterModel(), modelBPath),
                   "second batch model saved");

    const std::vector<BatchAnalysisJob> jobs = {
        BatchAnalysisJob{modelAPath, makeBatchReportPaths(outDir, 1)},
        BatchAnalysisJob{modelBPath, makeBatchReportPaths(outDir, 2)}};

    BatchAnalysisResult result = runBatchModelFileAnalysis(jobs, 32, 12345, 2);

    dvatest::check(result.succeeded == 2, "two batch jobs succeeded");
    dvatest::check(result.failed == 0, "no batch jobs failed");
    dvatest::check(result.items.size() == 2, "two batch item results returned");
    dvatest::check(result.items[0].ok, result.items[0].message);
    dvatest::check(result.items[0].config.threads == 2,
                   "batch workflow preserves requested thread count");
    for (int index = 1; index <= 2; ++index) {
        const ReportExportPaths paths = makeBatchReportPaths(outDir, index);
        dvatest::check(std::filesystem::exists(paths.htmlPath),
                       "batch workflow writes HTML");
        dvatest::check(std::filesystem::exists(paths.csvPath),
                       "batch workflow writes CSV");
        dvatest::check(std::filesystem::exists(paths.excelXmlPath),
                       "batch workflow writes Excel XML");
        dvatest::check(std::filesystem::exists(paths.hstPath),
                       "batch workflow writes HST");
        dvatest::check(std::filesystem::exists(paths.hlmPath),
                       "batch workflow writes HLM");
    }

    std::remove(modelAPath.c_str());
    std::remove(modelBPath.c_str());
    std::filesystem::remove_all(outDir);
}

TEST("desktop workflow: batch jobs are built from model paths") {
    const std::vector<std::string> modelPaths = {"a.xml", "b.xml"};
    const std::vector<BatchAnalysisJob> jobs =
        makeBatchAnalysisJobs(modelPaths, "batch-out");

    dvatest::check(jobs.size() == 2, "two batch jobs created");
    dvatest::check(jobs[0].modelPath == "a.xml", "first model path preserved");
    dvatest::check(jobs[1].modelPath == "b.xml", "second model path preserved");
    dvatest::check(jobs[0].reports.htmlPath.find("batch-1.html") !=
                       std::string::npos,
                   "first batch report path created");
    dvatest::check(jobs[0].reports.hlmPath.find("batch-1.hlm") !=
                       std::string::npos,
                   "first batch HLM path created");
    dvatest::check(jobs[1].reports.csvPath.find("batch-2.csv") !=
                       std::string::npos,
                   "second batch CSV path created");
}

TEST("desktop workflow: batch jobs are built from per-item settings") {
    const std::vector<BatchAnalysisJobInput> inputs = {
        BatchAnalysisJobInput{"a.xml", 16, 111, 1},
        BatchAnalysisJobInput{"b.xml", 32, 222, 2}};

    const std::vector<BatchAnalysisJob> jobs =
        makeBatchAnalysisJobs(inputs, "batch-out");

    dvatest::check(jobs.size() == 2, "two settings batch jobs created");
    dvatest::check(jobs[0].modelPath == "a.xml", "first settings path kept");
    dvatest::check(jobs[0].runs == 16, "first settings runs kept");
    dvatest::check(jobs[0].seed == 111, "first settings seed kept");
    dvatest::check(jobs[0].threads == 1, "first settings threads kept");
    dvatest::check(jobs[1].modelPath == "b.xml", "second settings path kept");
    dvatest::check(jobs[1].runs == 32, "second settings runs kept");
    dvatest::check(jobs[1].seed == 222, "second settings seed kept");
    dvatest::check(jobs[1].threads == 2, "second settings threads kept");
    dvatest::check(jobs[1].reports.hstPath.find("batch-2.hst") !=
                       std::string::npos,
                   "second settings HST path created");
}

TEST("desktop workflow: batch jobs can override run settings") {
    const auto base = std::filesystem::temp_directory_path();
    const std::string modelAPath =
        (base / "opendva_batch_settings_a.xml").string();
    const std::string modelBPath =
        (base / "opendva_batch_settings_b.xml").string();
    const std::string outDir =
        (base / "opendva_batch_settings_reports").string();
    std::filesystem::remove_all(outDir);
    std::remove(modelAPath.c_str());
    std::remove(modelBPath.c_str());

    dvatest::check(saveModel(createStarterModel(), modelAPath),
                   "first settings batch model saved");
    dvatest::check(saveModel(createStarterModel(), modelBPath),
                   "second settings batch model saved");

    std::vector<BatchAnalysisJob> jobs = {
        BatchAnalysisJob{modelAPath, makeBatchReportPaths(outDir, 1)},
        BatchAnalysisJob{modelBPath, makeBatchReportPaths(outDir, 2)}};
    jobs[0].runs = 16;
    jobs[0].seed = 111;
    jobs[0].threads = 1;
    jobs[1].runs = 32;
    jobs[1].seed = 222;
    jobs[1].threads = 2;

    const BatchAnalysisResult result =
        runBatchModelFileAnalysis(jobs, 64, 12345, 4);

    dvatest::check(result.succeeded == 2, "settings batch jobs succeeded");
    dvatest::check(result.items.size() == 2, "settings batch items returned");
    dvatest::check(result.items[0].config.totalRuns == 16,
                   "first job run count overrides default");
    dvatest::check(result.items[0].config.initialSeed == 111,
                   "first job seed overrides default");
    dvatest::check(result.items[0].config.threads == 1,
                   "first job threads override default");
    dvatest::check(result.items[1].config.totalRuns == 32,
                   "second job run count overrides default");
    dvatest::check(result.items[1].config.initialSeed == 222,
                   "second job seed overrides default");
    dvatest::check(result.items[1].config.threads == 2,
                   "second job threads override default");

    std::remove(modelAPath.c_str());
    std::remove(modelBPath.c_str());
    std::filesystem::remove_all(outDir);
}

TEST("desktop workflow: failed batch items can be rebuilt for retry") {
    BatchAnalysisResult result;
    BatchAnalysisItemResult okItem;
    okItem.ok = true;
    okItem.modelPath = "ok.xml";
    okItem.reports = makeBatchReportPaths("batch-out", 1);
    okItem.config.totalRuns = 16;
    okItem.config.initialSeed = 111;
    okItem.config.threads = 1;

    BatchAnalysisItemResult failedItem;
    failedItem.ok = false;
    failedItem.modelPath = "failed.xml";
    failedItem.reports = makeBatchReportPaths("batch-out", 2);
    failedItem.config.totalRuns = 32;
    failedItem.config.initialSeed = 222;
    failedItem.config.threads = 2;
    failedItem.message = "Could not load model";

    result.succeeded = 1;
    result.failed = 1;
    result.items = {okItem, failedItem};

    const std::vector<BatchAnalysisJob> retryJobs =
        makeFailedBatchRetryJobs(result);

    dvatest::check(retryJobs.size() == 1, "one failed batch retry job");
    dvatest::check(retryJobs[0].modelPath == "failed.xml",
                   "failed retry model path kept");
    dvatest::check(retryJobs[0].reports.htmlPath.find("batch-2.html") !=
                       std::string::npos,
                   "failed retry report paths kept");
    dvatest::check(retryJobs[0].runs == 32, "failed retry runs kept");
    dvatest::check(retryJobs[0].seed == 222, "failed retry seed kept");
    dvatest::check(retryJobs[0].threads == 2, "failed retry threads kept");
}

TEST("desktop workflow: retry results replace matching failed batch items") {
    BatchAnalysisItemResult okItem;
    okItem.ok = true;
    okItem.modelPath = "ok.xml";
    okItem.reports = makeBatchReportPaths("batch-out", 1);
    okItem.message = "Original ok";

    BatchAnalysisItemResult failedItem;
    failedItem.ok = false;
    failedItem.modelPath = "failed.xml";
    failedItem.reports = makeBatchReportPaths("batch-out", 2);
    failedItem.config.totalRuns = 32;
    failedItem.config.initialSeed = 222;
    failedItem.config.threads = 2;
    failedItem.message = "Could not load model";

    BatchAnalysisResult original;
    original.succeeded = 1;
    original.failed = 1;
    original.items = {okItem, failedItem};

    BatchAnalysisItemResult retryItem = failedItem;
    retryItem.ok = true;
    retryItem.config.totalRuns = 48;
    retryItem.config.initialSeed = 333;
    retryItem.config.threads = 3;
    retryItem.message = "Model analysis workflow completed";

    BatchAnalysisResult retry;
    retry.succeeded = 1;
    retry.failed = 0;
    retry.items = {retryItem};

    const BatchAnalysisResult merged =
        mergeBatchRetryResult(original, retry);

    dvatest::check(merged.succeeded == 2, "merged retry succeeded count");
    dvatest::check(merged.failed == 0, "merged retry failed count");
    dvatest::check(merged.items.size() == 2, "merged retry item count");
    dvatest::check(merged.items[0].modelPath == "ok.xml",
                   "merged retry keeps successful item");
    dvatest::check(merged.items[1].ok, "merged retry replaces failed item");
    dvatest::check(merged.items[1].config.totalRuns == 48,
                   "merged retry keeps retry config");
    dvatest::check(merged.items[1].message ==
                       "Model analysis workflow completed",
                   "merged retry keeps retry message");
}

TEST("desktop workflow: batch manifest file creates reusable jobs") {
    const auto base = std::filesystem::temp_directory_path();
    const std::string manifestPath =
        (base / "opendva_batch_manifest_reusable.txt").string();
    const std::string outDir =
        (base / "opendva_batch_manifest_reusable_reports").string();
    std::remove(manifestPath.c_str());

    {
        std::ofstream manifest(manifestPath);
        manifest << "\xEF\xBB\xBF";
        manifest << "# reusable batch manifest\n";
        manifest << "  a.xml,16,111,1  # first item settings\n";
        manifest << "incomplete.xml,64\n";
        manifest << "bad-runs.xml,abc,111,1\n";
        manifest << "\n";
        manifest << "\t# second model\n";
        manifest << "b.xml,32,222,2\n";
        manifest << "c.xml  # default settings\n";
    }

    const std::vector<BatchAnalysisJob> jobs =
        readBatchAnalysisJobs(manifestPath, outDir);

    dvatest::check(jobs.size() == 3, "manifest jobs skip comments");
    dvatest::check(std::filesystem::path(jobs[0].modelPath) ==
                       base / "a.xml",
                   "manifest first path is relative to manifest");
    dvatest::check(jobs[0].runs == 16, "manifest first runs kept");
    dvatest::check(jobs[0].seed == 111, "manifest first seed kept");
    dvatest::check(jobs[0].threads == 1, "manifest first threads kept");
    dvatest::check(jobs[0].reports.htmlPath.find("batch-1.html") !=
                       std::string::npos,
                   "manifest first report path created");
    dvatest::check(std::filesystem::path(jobs[1].modelPath) ==
                       base / "b.xml",
                   "manifest second path is relative to manifest");
    dvatest::check(jobs[1].runs == 32, "manifest second runs kept");
    dvatest::check(jobs[1].seed == 222, "manifest second seed kept");
    dvatest::check(jobs[1].threads == 2, "manifest second threads kept");
    dvatest::check(jobs[1].reports.hlmPath.find("batch-2.hlm") !=
                       std::string::npos,
                   "manifest second HLM path created");
    dvatest::check(std::filesystem::path(jobs[2].modelPath) ==
                       base / "c.xml",
                   "manifest inline path comment stripped");
    dvatest::check(jobs[2].runs == 0, "manifest default runs kept");
    dvatest::check(jobs[2].seed == 0, "manifest default seed kept");
    dvatest::check(jobs[2].threads == -1, "manifest default threads kept");

    std::remove(manifestPath.c_str());
}

TEST("desktop workflow: batch manifest preserves hash in model path") {
    const auto base = std::filesystem::temp_directory_path();
    const std::string manifestPath =
        (base / "opendva_batch_manifest_hash_paths.txt").string();
    const std::string outDir =
        (base / "opendva_batch_manifest_hash_paths_reports").string();
    std::remove(manifestPath.c_str());

    {
        std::ofstream manifest(manifestPath);
        manifest << "fixture#1.xml,16,111,1\n";
        manifest << "plain.xml # inline comment\n";
    }

    const std::vector<BatchAnalysisJob> jobs =
        readBatchAnalysisJobs(manifestPath, outDir);

    dvatest::check(jobs.size() == 2, "manifest keeps hash-path rows");
    dvatest::check(std::filesystem::path(jobs[0].modelPath) ==
                       base / "fixture#1.xml",
                   "manifest hash path is not treated as comment");
    dvatest::check(jobs[0].runs == 16, "manifest hash path settings kept");
    dvatest::check(std::filesystem::path(jobs[1].modelPath) ==
                       base / "plain.xml",
                   "manifest whitespace hash still starts inline comment");

    std::remove(manifestPath.c_str());
}

TEST("desktop workflow: batch manifest rejects invalid per-item settings") {
    const auto base = std::filesystem::temp_directory_path();
    const std::string manifestPath =
        (base / "opendva_batch_manifest_invalid_settings.txt").string();
    const std::string outDir =
        (base / "opendva_batch_manifest_invalid_settings_reports").string();
    std::remove(manifestPath.c_str());

    {
        std::ofstream manifest(manifestPath);
        manifest << "bad-runs.xml,0,111,1\n";
        manifest << "bad-seed.xml,16,0,1\n";
        manifest << "bad-threads.xml,16,111,-1\n";
        manifest << ",16,111,1\n";
        manifest << "good.xml,24,222,0\n";
    }

    const std::vector<BatchAnalysisJob> jobs =
        readBatchAnalysisJobs(manifestPath, outDir);

    dvatest::check(jobs.size() == 1,
                   "manifest skips invalid per-item settings");
    dvatest::check(std::filesystem::path(jobs[0].modelPath) ==
                       base / "good.xml",
                   "manifest keeps valid settings row");
    dvatest::check(jobs[0].runs == 24, "valid manifest runs kept");
    dvatest::check(jobs[0].seed == 222, "valid manifest seed kept");
    dvatest::check(jobs[0].threads == 0,
                   "valid manifest auto threads kept");

    std::remove(manifestPath.c_str());
}
