#pragma once

#include <complex>
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <stdexcept>

namespace spectrum_sentinel {

constexpr double PI = 3.141592653589793238462643383279502884;

using Complex = std::complex<double>;
using SampleVector = std::vector<Complex>;

struct GroundTruthEvent {
    int id{0};
    std::string type;          // "narrowband", "broadband", "drift"
    double start_time{0.0};    // seconds
    double end_time{0.0};      // seconds
    double frequency{0.0};     // Hz (for narrowband)
    double freq_min{0.0};      // Hz (for broadband/drift)
    double freq_max{0.0};      // Hz (for broadband/drift)
    double amplitude{0.0};     // Linear amplitude
    double power_db{0.0};      // Power in dB relative to 1.0
};

struct Config {
    double fs{1000000.0};
    size_t num_samples{262144};
    size_t fft_size{1024};
    size_t hop_size{512};
    double desired_freq{100000.0};
    double desired_amp{1.0};
    std::string scenario{"clean"};
    uint64_t seed{42};
    std::string output_dir{"results/clean"};
    double threshold_db{13.0};
    double noise_floor_floor_db{-160.0};
};

struct SimulationResult {
    SampleVector samples;
    double fs{1000000.0};
    std::string scenario{"clean"};
    uint64_t seed{42};
    std::vector<GroundTruthEvent> ground_truth;
};

inline bool is_power_of_two(size_t n) {
    return n > 0 && ((n & (n - 1)) == 0);
}

inline void validate_parameters(double fs, size_t fft_size, size_t hop_size, double desired_freq) {
    if (fs <= 0.0) {
        throw std::invalid_argument("Sampling rate fs must be positive.");
    }
    if (!is_power_of_two(fft_size)) {
        throw std::invalid_argument("FFT size must be a positive power of 2, got: " + std::to_string(fft_size));
    }
    if (hop_size == 0) {
        throw std::invalid_argument("Hop size must be greater than 0.");
    }
    if (std::abs(desired_freq) >= fs / 2.0) {
        throw std::invalid_argument("Desired frequency |f| >= fs/2 violates Nyquist criterion.");
    }
}

} // namespace spectrum_sentinel
