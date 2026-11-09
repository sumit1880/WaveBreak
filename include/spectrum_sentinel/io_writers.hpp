#pragma once

#include "spectrum_sentinel/common.hpp"
#include <string>
#include <vector>

namespace spectrum_sentinel {

class IoWriters {
public:
    static bool ensure_directory(const std::string& dirpath);

    static bool write_samples_csv(
        const std::string& filepath,
        const SampleVector& samples);

    static bool read_samples_csv(
        const std::string& filepath,
        SampleVector& out_samples);

    static bool write_samples_meta_json(
        const std::string& filepath,
        const Config& config);

    static bool read_samples_meta_json(
        const std::string& filepath,
        double& out_fs,
        size_t& out_num_samples,
        std::string& out_scenario,
        uint64_t& out_seed);

    static bool write_ground_truth_json(
        const std::string& filepath,
        const std::vector<GroundTruthEvent>& events);

    static bool read_ground_truth_json(
        const std::string& filepath,
        std::vector<GroundTruthEvent>& out_events);

    static bool write_spectrogram_csv(
        const std::string& filepath,
        const std::vector<std::vector<double>>& spectrogram_db);
};

} // namespace spectrum_sentinel
