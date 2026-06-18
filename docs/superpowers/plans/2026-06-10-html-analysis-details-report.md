# HTML Analysis Details Report Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Include Simulation Window sample rows and Contributor rows in exported HTML reports.

**Architecture:** Keep HTML generation in `opendva_report` and expose a small overload that accepts optional sample/contributor detail data. Preserve existing callers by forwarding old overloads to the new one with empty detail vectors. Let `SimulationResultsDialog` pass the rows it already owns when exporting HTML.

**Tech Stack:** C++17, Qt6 Widgets caller, existing `opendva_report`, existing `dva_test` harness, CMake/Ninja/MSVC.

---

### Task 1: HTML Report Detail Sections

**Files:**
- Modify: `src/engine/report/include/opendva/report/HtmlReport.h`
- Modify: `src/engine/report/src/HtmlReport.cpp`
- Test: `src/tests/test_report.cpp`

- [ ] **Step 1: Write the failing test**

Add helpers in `src/tests/test_report.cpp`:

```cpp
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
```

Add test:

```cpp
TEST("html_report_includes_samples_and_contributors") {
    auto stats = makeStats();
    std::map<MeasureId, std::string> names = {{401, "gap"}, {402, "flush"}};
    std::map<ToleranceId, std::string> toleranceNames = {{701, "pin X"}, {702, "slot Y"}};
    std::string path = tempPath("_report_details.html");

    dvatest::check(writeHtmlReport(path, "Simulation Window", stats, names, {},
                                   makeSamples(), makeContributors(), toleranceNames),
                   "writeHtmlReport details ok");

    std::string body = slurp(path);
    std::remove(path.c_str());
    dvatest::check(body.find("Samples") != std::string::npos, "samples section present");
    dvatest::check(body.find("<td>1</td>") != std::string::npos, "sample build present");
    dvatest::check(body.find("10.1000") != std::string::npos, "sample value present");
    dvatest::check(body.find("Contributor") != std::string::npos, "contributor section present");
    dvatest::check(body.find("pin X") != std::string::npos, "contributor name present");
    dvatest::check(body.find("75.0000") != std::string::npos, "contribution percent present");
}
```

- [ ] **Step 2: Run RED verification**

Run:

```powershell
cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build-cmake328-ui --target opendva_tests'
```

Expected: compile fails because the detailed `writeHtmlReport` overload does not exist.

- [ ] **Step 3: Implement the detailed overload**

In `HtmlReport.h`, add an overload:

```cpp
bool writeHtmlReport(const std::string& path,
                     const std::string& title,
                     const std::map<MeasureId, MeasureStats>& stats,
                     const std::map<MeasureId, std::string>& measureNames,
                     const std::vector<ReportImage>& images,
                     const std::vector<SimulationSampleRow>& samples,
                     const std::vector<ContributorRow>& contributors,
                     const std::map<ToleranceId, std::string>& toleranceNames);
```

In `HtmlReport.cpp`, forward existing overloads to the detailed overload with empty vectors/maps. After the histogram table, emit a `Samples` section when samples are not empty and a `Contributor` section when contributors are not empty. Use existing measure names and tolerance names, fallback to `M<id>` and `T<id>`, and keep numeric output at four decimals.

- [ ] **Step 4: Run GREEN verification**

Run:

```powershell
cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build-cmake328-ui --target opendva_tests && build-cmake328-ui\tests\opendva_tests.exe'
```

Expected: all tests pass and total increases by one.

### Task 2: UI Export Wiring and Documentation

**Files:**
- Modify: `src/ui/src/SimulationResultsDialog.cpp`
- Modify: `src/README.md`
- Modify: `doc/交付差距审计.md`
- Modify: `doc/全量需求实现路线图.md`

- [ ] **Step 1: Pass details from the Simulation Window**

In `SimulationResultsDialog::exportHtml()`, call the new detailed overload with `samples_`, `contributors_`, and `toleranceNames_`. Preserve the current image behavior.

- [ ] **Step 2: Update documentation**

Update test total and `test_report.cpp` count. Mention that HTML reports now include viewport snapshots plus sample/contributor detail sections.

- [ ] **Step 3: Run full canonical verification**

Run:

```powershell
cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S src -B build-cmake328-ui -G Ninja -DOPENDVA_BUILD_TESTS=ON -DOPENDVA_BUILD_UI=ON && "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build-cmake328-ui --target opendva_tests opendva_cli opendva_gui && "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build-cmake328-ui --output-on-failure'
```

Expected: configure succeeds, all targets build, and CTest reports 4/4 passed.
