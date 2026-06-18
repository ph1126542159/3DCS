# Color Contour Controls Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add interactive Color Contour controls to the Qt3D viewport so users can toggle auto scaling, enter manual min/max values, and clear the current contour overlay.

**Architecture:** Keep testable range resolution in the pure C++ domain visualization layer. Keep Qt UI behavior in `ColorContourLegendWidget` and `ModelViewport`, with the widget emitting range/clear intents and the viewport owning contour state.

**Tech Stack:** C++17, Qt6 Widgets, Qt3D, CMake/Ninja, existing `dva_test` test harness.

---

### Task 1: Domain Range Resolution

**Files:**
- Modify: `src/domain/include/opendva/domain/ModelVisualization.h`
- Modify: `src/domain/src/ModelVisualization.cpp`
- Test: `src/tests/test_model_visualization.cpp`

- [ ] **Step 1: Write the failing test**

Add a test named `model visualization: contour range resolves auto manual and invalid manual bounds` after the existing legend scale test. It should call:

```cpp
resolveContourRange(deviations, true, 99.0, 100.0)
resolveContourRange(deviations, false, -1.5, 3.0)
resolveContourRange(deviations, false, 3.0, -1.5)
```

Expected behavior:
- Auto range ignores manual values and returns min/max from deviations.
- Valid manual range returns the manual min/max.
- Invalid manual range falls back to auto range.

- [ ] **Step 2: Run RED verification**

Run:

```powershell
cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build-cmake328-ui --target opendva_tests'
```

Expected: compile fails because `resolveContourRange` is not declared.

- [ ] **Step 3: Implement minimal domain API**

Add:

```cpp
struct ContourRange {
    bool valid{false};
    double minValue{0.0};
    double maxValue{0.0};
};

ContourRange resolveContourRange(const std::map<PointId, double>& pointDeviations,
                                 bool autoScale,
                                 double manualMin,
                                 double manualMax);
```

Implementation:
- Empty deviations return invalid.
- `autoScale == false && manualMin <= manualMax` returns manual range.
- Otherwise return min/max from point deviations.

- [ ] **Step 4: Run GREEN verification**

Run the same build target and then:

```powershell
build-cmake328-ui\tests\opendva_tests.exe
```

Expected: all tests pass and total increases by one.

### Task 2: Qt Legend Controls

**Files:**
- Modify: `src/ui/src/ColorContourLegendWidget.h`
- Modify: `src/ui/src/ColorContourLegendWidget.cpp`
- Modify: `src/ui/src/ModelViewport.h`
- Modify: `src/ui/src/ModelViewport.cpp`

- [ ] **Step 1: Add widget controls**

Extend `ColorContourLegendWidget` with:
- `QCheckBox* autoScaleCheck_`
- `QDoubleSpinBox* minSpin_`
- `QDoubleSpinBox* maxSpin_`
- `QPushButton* clearButton_`

Add signals:

```cpp
void rangeSettingsChanged(bool autoScale, double manualMin, double manualMax);
void clearRequested();
```

- [ ] **Step 2: Wire controls**

Use a vertical layout with the existing painted ramp area plus controls. Disable min/max spin boxes while auto scale is checked. Emit `rangeSettingsChanged` when the checkbox or spin values change. Emit `clearRequested` from the clear button.

- [ ] **Step 3: Apply settings in viewport**

Add state to `ModelViewport`:

```cpp
bool contourAutoScale_{true};
double contourManualMin_{0.0};
double contourManualMax_{0.0};
```

Connect Legend signals in the constructor. Recompute the effective range with `resolveContourRange`, use that range for `viewportPointMarkers`, and call `legend_->setLegend(contourLegendScale(...))` after range changes. Clear should empty `pointDeviations_`, hide the legend, and rebuild the scene with default point colors.

- [ ] **Step 4: Build UI target**

Run:

```powershell
cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build-cmake328-ui --target opendva_gui'
```

Expected: `opendva_gui` builds with no compiler errors.

### Task 3: Docs and Full Verification

**Files:**
- Modify: `src/README.md`
- Modify: `doc/交付差距审计.md`
- Modify: `doc/全量需求实现路线图.md`

- [ ] **Step 1: Update documentation**

Update test total and `test_model_visualization.cpp` count. Change Color Contour status from basic Legend UI to interactive Legend controls.

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
