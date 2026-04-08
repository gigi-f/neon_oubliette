#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_WANTED_LEVEL_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_WANTED_LEVEL_SYSTEM_H

#include <entt/entt.hpp>
#include "../simulation_coordinator.h"
#include "../components/components.h"
#include "../event_declarations.h"

namespace NeonOubliette {

/**
 * @brief [I.5] Manages the player's wanted level across different factions.
 *        Handles increase on crime, decay over time, and guard alerts.
 */
class WantedLevelSystem : public ISimulationSystem {
public:
    WantedLevelSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override;
    void update(double delta_time) override;
    SimulationLayer simulation_layer() const override { return SimulationLayer::L4_Political; }

    void handleCrimeReport(const CrimeReportEvent& event);

private:
    void processDecay();
    void updateGuardAlerts();

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    uint64_t m_last_decay_tick = 0;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_WANTED_LEVEL_SYSTEM_H
