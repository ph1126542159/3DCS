// A7 report tests (README §8): assembly state machine, Color Contour shading
// and CSV/Excel report writers. Headless, no GUI deps.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "dva_test.h"
#include "opendva/report/AssemblyState.h"
#include "opendva/report/AnalysisDataFiles.h"
#include "opendva/report/ColorContour.h"
#include "opendva/report/CsvReport.h"
#include "opendva/report/HtmlReport.h"
#include "opendva/report/ReportAssets.h"

using namespace opendva;

namespace {

// Reads an entire file into a string. Returns empty on failure.
std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Builds a unique temp file path with the given suffix (no deprecated APIs).
std::string tempPath(const std::string& suffix) {
    static int counter = 0;
    auto p = std::filesystem::temp_directory_path() /
             ("opendva_" + std::to_string(++counter) + suffix);
    return p.string();
}

// Builds a small two-measure stats map for the report writers.
std::map<MeasureId, MeasureStats> makeStats() {
    std::map<MeasureId, MeasureStats> stats;
    MeasureStats a;
    a.nominal = 10.0; a.mean = 10.25; a.sigma = 0.5; a.sixSigma = 3.0;
    a.minVal = 8.5; a.maxVal = 12.0; a.cp = 1.33; a.cpk = 1.10;
    a.totOutPct = 0.27; a.dpmo = 2700.0;
    a.histogram = {1, 3, 6, 3, 1};
    stats[401] = a;
    MeasureStats b;
    b.nominal = -2.0; b.mean = -1.9; b.sigma = 0.2; b.sixSigma = 1.2;
    b.minVal = -2.6; b.maxVal = -1.2; b.cp = 2.0; b.cpk = 1.8;
    b.totOutPct = 0.0; b.dpmo = 0.0;
    b.histogram = {0, 2, 4, 2, 0};
    stats[402] = b;
    return stats;
}

std::vector<SimulationSampleRow> makeSamples() {
    SimulationSampleRow first;
    first.buildIndex = 1;
    first.measureValues = {{401, 10.1}, {402, -1.8}};
    SimulationSampleRow second;
    second.buildIndex = 2;
    second.measureValues = {{401, 10.4}, {402, -2.1}};
    return {first, second};
}

std::vector<ContributorRow> makeContributors() {
    return {ContributorRow{401, 701, 0.5, 3.0, 75.0},
            ContributorRow{402, 702, -0.25, 1.5, 25.0}};
}

}  // namespace

// README §8.1: Nominal Build resets points, runs the move, never tolerances.
TEST("state_nominal_build") {
    AssemblyStateMachine m;
    auto e = m.nominalBuild();
    dvatest::check(m.state() == AssemblyState::Built, "nominalBuild -> Built");
    dvatest::check(e.reset, "nominalBuild resets to nominal");
    dvatest::check(!e.runTolerances, "nominalBuild never runs tolerances");
    dvatest::check(!e.leaveTrace, "nominalBuild leaves no trace");
}

// README §8.1: Assemble runs the move but keeps deviations (no reset/tol).
TEST("state_assemble_keeps_deviation") {
    AssemblyStateMachine m;
    m.nominalBuild();
    auto e = m.assemble();
    dvatest::check(m.state() == AssemblyState::Built, "assemble -> Built");
    dvatest::check(!e.reset, "assemble keeps existing deviation");
    dvatest::check(!e.runTolerances, "assemble never runs tolerances");
}

// README §8.1: Separate tears the assembly apart, no tolerances.
TEST("state_separate") {
    AssemblyStateMachine m;
    m.nominalBuild();
    auto e = m.separate();
    dvatest::check(m.state() == AssemblyState::Separated,
                   "separate -> Separated");
    dvatest::check(!e.runTolerances, "separate never runs tolerances");
}

// README §8.1: Deviate applies tolerances and lands in the statistical state.
TEST("state_deviate_runs_tolerances") {
    AssemblyStateMachine m;
    m.nominalBuild();
    auto e = m.deviate();
    dvatest::check(m.state() == AssemblyState::Deviated,
                   "deviate -> Deviated");
    dvatest::check(e.runTolerances, "deviate runs tolerances");
    dvatest::check(e.reset, "first deviate off a build resets");
    // Looping in the statistical state no longer resets.
    auto e2 = m.deviate();
    dvatest::check(!e2.reset, "subsequent deviate keeps stepping");
    dvatest::check(e2.runTolerances, "loop deviate still runs tolerances");
}

// README §8.1: Sweep is Deviate but keeps the prior result on screen (trace).
TEST("state_sweep_leaves_trace") {
    AssemblyStateMachine m;
    m.nominalBuild();
    auto e = m.sweep();
    dvatest::check(m.state() == AssemblyState::Deviated, "sweep -> Deviated");
    dvatest::check(e.runTolerances, "sweep runs tolerances");
    dvatest::check(e.leaveTrace, "sweep leaves a residue trace");
}

// README §8.3: dev=min is blue, dev=max is red, midpoint is a transition.
TEST("color_contour_endpoints") {
    Color lo = contourColor(0.0, 0.0, 100.0);
    dvatest::check(lo.b > 200 && lo.r == 0 && lo.g == 0,
                   "min deviation -> blue");
    Color hi = contourColor(100.0, 0.0, 100.0);
    dvatest::check(hi.r > 200 && hi.g == 0 && hi.b == 0,
                   "max deviation -> red");
    Color mid = contourColor(50.0, 0.0, 100.0);
    // Midpoint is a spectral transition (green-ish), neither pure blue nor red.
    dvatest::check(!(mid.b > 200 && mid.r == 0),
                   "midpoint is not pure blue");
    dvatest::check(!(mid.r > 200 && mid.b == 0),
                   "midpoint is not pure red");
    dvatest::check(mid.g > 200, "midpoint passes through green");
}

// Clamping: out-of-range deviations saturate to the endpoint colours.
TEST("color_contour_clamps") {
    Color below = contourColor(-50.0, 0.0, 100.0);
    dvatest::check(below.b > 200 && below.r == 0, "below min clamps to blue");
    Color above = contourColor(150.0, 0.0, 100.0);
    dvatest::check(above.r > 200 && above.b == 0, "above max clamps to red");
}

