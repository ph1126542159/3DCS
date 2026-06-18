cmake_minimum_required(VERSION 3.28)
project(OpenDVA
        VERSION 0.1.0
        DESCRIPTION "Open Dimensional Variation Analysis - 3DCS-style tolerance simulation"
        LANGUAGES CXX C)

# ----- Global settings -----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

# Keep runnable artifacts in one predictable folder. On Windows this also gives
# windeployqt a single place to copy Qt DLLs/plugins for double-click launch.
set(OPENDVA_RUNTIME_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OPENDVA_RUNTIME_OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OPENDVA_RUNTIME_OUTPUT_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
foreach(_opendva_config IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER "${_opendva_config}" _opendva_config_upper)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_opendva_config_upper} "${OPENDVA_RUNTIME_OUTPUT_DIR}")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_opendva_config_upper} "${OPENDVA_RUNTIME_OUTPUT_DIR}")
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${_opendva_config_upper} "${CMAKE_BINARY_DIR}/lib")
endforeach()
unset(_opendva_config)
unset(_opendva_config_upper)

# ----- Options -----
option(OPENDVA_BUILD_TESTS "Build unit & contract tests" ON)
option(OPENDVA_BUILD_UI    "Build Qt desktop UI (requires Qt6/Qt3D)" ON)
option(OPENDVA_USE_OCCT    "Use OpenCASCADE geometry kernel (else stub)" OFF)
option(OPENDVA_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

# ----- Warning flags -----
add_library(opendva_warnings INTERFACE)
if(MSVC)
  target_compile_options(opendva_warnings INTERFACE /W4 /permissive-)
  if(OPENDVA_WARNINGS_AS_ERRORS)
    target_compile_options(opendva_warnings INTERFACE /WX)
  endif()
else()
  target_compile_options(opendva_warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
  if(OPENDVA_WARNINGS_AS_ERRORS)
    target_compile_options(opendva_warnings INTERFACE -Werror)
  endif()
endif()

# ----- Modules (A0..A8) -----
# A0: interfaces (header-only contracts) + mocks
add_subdirectory(interfaces)
# A1: geometry kernel
add_subdirectory(engine/geometry)
# A2: domain model & logic tree
add_subdirectory(domain)
# A3: move solver
add_subdirectory(engine/move)
# A4: tolerance sampler
add_subdirectory(engine/tolerance)
# A5: measure evaluator
add_subdirectory(engine/measure)
# A6: simulation engine
add_subdirectory(engine/sim)
# A7: visualization & report (no-UI services for now)
add_subdirectory(engine/report)
# App workflow helpers for desktop/CLI smoke flows
add_subdirectory(app/workflow)
# Headless CLI (Batch Processor analogue)
add_subdirectory(app/cli)
# A8: plugin host (User-DLL C ABI loader, headless)
add_subdirectory(app/plugin_host)

if(OPENDVA_BUILD_UI)
  add_subdirectory(ui)
endif()

# ----- Tests -----
if(OPENDVA_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

message(STATUS "OpenDVA ${PROJECT_VERSION} configured. Tests=${OPENDVA_BUILD_TESTS} UI=${OPENDVA_BUILD_UI} OCCT=${OPENDVA_USE_OCCT}")
