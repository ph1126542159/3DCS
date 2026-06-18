# A8: Qt6 desktop UI (README §3.1 / §8). Gated by OPENDVA_BUILD_UI; not part
# of the headless build or tests. Requires Qt6 on CMAKE_PREFIX_PATH.
find_package(Qt6 REQUIRED COMPONENTS Widgets Quick Qml QuickControls2 3DCore 3DRender 3DInput 3DExtras)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_executable(opendva_gui
  assets/opendva_assets.qrc
  src/main.cpp
  src/BatchProcessorDialog.cpp
  src/BatchProcessorDialog.h
  src/ColorContourLegendWidget.cpp
  src/ColorContourLegendWidget.h
  src/CommandExecutionModel.cpp
  src/CommandExecutionModel.h
  src/FeatureCatalogModel.cpp
  src/FeatureCatalogModel.h
  src/WorkbenchModel.cpp
  src/WorkbenchModel.h
  src/MainWindow.cpp
  src/MainWindow.h
  src/ModelViewport.cpp
  src/ModelViewport.h
  src/ModuleOverviewDialog.cpp
  src/ModuleOverviewDialog.h
  src/PreferencesDialog.cpp
  src/PreferencesDialog.h
  src/RunAnalysisDialog.cpp
  src/RunAnalysisDialog.h
  src/SimulationResultsDialog.cpp
  src/SimulationResultsDialog.h
  src/ValidationResultsDialog.cpp
  src/ValidationResultsDialog.h)

target_link_libraries(opendva_gui
  PRIVATE Qt6::Widgets
          Qt6::Quick Qt6::Qml Qt6::QuickControls2
          Qt6::3DCore Qt6::3DRender Qt6::3DInput Qt6::3DExtras
          OpenDVA::sim OpenDVA::domain OpenDVA::report
          OpenDVA::workflow
          opendva_warnings)

# Windows: GUI subsystem (no console window).
if(WIN32)
  set_target_properties(opendva_gui PROPERTIES WIN32_EXECUTABLE ON)

  get_target_property(_opendva_qmake_executable Qt6::qmake IMPORTED_LOCATION)
  get_filename_component(_opendva_qt_bin_dir "${_opendva_qmake_executable}" DIRECTORY)
  find_program(_opendva_windeployqt_executable
    NAMES windeployqt
    HINTS "${_opendva_qt_bin_dir}"
    NO_DEFAULT_PATH)
  if(_opendva_windeployqt_executable)
    add_custom_command(TARGET opendva_gui POST_BUILD
      COMMAND "${_opendva_windeployqt_executable}"
              --no-translations
              --compiler-runtime
              --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
              --dir "$<TARGET_FILE_DIR:opendva_gui>"
              "$<TARGET_FILE:opendva_gui>"
      COMMENT "Deploying Qt runtime for opendva_gui")
    foreach(_opendva_platform_plugin IN ITEMS qoffscreen qminimal)
      add_custom_command(TARGET opendva_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_opendva_qt_bin_dir}/../plugins/platforms/${_opendva_platform_plugin}.dll"
                "$<TARGET_FILE_DIR:opendva_gui>/platforms/${_opendva_platform_plugin}.dll"
        COMMENT "Deploying Qt ${_opendva_platform_plugin} platform plugin")
    endforeach()
    unset(_opendva_platform_plugin)
  else()
    message(WARNING "windeployqt was not found next to Qt qmake; opendva_gui may not double-click run outside the Qt environment.")
  endif()
endif()