// README §8.3: manual scale used when valid, else auto from data range.
TEST("color_contour_non_finite_deviation_is_neutral") {
    const Color neutral =
        contourColor(std::numeric_limits<double>::quiet_NaN(), 0.0, 100.0);

    dvatest::check(neutral.g > 200, "non-finite deviation maps to neutral");
    dvatest::check(neutral.r == 0 && neutral.b == 0,
                   "neutral contour color is green");
}

TEST("color_contour_shade_nodes") {
    std::vector<double> devs = {0.0, 50.0, 100.0};
    // Manual scale 0-100 (default): values map across the full spectrum.
    auto manual = shadeNodes(devs, 0.0, 100.0);
    dvatest::check(manual.size() == 3, "one colour per node");
    dvatest::check(manual[0].b > 200 && manual[0].r == 0, "node 0 blue");
    dvatest::check(manual[2].r > 200 && manual[2].b == 0, "node 2 red");

    // Auto scale (manualMin > manualMax): same here since data spans 0..100.
    auto autoScale = shadeNodes(devs, 1.0, 0.0);
    dvatest::check(autoScale[0].b > 200 && autoScale[0].r == 0,
                   "auto min -> blue");
    dvatest::check(autoScale[2].r > 200 && autoScale[2].b == 0,
                   "auto max -> red");

    // Manual scale wider than data keeps the extremes off the endpoints.
    auto wide = shadeNodes(devs, -100.0, 200.0);
    dvatest::check(!(wide[0].r == 0 && wide[0].b > 200 && wide[0].g == 0),
                   "wider manual scale shifts node 0 off pure blue");
}

// README §8.4: CSV report has the expected header and data rows read back.
TEST("color_contour_auto_scale_ignores_non_finite_deviations") {
    const std::vector<double> devs = {
        0.0, std::numeric_limits<double>::quiet_NaN(), 100.0};

    const std::vector<Color> shaded = shadeNodes(devs, 1.0, 0.0);

    dvatest::check(shaded.size() == 3, "one color per deviation");
    dvatest::check(shaded[0].b > 200 && shaded[0].r == 0,
                   "finite minimum stays blue");
    dvatest::check(shaded[1].g > 200 && shaded[1].r == 0 &&
                       shaded[1].b == 0,
                   "non-finite node is neutral");
    dvatest::check(shaded[2].r > 200 && shaded[2].b == 0,
                   "finite maximum stays red");
}

TEST("csv_report_round_trip") {
    auto stats = makeStats();
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::string path = tempPath("_report.csv");
    dvatest::check(writeCsvReport(path, stats, names), "writeCsvReport ok");

    std::string body = slurp(path);
    std::remove(path.c_str());
    dvatest::check(
        body.find("Measure,Nominal,Mean,Sigma,6Sigma,Min,Max,Cp,Cpk,TotOUT%,"
                  "DPMO") != std::string::npos,
        "CSV header present");
    dvatest::check(body.find("gap") != std::string::npos, "gap row present");
    dvatest::check(body.find("flush") != std::string::npos,
                   "flush row present");
    // Spot-check a value from the first measure (nominal 10 -> 10.000000).
    dvatest::check(body.find("10.000000") != std::string::npos,
                   "nominal value written");
    dvatest::check(body.find("1.330000") != std::string::npos,
                   "Cp value written");
}

// README §8.4: Excel SpreadsheetML has the Workbook root and measure names.
TEST("csv_report_rejects_non_finite_statistics") {
    auto stats = makeStats();
    stats[401].mean = std::numeric_limits<double>::quiet_NaN();
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::string path = tempPath("_report_nonfinite.csv");

    dvatest::check(!writeCsvReport(path, stats, names),
                   "reject non-finite CSV stats");
    std::remove(path.c_str());
}

TEST("csv_report_rejects_negative_sigma_statistics") {
    auto stats = makeStats();
    stats[401].sigma = -0.5;
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::string path = tempPath("_report_negative_sigma.csv");

    dvatest::check(!writeCsvReport(path, stats, names),
                   "reject negative CSV sigma stats");
    std::remove(path.c_str());
}

TEST("csv_report_rejects_negative_out_of_spec_statistics") {
    auto stats = makeStats();
    stats[401].totOutPct = -0.1;
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::string path = tempPath("_report_negative_outspec.csv");

    dvatest::check(!writeCsvReport(path, stats, names),
                   "reject negative CSV out-of-spec stats");
    std::remove(path.c_str());
}

TEST("csv_report_rejects_over_max_dpmo_statistics") {
    auto stats = makeStats();
    stats[401].dpmo = 1000000.1;
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::string path = tempPath("_report_over_dpmo.csv");

    dvatest::check(!writeCsvReport(path, stats, names),
                   "reject over-max CSV DPMO stats");
    std::remove(path.c_str());
}

TEST("csv_report_rejects_reversed_min_max_statistics") {
    auto stats = makeStats();
    stats[401].minVal = 12.0;
    stats[401].maxVal = 8.5;
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::string path = tempPath("_report_reversed_minmax.csv");

    dvatest::check(!writeCsvReport(path, stats, names),
                   "reject reversed CSV min/max stats");
    std::remove(path.c_str());
}

TEST("csv_report_rejects_negative_cp_statistics") {
    auto stats = makeStats();
    stats[401].cp = -1.0;
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::string path = tempPath("_report_negative_cp.csv");

    dvatest::check(!writeCsvReport(path, stats, names),
                   "reject negative CSV Cp stats");
    std::remove(path.c_str());
}

TEST("csv_report_rejects_invalid_measure_id_statistics") {
    auto stats = makeStats();
    MeasureStats invalid = stats[401];
    stats.erase(401);
    stats[kInvalidId] = invalid;
    std::map<MeasureId, std::string> names = {{402, "flush"}};
    std::string path = tempPath("_report_invalid_measure.csv");

    dvatest::check(!writeCsvReport(path, stats, names),
                   "reject invalid CSV measure id stats");
    std::remove(path.c_str());
}

