// A6 reference simulation engine factory.
#pragma once
#include <memory>

#include "opendva/ISimulationEngine.h"

namespace opendva {

std::unique_ptr<ISimulationEngine> makeMonteCarloEngine();

}  // namespace opendva
