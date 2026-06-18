# A7: report service (HTML for now; Excel/PPT later).
add_library(opendva_report
  src/AnalysisDataFiles.cpp
  src/HtmlReport.cpp
  src/ReportAssets.cpp
  src/AssemblyState.cpp
  src/ColorContour.cpp
  src/CsvReport.cpp)
add_library(OpenDVA::report ALIAS opendva_report)

target_include_directories(opendva_report PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(opendva_report
  PUBLIC OpenDVA::interfaces
  PRIVATE opendva_warnings)
