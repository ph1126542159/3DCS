# HTML Report Images Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Allow HTML reports to include optional model/viewport image assets as the first step toward report screenshots.

**Architecture:** Extend the existing pure C++ `HtmlReport` API with a small `ReportImage` value type and an overload accepting a vector of images. Preserve the existing `writeHtmlReport(path, title, stats, names)` signature by forwarding to the new overload with an empty image list.

**Tech Stack:** C++17, existing `opendva_report` library, existing `dva_test` harness, CMake/Ninja/MSVC.

---

### Task 1: HTML Report Image Assets

**Files:**
- Modify: `src/engine/report/include/opendva/report/HtmlReport.h`
- Modify: `src/engine/report/src/HtmlReport.cpp`
- Test: `src/tests/test_report.cpp`

- [ ] **Step 1: Write the failing test**

Add this test after `html_report_includes_histogram_bins`:

```cpp
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
```

- [ ] **Step 2: Run RED verification**

Run:

```powershell
cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build-cmake328-ui --target opendva_tests'
```

Expected: compile fails because `ReportImage` or the five-argument `writeHtmlReport` overload is not declared.

- [ ] **Step 3: Implement minimal API**

In `HtmlReport.h`, add:

```cpp
#include <vector>

struct ReportImage {
    std::string title;
    std::string source;
    std::string caption;
};

bool writeHtmlReport(const std::string& path,
                     const std::string& title,
                     const std::map<MeasureId, MeasureStats>& stats,
                     const std::map<MeasureId, std::string>& measureNames,
                     const std::vector<ReportImage>& images);
```

Keep the existing four-argument function declaration.

- [ ] **Step 4: Implement HTML output**

In `HtmlReport.cpp`, implement the four-argument overload as:

```cpp
return writeHtmlReport(path, title, stats, names, {});
```

In the new overload, write image sections after `<h1>` and before the statistics table:

```html
<section><h2>Model View</h2><img src="captures/model-view.png" alt="Model View" style="max-width:100%;height:auto"><p>Contributor color contour after Monte Carlo</p></section>
```

Escape `title`, `source`, and `caption` using the same helper used for report title and measure names. If no helper exists, add a local `escapeHtml` helper for `&`, `<`, `>`, `"`, and `'`, then use it for new image fields.

- [ ] **Step 5: Run GREEN verification**

Run:

```powershell
cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build-cmake328-ui --target opendva_tests && build-cmake328-ui\tests\opendva_tests.exe'
```

Expected: all tests pass and total increases by one.

### Task 2: Documentation and Full Verification

**Files:**
- Modify: `src/README.md`
- Modify: `doc/交付差距审计.md`
- Modify: `doc/全量需求实现路线图.md`

- [ ] **Step 1: Update documentation**

Update test total and `test_report.cpp` count. Change report status to mention optional HTML report view images/assets, while leaving real Qt3D screenshot capture as a remaining gap.

- [ ] **Step 2: Run full canonical verification**

Run:

```powershell
cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S src -B build-cmake328-ui -G Ninja -DOPENDVA_BUILD_TESTS=ON -DOPENDVA_BUILD_UI=ON && "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build-cmake328-ui --target opendva_tests opendva_cli opendva_gui && "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build-cmake328-ui --output-on-failure'
```

Expected: configure succeeds, all targets build, and CTest reports 4/4 passed.

- [ ] **Step 3: Final local checks**

Run:

```powershell
Select-String -Path 'src/tests/*.cpp' -Pattern '^TEST\(' | Measure-Object
git status --short
```

Expected: test count reflects the added test. `git status` may fail because `e:\3DCS` is not currently a git repository; report that fact instead of treating it as a code failure.
