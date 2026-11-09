#pragma once

#include "spectrum_sentinel/common.hpp"
#include <random>
#include <cstdint>

namespace spectrum_sentinel {

class BoxMullerRng {
public:
    explicit BoxMullerRng(uint64_t seed = 42);

    void reset(uint64_t seed);

    // Standard Gaussian N(0, 1) using Box-Muller transform
    double next_gaussian();

    // Gaussian with specified mean and standard deviation
    double next_gaussian(double mean, double stddev);

    // Complex Gaussian sample with total variance sigma^2 (sigma^2/2 per I and Q)
    Complex next_complex_gaussian(double total_variance);

private:
    std::mt19937_64 rng_;
    std::uniform_real_distribution<double> uniform_dist_{0.0, 1.0};
    bool has_cached_{false};
    double cached_val_{0.0};
};

class SignalGenerator {
public:
    explicit SignalGenerator(const Config& config);

    // Generates the scenario specified in the config
    SimulationResult generate();

    // Generates a specific scenario with explicit parameters
    static SimulationResult generate_scenario(const Config& config);

private:
    Config config_;
};

} // namespace spectrum_sentinel