TEST("excel_xml_report") {
    auto stats = makeStats();
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::string path = tempPath("_report.xml");
    dvatest::check(writeExcelXmlReport(path, stats, names),
                   "writeExcelXmlReport ok");

    std::string body = slurp(path);
    std::remove(path.c_str());
    dvatest::check(body.find("<Workbook") != std::string::npos,
                   "Workbook root tag present");
    dvatest::check(
        body.find("urn:schemas-microsoft-com:office:spreadsheet") !=
            std::string::npos,
        "SpreadsheetML namespace present");
    dvatest::check(body.find("gap") != std::string::npos,
                   "measure name gap present");
    dvatest::check(body.find("flush") != std::string::npos,
                   "measure name flush present");
}

// README §7.6: Simulation Window / reports expose histogram graph data so the
// user can inspect distribution shape, not only summary statistics.
TEST("excel_xml_report_rejects_non_finite_statistics") {
    auto stats = makeStats();
    stats[401].sigma = std::numeric_limits<double>::infinity();
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::string path = tempPath("_report_nonfinite.xml");

    dvatest::check(!writeExcelXmlReport(path, stats, names),
                   "reject non-finite Excel XML stats");
    std::remove(path.c_str());
}

TEST("excel_xml_report_rejects_negative_sigma_statistics") {
    auto stats = makeStats();
    stats[401].sixSigma = -3.0;
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::string path = tempPath("_report_negative_sigma.xml");

    dvatest::check(!writeExcelXmlReport(path, stats, names),
                   "reject negative Excel XML sigma stats");
    std::remove(path.c_str());
}

TEST("excel_xml_report_rejects_negative_dpmo_statistics") {
    auto stats = makeStats();
    stats[401].dpmo = -1.0;
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::string path = tempPath("_report_negative_dpmo.xml");

    dvatest::check(!writeExcelXmlReport(path, stats, names),
                   "reject negative Excel XML DPMO stats");
    std::remove(path.c_str());
}

TEST("excel_xml_report_rejects_reversed_min_max_statistics") {
    auto stats = makeStats();
    stats[401].minVal = 12.0;
    stats[401].maxVal = 8.5;
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::string path = tempPath("_report_reversed_minmax.xml");

    dvatest::check(!writeExcelXmlReport(path, stats, names),
                   "reject reversed Excel XML min/max stats");
    std::remove(path.c_str());
}

TEST("excel_xml_report_rejects_negative_cp_statistics") {
    auto stats = makeStats();
    stats[401].cp = -1.0;
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::string path = tempPath("_report_negative_cp.xml");

    dvatest::check(!writeExcelXmlReport(path, stats, names),
                   "reject negative Excel XML Cp stats");
    std::remove(path.c_str());
}

TEST("excel_xml_report_rejects_invalid_measure_id_statistics") {
    auto stats = makeStats();
    MeasureStats invalid = stats[401];
    stats.erase(401);
    stats[kInvalidId] = invalid;
    std::map<MeasureId, std::string> names = {{402, "flush"}};
    std::string path = tempPath("_report_invalid_measure.xml");

    dvatest::check(!writeExcelXmlReport(path, stats, names),
                   "reject invalid Excel XML measure id stats");
    std::remove(path.c_str());
}

TEST("html_report_includes_histogram_bins") {
    auto stats = makeStats();
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::string path = tempPath("_report.html");
    dvatest::check(writeHtmlReport(path, "Simulation Window", stats, names),
                   "writeHtmlReport ok");

    std::string body = slurp(path);
    std::remove(path.c_str());
    dvatest::check(body.find("Histogram") != std::string::npos,
                   "histogram section present");
    dvatest::check(body.find("gap bin 2") != std::string::npos,
                   "gap bin label present");
    dvatest::check(body.find("<td>6</td>") != std::string::npos,
                   "gap bin count present");
    dvatest::check(body.find("flush bin 2") != std::string::npos,
                   "flush bin label present");
}

TEST("html_report_rejects_non_finite_statistics") {
    auto stats = makeStats();
    stats[401].cp = std::numeric_limits<double>::quiet_NaN();
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::string path = tempPath("_report_nonfinite.html");

    dvatest::check(!writeHtmlReport(path, "Simulation Window", stats, names),
                   "reject non-finite HTML stats");
    std::remove(path.c_str());
}

TEST("html_report_rejects_negative_sigma_statistics") {
    auto stats = makeStats();
    stats[401].sixSigma = -3.0;
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::string path = tempPath("_report_negative_sigma.html");

    dvatest::check(!writeHtmlReport(path, "Simulation Window", stats, names),
                   "reject negative HTML sigma stats");
    std::remove(path.c_str());
}

TEST("html_report_rejects_over_100_out_of_spec_statistics") {
    auto stats = makeStats();
    stats[401].totOutPct = 100.1;
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::string path = tempPath("_report_over_outspec.html");

    dvatest::check(!writeHtmlReport(path, "Simulation Window", stats, names),
                   "reject over-100 HTML out-of-spec stats");
    std::remove(path.c_str());
}

TEST("html_report_rejects_over_max_dpmo_statistics") {
    auto stats = makeStats();
    stats[401].dpmo = 1000000.1;
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::string path = tempPath("_report_over_dpmo.html");

    dvatest::check(!writeHtmlReport(path, "Simulation Window", stats, names),
                   "reject over-max HTML DPMO stats");
    std::remove(path.c_str());
}

TEST("html_report_rejects_reversed_min_max_statistics") {
    auto stats = makeStats();
    stats[401].minVal = 12.0;
    stats[401].maxVal = 8.5;
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::string path = tempPath("_report_reversed_minmax.html");

    dvatest::check(!writeHtmlReport(path, "Simulation Window", stats, names),
                   "reject reversed HTML min/max stats");
    std::remove(path.c_str());
}

TEST("html_report_rejects_negative_cp_statistics") {
    auto stats = makeStats();
    stats[401].cp = -1.0;
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::string path = tempPath("_report_negative_cp.html");

    dvatest::check(!writeHtmlReport(path, "Simulation Window", stats, names),
                   "reject negative HTML Cp stats");
    std::remove(path.c_str());
}

TEST("html_report_rejects_invalid_measure_id_statistics") {
    auto stats = makeStats();
    MeasureStats invalid = stats[401];
    stats.erase(401);
    stats[kInvalidId] = invalid;
    std::map<MeasureId, std::string> names = {{402, "flush"}};
    std::string path = tempPath("_report_invalid_measure.html");

    dvatest::check(!writeHtmlReport(path, "Simulation Window", stats, names),
                   "reject invalid HTML measure id stats");
    std::remove(path.c_str());
}

