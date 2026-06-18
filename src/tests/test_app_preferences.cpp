// Desktop application preferences independent of Qt widgets.
#include "dva_test.h"
#include "opendva/domain/AppPreferences.h"

#include <cstdio>
#include <fstream>
#include <string>

using namespace opendva;

TEST("app preferences: defaults cover units analysis and report path") {
    const AppPreferences preferences;

    dvatest::check(preferences.lengthUnit == LengthUnit::Millimeter,
                   "default length unit is mm");
    dvatest::check(preferences.analysisDefaults.totalRuns == 10000,
                   "default runs come from simulation settings");
    dvatest::check(preferences.analysisDefaults.initialSeed == 12345,
                   "default seed comes from simulation settings");
    dvatest::check(preferences.defaultReportPath == "report.html",
                   "default report path");
}

TEST("app preferences: normalization clamps settings and restores report path") {
    AppPreferences preferences;
    preferences.analysisDefaults.totalRuns = -50;
    preferences.analysisDefaults.initialSeed = 0;
    preferences.analysisDefaults.threads = 99;
    preferences.defaultReportPath.clear();

    const AppPreferences normalized = normalizeAppPreferences(preferences, 8);

    dvatest::check(normalized.analysisDefaults.totalRuns == 1,
                   "runs normalized");
    dvatest::check(normalized.analysisDefaults.initialSeed == 1,
                   "seed normalized");
    dvatest::check(normalized.analysisDefaults.threads == 8,
                   "threads normalized");
    dvatest::check(normalized.defaultReportPath == "report.html",
                   "empty report path restored");
}

TEST("app preferences: recent model paths are unique newest first") {
    AppPreferences preferences;

    rememberRecentModelPath(preferences, "a.xml", 3);
    rememberRecentModelPath(preferences, "b.xml", 3);
    rememberRecentModelPath(preferences, "c.xml", 3);
    rememberRecentModelPath(preferences, "b.xml", 3);
    rememberRecentModelPath(preferences, "d.xml", 3);
    rememberRecentModelPath(preferences, "", 3);

    dvatest::check(preferences.recentModelPaths.size() == 3,
                   "recent file list is capped");
    dvatest::check(preferences.recentModelPaths[0] == "d.xml",
                   "newest path first");
    dvatest::check(preferences.recentModelPaths[1] == "b.xml",
                   "duplicate moved to front before cap");
    dvatest::check(preferences.recentModelPaths[2] == "c.xml",
                   "oldest retained path after cap");
}

TEST("app preferences: recent model paths reject multiline values") {
    AppPreferences preferences;

    rememberRecentModelPath(preferences, "good.xml", 8);
    rememberRecentModelPath(preferences, "bad.xml\nrecent=injected.xml", 8);
    rememberRecentModelPath(preferences, "also-bad.xml\rrecent=injected.xml", 8);

    dvatest::check(preferences.recentModelPaths.size() == 1,
                   "multiline recent values ignored");
    dvatest::check(preferences.recentModelPaths[0] == "good.xml",
                   "valid recent path retained");
}

TEST("app preferences: save and load round trip all persisted fields") {
    const std::string path = "test_app_preferences_roundtrip.ini";
    std::remove(path.c_str());

    AppPreferences preferences;
    preferences.lengthUnit = LengthUnit::Inch;
    preferences.analysisDefaults.totalRuns = 2500;
    preferences.analysisDefaults.initialSeed = 777;
    preferences.analysisDefaults.threads = 4;
    preferences.analysisDefaults.monteCarloEnabled = false;
    preferences.analysisDefaults.contributorEnabled = true;
    preferences.defaultReportPath = "reports/latest.html";
    preferences.recentModelPaths = {"first.xml", "second.xml"};

    dvatest::check(saveAppPreferences(preferences, path),
                   "preferences saved to disk");

    const AppPreferences loaded = loadAppPreferences(path);

    dvatest::check(loaded.lengthUnit == LengthUnit::Inch,
                   "length unit round-trips");
    dvatest::check(loaded.analysisDefaults.totalRuns == 2500,
                   "total runs round-trips (got " +
                       std::to_string(loaded.analysisDefaults.totalRuns) + ")");
    dvatest::check(loaded.analysisDefaults.initialSeed == 777,
                   "seed round-trips");
    dvatest::check(loaded.analysisDefaults.threads == 4,
                   "threads round-trips");
    dvatest::check(!loaded.analysisDefaults.monteCarloEnabled,
                   "monte carlo flag round-trips");
    dvatest::check(loaded.analysisDefaults.contributorEnabled,
                   "contributor flag round-trips");
    dvatest::check(loaded.defaultReportPath == "reports/latest.html",
                   "report path round-trips");
    dvatest::check(loaded.recentModelPaths.size() == 2,
                   "recent path count round-trips");
    dvatest::check(loaded.recentModelPaths[0] == "first.xml",
                   "first recent path round-trips");
    dvatest::check(loaded.recentModelPaths[1] == "second.xml",
                   "second recent path round-trips");

    std::remove(path.c_str());
}

