#pragma once

#include "wavebreak/common.hpp"
#include <string>

namespace wavebreak {

class ExperimentRunner {
public:
    static int run_all_experiments(const Config& base_config, const std::string& out_dir);
    static int run_benchmark();
    static int run_demo();
};

} // namespace wavebreak
