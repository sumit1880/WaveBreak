#include "spectrum_sentinel/experiment_runner.hpp"
#include <iostream>

namespace spectrum_sentinel {

int ExperimentRunner::run_all_experiments(const Config& /*base_config*/, const std::string& /*out_dir*/) {
    std::cout << "Experiment runner will be activated in Phase 2.\n";
    return 0;
}

int ExperimentRunner::run_benchmark() {
    std::cout << "Benchmark will be activated in Phase 2.\n";
    return 0;
}

int ExperimentRunner::run_demo() {
    std::cout << "Demo will be activated in Phase 2.\n";
    return 0;
}

} // namespace spectrum_sentinel
