# A6: simulation engine (Monte Carlo / GeoFactor / Contributor).
add_library(opendva_sim
  src/MonteCarloEngine.cpp
  src/WorstCase.cpp)
add_library(OpenDVA::sim ALIAS opendva_sim)

target_include_directories(opendva_sim PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(opendva_sim
  PUBLIC OpenDVA::interfaces OpenDVA::domain
  PRIVATE OpenDVA::tolerance OpenDVA::measure OpenDVA::move OpenDVA::plugin_host
          opendva_warnings)
