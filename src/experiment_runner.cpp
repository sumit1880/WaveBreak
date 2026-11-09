#include "spectrum_sentinel/experiment_runner.hpp"
#include "spectrum_sentinel/config_loader.hpp"
#include "spectrum_sentinel/signal_generator.hpp"
#include "spectrum_sentinel/spectrum_analyzer.hpp"
#include "spectrum_sentinel/interference_detector.hpp"
#include "spectrum_sentinel/detection_evaluator.hpp"
#include "spectrum_sentinel/io_writers.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <vector>
#include <map>

namespace spectrum_sentinel {

struct ScenarioAggregate {
    std::string scenario;
    int runs{0};
    int total_truth{0};
    int total_detections{0};
    int total_tp{0};
    int total_fp{0};
    int total_fn{0};
    double total_precision{0.0};
    int precision_defined_count{0};
    double total_recall{0.0};
    int recall_defined_count{0};
    double total_f1{0.0};
    int f1_defined_count{0};
    double total_fa_per_min{0.0};
    double total_delay_sec{0.0};
    int delay_count{0};
    int win_tp{0};
    int win_fp{0};
    int win_tn{0};
    int win_fn{0};
};

int ExperimentRunner::run_all_experiments(const Config& base_config, const std::string& out_dir) {
    std::cout << "========================================================\n"
              << "       SPECTRUM SENTINEL - EXPERIMENT RUNNER            \n"
              << "  Evaluating 8 Scenarios across 30 Seeds (240 runs)     \n"
              << "========================================================\n";

    IoWriters::ensure_directory(out_dir);

    std::vector<std::string> scenarios = {
        "clean", "low_noise", "high_noise",
        "narrowband", "broadband", "burst",
        "multi", "drift"
    };

    std::vector<ScenarioAggregate> aggregates;
    double total_duration = static_cast<double>(base_config.num_samples) / base_config.fs;
    double window_duration = static_cast<double>(base_config.fft_size) / base_config.fs;

    for (const auto& scen : scenarios) {
        std::cout << "[Evaluating] Scenario: " << std::left << std::setw(12) << scen << " ... " << std::flush;
        auto t_start = std::chrono::steady_clock::now();

        ScenarioAggregate agg;
        agg.scenario = scen;

        std::vector<DetectedEvent> rep_events;
        EvaluationMetrics rep_metrics;

        for (uint64_t seed = 1; seed <= 30; ++seed) {
            Config cfg = base_config;
            cfg.scenario = scen;
            cfg.seed = seed;
            cfg.output_dir = out_dir + "/" + scen;

            SimulationResult sim = SignalGenerator::generate_scenario(cfg);
            SpectrogramResult spec = SpectrumAnalyzer::compute_spectrogram(
                sim.samples, cfg.fft_size, cfg.hop_size, cfg.fs, cfg.noise_floor_floor_db
            );

            std::vector<DetectedEvent> dets = InterferenceDetector::detect(spec, cfg);
            EvaluationMetrics m = DetectionEvaluator::evaluate(sim.ground_truth, dets, total_duration, window_duration);

            agg.runs++;
            agg.total_truth += static_cast<int>(sim.ground_truth.size());
            agg.total_detections += static_cast<int>(dets.size());
            agg.total_tp += m.tp;
            agg.total_fp += m.fp;
            agg.total_fn += m.fn;
            if (m.precision_defined) {
                agg.total_precision += m.precision;
                agg.precision_defined_count++;
            }
            if (m.recall_defined) {
                agg.total_recall += m.recall;
                agg.recall_defined_count++;
            }
            if (m.f1_defined) {
                agg.total_f1 += m.f1;
                agg.f1_defined_count++;
            }
            agg.total_fa_per_min += m.false_alarms_per_minute;
            if (m.tp > 0) {
                agg.total_delay_sec += m.mean_detection_delay_sec;
                agg.delay_count++;
            }
            agg.win_tp += m.window_confusion.tp;
            agg.win_fp += m.window_confusion.fp;
            agg.win_tn += m.window_confusion.tn;
            agg.win_fn += m.window_confusion.fn;

            // Save seed 42 or first seed as representative
            if (seed == 42 || (seed == 1 && rep_events.empty())) {
                rep_events = dets;
                rep_metrics = m;
            }
        }

        auto t_end = std::chrono::steady_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();

        // Write per-scenario representative events.json and metrics.json
        std::string scen_dir = out_dir + "/" + scen;
        IoWriters::ensure_directory(scen_dir);
        DetectionEvaluator::write_events_json(scen_dir + "/events.json", rep_events);
        DetectionEvaluator::write_metrics_json(scen_dir + "/metrics.json", rep_metrics);

        double mean_f1 = (agg.f1_defined_count > 0) ? (agg.total_f1 / agg.f1_defined_count) : 0.0;
        double mean_fa = agg.total_fa_per_min / agg.runs;

        std::cout << "Done (" << std::fixed << std::setprecision(1) << elapsed_ms << " ms) | "
                  << "TP=" << std::setw(2) << agg.total_tp << " FP=" << std::setw(2) << agg.total_fp
                  << " FN=" << std::setw(2) << agg.total_fn;
        if (agg.f1_defined_count > 0) {
            std::cout << " F1=" << std::fixed << std::setprecision(3) << mean_f1;
        } else {
            std::cout << " F1=null ";
        }
        std::cout << " FA/min=" << std::fixed << std::setprecision(2) << mean_fa << "\n";

        aggregates.push_back(agg);
    }

    // Write summary.csv
    std::string csv_path = out_dir + "/summary.csv";
    std::ofstream csv(csv_path);
    if (!csv.is_open()) {
        std::cerr << "Failed to write " << csv_path << "\n";
        return 1;
    }

    csv << "scenario,runs,total_truth,total_detections,tp,fp,fn,precision,recall,f1,fa_per_min,mean_delay_sec,win_tp,win_fp,win_tn,win_fn\n";
    for (const auto& a : aggregates) {
        std::string p_str = (a.precision_defined_count > 0) ? std::to_string(a.total_precision / a.precision_defined_count) : "null";
        std::string r_str = (a.recall_defined_count > 0) ? std::to_string(a.total_recall / a.recall_defined_count) : "null";
        std::string f_str = (a.f1_defined_count > 0) ? std::to_string(a.total_f1 / a.f1_defined_count) : "null";
        double fa_avg = a.total_fa_per_min / static_cast<double>(a.runs);
        double delay_avg = (a.delay_count > 0) ? (a.total_delay_sec / a.delay_count) : 0.0;

        csv << a.scenario << ","
            << a.runs << ","
            << a.total_truth << ","
            << a.total_detections << ","
            << a.total_tp << ","
            << a.total_fp << ","
            << a.total_fn << ","
            << p_str << ","
            << r_str << ","
            << f_str << ","
            << std::fixed << std::setprecision(2) << fa_avg << ","
            << std::fixed << std::setprecision(6) << delay_avg << ","
            << a.win_tp << ","
            << a.win_fp << ","
            << a.win_tn << ","
            << a.win_fn << "\n";
    }
    csv.close();

    // Write summary.json
    std::string json_path = out_dir + "/summary.json";
    std::ofstream js(json_path);
    if (js.is_open()) {
        js << "{\n  \"scenarios\": [\n";
        for (size_t i = 0; i < aggregates.size(); ++i) {
            const auto& a = aggregates[i];
            double fa_avg = a.total_fa_per_min / static_cast<double>(a.runs);
            double delay_avg = (a.delay_count > 0) ? (a.total_delay_sec / a.delay_count) : 0.0;
            js << "    {\n"
               << "      \"scenario\": \"" << a.scenario << "\",\n"
               << "      \"runs\": " << a.runs << ",\n"
               << "      \"total_truth\": " << a.total_truth << ",\n"
               << "      \"total_detections\": " << a.total_detections << ",\n"
               << "      \"tp\": " << a.total_tp << ",\n"
               << "      \"fp\": " << a.total_fp << ",\n"
               << "      \"fn\": " << a.total_fn << ",\n"
               << "      \"precision\": " << ((a.precision_defined_count > 0) ? std::to_string(a.total_precision / a.precision_defined_count) : "null") << ",\n"
               << "      \"recall\": " << ((a.recall_defined_count > 0) ? std::to_string(a.total_recall / a.recall_defined_count) : "null") << ",\n"
               << "      \"f1\": " << ((a.f1_defined_count > 0) ? std::to_string(a.total_f1 / a.f1_defined_count) : "null") << ",\n"
               << "      \"fa_per_min\": " << std::fixed << std::setprecision(2) << fa_avg << ",\n"
               << "      \"mean_delay_sec\": " << std::fixed << std::setprecision(6) << delay_avg << ",\n"
               << "      \"window_confusion\": {\"tp\": " << a.win_tp << ", \"fp\": " << a.win_fp
               << ", \"tn\": " << a.win_tn << ", \"fn\": " << a.win_fn << "}\n"
               << "    }" << (i + 1 < aggregates.size() ? ",\n" : "\n");
        }
        js << "  ]\n}\n";
        js.close();
    }

    std::cout << "\nSummary exported successfully to:\n"
              << "  - " << csv_path << "\n"
              << "  - " << json_path << "\n";
    return 0;
}

int ExperimentRunner::run_benchmark() {
    std::cout << "========================================================\n"
              << "           SPECTRUM SENTINEL - BENCHMARK                \n"
              << "========================================================\n";

    std::string compiler_info = "Unknown C++ Compiler";
#if defined(__GNUC__)
    compiler_info = "GCC " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__) + "." + std::to_string(__GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    compiler_info = "MSVC " + std::to_string(_MSC_VER);
#elif defined(__clang__)
    compiler_info = "Clang " + std::string(__clang_version__);
#endif

    std::string build_type = "Release";
#if defined(DEBUG) || !defined(NDEBUG)
    build_type = "Debug";
#endif

    std::cout << "Compiler:   " << compiler_info << "\n"
              << "Build Type: " << build_type << "\n"
              << "FFT Size:   1024 (Hop: 512)\n"
              << "Warm-ups:   3\n"
              << "Iterations: 20\n"
              << "--------------------------------------------------------\n"
              << std::left << std::setw(12) << "Samples"
              << std::setw(14) << "Duration (s)"
              << std::setw(14) << "Windows"
              << std::setw(16) << "Median (ms)"
              << std::setw(16) << "Throughput (MS/s)"
              << "\n"
              << "--------------------------------------------------------\n";

    std::vector<size_t> sample_sizes = {
        65536,    // 2^16
        262144,   // 2^18
        1048576   // 2^20
    };

    size_t fft_size = 1024;
    size_t hop_size = 512;
    double fs = 1e6;

    for (size_t n_samples : sample_sizes) {
        // Pre-generate sample data
        Config cfg;
        cfg.num_samples = n_samples;
        cfg.scenario = "narrowband";
        cfg.seed = 42;
        SimulationResult sim = SignalGenerator::generate_scenario(cfg);

        // Warm-ups
        for (int w = 0; w < 3; ++w) {
            auto spec = SpectrumAnalyzer::compute_spectrogram(sim.samples, fft_size, hop_size, fs, -160.0);
            auto dets = InterferenceDetector::detect(spec, cfg);
            (void)dets;
        }

        // 20 timed iterations
        std::vector<double> timings_ms;
        timings_ms.reserve(20);
        size_t n_windows = 0;

        for (int it = 0; it < 20; ++it) {
            auto t0 = std::chrono::steady_clock::now();

            auto spec = SpectrumAnalyzer::compute_spectrogram(sim.samples, fft_size, hop_size, fs, -160.0);
            auto dets = InterferenceDetector::detect(spec, cfg);
            (void)dets;

            auto t1 = std::chrono::steady_clock::now();
            double dur_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            timings_ms.push_back(dur_ms);
            n_windows = spec.num_windows;
        }

        std::sort(timings_ms.begin(), timings_ms.end());
        double median_ms = (timings_ms[9] + timings_ms[10]) / 2.0;
        double throughput_msps = (static_cast<double>(n_samples) / 1e6) / (median_ms / 1000.0);
        double signal_duration = static_cast<double>(n_samples) / fs;

        std::cout << std::left << std::setw(12) << n_samples
                  << std::fixed << std::setprecision(3) << std::setw(14) << signal_duration
                  << std::setw(14) << n_windows
                  << std::fixed << std::setprecision(2) << std::setw(16) << median_ms
                  << std::fixed << std::setprecision(2) << std::setw(16) << throughput_msps
                  << "\n";
    }

    std::cout << "--------------------------------------------------------\n";
    return 0;
}

int ExperimentRunner::run_demo() {
    std::cout << "========================================================\n"
              << "         SPECTRUM SENTINEL - DEMO EXECUTION             \n"
              << "========================================================\n";

    Config cfg;
    cfg.scenario = "narrowband";
    cfg.seed = 42;
    cfg.output_dir = "results/narrowband";

    std::cout << "1. Simulating scenario: " << cfg.scenario << " (SNR=20dB, Tone=+250kHz, Amp=0.3)...\n";
    SimulationResult sim = SignalGenerator::generate_scenario(cfg);

    std::cout << "2. Computing 1024-FFT sliding spectrogram (hop=512)...\n";
    SpectrogramResult spec = SpectrumAnalyzer::compute_spectrogram(
        sim.samples, cfg.fft_size, cfg.hop_size, cfg.fs, cfg.noise_floor_floor_db
    );

    std::cout << "3. Running Interference Detector with median floor bias correction...\n";
    std::vector<DetectedEvent> dets = InterferenceDetector::detect(spec, cfg);

    std::cout << "4. Evaluating Detections against Ground Truth...\n";
    double total_duration = static_cast<double>(cfg.num_samples) / cfg.fs;
    double window_duration = static_cast<double>(cfg.fft_size) / cfg.fs;
    EvaluationMetrics m = DetectionEvaluator::evaluate(sim.ground_truth, dets, total_duration, window_duration);

    std::cout << "\n---------------- DETECTED EVENTS ----------------\n";
    if (dets.empty()) {
        std::cout << "  No events detected.\n";
    } else {
        for (const auto& ev : dets) {
            std::cout << "  Event #" << ev.id << " [" << ev.spectral_category << " | " << ev.temporal_flag << "]\n"
                      << "    Time span:  " << std::fixed << std::setprecision(4) << ev.start_time << " s -> " << ev.end_time << " s\n"
                      << "    Frequency:  " << std::fixed << std::setprecision(2) << (ev.frequency / 1000.0) << " kHz\n"
                      << "    Peak Power: " << std::fixed << std::setprecision(2) << ev.peak_power_db << " dB\n"
                      << "    Margin:     " << std::fixed << std::setprecision(2) << ev.margin_db << " dB (Severity: " << ev.severity << ")\n"
                      << "    Reason:     " << ev.reason << "\n\n";
        }
    }

    std::cout << "---------------- EVALUATION METRICS ----------------\n"
              << "  True Positives (TP):  " << m.tp << "\n"
              << "  False Positives (FP): " << m.fp << "\n"
              << "  False Negatives (FN): " << m.fn << "\n"
              << "  Precision:            " << (m.precision_defined ? std::to_string(m.precision) : "null") << "\n"
              << "  Recall:               " << (m.recall_defined ? std::to_string(m.recall) : "null") << "\n"
              << "  F1 Score:             " << (m.f1_defined ? std::to_string(m.f1) : "null") << "\n"
              << "  False Alarms / min:   " << std::fixed << std::setprecision(2) << m.false_alarms_per_minute << "\n"
              << "  Detection Delay:      " << std::fixed << std::setprecision(6) << m.mean_detection_delay_sec << " s\n"
              << "  Window Confusion:     TP=" << m.window_confusion.tp << " FP=" << m.window_confusion.fp
              << " TN=" << m.window_confusion.tn << " FN=" << m.window_confusion.fn << "\n"
              << "========================================================\n";

    return 0;
}

} // namespace spectrum_sentinel
