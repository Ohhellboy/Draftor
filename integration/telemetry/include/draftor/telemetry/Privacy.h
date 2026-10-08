#pragma once
#include "BehaviorEvent.h"
namespace draftor::telemetry {
enum class FilterResult { Accepted, Suppressed, Invalid };
// Checks only supplied fixed fields, before sequence or loss accounting.
FilterResult validate_for_emission(const BehaviorEvent& source, PrivacyLevel level) noexcept;
// The only path into the queue. BASIC never copies sensitive bytes into it.
FilterResult filter_for_emission(const BehaviorEvent& source, PrivacyLevel level,
                                BehaviorEvent& destination) noexcept;
}
