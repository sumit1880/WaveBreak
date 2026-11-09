#include "wavebreak/cli.hpp"
#include "wavebreak/config_loader.hpp"
#include "wavebreak/signal_generator.hpp"
#include "wavebreak/spectrum_analyzer.hpp"
#include "wavebreak/interference_detector.hpp"
#include "wavebreak/detection_evaluator.hpp"
#include "wavebreak/io_writers.hpp"
#include "wavebreak/experiment_runner.hpp"
#include <iostream>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace wavebreak {

void CLI::print_usage() {
    std::cout << "WaveBreak: Synthetic RF/Baseband Interference Detection Simulator\n"
              << "Usage:\n"
              << "  wavebreak simulate --scenario <name> [--config <path>] [--seed <num>] [--out <dir>]\n"
              << "  wavebreak analyze --input <samples.csv> --meta <samples_meta.json> [--out <dir>]\n"
              << "  wavebreak evaluate [--out <dir>]\n"
              << "  wavebreak benchmark\n"
              << "  wavebreak demo\n"
              << "  wavebreak --help | -h\n"
              << "  wavebreak --version | -v\n\n"
              << "Available scenarios:\n"
              << "  clean, low_noise, high_noise, narrowband, broadband, burst, multi, drift\n\n"
              << "Exit Codes:\n"
              << "  0 - OK\n"
              << "  1 - Runtime error\n"
              << "  2 - Usage or config error\n"
              << "  3 - File I/O error\n";
}

void CLI::print_version() {
    std::cout << "WaveBreak version 1.0.0 (C++17 simulation)\n";
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
        if (fs::exists("config/default_config.json")) {
            try {
                cfg = ConfigLoader::load_from_file("config/default_config.json");
            } catch (...) {}
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

        if (!config_file.empty()) {
            try {
                cfg = ConfigLoader::load_from_file(config_file);
            } catch (const std::exception& e) {
                std::cerr << "Config load error: " << e.what() << "\n";
                return static_cast<int>(ExitCode::FileIOError);
            }
        }

        if (!scenario_arg.empty()) {
            cfg.scenario = scenario_arg;
        }
        if (seed_arg >= 0) {
            cfg.seed = static_cast<uint64_t>(seed_arg);
        }
        if (!out_arg.empty()) {
            cfg.output_dir = out_arg;
        } else {
            cfg.output_dir = "results/" + cfg.scenario;
        }

        try {
            ConfigLoader::validate(cfg);
        } catch (const std::exception& e) {
            std::cerr << "Configuration validation error: " << e.what() << "\n";
            return static_cast<int>(ExitCode::UsageError);
        }

        try {
            SimulationResult sim = SignalGenerator::generate_scenario(cfg);

            SpectrogramResult spec = SpectrumAnalyzer::compute_spectrogram(
                sim.samples, cfg.fft_size, cfg.hop_size, cfg.fs, cfg.noise_floor_floor_db
            );

            auto decimated_spec = SpectrumAnalyzer::decimate_bins(spec.power_db, 512);

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
        std::string input_path;
        std::string meta_path;
        std::string out_dir;

        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--input" && i + 1 < argc) {
                input_path = argv[++i];
            } else if (arg == "--meta" && i + 1 < argc) {
                meta_path = argv[++i];
            } else if (arg == "--out" && i + 1 < argc) {
                out_dir = argv[++i];
            } else {
                std::cerr << "Error: Unknown argument: " << arg << "\n";
                print_usage();
                return static_cast<int>(ExitCode::UsageError);
            }
        }

        if (input_path.empty() || meta_path.empty()) {
            std::cerr << "Error: --input and --meta are required for analyze command.\n";
            print_usage();
            return static_cast<int>(ExitCode::UsageError);
        }

        if (out_dir.empty()) {
            fs::path p(input_path);
            out_dir = p.has_parent_path() ? p.parent_path().string() : ".";
        }

        SampleVector samples;
        if (!IoWriters::read_samples_csv(input_path, samples)) {
            std::cerr << "File I/O error: failed reading input samples from " << input_path << "\n";
            return static_cast<int>(ExitCode::FileIOError);
        }

        double fs_val = 1e6;
        size_t n_samples = 0;
        std::string scenario;
        uint64_t seed = 42;
        if (!IoWriters::read_samples_meta_json(meta_path, fs_val, n_samples, scenario, seed)) {
            std::cerr << "File I/O error: failed reading metadata from " << meta_path << "\n";
            return static_cast<int>(ExitCode::FileIOError);
        }

        Config cfg;
        cfg.fs = fs_val;
        cfg.num_samples = samples.size();
        cfg.scenario = scenario;
        cfg.seed = seed;
        cfg.output_dir = out_dir;

        SpectrogramResult spec = SpectrumAnalyzer::compute_spectrogram(
            samples, cfg.fft_size, cfg.hop_size, cfg.fs, cfg.noise_floor_floor_db
        );

        std::vector<DetectedEvent> dets = InterferenceDetector::detect(spec, cfg);

        std::string events_path = out_dir + "/events.json";
        if (!DetectionEvaluator::write_events_json(events_path, dets)) {
            std::cerr << "File I/O error: failed writing " << events_path << "\n";
            return static_cast<int>(ExitCode::FileIOError);
        }

        std::cout << "Analysis complete. Detected " << dets.size() << " event(s).\n"
                  << "Events written to: " << events_path << "\n";

        // If ground_truth.json exists in the input directory, evaluate metrics
        std::string truth_path = out_dir + "/ground_truth.json";
        if (fs::exists(truth_path)) {
            std::vector<GroundTruthEvent> truth;
            if (IoWriters::read_ground_truth_json(truth_path, truth)) {
                double total_dur = static_cast<double>(samples.size()) / cfg.fs;
                double win_dur = static_cast<double>(cfg.fft_size) / cfg.fs;
                EvaluationMetrics m = DetectionEvaluator::evaluate(truth, dets, total_dur, win_dur);
                std::string metrics_path = out_dir + "/metrics.json";
                DetectionEvaluator::write_metrics_json(metrics_path, m);
                std::cout << "Evaluation metrics written to: " << metrics_path << "\n"
                          << "  TP=" << m.tp << " FP=" << m.fp << " FN=" << m.fn;
                if (m.f1_defined) std::cout << " F1=" << m.f1;
                std::cout << "\n";
            }
        }

        return static_cast<int>(ExitCode::Success);
    }
    else if (command == "evaluate") {
        std::string out_dir = "results";
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--out" && i + 1 < argc) {
                out_dir = argv[++i];
            }
        }
        Config cfg;
        return ExperimentRunner::run_all_experiments(cfg, out_dir);
    }
    else if (command == "benchmark") {
        return ExperimentRunner::run_benchmark();
    }
    else if (command == "demo") {
        return ExperimentRunner::run_demo();
    }

    std::cerr << "Error: Unknown command '" << command << "'\n";
    print_usage();
    return static_cast<int>(ExitCode::UsageError);
}

} // namespace wavebreak
