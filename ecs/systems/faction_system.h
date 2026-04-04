#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_FACTION_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_FACTION_SYSTEM_H

#include <entt/entt.hpp>
#include "../simulation_coordinator.h"
#include "../components/components.h"
#include "../components/simulation_layers.h"
#include "../event_declarations.h"

namespace NeonOubliette {

class FactionSystem : public ISimulationSystem {
public:
    FactionSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override;
    void update(double delta_time) override;
    SimulationLayer simulation_layer() const override { return SimulationLayer::L4_Political; }

    void handleChangeFactionStanding(const ChangeFactionStandingEvent& event);
    void handleAgentReputation(const AgentFactionReputationEvent& event);

private:
    void diffuseInfluence();
    void updateAgentAffinities();
    void updateLeaders();
    void applyDirectivesToAgents();

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_FACTION_SYSTEM_H
