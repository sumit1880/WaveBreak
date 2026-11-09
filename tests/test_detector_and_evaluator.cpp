#include "test_harness.hpp"
#include "spectrum_sentinel/signal_generator.hpp"
#include "spectrum_sentinel/spectrum_analyzer.hpp"
#include "spectrum_sentinel/interference_detector.hpp"
#include "spectrum_sentinel/detection_evaluator.hpp"

using namespace spectrum_sentinel;

void run_detector_and_evaluator_tests() {
    std::cout << "[TEST] Running Detector and Evaluator tests...\n";

    Config cfg;
    cfg.num_samples = 262144;
    cfg.fft_size = 1024;
    cfg.hop_size = 512;
    cfg.fs = 1e6;
    cfg.threshold_db = 20.0;
    double total_dur = static_cast<double>(cfg.num_samples) / cfg.fs;
    double win_dur = static_cast<double>(cfg.fft_size) / cfg.fs;

    // 1. Clean gives no events
    {
        cfg.scenario = "clean";
        cfg.seed = 42;
        SimulationResult sim = SignalGenerator::generate_scenario(cfg);
        SpectrogramResult spec = SpectrumAnalyzer::compute_spectrogram(
            sim.samples, cfg.fft_size, cfg.hop_size, cfg.fs, cfg.noise_floor_floor_db);
        auto dets = InterferenceDetector::detect(spec, cfg);
        ASSERT_TRUE(dets.empty());

        EvaluationMetrics m = DetectionEvaluator::evaluate(sim.ground_truth, dets, total_dur, win_dur);
        ASSERT_TRUE(m.tp == 0);
        ASSERT_TRUE(m.fp == 0);
        ASSERT_TRUE(m.fn == 0);
        ASSERT_FALSE(m.precision_defined);
        ASSERT_FALSE(m.recall_defined);
        ASSERT_FALSE(m.f1_defined);
        ASSERT_NEAR(m.false_alarms_per_minute, 0.0, 1e-6);
    }

    // 2. Narrowband detected accurately
    {
        cfg.scenario = "narrowband";
        cfg.seed = 42;
        SimulationResult sim = SignalGenerator::generate_scenario(cfg);
        SpectrogramResult spec = SpectrumAnalyzer::compute_spectrogram(
            sim.samples, cfg.fft_size, cfg.hop_size, cfg.fs, cfg.noise_floor_floor_db);
        auto dets = InterferenceDetector::detect(spec, cfg);

        ASSERT_TRUE(dets.size() == 1);
        if (!dets.empty()) {
            ASSERT_TRUE(dets[0].spectral_category == "narrowband");
            ASSERT_TRUE(dets[0].temporal_flag == "continuous");
            ASSERT_NEAR(dets[0].frequency, 250000.0, 1000.0);
            ASSERT_TRUE(dets[0].margin_db > 10.0);
        }

        EvaluationMetrics m = DetectionEvaluator::evaluate(sim.ground_truth, dets, total_dur, win_dur);
        ASSERT_TRUE(m.tp == 1);
        ASSERT_TRUE(m.fp == 0);
        ASSERT_TRUE(m.fn == 0);
        ASSERT_TRUE(m.precision_defined);
        ASSERT_NEAR(m.precision, 1.0, 1e-6);
        ASSERT_TRUE(m.recall_defined);
        ASSERT_NEAR(m.recall, 1.0, 1e-6);
        ASSERT_TRUE(m.f1_defined);
        ASSERT_NEAR(m.f1, 1.0, 1e-6);
        ASSERT_NEAR(m.false_alarms_per_minute, 0.0, 1e-6);
    }

    // 3. Broadband detected accurately
    {
        cfg.scenario = "broadband";
        cfg.seed = 42;
        SimulationResult sim = SignalGenerator::generate_scenario(cfg);
        SpectrogramResult spec = SpectrumAnalyzer::compute_spectrogram(
            sim.samples, cfg.fft_size, cfg.hop_size, cfg.fs, cfg.noise_floor_floor_db);
        auto dets = InterferenceDetector::detect(spec, cfg);

        ASSERT_TRUE(dets.size() == 1);
        if (!dets.empty()) {
            ASSERT_TRUE(dets[0].spectral_category == "broadband");
            ASSERT_TRUE(dets[0].temporal_flag == "continuous");
            ASSERT_TRUE((dets[0].freq_max - dets[0].freq_min) >= 50000.0);
            ASSERT_TRUE(dets[0].freq_min <= 250000.0 && dets[0].freq_max >= 300000.0);
        }

        EvaluationMetrics m = DetectionEvaluator::evaluate(sim.ground_truth, dets, total_dur, win_dur);
        ASSERT_TRUE(m.tp == 1);
        ASSERT_TRUE(m.fp == 0);
        ASSERT_TRUE(m.fn == 0);
        ASSERT_NEAR(m.f1, 1.0, 1e-6);
    }

    // 4. Burst detected as 3 separate events
    {
        cfg.scenario = "burst";
        cfg.seed = 42;
        SimulationResult sim = SignalGenerator::generate_scenario(cfg);
        SpectrogramResult spec = SpectrumAnalyzer::compute_spectrogram(
            sim.samples, cfg.fft_size, cfg.hop_size, cfg.fs, cfg.noise_floor_floor_db);
        auto dets = InterferenceDetector::detect(spec, cfg);

        ASSERT_TRUE(dets.size() == 3);
        for (const auto& ev : dets) {
            ASSERT_TRUE(ev.spectral_category == "narrowband");
            ASSERT_TRUE(ev.temporal_flag == "burst");
            ASSERT_NEAR(ev.frequency, 250000.0, 1000.0);
        }

        EvaluationMetrics m = DetectionEvaluator::evaluate(sim.ground_truth, dets, total_dur, win_dur);
        ASSERT_TRUE(m.tp == 3);
        ASSERT_TRUE(m.fp == 0);
        ASSERT_TRUE(m.fn == 0);
        ASSERT_NEAR(m.f1, 1.0, 1e-6);
    }

    // 5. Metric formulas against hand-computed values
    {
        std::vector<GroundTruthEvent> truth = {
            {1, "narrowband", 0.0, 0.1, 200000.0, 199000.0, 201000.0, 0.5, 0.0},
            {2, "narrowband", 0.15, 0.25, 300000.0, 299000.0, 301000.0, 0.5, 0.0}
        };

        // 3 detections: 2 match truth, 1 is a false alarm
        std::vector<DetectedEvent> dets = {
            {1, 0.0, 0.1, 200000.0, 199000.0, 201000.0, 0.0, 25.0, "narrowband", "burst", "medium", "r1"},
            {2, 0.15, 0.25, 300000.0, 299000.0, 301000.0, 0.0, 26.0, "narrowband", "burst", "medium", "r2"},
            {3, 0.05, 0.15, -150000.0, -151000.0, -149000.0, 0.0, 24.0, "narrowband", "burst", "low", "r3"} // FP
        };

        EvaluationMetrics m = DetectionEvaluator::evaluate(truth, dets, 1.0, 0.001);
        ASSERT_TRUE(m.tp == 2);
        ASSERT_TRUE(m.fp == 1);
        ASSERT_TRUE(m.fn == 0);

        // Hand calculation:
        // Precision = 2 / (2 + 1) = 2/3 ~= 0.6666667
        // Recall = 2 / (2 + 0) = 1.0
        // F1 = 2 * (2/3) * 1 / (2/3 + 1) = 0.8
        ASSERT_TRUE(m.precision_defined);
        ASSERT_NEAR(m.precision, 2.0 / 3.0, 1e-6);
        ASSERT_TRUE(m.recall_defined);
        ASSERT_NEAR(m.recall, 1.0, 1e-6);
        ASSERT_TRUE(m.f1_defined);
        ASSERT_NEAR(m.f1, 0.8, 1e-6);

        // False alarms per minute: 1 FP in 1.0 second = 60 FA / min
        ASSERT_NEAR(m.false_alarms_per_minute, 60.0, 1e-6);
    }

    // 6. Empty inputs and boundaries
    {
        SpectrogramResult empty_spec;
        auto dets = InterferenceDetector::detect(empty_spec, cfg);
        ASSERT_TRUE(dets.empty());

        std::vector<GroundTruthEvent> empty_gt;
        EvaluationMetrics m = DetectionEvaluator::evaluate(empty_gt, dets, 0.0, 0.0);
        ASSERT_TRUE(m.tp == 0 && m.fp == 0 && m.fn == 0);
        ASSERT_FALSE(m.precision_defined);
        ASSERT_FALSE(m.recall_defined);
        ASSERT_FALSE(m.f1_defined);
    }
}
