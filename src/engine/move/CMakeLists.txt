# A3: move solver (21 types). Six-Plane general kernel + factory.
add_library(opendva_move
  src/SixPlaneSolver.cpp
  src/ThreePointSolver.cpp
  src/TwoPointSolver.cpp
  src/TransformSolver.cpp
  src/ThermalScalingSolver.cpp
  src/CrossProductSolver.cpp
  src/RotateLineSolver.cpp
  src/RTouchSolver.cpp
  src/MatchSolver.cpp
  src/GravitySolver.cpp
  src/BestFitSolver.cpp
  src/LeastSquaresAxisSolver.cpp
  src/MoveSolverFactory.cpp)
add_library(OpenDVA::move ALIAS opendva_move)

target_include_directories(opendva_move PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(opendva_move
  PUBLIC OpenDVA::interfaces
  PRIVATE OpenDVA::plugin_host opendva_warnings)
