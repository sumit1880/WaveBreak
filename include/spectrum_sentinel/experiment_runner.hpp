#pragma once

#include "spectrum_sentinel/common.hpp"
#include <string>

namespace spectrum_sentinel {

class ExperimentRunner {
public:
    static int run_all_experiments(const Config& base_config, const std::string& out_dir);
    static int run_benchmark();
    static int run_demo();
};

} // namespace spectrum_sentinel
