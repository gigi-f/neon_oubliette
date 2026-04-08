#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_FACTION_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_FACTION_SYSTEM_H

#include <entt/entt.hpp>
#include "../simulation_coordinator.h"
#include "../components/components.h"
#include "../components/simulation_layers.h"
#include "../event_declarations.h"

namespace NeonOubliette {

struct GlobalFactionTensionComponent {
    std::map<std::pair<std::string, std::string>, float> relations; // -100 to 100
    template <class Archive> void serialize(Archive& ar) { ar(cereal::make_nvp("relations", relations)); }
};

class FactionSystem : public ISimulationSystem {
public:
    FactionSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override;
    void update(double delta_time) override;
    SimulationLayer simulation_layer() const override { return SimulationLayer::L4_Political; }

    void handleChangeFactionStanding(const ChangeFactionStandingEvent& event);
    void handleAgentReputation(const AgentFactionReputationEvent& event);
    void handleBackroomDeal(const BackroomDealEvent& event);
    void handleCrisisEffect(const CrisisEffectEvent& event);

private:
    void diffuseInfluence();
    void updateAgentAffinities();
    void updateLeaders();
    void applyDirectivesToAgents();
    void updatePoliticalClimate();
    void updateReputationDecay();

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_FACTION_SYSTEM_H