TEST("html_report_includes_optional_view_images") {
    auto stats = makeStats();
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::string path = tempPath("_report_image.html");

    const std::vector<ReportImage> images = {
        ReportImage{"Model View", "captures/model-view.png",
                    "Contributor color contour after Monte Carlo"}
    };

    dvatest::check(writeHtmlReport(path, "Simulation Window", stats, names, images),
                   "writeHtmlReport with image ok");

    std::string body = slurp(path);
    std::remove(path.c_str());
    dvatest::check(body.find("Model View") != std::string::npos,
                   "image section title present");
    dvatest::check(body.find("<img") != std::string::npos,
                   "image element present");
    dvatest::check(body.find("src=\"captures/model-view.png\"") != std::string::npos,
                   "image source present");
    dvatest::check(body.find("Contributor color contour after Monte Carlo") !=
                       std::string::npos,
                   "image caption present");
}

TEST("html_report_snapshot_asset_paths_follow_report_stem") {
    const ReportImageAsset asset =
        planHtmlSnapshotAsset("C:/work/reports/run-42.html", "Model View",
                              "Current Qt3D color contour view");

    dvatest::check(asset.image.title == "Model View", "asset title carried");
    dvatest::check(asset.image.caption == "Current Qt3D color contour view",
                   "asset caption carried");
    dvatest::check(asset.filePath.find("run-42-view.png") != std::string::npos,
                   "snapshot file uses report stem");
    dvatest::check(asset.image.source == "run-42-view.png",
                   "HTML image source is relative filename");
}

TEST("html_report_includes_samples_and_contributors") {
    auto stats = makeStats();
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::map<ToleranceId, std::string> toleranceNames = {{701, "pin X"},
                                                         {702, "slot Y"}};
    std::string path = tempPath("_report_details.html");

    dvatest::check(writeHtmlReport(path, "Simulation Window", stats, names, {},
                                   makeSamples(), makeContributors(),
                                   toleranceNames),
                   "writeHtmlReport details ok");

    std::string body = slurp(path);
    std::remove(path.c_str());
    dvatest::check(body.find("Samples") != std::string::npos,
                   "samples section present");
    dvatest::check(body.find("<td>1</td>") != std::string::npos,
                   "sample build present");
    dvatest::check(body.find("10.1000") != std::string::npos,
                   "sample value present");
    dvatest::check(body.find("Contributor") != std::string::npos,
                   "contributor section present");
    dvatest::check(body.find("pin X") != std::string::npos,
                   "contributor name present");
    dvatest::check(body.find("75.0000") != std::string::npos,
                   "contribution percent present");
}

TEST("html_report_rejects_non_finite_sample_details") {
    auto stats = makeStats();
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::vector<SimulationSampleRow> samples = makeSamples();
    samples[0].measureValues[401] = std::numeric_limits<double>::infinity();
    std::string path = tempPath("_report_sample_nonfinite.html");

    dvatest::check(!writeHtmlReport(path, "Simulation Window", stats, names, {},
                                    samples, makeContributors(), {}),
                   "reject non-finite HTML sample details");
    std::remove(path.c_str());
}

TEST("html_report_rejects_invalid_sample_measure_id") {
    auto stats = makeStats();
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::vector<SimulationSampleRow> samples = makeSamples();
    samples[0].measureValues[kInvalidId] = 1.0;
    std::string path = tempPath("_report_sample_invalid_measure.html");

    dvatest::check(!writeHtmlReport(path, "Simulation Window", stats, names, {},
                                    samples, makeContributors(), {}),
                   "reject invalid HTML sample measure id");
    std::remove(path.c_str());
}

TEST("html_report_rejects_sample_for_unreported_measure") {
    auto stats = makeStats();
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::vector<SimulationSampleRow> samples = makeSamples();
    samples[0].measureValues[999] = 1.0;
    std::string path = tempPath("_report_sample_unknown_measure.html");

    dvatest::check(!writeHtmlReport(path, "Simulation Window", stats, names, {},
                                    samples, makeContributors(), {}),
                   "reject HTML sample measure outside report stats");
    std::remove(path.c_str());
}

TEST("html_report_rejects_negative_sample_builds") {
    auto stats = makeStats();
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::vector<SimulationSampleRow> samples = makeSamples();
    samples[0].buildIndex = -1;
    std::string path = tempPath("_report_sample_negative_build.html");

    dvatest::check(!writeHtmlReport(path, "Simulation Window", stats, names, {},
                                    samples, makeContributors(), {}),
                   "reject negative HTML sample build indices");
    std::remove(path.c_str());
}

TEST("html_report_rejects_duplicate_sample_builds") {
    auto stats = makeStats();
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::vector<SimulationSampleRow> samples = makeSamples();
    samples[1].buildIndex = samples[0].buildIndex;
    std::string path = tempPath("_report_sample_duplicate_build.html");

    dvatest::check(!writeHtmlReport(path, "Simulation Window", stats, names, {},
                                    samples, makeContributors(), {}),
                   "reject duplicate HTML sample build indices");
    std::remove(path.c_str());
}

TEST("html_report_rejects_non_finite_contributor_details") {
    auto stats = makeStats();
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::vector<ContributorRow> contributors = makeContributors();
    contributors[0].sixSigma = std::numeric_limits<double>::quiet_NaN();
    std::string path = tempPath("_report_contributor_nonfinite.html");

    dvatest::check(!writeHtmlReport(path, "Simulation Window", stats, names, {},
                                    makeSamples(), contributors, {}),
                   "reject non-finite HTML contributor details");
    std::remove(path.c_str());
}

TEST("html_report_rejects_negative_contributor_sixsigma") {
    auto stats = makeStats();
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::vector<ContributorRow> contributors = makeContributors();
    contributors[0].sixSigma = -3.0;
    std::string path = tempPath("_report_contributor_negative_sixsigma.html");

    dvatest::check(!writeHtmlReport(path, "Simulation Window", stats, names, {},
                                    makeSamples(), contributors, {}),
                   "reject negative HTML contributor six sigma");
    std::remove(path.c_str());
}

