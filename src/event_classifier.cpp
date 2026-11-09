#include "spectrum_sentinel/event_classifier.hpp"
#include <sstream>
#include <iomanip>

namespace spectrum_sentinel {

std::string EventClassifier::determine_severity(
    const std::string& category,
    double margin_db)
{
    if (category == "narrowband") {
        if (margin_db < 18.0) return "low";
        if (margin_db < 25.0) return "medium";
        return "high";
    } else if (category == "broadband") {
        if (margin_db < 6.0) return "low";
        if (margin_db < 12.0) return "medium";
        return "high";
    } else {
        if (margin_db < 8.0) return "low";
        if (margin_db < 15.0) return "medium";
        return "high";
    }
}

void EventClassifier::classify_and_assign_severity(
    DetectedEvent& event,
    double total_duration)
{
    // 1. Spectral Category (deterministic check)
    double bw = event.freq_max - event.freq_min;
    if (event.spectral_category.empty()) {
        if (bw >= 50000.0) {
            event.spectral_category = "broadband";
        } else {
            event.spectral_category = "narrowband";
        }
    }

    // 2. Temporal Flag: continuous if it covers >= 90% of recording, else burst
    double active_duration = event.end_time - event.start_time;
    if (total_duration > 0.0 && (active_duration / total_duration) >= 0.90) {
        event.temporal_flag = "continuous";
    } else {
        event.temporal_flag = "burst";
    }

    // 3. Severity
    event.severity = determine_severity(event.spectral_category, event.margin_db);

    // 4. Reason naming the rule that fired
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);
    if (event.spectral_category == "narrowband") {
        ss << "Narrowband tone detected at " << (event.frequency >= 0 ? "+" : "")
           << (event.frequency / 1000.0) << " kHz with " << event.margin_db
           << " dB margin above median noise floor (" << event.temporal_flag << ")";
    } else if (event.spectral_category == "broadband") {
        ss << "Broadband energy detected in [" << (event.freq_min / 1000.0) << ", "
           << (event.freq_max / 1000.0) << "] kHz (" << (bw / 1000.0)
           << " kHz wide) with " << event.margin_db << " dB margin above median noise floor ("
           << event.temporal_flag << ")";
    } else {
        ss << "Uncertain spectral anomaly detected with " << event.margin_db
           << " dB margin above median noise floor";
    }
    event.reason = ss.str();
}

} // namespace spectrum_sentinel
