# UI HTML Snapshot Report Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Embed a current Qt3D viewport screenshot into HTML reports exported from the Simulation Window.

**Architecture:** Keep path naming logic in a small pure C++ helper so it is testable. Let `MainWindow` provide a screenshot callback to `SimulationResultsDialog`; the dialog owns report export and passes a `ReportImage` to `writeHtmlReport` only when screenshot capture succeeds.

**Tech Stack:** C++17, Qt6 Widgets/Qt3D, existing `opendva_report`, existing `dva_test` harness, CMake/Ninja/MSVC.

---

### Task 1: Snapshot Asset Path Planning

**Files:**
- Create: `src/engine/report/include/opendva/report/ReportAssets.h`
- Create: `src/engine/report/src/ReportAssets.cpp`
- Modify: `src/engine/report/CMakeLists.txt`
- Test: `src/tests/test_report.cpp`

- [ ] **Step 1: Write the failing test**

Add this include to `src/tests/test_report.cpp`:

```cpp
#include "opendva/report/ReportAssets.h"
```

Add this test after `html_report_includes_optional_view_images`:

```cpp
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
```

- [ ] **Step 2: Run RED verification**

Run:

```powershell
cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build-cmake328-ui --target opendva_tests'
```

Expected: compile fails because `ReportAssets.h` does not exist.

- [ ] **Step 3: Implement minimal helper**

Create `ReportAssets.h`:

```cpp
#pragma once

#include <string>

#include "opendva/report/HtmlReport.h"

namespace opendva {

struct ReportImageAsset {
    std::string filePath;
    ReportImage image;
};

ReportImageAsset planHtmlSnapshotAsset(const std::string& htmlReportPath,
                                       const std::string& title,
                                       const std::string& caption);

}  // namespace opendva
```

Create `ReportAssets.cpp` using `std::filesystem::path`. For `C:/work/reports/run-42.html`, `filePath` should be `C:/work/reports/run-42-view.png` and `image.source` should be `run-42-view.png`.

- [ ] **Step 4: Wire CMake and run GREEN**

Add `src/ReportAssets.cpp` to `opendva_report`. Run:

```powershell
cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build-cmake328-ui --target opendva_tests && build-cmake328-ui\tests\opendva_tests.exe'
```

Expected: all tests pass and total increases by one.

### Task 2: UI Snapshot Capture and HTML Export

**Files:**
- Modify: `src/ui/src/ModelViewport.h`
- Modify: `src/ui/src/ModelViewport.cpp`
- Modify: `src/ui/src/SimulationResultsDialog.h`
- Modify: `src/ui/src/SimulationResultsDialog.cpp`
- Modify: `src/ui/src/MainWindow.cpp`

- [ ] **Step 1: Add viewport snapshot method**

Add to `ModelViewport` public API:

```cpp
bool saveSnapshot(const QString& path) const;
```

Implement with:

```cpp
return grab().save(path, "PNG");
```

- [ ] **Step 2: Add report screenshot callback**

In `SimulationResultsDialog.h`, include `<functional>` and add:

```cpp
using SnapshotWriter = std::function<bool(const QString&)>;
```

Add an optional constructor parameter before `QWidget* parent`:

```cpp
SnapshotWriter snapshotWriter = {}
```

Store it as `SnapshotWriter snapshotWriter_;`.

- [ ] **Step 3: Embed snapshot on HTML export**

In `SimulationResultsDialog::exportHtml()`:
- Keep old behavior if `snapshotWriter_` is empty.
- If callback exists, call `planHtmlSnapshotAsset(path.toStdString(), "Model View", "Current Qt3D color contour view")`.
- Call `snapshotWriter_(QString::fromStdString(asset.filePath))`.
- If true, call `writeHtmlReport(..., std::vector<ReportImage>{asset.image})`.
- If false, fall back to existing four-argument `writeHtmlReport`.

- [ ] **Step 4: Pass callback from MainWindow**

In `MainWindow::runMonteCarlo()`, pass:

```cpp
[this](const QString& path) { return viewport_->saveSnapshot(path); }
```

to the `SimulationResultsDialog` constructor.

- [ ] **Step 5: Build GUI**

Run:

```powershell
cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build-cmake328-ui --target opendva_gui'
```

Expected: `opendva_gui` builds successfully.

### Task 3: Documentation and Full Verification

**Files:**
- Modify: `src/README.md`
- Modify: `doc/交付差距审计.md`
- Modify: `doc/全量需求实现路线图.md`

- [ ] **Step 1: Update documentation**

Update test total and `test_report.cpp` count. Change report status to mention UI HTML export embeds the current Qt3D viewport snapshot when capture succeeds.

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
