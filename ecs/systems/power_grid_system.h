#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_POWER_GRID_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_POWER_GRID_SYSTEM_H

#include <entt/entt.hpp>
#include "../simulation_coordinator.h"
#include "../components/infrastructure_components.h"
#include "../event_declarations.h"

namespace NeonOubliette::Systems {

/**
 * @brief [L.5] Simulates the city's electrical grid, power flow, and failures.
 */
class PowerGridSystem : public ISimulationSystem {
public:
    PowerGridSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override;
    void update(double delta_time) override;
    SimulationLayer simulation_layer() const override { return SimulationLayer::L0_Physics; }

    // Event Handlers
    void on_crisis_started(const CrisisStartedEvent& event);
    void on_crisis_resolved(const CrisisResolvedEvent& event);
    void on_power_outage(const PowerOutageEvent& event);

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;

    void update_power_flow();
    void apply_outage_effects(entt::entity chunk_entity, float severity);
    void restore_power(entt::entity chunk_entity);
};

} // namespace NeonOubliette::Systems

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_POWER_GRID_SYSTEM_H
