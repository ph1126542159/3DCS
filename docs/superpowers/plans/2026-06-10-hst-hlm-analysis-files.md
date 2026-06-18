# HST HLM Analysis Files Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add minimal pure C++ read/write support for Monte Carlo HST sample files and Contributor HLM files.

**Architecture:** Keep file-format helpers in `opendva_report` because they are analysis/report artifacts, not simulation algorithms. Use deterministic text files with explicit numeric IDs so the data can round-trip without a model. Preserve existing report APIs and add small focused functions.

**Tech Stack:** C++17, existing `opendva_report`, existing `dva_test` harness, CMake/Ninja/MSVC.

---

### Task 1: HST/HLM File Helpers

**Files:**
- Create: `src/engine/report/include/opendva/report/AnalysisDataFiles.h`
- Create: `src/engine/report/src/AnalysisDataFiles.cpp`
- Modify: `src/engine/report/CMakeLists.txt`
- Test: `src/tests/test_report.cpp`

- [ ] **Step 1: Write failing tests**

Add `#include "opendva/report/AnalysisDataFiles.h"` to `src/tests/test_report.cpp`.

Add tests that write HST and HLM temp files, read them back, and check IDs and values:

```cpp
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

TEST("hlm_file_round_trips_contributor_rows") {
    std::string path = tempPath("_contributors.hlm");
    const std::vector<ContributorRow> contributors = makeContributors();

    dvatest::check(writeHlmFile(path, contributors), "write HLM ok");

    std::vector<ContributorRow> loaded;
    dvatest::check(readHlmFile(path, loaded), "read HLM ok");
    std::remove(path.c_str());

    dvatest::check(loaded.size() == 2, "two HLM rows loaded");
    dvatest::check(loaded[0].measure == 401, "first measure id loaded");
    dvatest::check(loaded[0].contributor == 701, "first contributor id loaded");
    dvatest::checkNear(loaded[0].geoFactor, 0.5, 1e-12, "GF loaded");
    dvatest::checkNear(loaded[1].contributionPct, 25.0, 1e-12,
                       "contribution percent loaded");
}
```

- [ ] **Step 2: Run RED verification**

Run:

```powershell
cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build-cmake328-ui --target opendva_tests'
```

Expected: compile fails because `AnalysisDataFiles.h` does not exist.

- [ ] **Step 3: Implement minimal helpers**

Create functions:

```cpp
bool writeHstFile(const std::string& path,
                  const std::vector<SimulationSampleRow>& samples,
                  const std::vector<MeasureId>& measureOrder);
bool readHstFile(const std::string& path,
                 std::vector<SimulationSampleRow>& samples);
bool writeHlmFile(const std::string& path,
                  const std::vector<ContributorRow>& contributors);
bool readHlmFile(const std::string& path,
                 std::vector<ContributorRow>& contributors);
```

HST header: `Build,M401,M402`. HLM header: `Measure,Contributor,GeoFactor,6Sigma,ContributionPct`. Return `false` on missing files, malformed headers, or invalid numeric fields.

- [ ] **Step 4: Run GREEN verification**

Run:

```powershell
cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build-cmake328-ui --target opendva_tests && build-cmake328-ui\tests\opendva_tests.exe'
```

Expected: all tests pass and total increases by two.
