#include "spectrum_sentinel/detection_evaluator.hpp"
#include "spectrum_sentinel/io_writers.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace spectrum_sentinel {

EvaluationMetrics DetectionEvaluator::evaluate(
    const std::vector<GroundTruthEvent>& truth,
    const std::vector<DetectedEvent>& detections,
    double total_duration,
    double window_duration)
{
    EvaluationMetrics metrics;

    // Initialize type confusion matrix categories
    std::vector<std::string> known_types = {"none", "narrowband", "broadband", "drift"};
    for (const auto& t1 : known_types) {
        for (const auto& t2 : known_types) {
            metrics.type_confusion[t1][t2] = 0;
        }
    }

    std::vector<bool> truth_matched(truth.size(), false);
    std::vector<bool> det_matched(detections.size(), false);
    double total_delay = 0.0;
    int match_count = 0;

    // One-to-one matching
    for (size_t d = 0; d < detections.size(); ++d) {
        const auto& det = detections[d];
        int best_truth_idx = -1;
        double best_score = 1e9;

        for (size_t t = 0; t < truth.size(); ++t) {
            if (truth_matched[t]) continue;
            const auto& gt = truth[t];

            // 1. Time overlap with +/- 1 window tolerance
            bool time_overlaps = (det.end_time >= gt.start_time - window_duration) &&
                                 (det.start_time <= gt.end_time + window_duration);
            if (!time_overlaps) continue;

            // 2. Frequency matching
            bool freq_matches = false;
            if (gt.type == "narrowband" && det.spectral_category == "narrowband") {
                if (std::abs(det.frequency - gt.frequency) <= 2000.0) {
                    freq_matches = true;
                }
            } else if (gt.type == "broadband" || det.spectral_category == "broadband") {
                // Band overlap
                if (std::max(det.freq_min, gt.freq_min) <= std::min(det.freq_max, gt.freq_max)) {
                    freq_matches = true;
                }
            } else if (gt.type == "drift") {
                // Drift band overlap or detection frequency within drift range
                if (det.frequency >= gt.freq_min - 2000.0 && det.frequency <= gt.freq_max + 2000.0) {
                    freq_matches = true;
                } else if (std::max(det.freq_min, gt.freq_min) <= std::min(det.freq_max, gt.freq_max)) {
                    freq_matches = true;
                }
            }

            if (freq_matches) {
                // Score based on frequency closeness
                double score = std::abs(det.frequency - gt.frequency);
                if (score < best_score) {
                    best_score = score;
                    best_truth_idx = static_cast<int>(t);
                }
            }
        }

        if (best_truth_idx >= 0) {
            truth_matched[best_truth_idx] = true;
            det_matched[d] = true;
            match_count++;

            const auto& gt = truth[best_truth_idx];
            double delay = std::max(0.0, det.start_time - gt.start_time);
            total_delay += delay;

            // Type confusion
            metrics.type_confusion[gt.type][det.spectral_category]++;
        }
    }

    metrics.tp = match_count;
    metrics.fp = static_cast<int>(detections.size()) - match_count;
    metrics.fn = static_cast<int>(truth.size()) - match_count;

    // Unmatched detections are FPs (truth: none)
    for (size_t d = 0; d < detections.size(); ++d) {
        if (!det_matched[d]) {
            metrics.type_confusion["none"][detections[d].spectral_category]++;
        }
    }

    // Unmatched ground truth are FNs (prediction: none)
    for (size_t t = 0; t < truth.size(); ++t) {
        if (!truth_matched[t]) {
            metrics.type_confusion[truth[t].type]["none"]++;
        }
    }

    // Precision, Recall, F1
    if (metrics.tp + metrics.fp > 0) {
        metrics.precision = static_cast<double>(metrics.tp) / static_cast<double>(metrics.tp + metrics.fp);
        metrics.precision_defined = true;
    } else {
        metrics.precision_defined = false;
    }

    if (metrics.tp + metrics.fn > 0) {
        metrics.recall = static_cast<double>(metrics.tp) / static_cast<double>(metrics.tp + metrics.fn);
        metrics.recall_defined = true;
    } else {
        metrics.recall_defined = false;
    }

    if (metrics.precision_defined && metrics.recall_defined && (metrics.precision + metrics.recall > 0.0)) {
        metrics.f1 = 2.0 * metrics.precision * metrics.recall / (metrics.precision + metrics.recall);
        metrics.f1_defined = true;
    } else {
        metrics.f1_defined = false;
    }

    // False alarms per simulated minute
    if (total_duration > 0.0) {
        metrics.false_alarms_per_minute = static_cast<double>(metrics.fp) * (60.0 / total_duration);
    }

    // Mean detection delay
    if (metrics.tp > 0) {
        metrics.mean_detection_delay_sec = total_delay / static_cast<double>(metrics.tp);
    } else {
        metrics.mean_detection_delay_sec = 0.0;
    }

    // Window-level confusion matrix
    if (window_duration > 0.0 && total_duration > 0.0) {
        size_t num_windows = static_cast<size_t>(std::ceil(total_duration / window_duration));
        for (size_t w = 0; w < num_windows; ++w) {
            double t_win = (static_cast<double>(w) + 0.5) * window_duration;

            bool is_truth_pos = false;
            for (const auto& gt : truth) {
                if (t_win >= gt.start_time && t_win <= gt.end_time) {
                    is_truth_pos = true;
                    break;
                }
            }

            bool is_det_pos = false;
            for (const auto& det : detections) {
                if (t_win >= det.start_time && t_win <= det.end_time) {
                    is_det_pos = true;
                    break;
                }
            }

            if (is_truth_pos && is_det_pos) {
                metrics.window_confusion.tp++;
            } else if (!is_truth_pos && is_det_pos) {
                metrics.window_confusion.fp++;
            } else if (!is_truth_pos && !is_det_pos) {
                metrics.window_confusion.tn++;
            } else if (is_truth_pos && !is_det_pos) {
                metrics.window_confusion.fn++;
            }
        }
    }

    return metrics;
}

