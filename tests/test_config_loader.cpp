#include "test_harness.hpp"
#include "spectrum_sentinel/config_loader.hpp"

using namespace spectrum_sentinel;

void run_config_loader_tests() {
    std::cout << "[TEST] Running Config Loader tests...\n";

    // 1. Valid flat JSON parsing
    std::string json_ok = R"({
        "fs": 1000000.0,
        "num_samples": 262144,
        "fft_size": 1024,
        "hop_size": 512,
        "desired_freq": 100000.0,
        "desired_amp": 1.0,
        "scenario": "narrowband",
        "seed": 42,
        "output_dir": "results/test",
        "threshold_db": 13.5,
        "noise_floor_floor_db": -160.0
    })";

    Config cfg = ConfigLoader::parse_config(json_ok);
    ASSERT_NEAR(cfg.fs, 1000000.0, 1e-6);
    ASSERT_TRUE(cfg.num_samples == 262144);
    ASSERT_TRUE(cfg.fft_size == 1024);
    ASSERT_TRUE(cfg.hop_size == 512);
    ASSERT_NEAR(cfg.desired_freq, 100000.0, 1e-6);
    ASSERT_NEAR(cfg.desired_amp, 1.0, 1e-6);
    ASSERT_TRUE(cfg.scenario == "narrowband");
    ASSERT_TRUE(cfg.seed == 42);
    ASSERT_TRUE(cfg.output_dir == "results/test");
    ASSERT_NEAR(cfg.threshold_db, 13.5, 1e-6);
    ASSERT_NEAR(cfg.noise_floor_floor_db, -160.0, 1e-6);

    // 2. Malformed JSON errors
    std::string json_missing_brace = R"({ "fs": 1000000.0 )";
    ASSERT_THROWS(ConfigLoader::parse_config(json_missing_brace), std::runtime_error);

    std::string json_missing_colon = R"({ "fs" 1000000.0 })";
    ASSERT_THROWS(ConfigLoader::parse_config(json_missing_colon), std::runtime_error);

    std::string json_trailing_comma = R"({ "fs": 1000000.0, })";
    ASSERT_THROWS(ConfigLoader::parse_config(json_trailing_comma), std::runtime_error);

    std::string json_unquoted_key = R"({ fs: 1000000.0 })";
    ASSERT_THROWS(ConfigLoader::parse_config(json_unquoted_key), std::runtime_error);

    // 3. Missing file
    ASSERT_THROWS(ConfigLoader::load_from_file("non_existent_file_xyz123.json"), std::runtime_error);

    // 4. Parameter validation failures
    Config bad_cfg = cfg;

    // Non-power-of-2 FFT
    bad_cfg.fft_size = 1000;
    ASSERT_THROWS(ConfigLoader::validate(bad_cfg), std::invalid_argument);

    // Hop size = 0
    bad_cfg = cfg;
    bad_cfg.hop_size = 0;
    ASSERT_THROWS(ConfigLoader::validate(bad_cfg), std::invalid_argument);

    // Nyquist violation |f| >= fs/2
    bad_cfg = cfg;
    bad_cfg.desired_freq = 500000.0; // equal to fs/2
    ASSERT_THROWS(ConfigLoader::validate(bad_cfg), std::invalid_argument);

    bad_cfg = cfg;
    bad_cfg.desired_freq = 600000.0; // > fs/2
    ASSERT_THROWS(ConfigLoader::validate(bad_cfg), std::invalid_argument);

    // Unknown scenario
    bad_cfg = cfg;
    bad_cfg.scenario = "alien_signals";
    ASSERT_THROWS(ConfigLoader::validate(bad_cfg), std::invalid_argument);

    // Samples < FFT size
    bad_cfg = cfg;
    bad_cfg.num_samples = 512;
    bad_cfg.fft_size = 1024;
    ASSERT_THROWS(ConfigLoader::validate(bad_cfg), std::invalid_argument);
}
