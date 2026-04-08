#ifndef NEON_OUBLIETTE_BROADCAST_TOWER_SYSTEM_H
#define NEON_OUBLIETTE_BROADCAST_TOWER_SYSTEM_H

#include <entt/entt.hpp>
#include "../components/components.h"
#include "../components/broadcast_components.h"
#include "../components/simulation_layers.h"
#include "../simulation_coordinator.h"
#include "../event_declarations.h"

namespace NeonOubliette::Systems {

/**
 * @brief [NEW SYSTEM] Manages Broadcast Towers as propagation nodes.
 *        Ticks at L4 (Political) every 20 ticks.
 */
class BroadcastTowerSystem : public NeonOubliette::ISimulationSystem {
public:
    BroadcastTowerSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override;
    void update(double delta_time) override;
    SimulationLayer simulation_layer() const override { return SimulationLayer::L4_Political; }

    /**
     * @brief Handle pulse events to trigger visual feedback or downstream effects.
     */
    void onBroadcastPulse(const BroadcastPulseEvent& event);

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    uint64_t m_current_tick = 0;

    /**
     * @brief Distributes information from a tower to nearby agents.
     */
    void emit_information(entt::entity tower_entity, const BroadcastTowerComponent& tower);
};

} // namespace NeonOubliette::Systems

#endif // NEON_OUBLIETTE_BROADCAST_TOWER_SYSTEM_H
