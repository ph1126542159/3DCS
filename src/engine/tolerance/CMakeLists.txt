# A4: tolerance sampler & distribution library.
add_library(opendva_tolerance
  src/Distributions.cpp
  src/GeomRule.cpp
  src/ToleranceSampler.cpp)
add_library(OpenDVA::tolerance ALIAS opendva_tolerance)

target_include_directories(opendva_tolerance PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(opendva_tolerance
  PUBLIC OpenDVA::interfaces
  PRIVATE opendva_warnings)