TEST("html_report_rejects_negative_contributor_percentage") {
    auto stats = makeStats();
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::vector<ContributorRow> contributors = makeContributors();
    contributors[0].contributionPct = -1.0;
    std::string path = tempPath("_report_contributor_negative_pct.html");

    dvatest::check(!writeHtmlReport(path, "Simulation Window", stats, names, {},
                                    makeSamples(), contributors, {}),
                   "reject negative HTML contributor percentage");
    std::remove(path.c_str());
}

TEST("html_report_rejects_duplicate_contributor_details") {
    auto stats = makeStats();
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::vector<ContributorRow> contributors = makeContributors();
    contributors[1].measure = contributors[0].measure;
    contributors[1].contributor = contributors[0].contributor;
    std::string path = tempPath("_report_contributor_duplicate.html");

    dvatest::check(!writeHtmlReport(path, "Simulation Window", stats, names, {},
                                   makeSamples(), contributors, {}),
                   "reject duplicate HTML contributor details");
    std::remove(path.c_str());
}

TEST("html_report_rejects_invalid_contributor_ids") {
    auto stats = makeStats();
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::vector<ContributorRow> contributors = makeContributors();
    contributors[0].contributor = kInvalidId;
    std::string path = tempPath("_report_contributor_invalid_id.html");

    dvatest::check(!writeHtmlReport(path, "Simulation Window", stats, names, {},
                                    makeSamples(), contributors, {}),
                   "reject invalid HTML contributor ids");
    std::remove(path.c_str());
}

TEST("html_report_rejects_contributor_for_unreported_measure") {
    auto stats = makeStats();
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::vector<ContributorRow> contributors = makeContributors();
    contributors[0].measure = 999;
    std::string path = tempPath("_report_contributor_unknown_measure.html");

    dvatest::check(!writeHtmlReport(path, "Simulation Window", stats, names, {},
                                    makeSamples(), contributors, {}),
                   "reject HTML contributor measure outside report stats");
    std::remove(path.c_str());
}

TEST("hst_file_round_trips_sample_rows") {
    std::string path = tempPath("_samples.hst");
    const std::vector<MeasureId> measureOrder = {401, 402};
    const std::vector<SimulationSampleRow> samples = makeSamples();

    dvatest::check(writeHstFile(path, samples, measureOrder), "write HST ok");

    std::vector<SimulationSampleRow> loaded;
    dvatest::check(readHstFile(path, loaded), "read HST ok");
    std::remove(path.c_str());

    dvatest::check(loaded.size() == 2, "two HST rows loaded");
    dvatest::check(loaded[0].buildIndex == 1, "first build index loaded");
    dvatest::checkNear(loaded[0].measureValues.at(401), 10.1, 1e-12,
                       "first gap sample loaded");
    dvatest::checkNear(loaded[1].measureValues.at(402), -2.1, 1e-12,
                       "second flush sample loaded");
}

TEST("hst_file_reads_measure_order_for_window_reload") {
    std::string path = tempPath("_samples_order.hst");
    const std::vector<MeasureId> measureOrder = {402, 401};
    const std::vector<SimulationSampleRow> samples = makeSamples();

    dvatest::check(writeHstFile(path, samples, measureOrder), "write HST ok");

    std::vector<SimulationSampleRow> loaded;
    std::vector<MeasureId> loadedOrder;
    dvatest::check(readHstFile(path, loaded, loadedOrder),
                   "read HST with measure order ok");
    std::remove(path.c_str());

    dvatest::check(loadedOrder.size() == 2, "two measure headers loaded");
    dvatest::check(loadedOrder[0] == 402, "first HST measure header loaded");
    dvatest::check(loadedOrder[1] == 401, "second HST measure header loaded");
    dvatest::check(loaded.size() == 2, "sample rows still loaded");
}

TEST("hst_file_writes_measure_columns_from_stats_order") {
    std::string path = tempPath("_samples_from_stats.hst");
    const std::vector<SimulationSampleRow> samples = makeSamples();

    dvatest::check(writeHstFile(path, samples, makeStats()),
                   "write HST from stats ok");

    const std::string body = slurp(path);
    std::remove(path.c_str());

    dvatest::check(body.find("Build,M401,M402") != std::string::npos,
                   "HST header follows stats measure order");
}

TEST("hst_file_rejects_non_finite_sample_export") {
    std::string path = tempPath("_samples_export_nonfinite.hst");
    std::vector<SimulationSampleRow> samples = makeSamples();
    samples[0].measureValues[401] = std::numeric_limits<double>::infinity();

    dvatest::check(!writeHstFile(path, samples, std::vector<MeasureId>{401}),
                   "reject non-finite HST export value");
    std::remove(path.c_str());
}

TEST("hst_file_rejects_invalid_sample_measure_id_export") {
    std::string path = tempPath("_samples_export_invalid_measure.hst");
    std::vector<SimulationSampleRow> samples = makeSamples();
    samples[0].measureValues[kInvalidId] = 1.0;

    dvatest::check(!writeHstFile(path, samples, std::vector<MeasureId>{401}),
                   "reject invalid HST export sample measure id");
    std::remove(path.c_str());
}

TEST("hst_file_rejects_non_finite_unexported_sample_value") {
    std::string path = tempPath("_samples_export_extra_nonfinite.hst");
    std::vector<SimulationSampleRow> samples = makeSamples();
    samples[0].measureValues[999] = std::numeric_limits<double>::infinity();

    dvatest::check(!writeHstFile(path, samples, std::vector<MeasureId>{401}),
                   "reject non-finite HST value outside export order");
    std::remove(path.c_str());
}

TEST("hst_file_rejects_negative_build_index_export") {
    std::string path = tempPath("_samples_export_negative_build.hst");
    std::vector<SimulationSampleRow> samples = makeSamples();
    samples[0].buildIndex = -1;

    dvatest::check(!writeHstFile(path, samples, std::vector<MeasureId>{401}),
                   "reject negative HST export build index");
    std::remove(path.c_str());
}

