#include "faction_system.h"

namespace NeonOubliette {

FactionSystem::FactionSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher)
{
    m_dispatcher.sink<ChangeFactionStandingEvent>().connect<&FactionSystem::handleChangeFactionStanding>(*this);
}

void FactionSystem::handleChangeFactionStanding(const ChangeFactionStandingEvent& event) {
    m_dispatcher.enqueue<LogEvent>({
        "Faction standing changed: " + event.acting_faction_id + " -> " + event.target_faction_id + " by " + std::to_string(event.standing_change),
        LogSeverity::INFO,
        "FactionSystem"
    });
}

} // namespace NeonOubliette
