#pragma once

#include "wavebreak/common.hpp"
#include "wavebreak/spectrum_analyzer.hpp"
#include <vector>
#include <string>

namespace wavebreak {

struct WindowDetection {
    size_t window_idx{0};
    double window_time{0.0};
    std::string type;          // "narrowband", "broadband"
    double frequency{0.0};     // Hz
    double freq_min{0.0};      // Hz
    double freq_max{0.0};      // Hz
    double peak_power_db{0.0};
    double margin_db{0.0};
    size_t bin_width{0};
};

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
    // Detects interference events from spectrogram
    static std::vector<DetectedEvent> detect(
        const SpectrogramResult& spec,
        const Config& config);

    // Computes noise floor for a single window (linear power)
    // Excludes desired tone +/- 5 bins, computes median * (1 / ln 2)
    static double compute_window_noise_floor_lin(
        const std::vector<double>& window_power_db,
        int desired_bin,
        int guard_bins = 5);

    // Parabolic interpolation around peak bin
    // alpha = y[k-1], beta = y[k], gamma = y[k+1]
    // Returns {delta_bin, interpolated_peak_db}
    static std::pair<double, double> parabolic_interpolation(
        double alpha, double beta, double gamma);

    // Merges window-level detections into events (merge gap 2, min 2 windows)
    static std::vector<DetectedEvent> merge_window_detections(
        const std::vector<std::vector<WindowDetection>>& detections_per_window,
        const std::vector<double>& window_times,
        double window_duration,
        double total_duration,
        size_t merge_gap_windows = 2,
        size_t min_windows = 2);
};

} // namespace wavebreak
