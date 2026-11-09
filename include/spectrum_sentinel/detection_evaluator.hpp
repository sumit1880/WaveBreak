#pragma once

#include "spectrum_sentinel/common.hpp"
#include "spectrum_sentinel/interference_detector.hpp"
#include <vector>

namespace spectrum_sentinel {

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
    // Window-level confusion
    int win_tp{0};
    int win_fp{0};
    int win_tn{0};
    int win_fn{0};
};

class DetectionEvaluator {
public:
    static EvaluationMetrics evaluate(
        const std::vector<GroundTruthEvent>& truth,
        const std::vector<DetectedEvent>& detections,
        double total_duration,
        double window_duration);
};

} // namespace spectrum_sentinel
