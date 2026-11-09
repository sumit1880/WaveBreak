#include "spectrum_sentinel/detection_evaluator.hpp"

namespace spectrum_sentinel {

EvaluationMetrics DetectionEvaluator::evaluate(
    const std::vector<GroundTruthEvent>& /*truth*/,
    const std::vector<DetectedEvent>& /*detections*/,
    double /*total_duration*/,
    double /*window_duration*/)
{
    return {};
}

} // namespace spectrum_sentinel