TEST("hst_file_rejects_duplicate_measure_headers_export") {
    std::string path = tempPath("_samples_export_duplicate_header.hst");

    dvatest::check(!writeHstFile(path, makeSamples(), std::vector<MeasureId>{401, 401}),
                   "reject duplicate HST export measure headers");
    std::remove(path.c_str());
}

TEST("hst_file_rejects_invalid_measure_header_export") {
    std::string path = tempPath("_samples_export_invalid_header.hst");

    dvatest::check(!writeHstFile(path, makeSamples(),
                                 std::vector<MeasureId>{401, kInvalidId}),
                   "reject invalid HST export measure header");
    std::remove(path.c_str());
}

TEST("hst_file_rejects_empty_measure_order_without_clobbering_file") {
    std::string path = tempPath("_samples_export_empty_order.hst");
    {
        std::ofstream out(path, std::ios::binary);
        out << "keep existing content\n";
    }

    dvatest::check(!writeHstFile(path, makeSamples(), std::vector<MeasureId>{}),
                   "reject empty HST export measure order");
    dvatest::check(slurp(path) == "keep existing content\n",
                   "failed empty-order HST export leaves existing file");
    std::remove(path.c_str());
}

TEST("hst_file_rejects_duplicate_build_indices_export") {
    std::string path = tempPath("_samples_export_duplicate_build.hst");
    std::vector<SimulationSampleRow> samples = makeSamples();
    samples[1].buildIndex = samples[0].buildIndex;

    dvatest::check(!writeHstFile(path, samples, std::vector<MeasureId>{401}),
                   "reject duplicate HST export build indices");
    std::remove(path.c_str());
}

TEST("hst_file_rejects_non_finite_sample_values") {
    std::string path = tempPath("_samples_nonfinite.hst");
    {
        std::ofstream out(path, std::ios::binary);
        out << "Build,M401\n";
        out << "1,nan\n";
    }

    std::vector<SimulationSampleRow> loaded = makeSamples();
    std::vector<MeasureId> loadedOrder = {401};
    dvatest::check(!readHstFile(path, loaded, loadedOrder),
                   "reject non-finite HST value");
    std::remove(path.c_str());

    dvatest::check(loaded.size() == 2, "failed HST read leaves rows");
    dvatest::check(loadedOrder.size() == 1 && loadedOrder[0] == 401,
                   "failed HST read leaves order");
}

TEST("hst_file_rejects_hexadecimal_sample_values") {
    std::string path = tempPath("_samples_hexfloat.hst");
    {
        std::ofstream out(path, std::ios::binary);
        out << "Build,M401\n";
        out << "1,0x1p1\n";
    }

    std::vector<SimulationSampleRow> loaded = makeSamples();
    std::vector<MeasureId> loadedOrder = {401};
    dvatest::check(!readHstFile(path, loaded, loadedOrder),
                   "reject hexadecimal HST value");
    std::remove(path.c_str());

    dvatest::check(loaded.size() == 2, "failed HST hex read leaves rows");
    dvatest::check(loadedOrder.size() == 1 && loadedOrder[0] == 401,
                   "failed HST hex read leaves order");
}

TEST("hst_file_rejects_duplicate_measure_headers") {
    std::string path = tempPath("_samples_duplicate_measure_header.hst");
    {
        std::ofstream out(path, std::ios::binary);
        out << "Build,M401,M401\n";
        out << "1,10.1,20.2\n";
    }

    std::vector<SimulationSampleRow> loaded = makeSamples();
    std::vector<MeasureId> loadedOrder = {401};
    dvatest::check(!readHstFile(path, loaded, loadedOrder),
                   "reject duplicate HST measure header");
    std::remove(path.c_str());

    dvatest::check(loaded.size() == 2, "failed HST duplicate header read leaves rows");
    dvatest::check(loadedOrder.size() == 1 && loadedOrder[0] == 401,
                   "failed HST duplicate header read leaves order");
}

TEST("hst_file_rejects_invalid_measure_header") {
    std::string path = tempPath("_samples_invalid_measure_header.hst");
    {
        std::ofstream out(path, std::ios::binary);
        out << "Build,M0\n";
        out << "1,10.1\n";
    }

    std::vector<SimulationSampleRow> loaded = makeSamples();
    std::vector<MeasureId> loadedOrder = {401};
    dvatest::check(!readHstFile(path, loaded, loadedOrder),
                   "reject invalid HST measure header");
    std::remove(path.c_str());

    dvatest::check(loaded.size() == 2, "failed HST invalid header read leaves rows");
    dvatest::check(loadedOrder.size() == 1 && loadedOrder[0] == 401,
                   "failed HST invalid header read leaves order");
}

TEST("hst_file_rejects_duplicate_build_indices") {
    std::string path = tempPath("_samples_duplicate_build.hst");
    {
        std::ofstream out(path, std::ios::binary);
        out << "Build,M401\n";
        out << "1,10.1\n";
        out << "1,20.2\n";
    }

    std::vector<SimulationSampleRow> loaded = makeSamples();
    std::vector<MeasureId> loadedOrder = {401};
    dvatest::check(!readHstFile(path, loaded, loadedOrder),
                   "reject duplicate HST build indices");
    std::remove(path.c_str());

    dvatest::check(loaded.size() == 2, "failed HST duplicate build read leaves rows");
    dvatest::check(loadedOrder.size() == 1 && loadedOrder[0] == 401,
                   "failed HST duplicate build read leaves order");
}

TEST("hst_file_rejects_negative_build_index") {
    std::string path = tempPath("_samples_negative_build.hst");
    {
        std::ofstream out(path, std::ios::binary);
        out << "Build,M401\n";
        out << "-1,10.1\n";
    }

    std::vector<SimulationSampleRow> loaded = makeSamples();
    std::vector<MeasureId> loadedOrder = {401};
    dvatest::check(!readHstFile(path, loaded, loadedOrder),
                   "reject negative HST build index");
    std::remove(path.c_str());

    dvatest::check(loaded.size() == 2, "failed HST negative read leaves rows");
    dvatest::check(loadedOrder.size() == 1 && loadedOrder[0] == 401,
                   "failed HST negative read leaves order");
}

