# A8: plugin host (User-DLL C ABI loader, README §13). Headless / testable.
add_library(opendva_plugin_host
  src/PluginHost.cpp)
add_library(OpenDVA::plugin_host ALIAS opendva_plugin_host)

target_include_directories(opendva_plugin_host PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(opendva_plugin_host
  PUBLIC OpenDVA::interfaces
  PRIVATE opendva_warnings)

# POSIX needs libdl for dlopen/dlsym; Windows uses kernel32 (always linked).
if(NOT WIN32)
  target_link_libraries(opendva_plugin_host PRIVATE ${CMAKE_DL_LIBS})
endif()
