#include "draftor/telemetry/Privacy.h"
namespace draftor::telemetry {
FilterResult validate_for_emission(const BehaviorEvent& source, PrivacyLevel level) noexcept {
    if (!is_known_event_type(source.type)) return FilterResult::Invalid;
    if (level == PrivacyLevel::Off) return FilterResult::Suppressed;
    if (level == PrivacyLevel::Basic && source.type == EventType::CandidateItemDetail)
        return FilterResult::Suppressed;
#include "ValidateEnvelope.inc"
#include "ValidatePayload.inc"
    if (level == PrivacyLevel::Detailed &&
        (source.detail.text_length > kTextCapacity || source.detail.path_length > kPathCapacity))
        return FilterResult::Invalid;
    return FilterResult::Accepted;
}
FilterResult filter_for_emission(const BehaviorEvent& source, PrivacyLevel level,
                                BehaviorEvent& destination) noexcept {
    const auto result = validate_for_emission(source, level);
    if (result != FilterResult::Accepted) return result;
    destination = {};
    destination.envelope = source.envelope;
    destination.type = source.type;
    destination.payload = source.payload;
    if (level == PrivacyLevel::Detailed) {
        destination.detail = source.detail;
    }
    return FilterResult::Accepted;
}
}