TEST("hst_file_rejects_overflow_build_index") {
    std::string path = tempPath("_samples_overflow_build.hst");
    {
        std::ofstream out(path, std::ios::binary);
        out << "Build,M401\n";
        out << "2147483648,10.1\n";
    }

    std::vector<SimulationSampleRow> loaded = makeSamples();
    std::vector<MeasureId> loadedOrder = {401};
    dvatest::check(!readHstFile(path, loaded, loadedOrder),
                   "reject overflowing HST build index");
    std::remove(path.c_str());

    dvatest::check(loaded.size() == 2, "failed HST overflow read leaves rows");
    dvatest::check(loadedOrder.size() == 1 && loadedOrder[0] == 401,
                   "failed HST overflow read leaves order");
}

TEST("hlm_file_round_trips_contributor_rows") {
    std::string path = tempPath("_contributors.hlm");
    const std::vector<ContributorRow> contributors = makeContributors();

    dvatest::check(writeHlmFile(path, contributors), "write HLM ok");

    std::vector<ContributorRow> loaded;
    dvatest::check(readHlmFile(path, loaded), "read HLM ok");
    std::remove(path.c_str());

    dvatest::check(loaded.size() == 2, "two HLM rows loaded");
    dvatest::check(loaded[0].measure == 401, "first measure id loaded");
    dvatest::check(loaded[0].contributor == 701,
                   "first contributor id loaded");
    dvatest::checkNear(loaded[0].geoFactor, 0.5, 1e-12, "GF loaded");
    dvatest::checkNear(loaded[1].contributionPct, 25.0, 1e-12,
                       "contribution percent loaded");
}

TEST("analysis_data_files_read_crlf_line_endings") {
    std::string hstPath = tempPath("_samples_crlf.hst");
    {
        std::ofstream out(hstPath, std::ios::binary);
        out << "Build,M401,M402\r\n";
        out << "1,10.1,-1.8\r\n";
        out << "2,10.4,-2.1\r\n";
    }

    std::vector<SimulationSampleRow> samples;
    std::vector<MeasureId> measureOrder;
    dvatest::check(readHstFile(hstPath, samples, measureOrder),
                   "read CRLF HST file");
    std::remove(hstPath.c_str());

    dvatest::check(measureOrder.size() == 2, "CRLF HST measure order count");
    dvatest::check(measureOrder[1] == 402, "CRLF HST final header parsed");
    dvatest::check(samples.size() == 2, "CRLF HST sample rows loaded");
    dvatest::checkNear(samples[1].measureValues.at(402), -2.1, 1e-12,
                       "CRLF HST final value parsed");

    std::string hlmPath = tempPath("_contributors_crlf.hlm");
    {
        std::ofstream out(hlmPath, std::ios::binary);
        out << "Measure,Contributor,GeoFactor,6Sigma,ContributionPct\r\n";
        out << "401,701,0.5,3.0,75.0\r\n";
        out << "402,702,-0.25,1.5,25.0\r\n";
    }

    std::vector<ContributorRow> contributors;
    dvatest::check(readHlmFile(hlmPath, contributors),
                   "read CRLF HLM file");
    std::remove(hlmPath.c_str());

    dvatest::check(contributors.size() == 2, "CRLF HLM rows loaded");
    dvatest::check(contributors[1].contributor == 702,
                   "CRLF HLM final contributor id parsed");
    dvatest::checkNear(contributors[1].contributionPct, 25.0, 1e-12,
                       "CRLF HLM final value parsed");
}

TEST("analysis_data_files_read_utf8_bom_headers") {
    std::string hstPath = tempPath("_samples_bom.hst");
    {
        std::ofstream out(hstPath, std::ios::binary);
        out << "\xEF\xBB\xBF";
        out << "Build,M401,M402\n";
        out << "1,10.1,-1.8\n";
    }

    std::vector<SimulationSampleRow> samples;
    std::vector<MeasureId> measureOrder;
    dvatest::check(readHstFile(hstPath, samples, measureOrder),
                   "read UTF-8 BOM HST file");
    std::remove(hstPath.c_str());

    dvatest::check(measureOrder.size() == 2, "BOM HST measure order count");
    dvatest::check(measureOrder[0] == 401, "BOM HST first header parsed");
    dvatest::check(samples.size() == 1, "BOM HST sample row loaded");

    std::string hlmPath = tempPath("_contributors_bom.hlm");
    {
        std::ofstream out(hlmPath, std::ios::binary);
        out << "\xEF\xBB\xBF";
        out << "Measure,Contributor,GeoFactor,6Sigma,ContributionPct\n";
        out << "401,701,0.5,3.0,75.0\n";
    }

    std::vector<ContributorRow> contributors;
    dvatest::check(readHlmFile(hlmPath, contributors),
                   "read UTF-8 BOM HLM file");
    std::remove(hlmPath.c_str());

    dvatest::check(contributors.size() == 1, "BOM HLM row loaded");
    dvatest::check(contributors[0].measure == 401,
                   "BOM HLM measure id parsed");
}

TEST("hlm_file_rejects_non_finite_contributor_export") {
    std::string path = tempPath("_contributors_export_nonfinite.hlm");
    std::vector<ContributorRow> contributors = makeContributors();
    contributors[0].geoFactor = std::numeric_limits<double>::quiet_NaN();

    dvatest::check(!writeHlmFile(path, contributors),
                   "reject non-finite HLM export value");
    std::remove(path.c_str());
}

TEST("hlm_file_rejects_negative_contributor_sixsigma_export") {
    std::string path = tempPath("_contributors_export_negative_sixsigma.hlm");
    std::vector<ContributorRow> contributors = makeContributors();
    contributors[0].sixSigma = -3.0;

    dvatest::check(!writeHlmFile(path, contributors),
                   "reject negative HLM export contributor six sigma");
    std::remove(path.c_str());
}

TEST("hlm_file_rejects_over_100_contributor_percentage_export") {
    std::string path = tempPath("_contributors_export_over_pct.hlm");
    std::vector<ContributorRow> contributors = makeContributors();
    contributors[0].contributionPct = 101.0;

    dvatest::check(!writeHlmFile(path, contributors),
                   "reject over-100 HLM export contributor percentage");
    std::remove(path.c_str());
}

