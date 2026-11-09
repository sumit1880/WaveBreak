#pragma once

#include "wavebreak/common.hpp"
#include "wavebreak/interference_detector.hpp"
#include <vector>
#include <string>
#include <map>

namespace wavebreak {

struct WindowConfusionMatrix {
    int tp{0};
    int fp{0};
    int tn{0};
    int fn{0};
};

struct EvaluationMetrics {
    int tp{0};
    int fp{0};
    int fn{0};
    double precision{0.0};
    double recall{0.0};
    double f1{0.0};
    bool precision_defined{false};
    bool recall_defined{false};
    bool f1_defined{false};
    double false_alarms_per_minute{0.0};
    double mean_detection_delay_sec{0.0};

    WindowConfusionMatrix window_confusion;
    // Event-level type confusion: true_type -> (pred_type -> count)
    std::map<std::string, std::map<std::string, int>> type_confusion;
};

class DetectionEvaluator {
public:
    static EvaluationMetrics evaluate(
        const std::vector<GroundTruthEvent>& truth,
        const std::vector<DetectedEvent>& detections,
        double total_duration,
        double window_duration);

    // Converts metrics to JSON string
    static std::string to_json(const EvaluationMetrics& metrics);

    // Writes metrics.json
    static bool write_metrics_json(const std::string& filepath, const EvaluationMetrics& metrics);

    // Writes events.json
    static bool write_events_json(const std::string& filepath, const std::vector<DetectedEvent>& events);
};

} // namespace wavebreak
