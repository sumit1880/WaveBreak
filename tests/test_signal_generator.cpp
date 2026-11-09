#include "test_harness.hpp"
#include "spectrum_sentinel/signal_generator.hpp"
#include <cmath>
#include <numeric>

using namespace spectrum_sentinel;

void run_signal_generator_tests() {
    std::cout << "[TEST] Running Signal Generator tests...\n";

    // 1. Box-Muller statistics
    {
        BoxMullerRng rng(12345);
        size_t n = 20000;
        double sum = 0.0;
        double sum_sq = 0.0;
        for (size_t i = 0; i < n; ++i) {
            double v = rng.next_gaussian();
            sum += v;
            sum_sq += v * v;
        }
        double mean = sum / static_cast<double>(n);
        double var = (sum_sq / static_cast<double>(n)) - (mean * mean);

        // Expected mean ~ 0, variance ~ 1
        ASSERT_NEAR(mean, 0.0, 0.05);
        ASSERT_NEAR(var, 1.0, 0.08);
    }

    // 2. Same seed gives same output (tolerance asserts)
    {
        Config cfg;
        cfg.scenario = "narrowband";
        cfg.seed = 42;
        cfg.num_samples = 4096;

        SimulationResult r1 = SignalGenerator::generate_scenario(cfg);
        SimulationResult r2 = SignalGenerator::generate_scenario(cfg);

        ASSERT_TRUE(r1.samples.size() == r2.samples.size());
        for (size_t i = 0; i < r1.samples.size(); ++i) {
            ASSERT_COMPLEX_NEAR(r1.samples[i], r2.samples[i], 1e-15);
        }
    }

    // 3. Different seeds give different output
    {
        Config cfg1;
        cfg1.scenario = "clean";
        cfg1.seed = 42;
        cfg1.num_samples = 1024;

        Config cfg2 = cfg1;
        cfg2.seed = 999;

        SimulationResult r1 = SignalGenerator::generate_scenario(cfg1);
        SimulationResult r2 = SignalGenerator::generate_scenario(cfg2);

        bool has_diff = false;
        for (size_t i = 0; i < r1.samples.size(); ++i) {
            if (std::abs(r1.samples[i] - r2.samples[i]) > 1e-6) {
                has_diff = true;
                break;
            }
        }
        ASSERT_TRUE(has_diff);
    }

    // 4. Ground-truth verification across scenarios
    {
        Config cfg;
        cfg.num_samples = 8192;

        // Clean
        cfg.scenario = "clean";
        SimulationResult sim_clean = SignalGenerator::generate_scenario(cfg);
        ASSERT_TRUE(sim_clean.ground_truth.empty());

        // Low noise
        cfg.scenario = "low_noise";
        SimulationResult sim_low = SignalGenerator::generate_scenario(cfg);
        ASSERT_TRUE(sim_low.ground_truth.empty());

        // High noise
        cfg.scenario = "high_noise";
        SimulationResult sim_high = SignalGenerator::generate_scenario(cfg);
        ASSERT_TRUE(sim_high.ground_truth.empty());

        // Narrowband
        cfg.scenario = "narrowband";
        SimulationResult sim_nb = SignalGenerator::generate_scenario(cfg);
        ASSERT_TRUE(sim_nb.ground_truth.size() == 1);
        ASSERT_TRUE(sim_nb.ground_truth[0].type == "narrowband");
        ASSERT_NEAR(sim_nb.ground_truth[0].frequency, 250000.0, 1e-3);
        ASSERT_NEAR(sim_nb.ground_truth[0].amplitude, 0.3, 1e-3);

        // Broadband
        cfg.scenario = "broadband";
        SimulationResult sim_bb = SignalGenerator::generate_scenario(cfg);
        ASSERT_TRUE(sim_bb.ground_truth.size() == 1);
        ASSERT_TRUE(sim_bb.ground_truth[0].type == "broadband");
        ASSERT_NEAR(sim_bb.ground_truth[0].freq_min, 200000.0, 1e-3);
        ASSERT_NEAR(sim_bb.ground_truth[0].freq_max, 350000.0, 1e-3);

        // Burst
        cfg.scenario = "burst";
        SimulationResult sim_burst = SignalGenerator::generate_scenario(cfg);
        ASSERT_TRUE(sim_burst.ground_truth.size() == 3);
        ASSERT_TRUE(sim_burst.ground_truth[0].type == "narrowband");
        ASSERT_NEAR(sim_burst.ground_truth[0].start_time, 0.040, 1e-3);
        ASSERT_NEAR(sim_burst.ground_truth[0].end_time, 0.060, 1e-3);
        ASSERT_NEAR(sim_burst.ground_truth[1].start_time, 0.120, 1e-3);
        ASSERT_NEAR(sim_burst.ground_truth[1].end_time, 0.140, 1e-3);
        ASSERT_NEAR(sim_burst.ground_truth[2].start_time, 0.200, 1e-3);
        ASSERT_NEAR(sim_burst.ground_truth[2].end_time, 0.220, 1e-3);

        // Multi
        cfg.scenario = "multi";
        SimulationResult sim_multi = SignalGenerator::generate_scenario(cfg);
        ASSERT_TRUE(sim_multi.ground_truth.size() == 2);
        ASSERT_NEAR(sim_multi.ground_truth[0].frequency, -200000.0, 1e-3);
        ASSERT_NEAR(sim_multi.ground_truth[1].frequency, 350000.0, 1e-3);

        // Drift
        cfg.scenario = "drift";
        SimulationResult sim_drift = SignalGenerator::generate_scenario(cfg);
        ASSERT_TRUE(sim_drift.ground_truth.size() == 1);
        ASSERT_TRUE(sim_drift.ground_truth[0].type == "drift");
        ASSERT_NEAR(sim_drift.ground_truth[0].freq_min, 200000.0, 1e-3);
        ASSERT_NEAR(sim_drift.ground_truth[0].freq_max, 400000.0, 1e-3);
    }
}
