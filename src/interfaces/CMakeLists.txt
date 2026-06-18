# A0: header-only contract library. Every module links this.
add_library(opendva_interfaces INTERFACE)
add_library(OpenDVA::interfaces ALIAS opendva_interfaces)

target_include_directories(opendva_interfaces INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_features(opendva_interfaces INTERFACE cxx_std_17)
