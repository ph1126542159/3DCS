// Reproducible RNG built on std::mt19937_64 (README §7 可复现红线).
// Header-only so every module shares identical semantics.
#pragma once
#include <cmath>
#include <random>

#include "opendva/Rng.h"

namespace opendva {

class Mt19937Rng final : public IRng {
public:
    explicit Mt19937Rng(std::uint64_t s = 1) : engine_(s) {}

    double uniform01() override { return uni_(engine_); }
    double normal01() override { return nrm_(engine_); }
    void seed(std::uint64_t s) override {
        engine_.seed(s);
        nrm_.reset();  // clear the cached spare deviate
    }

private:
    std::mt19937_64 engine_;
    std::uniform_real_distribution<double> uni_{0.0, 1.0};
    std::normal_distribution<double> nrm_{0.0, 1.0};
};

}  // namespace opendva
