// A4 reference tolerance sampler factory.
#pragma once
#include <memory>

#include "opendva/IToleranceSampler.h"

namespace opendva {

std::unique_ptr<IToleranceSampler> makeReferenceToleranceSampler();

}  // namespace opendva
