#pragma once

#include "spectrum_sentinel/common.hpp"
#include "spectrum_sentinel/spectrum_analyzer.hpp"
#include <vector>

namespace spectrum_sentinel {

struct DetectedEvent {
    int id{0};
    double start_time{0.0};
    double end_time{0.0};
    double frequency{0.0};
    double freq_min{0.0};
    double freq_max{0.0};
    double peak_power_db{0.0};
    double margin_db{0.0};
    std::string spectral_category; // "narrowband", "broadband", "uncertain"
    std::string temporal_flag;     // "continuous", "burst"
    std::string severity;          // "low", "medium", "high"
    std::string reason;            // rule that fired
};

class InterferenceDetector {
public:
    static std::vector<DetectedEvent> detect(
        const SpectrogramResult& spec,
        const Config& config);
};

} // namespace spectrum_sentinel
