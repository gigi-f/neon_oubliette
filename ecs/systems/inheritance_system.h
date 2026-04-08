#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_INHERITANCE_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_INHERITANCE_SYSTEM_H

#include <entt/entt.hpp>
#include "../event_declarations.h"
#include "../system_scheduler.h"
#include "../components/components.h"
#include "../components/lod_components.h"
#include "../simulation_coordinator.h"

namespace NeonOubliette {

/**
 * @brief [J.3] Handles the transfer of assets (credits, items, portfolios) upon agent death.
 */
class InheritanceSystem : public ISimulationSystem {
public:
    InheritanceSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override;
    void update(double delta_time) override {} // Event-driven
    SimulationLayer simulation_layer() const override { return SimulationLayer::L3_Economic; }

    void handle_agent_death(const AgentDeathEvent& event);

private:
    void distribute_assets(uint64_t deceased_id, int credits, const std::string& faction_id, 
                           const std::map<std::string, uint64_t>& portfolio,
                           const std::vector<entt::entity>& items,
                           const std::unordered_map<uint64_t, RelationshipRecord>& relationships);

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_INHERITANCE_SYSTEM_H
