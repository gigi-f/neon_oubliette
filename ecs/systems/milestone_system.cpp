#include "milestone_system.h"
#include "../components/components.h"
#include "../components/milestone_components.h"

namespace NeonOubliette {

MilestoneSystem::MilestoneSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher)
{
    m_dispatcher.sink<MilestoneEvent>().connect<&MilestoneSystem::handleMilestoneEvent>(*this);
    m_dispatcher.sink<TurnEvent>().connect<&MilestoneSystem::handleTurnEvent>(*this);
}

void MilestoneSystem::initialize() {
    auto view = m_registry.view<WorldConfigComponent>();
    if (!view.empty()) {
        auto entity = view.front();
        if (!m_registry.all_of<MilestoneComponent>(entity)) {
            m_registry.emplace<MilestoneComponent>(entity);
        }
    }
}

void MilestoneSystem::update(double delta_time) {
    (void)delta_time;
}

void MilestoneSystem::handleTurnEvent(const TurnEvent& event) {
    m_currentTick = event.turn_number;
}

void MilestoneSystem::handleMilestoneEvent(const MilestoneEvent& event) {
    auto view = m_registry.view<MilestoneComponent>();
    if (view.empty()) return;

    auto& milestone_comp = m_registry.get<MilestoneComponent>(view.front());

    MilestoneRecord record;
    record.tick = m_currentTick;
    record.type = event.type;
    record.description = event.description;
    record.faction_a_id = event.faction_a_id;
    record.faction_b_id = event.faction_b_id;
    record.actor = event.actor;
    record.importance = event.importance;

    milestone_comp.history.push_back(record);

    if (milestone_comp.history.size() > milestone_comp.max_milestones) {
        milestone_comp.history.erase(milestone_comp.history.begin());
    }

    m_dispatcher.enqueue<LogEvent>({
        "[MILESTONE] " + event.type + ": " + event.description,
        LogSeverity::INFO,
        "MilestoneSystem"
    });

    if (event.importance >= 3.0f) {
        m_dispatcher.enqueue<HUDNotificationEvent>({
            "EVENT: " + event.description,
            5.0f,
            "#FFD700"
        });
    }
}

} // namespace NeonOubliette
