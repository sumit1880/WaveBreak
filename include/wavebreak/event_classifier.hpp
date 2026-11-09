#pragma once

#include "wavebreak/interference_detector.hpp"

namespace wavebreak {

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

} // namespace wavebreak
