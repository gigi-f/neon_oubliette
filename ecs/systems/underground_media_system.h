#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_UNDERGROUND_MEDIA_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_UNDERGROUND_MEDIA_SYSTEM_H

#include <entt/entt.hpp>
#include "../system_scheduler.h"
#include "../components/underground_media_components.h"
#include "../components/simulation_layers.h"
#include "../simulation_coordinator.h"
#include "../event_declarations.h"

namespace NeonOubliette::Systems {

/**
 * @brief [NEW SYSTEM] Manages clandestine information networks.
 *        Handles pirate broadcasts, physical data slabs, and guard detection.
 */
class UndergroundMediaSystem : public NeonOubliette::ISimulationSystem {
public:
    UndergroundMediaSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override;
    void update(double delta_time) override;
    SimulationLayer simulation_layer() const override { return SimulationLayer::L4_Political; }

    // Event Handlers
    void onUseItem(const UseItemEvent& event);
    void onObserve(const InspectEvent& event); // Detection logic

private:
    void process_node_pulses();
    void emit_pirate_information(entt::entity node_entity, PirateNodeComponent& node);
    void check_guard_detection();

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    uint64_t m_current_tick = 0;
};

} // namespace NeonOubliette::Systems

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_UNDERGROUND_MEDIA_SYSTEM_H
