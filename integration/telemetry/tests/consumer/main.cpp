#include "draftor/telemetry/TelemetryEmitter.h"
#include <stdexcept>
struct Base { virtual ~Base() = default; };
struct Derived : Base { int value{7}; };
int main() {
    int deliberately_unused; // Inherited -Wall/-Werror would reject this consumer.
    Base* object = new Derived;
    const bool rtti_works = dynamic_cast<Derived*>(object)->value == 7;
    delete object;
    bool exceptions_work = false;
    try { throw std::runtime_error("consumer keeps its exception policy"); }
    catch (const std::runtime_error&) { exceptions_work = true; }
    draftor::telemetry::TelemetryEmitter emitter(1,draftor::telemetry::PrivacyLevel::Off);
    draftor::telemetry::BehaviorEvent event{};
    return rtti_works && exceptions_work && emitter.try_emit(event)==draftor::telemetry::EmitResult::Disabled ? 0 : 1;
}