std::string DetectionEvaluator::to_json(const EvaluationMetrics& m) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"tp\": " << m.tp << ",\n";
    ss << "  \"fp\": " << m.fp << ",\n";
    ss << "  \"fn\": " << m.fn << ",\n";

    ss << "  \"precision\": ";
    if (m.precision_defined) {
        ss << std::fixed << std::setprecision(4) << m.precision << ",\n";
    } else {
        ss << "null,\n";
    }

    ss << "  \"recall\": ";
    if (m.recall_defined) {
        ss << std::fixed << std::setprecision(4) << m.recall << ",\n";
    } else {
        ss << "null,\n";
    }

    ss << "  \"f1\": ";
    if (m.f1_defined) {
        ss << std::fixed << std::setprecision(4) << m.f1 << ",\n";
    } else {
        ss << "null,\n";
    }

    ss << "  \"false_alarms_per_minute\": " << std::fixed << std::setprecision(2)
       << m.false_alarms_per_minute << ",\n";
    ss << "  \"mean_detection_delay_sec\": " << std::fixed << std::setprecision(6)
       << m.mean_detection_delay_sec << ",\n";

    ss << "  \"window_confusion\": {\n";
    ss << "    \"tp\": " << m.window_confusion.tp << ",\n";
    ss << "    \"fp\": " << m.window_confusion.fp << ",\n";
    ss << "    \"tn\": " << m.window_confusion.tn << ",\n";
    ss << "    \"fn\": " << m.window_confusion.fn << "\n";
    ss << "  },\n";

    ss << "  \"type_confusion\": {\n";
    size_t row_i = 0;
    for (const auto& row : m.type_confusion) {
        ss << "    \"" << row.first << "\": {";
        size_t col_j = 0;
        for (const auto& col : row.second) {
            ss << "\"" << col.first << "\": " << col.second;
            if (++col_j < row.second.size()) ss << ", ";
        }
        ss << "}";
        if (++row_i < m.type_confusion.size()) ss << ",";
        ss << "\n";
    }
    ss << "  }\n";
    ss << "}\n";
    return ss.str();
}

bool DetectionEvaluator::write_metrics_json(
    const std::string& filepath,
    const EvaluationMetrics& metrics)
{
    fs::path p(filepath);
    if (p.has_parent_path()) {
        IoWriters::ensure_directory(p.parent_path().string());
    }
    std::ofstream out(filepath);
    if (!out.is_open()) return false;
    out << to_json(metrics);
    return true;
}

bool DetectionEvaluator::write_events_json(
    const std::string& filepath,
    const std::vector<DetectedEvent>& events)
{
    fs::path p(filepath);
    if (p.has_parent_path()) {
        IoWriters::ensure_directory(p.parent_path().string());
    }

    std::ofstream out(filepath);
    if (!out.is_open()) return false;

    out << "[\n";
    for (size_t i = 0; i < events.size(); ++i) {
        const auto& ev = events[i];
        out << "  {\n";
        out << "    \"id\": " << ev.id << ",\n";
        out << "    \"start_time\": " << std::fixed << std::setprecision(6) << ev.start_time << ",\n";
        out << "    \"end_time\": " << std::fixed << std::setprecision(6) << ev.end_time << ",\n";
        out << "    \"frequency\": " << std::fixed << std::setprecision(2) << ev.frequency << ",\n";
        out << "    \"freq_min\": " << std::fixed << std::setprecision(2) << ev.freq_min << ",\n";
        out << "    \"freq_max\": " << std::fixed << std::setprecision(2) << ev.freq_max << ",\n";
        out << "    \"peak_power_db\": " << std::fixed << std::setprecision(2) << ev.peak_power_db << ",\n";
        out << "    \"margin_db\": " << std::fixed << std::setprecision(2) << ev.margin_db << ",\n";
        out << "    \"spectral_category\": \"" << ev.spectral_category << "\",\n";
        out << "    \"temporal_flag\": \"" << ev.temporal_flag << "\",\n";
        out << "    \"severity\": \"" << ev.severity << "\",\n";
        out << "    \"reason\": \"" << ev.reason << "\"\n";
        out << "  }" << (i + 1 < events.size() ? ",\n" : "\n");
    }
    out << "]\n";
    return true;
}

} // namespace spectrum_sentinel
