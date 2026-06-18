# Unit + contract tests (lightweight harness, no external deps).
add_executable(opendva_tests
  test_main.cpp
  test_engine.cpp
  test_distributions.cpp
  test_measures.cpp
  test_measures2.cpp
  test_gdtmeasure.cpp
  test_sensitivity.cpp
  test_moves.cpp
  test_moves2.cpp
  test_geomrule.cpp
  test_simengine.cpp
  test_domain.cpp
  test_app_preferences.cpp
  test_model_editing.cpp
  test_model_validation.cpp
  test_model_visualization.cpp
  test_simulation_settings.cpp
  test_geometry.cpp
  test_report.cpp
  test_pluginhost.cpp
  test_desktop_workflow.cpp)

target_include_directories(opendva_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(opendva_tests
  PRIVATE OpenDVA::sim OpenDVA::domain OpenDVA::move
          OpenDVA::tolerance OpenDVA::measure OpenDVA::geometry
          OpenDVA::report OpenDVA::plugin_host OpenDVA::workflow
          opendva_warnings)

add_test(NAME opendva_tests COMMAND opendva_tests)

if(OPENDVA_BUILD_UI)
  find_package(Qt6 REQUIRED COMPONENTS Widgets Quick Qml QuickControls2 3DCore 3DRender 3DInput 3DExtras)
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_AUTORCC ON)
  add_executable(opendva_ui_tests
    ../ui/assets/opendva_assets.qrc
    test_ui_main.cpp
    test_feature_catalog.cpp
    test_workbench_model.cpp
    test_qml_workbench.cpp
    test_mainwindow_properties.cpp
    ../ui/src/BatchProcessorDialog.cpp
    ../ui/src/BatchProcessorDialog.h
    ../ui/src/ColorContourLegendWidget.cpp
    ../ui/src/ColorContourLegendWidget.h
    ../ui/src/CommandExecutionModel.cpp
    ../ui/src/CommandExecutionModel.h
    ../ui/src/FeatureCatalogModel.cpp
    ../ui/src/FeatureCatalogModel.h
    ../ui/src/WorkbenchModel.cpp
    ../ui/src/WorkbenchModel.h
    ../ui/src/MainWindow.cpp
    ../ui/src/MainWindow.h
    ../ui/src/ModelViewport.cpp
    ../ui/src/ModelViewport.h
    ../ui/src/ModuleOverviewDialog.cpp
    ../ui/src/ModuleOverviewDialog.h
    ../ui/src/PreferencesDialog.cpp
    ../ui/src/PreferencesDialog.h
    ../ui/src/RunAnalysisDialog.cpp
    ../ui/src/RunAnalysisDialog.h
    ../ui/src/SimulationResultsDialog.cpp
    ../ui/src/SimulationResultsDialog.h
    ../ui/src/ValidationResultsDialog.cpp
    ../ui/src/ValidationResultsDialog.h)

  target_include_directories(opendva_ui_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/src)

  target_compile_definitions(opendva_ui_tests PRIVATE OPENDVA_UI_TESTING=1)

  target_link_libraries(opendva_ui_tests
    PRIVATE Qt6::Widgets
            Qt6::Quick Qt6::Qml Qt6::QuickControls2
            Qt6::3DCore Qt6::3DRender Qt6::3DInput Qt6::3DExtras
            OpenDVA::sim OpenDVA::domain OpenDVA::report OpenDVA::workflow
            OpenDVA::plugin_host
            opendva_warnings)

  add_test(NAME opendva_ui_tests COMMAND opendva_ui_tests)
  set_tests_properties(opendva_ui_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PATH=$<TARGET_FILE_DIR:Qt6::Widgets>;$ENV{PATH}")
endif()

add_test(NAME opendva_cli_starter_smoke
  COMMAND ${CMAKE_COMMAND}
          -DCLI_EXE=$<TARGET_FILE:opendva_cli>
          -DMODEL_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-starter-smoke.xml
          -DREPORT_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-starter-smoke.html
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_starter_smoke.cmake)

add_test(NAME opendva_cli_model_in_smoke
  COMMAND ${CMAKE_COMMAND}
          -DCLI_EXE=$<TARGET_FILE:opendva_cli>
          -DMODEL_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-model-in-smoke.xml
          -DREPORT_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-model-in-smoke.html
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_model_in_smoke.cmake)

add_test(NAME opendva_cli_report_exports_smoke
  COMMAND ${CMAKE_COMMAND}
          -DCLI_EXE=$<TARGET_FILE:opendva_cli>
          -DMODEL_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-report-exports.xml
          -DHTML_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-report-exports.html
          -DCSV_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-report-exports.csv
          -DEXCEL_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-report-exports.xml.xls
          -DHST_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-report-exports.hst
          -DHLM_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-report-exports.hlm
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_report_exports_smoke.cmake)

add_test(NAME opendva_cli_batch_smoke
  COMMAND ${CMAKE_COMMAND}
          -DCLI_EXE=$<TARGET_FILE:opendva_cli>
          -DMODEL_A_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-batch-a.xml
          -DMODEL_B_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-batch-b.xml
          -DBATCH_LIST_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-batch-list.txt
          -DBATCH_OUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-batch-out
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_batch_smoke.cmake)

add_test(NAME opendva_cli_batch_manifest_smoke
  COMMAND ${CMAKE_COMMAND}
          -DCLI_EXE=$<TARGET_FILE:opendva_cli>
          -DMODEL_A_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-batch-manifest-a.xml
          -DMODEL_B_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-batch-manifest-b.xml
          -DBATCH_LIST_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-batch-manifest-list.txt
          -DBATCH_OUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-batch-manifest-out
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_batch_manifest_smoke.cmake)

add_test(NAME opendva_cli_batch_comments_smoke
  COMMAND ${CMAKE_COMMAND}
          -DCLI_EXE=$<TARGET_FILE:opendva_cli>
          -DMODEL_A_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-batch-comments-a.xml
          -DMODEL_B_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-batch-comments-b.xml
          -DBATCH_LIST_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-batch-comments-list.txt
          -DBATCH_OUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-batch-comments-out
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_batch_comments_smoke.cmake)

add_test(NAME opendva_cli_invalid_numeric_args
  COMMAND ${CMAKE_COMMAND}
          -DCLI_EXE=$<TARGET_FILE:opendva_cli>
          -DREPORT_PATH=${CMAKE_CURRENT_BINARY_DIR}/cli-invalid-numeric.html
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_invalid_numeric_args.cmake)
