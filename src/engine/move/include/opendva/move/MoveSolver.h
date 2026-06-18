// A3 move solver public entry (factory lives in IMoveSolver.h).
#pragma once
#include "opendva/IMoveSolver.h"

namespace opendva {

// Exposed for direct unit testing of the general kernel.
MoveResult solveSixPlane(const MoveInputs& in);

}  // namespace opendva
