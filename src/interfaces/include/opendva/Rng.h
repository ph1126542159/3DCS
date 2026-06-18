// Reproducible RNG interface (README §7 "可复现红线").
// Random-number consumption order is part of the contract: same seed + same
// traversal order -> bit-for-bit reproducible results.
#pragma once
#include <cstdint>

namespace opendva {

class IRng {
public:
    virtual ~IRng() = default;
    // Uniform double in [0,1).
    virtual double uniform01() = 0;
    // Standard normal (mean 0, sigma 1).
    virtual double normal01() = 0;
    // Re-seed (for run segmentation / repeatability tests).
    virtual void seed(std::uint64_t s) = 0;
};

}  // namespace opendva
