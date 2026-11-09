#include "spectrum_sentinel/cli.hpp"
#include "spectrum_sentinel/config_loader.hpp"
#include "spectrum_sentinel/signal_generator.hpp"
#include "spectrum_sentinel/spectrum_analyzer.hpp"
#include "spectrum_sentinel/io_writers.hpp"
#include "spectrum_sentinel/experiment_runner.hpp"
#include <iostream>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace spectrum_sentinel {

void CLI::print_usage() {
    std::cout << "Spectrum Sentinel: Synthetic RF/Baseband Interference Detection Simulator\n"
              << "Usage:\n"
              << "  spectrum_sentinel simulate --scenario <name> [--config <path>] [--seed <num>] [--out <dir>]\n"
              << "  spectrum_sentinel analyze --input <samples.csv> --meta <samples_meta.json> [--out <dir>]\n"
              << "  spectrum_sentinel evaluate [--out <dir>]\n"
              << "  spectrum_sentinel benchmark\n"
              << "  spectrum_sentinel demo\n"
              << "  spectrum_sentinel --help | -h\n"
              << "  spectrum_sentinel --version | -v\n\n"
              << "Available scenarios:\n"
              << "  clean, low_noise, high_noise, narrowband, broadband, burst, multi, drift\n\n"
              << "Exit Codes:\n"
              << "  0 - OK\n"
              << "  1 - Runtime error\n"
              << "  2 - Usage or config error\n"
              << "  3 - File I/O error\n";
}

void CLI::print_version() {
    std::cout << "Spectrum Sentinel version 1.0.0 (C++17 simulation)\n";
}

int CLI::run(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return static_cast<int>(ExitCode::UsageError);
    }

    std::string command = argv[1];

    if (command == "--help" || command == "-h" || command == "help") {
        print_usage();
        return static_cast<int>(ExitCode::Success);
    }
    if (command == "--version" || command == "-v" || command == "version") {
        print_version();
        return static_cast<int>(ExitCode::Success);
    }

    if (command == "simulate") {
        Config cfg;
        // Check if default config exists in config/default_config.json
        if (fs::exists("config/default_config.json")) {
            try {
                cfg = ConfigLoader::load_from_file("config/default_config.json");
            } catch (...) {
                // fall back to default struct values
            }
        }

        std::string config_file;
        std::string scenario_arg;
        std::string out_arg;
        int seed_arg = -1;

        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--config" && i + 1 < argc) {
                config_file = argv[++i];
            } else if (arg == "--scenario" && i + 1 < argc) {
                scenario_arg = argv[++i];
            } else if (arg == "--seed" && i + 1 < argc) {
                try {
                    seed_arg = std::stoi(argv[++i]);
                } catch (...) {
                    std::cerr << "Error: Invalid seed parameter\n";
                    return static_cast<int>(ExitCode::UsageError);
                }
            } else if (arg == "--out" && i + 1 < argc) {
                out_arg = argv[++i];
            } else {
                std::cerr << "Error: Unknown argument: " << arg << "\n";
                print_usage();
                return static_cast<int>(ExitCode::UsageError);
            }
        }

        // If config file was specified, load it
        if (!config_file.empty()) {
            try {
                cfg = ConfigLoader::load_from_file(config_file);
            } catch (const std::exception& e) {
                std::cerr << "Config load error: " << e.what() << "\n";
                return static_cast<int>(ExitCode::FileIOError);
            }
        }

        // CLI flags override config
        if (!scenario_arg.empty()) {
            cfg.scenario = scenario_arg;
        }
        if (seed_arg >= 0) {
            cfg.seed = static_cast<uint64_t>(seed_arg);
        }
        if (!out_arg.empty()) {
            cfg.output_dir = out_arg;
        } else {
            // Default output dir to results/<scenario>
            cfg.output_dir = "results/" + cfg.scenario;
        }

        // Validate config
        try {
            ConfigLoader::validate(cfg);
        } catch (const std::exception& e) {
            std::cerr << "Configuration validation error: " << e.what() << "\n";
            return static_cast<int>(ExitCode::UsageError);
        }

        // Run simulation
        try {
            SimulationResult sim = SignalGenerator::generate_scenario(cfg);

            // Compute spectrogram
            SpectrogramResult spec = SpectrumAnalyzer::compute_spectrogram(
                sim.samples, cfg.fft_size, cfg.hop_size, cfg.fs, cfg.noise_floor_floor_db
            );

            // Decimate spectrogram bins to <= 512 if larger
            auto decimated_spec = SpectrumAnalyzer::decimate_bins(spec.power_db, 512);

            // Create output directory
            if (!IoWriters::ensure_directory(cfg.output_dir)) {
                std::cerr << "File I/O error: could not create output directory " << cfg.output_dir << "\n";
                return static_cast<int>(ExitCode::FileIOError);
            }

            std::string samples_path = cfg.output_dir + "/samples.csv";
            std::string meta_path = cfg.output_dir + "/samples_meta.json";
            std::string truth_path = cfg.output_dir + "/ground_truth.json";
            std::string spec_path = cfg.output_dir + "/spectrogram.csv";

            if (!IoWriters::write_samples_csv(samples_path, sim.samples)) {
                std::cerr << "File I/O error: failed writing " << samples_path << "\n";
                return static_cast<int>(ExitCode::FileIOError);
            }
            if (!IoWriters::write_samples_meta_json(meta_path, cfg)) {
                std::cerr << "File I/O error: failed writing " << meta_path << "\n";
                return static_cast<int>(ExitCode::FileIOError);
            }
            if (!IoWriters::write_ground_truth_json(truth_path, sim.ground_truth)) {
                std::cerr << "File I/O error: failed writing " << truth_path << "\n";
                return static_cast<int>(ExitCode::FileIOError);
            }
            if (!IoWriters::write_spectrogram_csv(spec_path, decimated_spec)) {
                std::cerr << "File I/O error: failed writing " << spec_path << "\n";
                return static_cast<int>(ExitCode::FileIOError);
            }

            std::cout << "Simulation completed successfully for scenario: " << cfg.scenario << "\n"
                      << "  Output directory: " << cfg.output_dir << "\n"
                      << "  Generated samples: " << sim.samples.size() << "\n"
                      << "  Ground-truth events: " << sim.ground_truth.size() << "\n"
                      << "  Spectrogram windows: " << spec.num_windows << " (bins: " 
                      << (decimated_spec.empty() ? 0 : decimated_spec[0].size()) << ")\n";

            return static_cast<int>(ExitCode::Success);

        } catch (const std::exception& e) {
            std::cerr << "Runtime simulation error: " << e.what() << "\n";
            return static_cast<int>(ExitCode::RuntimeError);
        }
    }
    else if (command == "analyze") {
        std::cout << "analyze command will be activated in Phase 2.\n";
        return static_cast<int>(ExitCode::Success);
    }
    else if (command == "evaluate") {
        std::cout << "evaluate command will be activated in Phase 2.\n";
        return static_cast<int>(ExitCode::Success);
    }
    else if (command == "benchmark") {
        std::cout << "benchmark command will be activated in Phase 2.\n";
        return static_cast<int>(ExitCode::Success);
    }
    else if (command == "demo") {
        std::cout << "demo command will be activated in Phase 2.\n";
        return static_cast<int>(ExitCode::Success);
    }

    std::cerr << "Error: Unknown command '" << command << "'\n";
    print_usage();
    return static_cast<int>(ExitCode::UsageError);
}

} // namespace spectrum_sentinel