TEST("hlm_file_rejects_duplicate_contributor_rows_export") {
    std::string path = tempPath("_contributors_export_duplicate_rows.hlm");
    std::vector<ContributorRow> contributors = makeContributors();
    contributors.push_back(contributors.front());

    dvatest::check(!writeHlmFile(path, contributors),
                   "reject duplicate HLM export contributor rows");
    std::remove(path.c_str());
}

TEST("hlm_file_rejects_invalid_contributor_ids_export") {
    std::string path = tempPath("_contributors_export_invalid_id.hlm");
    std::vector<ContributorRow> contributors = makeContributors();
    contributors[0].measure = kInvalidId;

    dvatest::check(!writeHlmFile(path, contributors),
                   "reject invalid HLM export contributor ids");
    std::remove(path.c_str());
}

TEST("hlm_file_rejects_non_finite_contributor_values") {
    std::string path = tempPath("_contributors_nonfinite.hlm");
    {
        std::ofstream out(path, std::ios::binary);
        out << "Measure,Contributor,GeoFactor,6Sigma,ContributionPct\n";
        out << "401,701,inf,1.0,75.0\n";
    }

    std::vector<ContributorRow> loaded = makeContributors();
    dvatest::check(!readHlmFile(path, loaded),
                   "reject non-finite HLM value");
    std::remove(path.c_str());

    dvatest::check(loaded.size() == 2, "failed HLM read leaves rows");
    dvatest::check(loaded[0].measure == 401 && loaded[0].contributor == 701,
                   "failed HLM read leaves row data");
}

TEST("hlm_file_rejects_negative_contributor_sixsigma") {
    std::string path = tempPath("_contributors_negative_sixsigma.hlm");
    {
        std::ofstream out(path, std::ios::binary);
        out << "Measure,Contributor,GeoFactor,6Sigma,ContributionPct\n";
        out << "401,701,0.5,-3.0,75.0\n";
    }

    std::vector<ContributorRow> loaded = makeContributors();
    dvatest::check(!readHlmFile(path, loaded),
                   "reject negative HLM contributor six sigma");
    std::remove(path.c_str());

    dvatest::check(loaded.size() == 2,
                   "failed HLM negative six sigma read leaves rows");
    dvatest::check(loaded[0].measure == 401 && loaded[0].contributor == 701,
                   "failed HLM negative six sigma read leaves row data");
}

TEST("hlm_file_rejects_negative_contributor_percentage") {
    std::string path = tempPath("_contributors_negative_pct.hlm");
    {
        std::ofstream out(path, std::ios::binary);
        out << "Measure,Contributor,GeoFactor,6Sigma,ContributionPct\n";
        out << "401,701,0.5,3.0,-1.0\n";
    }

    std::vector<ContributorRow> loaded = makeContributors();
    dvatest::check(!readHlmFile(path, loaded),
                   "reject negative HLM contributor percentage");
    std::remove(path.c_str());

    dvatest::check(loaded.size() == 2,
                   "failed HLM negative percentage read leaves rows");
    dvatest::check(loaded[0].measure == 401 && loaded[0].contributor == 701,
                   "failed HLM negative percentage read leaves row data");
}

TEST("hlm_file_rejects_hexadecimal_contributor_values") {
    std::string path = tempPath("_contributors_hexfloat.hlm");
    {
        std::ofstream out(path, std::ios::binary);
        out << "Measure,Contributor,GeoFactor,6Sigma,ContributionPct\n";
        out << "401,701,0x1p1,1.0,75.0\n";
    }

    std::vector<ContributorRow> loaded = makeContributors();
    dvatest::check(!readHlmFile(path, loaded),
                   "reject hexadecimal HLM value");
    std::remove(path.c_str());

    dvatest::check(loaded.size() == 2, "failed HLM hex read leaves rows");
    dvatest::check(loaded[0].measure == 401 && loaded[0].contributor == 701,
                   "failed HLM hex read leaves row data");
}

TEST("hlm_file_rejects_duplicate_contributor_rows") {
    std::string path = tempPath("_contributors_duplicate_rows.hlm");
    {
        std::ofstream out(path, std::ios::binary);
        out << "Measure,Contributor,GeoFactor,6Sigma,ContributionPct\n";
        out << "401,701,0.5,3.0,75.0\n";
        out << "401,701,0.6,3.6,25.0\n";
    }

    std::vector<ContributorRow> loaded = makeContributors();
    dvatest::check(!readHlmFile(path, loaded),
                   "reject duplicate HLM contributor rows");
    std::remove(path.c_str());

    dvatest::check(loaded.size() == 2, "failed HLM duplicate read leaves rows");
    dvatest::check(loaded[0].measure == 401 && loaded[0].contributor == 701,
                   "failed HLM duplicate read leaves row data");
}

TEST("hlm_file_rejects_invalid_contributor_ids") {
    std::string path = tempPath("_contributors_invalid_id.hlm");
    {
        std::ofstream out(path, std::ios::binary);
        out << "Measure,Contributor,GeoFactor,6Sigma,ContributionPct\n";
        out << "401,0,0.5,3.0,75.0\n";
    }

    std::vector<ContributorRow> loaded = makeContributors();
    dvatest::check(!readHlmFile(path, loaded),
                   "reject invalid HLM contributor ids");
    std::remove(path.c_str());

    dvatest::check(loaded.size() == 2, "failed HLM invalid id read leaves rows");
    dvatest::check(loaded[0].measure == 401 && loaded[0].contributor == 701,
                   "failed HLM invalid id read leaves row data");
}

TEST("hlm_file_rejects_overflow_ids") {
    std::string path = tempPath("_contributors_overflow_ids.hlm");
    {
        std::ofstream out(path, std::ios::binary);
        out << "Measure,Contributor,GeoFactor,6Sigma,ContributionPct\n";
        out << "18446744073709551616,701,0.5,1.0,75.0\n";
    }

    std::vector<ContributorRow> loaded = makeContributors();
    dvatest::check(!readHlmFile(path, loaded),
                   "reject overflowing HLM unsigned id");
    std::remove(path.c_str());

    dvatest::check(loaded.size() == 2, "failed HLM overflow read leaves rows");
    dvatest::check(loaded[0].measure == 401 && loaded[0].contributor == 701,
                   "failed HLM overflow read leaves row data");
}
