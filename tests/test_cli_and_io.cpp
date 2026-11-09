#include "test_harness.hpp"
#include "wavebreak/io_writers.hpp"
#include "wavebreak/cli.hpp"
#include <filesystem>

using namespace wavebreak;
namespace fs = std::filesystem;

void run_cli_and_io_tests() {
    std::cout << "[TEST] Running CLI and IO tests...\n";

    std::string test_dir = "results/test_io";
    IoWriters::ensure_directory(test_dir);

    // 1. Samples CSV write and read roundtrip
    {
        SampleVector samples = {
            Complex(1.0, 2.0),
            Complex(-3.5, 4.25),
            Complex(0.0, -0.001)
        };
        std::string path = test_dir + "/samples.csv";
        ASSERT_TRUE(IoWriters::write_samples_csv(path, samples));

        SampleVector read_samples;
        ASSERT_TRUE(IoWriters::read_samples_csv(path, read_samples));
        ASSERT_TRUE(read_samples.size() == samples.size());
        for (size_t i = 0; i < samples.size(); ++i) {
            ASSERT_COMPLEX_NEAR(read_samples[i], samples[i], 1e-6);
        }
    }

    // 2. Samples Meta JSON write and read
    {
        Config cfg;
        cfg.fs = 1e6;
        cfg.num_samples = 4096;
        cfg.scenario = "narrowband";
        cfg.seed = 123;
        cfg.desired_freq = 100000.0;
        cfg.desired_amp = 1.0;

        std::string path = test_dir + "/samples_meta.json";
        ASSERT_TRUE(IoWriters::write_samples_meta_json(path, cfg));

        double r_fs = 0.0;
        size_t r_samples = 0;
        std::string r_scenario;
        uint64_t r_seed = 0;

        ASSERT_TRUE(IoWriters::read_samples_meta_json(path, r_fs, r_samples, r_scenario, r_seed));
        ASSERT_NEAR(r_fs, 1e6, 1e-3);
        ASSERT_TRUE(r_samples == 4096);
        ASSERT_TRUE(r_scenario == "narrowband");
        ASSERT_TRUE(r_seed == 123);
    }

    // 3. Ground Truth JSON write and read
    {
        std::vector<GroundTruthEvent> events;
        GroundTruthEvent ev;
        ev.id = 1;
        ev.type = "narrowband";
        ev.start_time = 0.040;
        ev.end_time = 0.060;
        ev.frequency = 250000.0;
        ev.amplitude = 0.3;
        ev.power_db = -10.45;
        events.push_back(ev);

        std::string path = test_dir + "/ground_truth.json";
        ASSERT_TRUE(IoWriters::write_ground_truth_json(path, events));

        std::vector<GroundTruthEvent> r_events;
        ASSERT_TRUE(IoWriters::read_ground_truth_json(path, r_events));
        ASSERT_TRUE(r_events.size() == 1);
        ASSERT_TRUE(r_events[0].id == 1);
        ASSERT_TRUE(r_events[0].type == "narrowband");
        ASSERT_NEAR(r_events[0].start_time, 0.040, 1e-3);
        ASSERT_NEAR(r_events[0].end_time, 0.060, 1e-3);
        ASSERT_NEAR(r_events[0].frequency, 250000.0, 1e-1);
    }

    // 4. Missing file read returns false
    {
        SampleVector empty;
        ASSERT_FALSE(IoWriters::read_samples_csv("non_existent_samples_xyz.csv", empty));
    }

    // 5. CLI Invocation exit codes
    {
        // --help -> 0
        char* argv_help[] = { (char*)"wavebreak", (char*)"--help" };
        ASSERT_TRUE(CLI::run(2, argv_help) == static_cast<int>(ExitCode::Success));

        // --version -> 0
        char* argv_ver[] = { (char*)"wavebreak", (char*)"--version" };
        ASSERT_TRUE(CLI::run(2, argv_ver) == static_cast<int>(ExitCode::Success));

        // empty args -> 2
        char* argv_none[] = { (char*)"wavebreak" };
        ASSERT_TRUE(CLI::run(1, argv_none) == static_cast<int>(ExitCode::UsageError));

        // invalid command -> 2
        char* argv_inv[] = { (char*)"wavebreak", (char*)"unknown_cmd" };
        ASSERT_TRUE(CLI::run(2, argv_inv) == static_cast<int>(ExitCode::UsageError));

        // invalid scenario flag -> 2
        char* argv_bad_scen[] = { (char*)"wavebreak", (char*)"simulate", (char*)"--scenario", (char*)"unknown_scen" };
        ASSERT_TRUE(CLI::run(4, argv_bad_scen) == static_cast<int>(ExitCode::UsageError));
    }

    // Cleanup test dir
    try {
        fs::remove_all(test_dir);
    } catch (...) {}
}
