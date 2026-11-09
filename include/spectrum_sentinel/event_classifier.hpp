#pragma once

#include "spectrum_sentinel/interference_detector.hpp"

namespace spectrum_sentinel {

class EventClassifier {
public:
    // Classifies event spectral category, temporal flag, severity, and text reason
    static void classify_and_assign_severity(
        DetectedEvent& event,
        double total_duration);

    // Determines severity string from margin and category
    static std::string determine_severity(
        const std::string& category,
        double margin_db);
};

} // namespace spectrum_sentinel
