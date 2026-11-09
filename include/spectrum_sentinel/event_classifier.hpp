#pragma once

#include "spectrum_sentinel/interference_detector.hpp"

namespace spectrum_sentinel {

class EventClassifier {
public:
    static void classify_and_assign_severity(
        DetectedEvent& event,
        double total_duration);
};

} // namespace spectrum_sentinel
