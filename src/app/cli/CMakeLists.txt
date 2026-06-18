# Headless CLI (Batch Processor analogue).
add_executable(opendva_cli
  src/main.cpp)

target_link_libraries(opendva_cli
  PRIVATE OpenDVA::sim OpenDVA::domain OpenDVA::report OpenDVA::workflow
          opendva_warnings)

set_target_properties(opendva_cli PROPERTIES OUTPUT_NAME opendva)