TEST("app preferences: save filters multiline paths") {
    const std::string path = "test_app_preferences_multiline.ini";
    std::remove(path.c_str());

    AppPreferences preferences;
    preferences.defaultReportPath = "reports/latest.html\nrecent=injected.xml";
    preferences.recentModelPaths = {"good.xml", "bad.xml\nrecent=also.xml"};

    dvatest::check(saveAppPreferences(preferences, path),
                   "preferences with multiline paths saved");

    const AppPreferences loaded = loadAppPreferences(path);

    dvatest::check(loaded.defaultReportPath == "report.html",
                   "multiline report path falls back to default");
    dvatest::check(loaded.recentModelPaths.size() == 1,
                   "multiline recent path does not inject rows");
    dvatest::check(loaded.recentModelPaths[0] == "good.xml",
                   "valid recent path remains");

    std::remove(path.c_str());
}

TEST("app preferences: missing settings file loads defaults") {
    const AppPreferences loaded = loadAppPreferences("missing_app_preferences.ini");

    dvatest::check(loaded.lengthUnit == LengthUnit::Millimeter,
                   "missing file keeps default unit");
    dvatest::check(loaded.defaultReportPath == "report.html",
                   "missing file keeps default report path");
    dvatest::check(loaded.recentModelPaths.empty(),
                   "missing file keeps recent files empty");
}

TEST("app preferences: malformed numeric fields load defaults") {
    const std::string path = "test_app_preferences_malformed.ini";
    std::remove(path.c_str());
    {
        std::ofstream out(path, std::ios::binary);
        out << "version=1\n";
        out << "totalRuns=12oops\n";
        out << "initialSeed=99oops\n";
        out << "threads=4oops\n";
        out << "defaultReportPath=reports/latest.html\n";
    }

    const AppPreferences loaded = loadAppPreferences(path);

    dvatest::check(loaded.analysisDefaults.totalRuns == 10000,
                   "malformed totalRuns uses default");
    dvatest::check(loaded.analysisDefaults.initialSeed == 12345,
                   "malformed seed uses default");
    dvatest::check(loaded.analysisDefaults.threads == 1,
                   "malformed threads uses default");
    dvatest::check(loaded.defaultReportPath == "reports/latest.html",
                   "valid report path still loads");

    std::remove(path.c_str());
}

TEST("app preferences: CRLF files load persisted fields") {
    const std::string path = "test_app_preferences_crlf.ini";
    std::remove(path.c_str());
    {
        std::ofstream out(path, std::ios::binary);
        out << "version=1\r\n";
        out << "lengthUnit=inch\r\n";
        out << "totalRuns=2500\r\n";
        out << "initialSeed=777\r\n";
        out << "threads=4\r\n";
        out << "monteCarloEnabled=0\r\n";
        out << "contributorEnabled=1\r\n";
        out << "defaultReportPath=reports/latest.html\r\n";
        out << "recent=first.xml\r\n";
    }

    const AppPreferences loaded = loadAppPreferences(path);

    dvatest::check(loaded.lengthUnit == LengthUnit::Inch,
                   "CRLF length unit loads");
    dvatest::check(loaded.analysisDefaults.totalRuns == 2500,
                   "CRLF total runs loads");
    dvatest::check(loaded.analysisDefaults.initialSeed == 777,
                   "CRLF seed loads");
    dvatest::check(loaded.analysisDefaults.threads == 4,
                   "CRLF threads loads");
    dvatest::check(!loaded.analysisDefaults.monteCarloEnabled,
                   "CRLF monte carlo flag loads");
    dvatest::check(loaded.analysisDefaults.contributorEnabled,
                   "CRLF contributor flag loads");
    dvatest::check(loaded.defaultReportPath == "reports/latest.html",
                   "CRLF report path loads");
    dvatest::check(loaded.recentModelPaths.size() == 1,
                   "CRLF recent path count loads");
    dvatest::check(loaded.recentModelPaths[0] == "first.xml",
                   "CRLF recent path loads");

    std::remove(path.c_str());
}

TEST("app preferences: UTF-8 BOM first setting loads") {
    const std::string path = "test_app_preferences_bom.ini";
    std::remove(path.c_str());
    {
        std::ofstream out(path, std::ios::binary);
        out << "\xEF\xBB\xBF";
        out << "lengthUnit=inch\n";
        out << "totalRuns=2500\n";
        out << "defaultReportPath=reports/latest.html\n";
    }

    const AppPreferences loaded = loadAppPreferences(path);

    dvatest::check(loaded.lengthUnit == LengthUnit::Inch,
                   "BOM first preference key loads");
    dvatest::check(loaded.analysisDefaults.totalRuns == 2500,
                   "BOM preference later numeric key loads");
    dvatest::check(loaded.defaultReportPath == "reports/latest.html",
                   "BOM preference later path key loads");

    std::remove(path.c_str());
}
