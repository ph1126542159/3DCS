# A5: measure evaluator.
add_library(opendva_measure
  src/MeasureEvaluator.cpp)
add_library(OpenDVA::measure ALIAS opendva_measure)

target_include_directories(opendva_measure PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(opendva_measure
  PUBLIC OpenDVA::interfaces OpenDVA::domain
  PRIVATE opendva_warnings)
