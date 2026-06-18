# A1: geometry kernel. Stub by default; OCCT-backed when OPENDVA_USE_OCCT=ON.
add_library(opendva_geometry
  src/StubGeometryKernel.cpp)
add_library(OpenDVA::geometry ALIAS opendva_geometry)

target_include_directories(opendva_geometry PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(opendva_geometry
  PUBLIC OpenDVA::interfaces
  PRIVATE opendva_warnings)

if(OPENDVA_USE_OCCT)
  find_package(OpenCASCADE REQUIRED)
  target_sources(opendva_geometry PRIVATE src/OcctGeometryKernel.cpp)
  target_link_libraries(opendva_geometry PUBLIC ${OpenCASCADE_LIBRARIES})
  target_compile_definitions(opendva_geometry PUBLIC OPENDVA_HAVE_OCCT=1)
endif()
