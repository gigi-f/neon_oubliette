#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_MILESTONE_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_MILESTONE_SYSTEM_H

#include <entt/entt.hpp>
#include "../simulation_coordinator.h"
#include "../components/milestone_components.h"
#include "../event_declarations.h"

namespace NeonOubliette {

/**
 * @brief [NEW SYSTEM] Records major simulation outcomes as milestone events.
 */
class MilestoneSystem : public ISimulationSystem {
public:
    MilestoneSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override;
    void update(double delta_time) override;
    SimulationLayer simulation_layer() const override { return SimulationLayer::L4_Political; }

    void handleMilestoneEvent(const MilestoneEvent& event);
    void handleTurnEvent(const TurnEvent& event);

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    uint64_t m_currentTick = 0;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_MILESTONE_SYSTEM_H
