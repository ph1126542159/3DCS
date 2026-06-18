# App-level reusable workflows shared by GUI, CLI smoke tests, and future tools.
add_library(opendva_workflow
  src/DesktopSmokeWorkflow.cpp)

add_library(OpenDVA::workflow ALIAS opendva_workflow)

target_include_directories(opendva_workflow PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(opendva_workflow
  PUBLIC OpenDVA::interfaces
  PRIVATE OpenDVA::domain OpenDVA::sim OpenDVA::report opendva_warnings)
