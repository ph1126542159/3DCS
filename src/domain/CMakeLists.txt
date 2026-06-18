# A2: domain model & logic tree.
add_library(opendva_domain
  src/Model.cpp
  src/AppPreferences.cpp
  src/ModelEditing.cpp
  src/ModelVisualization.cpp
  src/ModelValidation.cpp
  src/ModelSerializer.cpp)
target_sources(opendva_domain PRIVATE
  src/SimulationSettings.cpp)
add_library(OpenDVA::domain ALIAS opendva_domain)

target_include_directories(opendva_domain PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(opendva_domain
  PUBLIC OpenDVA::interfaces
  PRIVATE OpenDVA::plugin_host opendva_warnings)
