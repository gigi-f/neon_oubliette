#include "turn_manager_system.h"

namespace NeonOubliette {

TurnManagerSystem::TurnManagerSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {
    m_dispatcher.sink<AdvanceTurnRequestEvent>().connect<&TurnManagerSystem::handleAdvanceTurnRequest>(*this);
}

void TurnManagerSystem::handleAdvanceTurnRequest(const AdvanceTurnRequestEvent& event) {
    m_currentTurnNumber++;
    
    m_dispatcher.trigger<LogEvent>({
        .message = "Turn Advanced to: " + std::to_string(m_currentTurnNumber),
        .severity = LogSeverity::INFO,
        .source_system = "TurnManagerSystem"
    });

    // Trigger turn-based logic in other systems (e.g. AI systems)
    m_dispatcher.trigger<TurnEvent>(TurnEvent{m_currentTurnNumber});
}

} // namespace NeonOubliette
