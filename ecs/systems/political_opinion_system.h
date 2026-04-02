#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_POLITICAL_OPINION_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_POLITICAL_OPINION_SYSTEM_H

#include "simulation_coordinator.h"
#include "../components/components.h"
#include "../event_declarations.h"
#include <iostream>

namespace NeonOubliette {

/**
 * @brief Manages the city-wide political opinion and faction influence (L4).
 */
class PoliticalOpinionSystem : public ISimulationSystem {
public:
    PoliticalOpinionSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {
        m_dispatcher.sink<CampaignEvent>().connect<&PoliticalOpinionSystem::handleCampaign>(this);
        m_dispatcher.sink<PolicyChangeEvent>().connect<&PoliticalOpinionSystem::handlePolicyChange>(this);
    }

    void update(double delta_time) override {
        // Global Public Opinion updates (L4)
        auto opinion_view = m_registry.view<PublicOpinionComponent>();
        for (auto opinion_entity : opinion_view) {
            auto& opinion = opinion_view.get<PublicOpinionComponent>(opinion_entity);
            
            // Factions' passive influence drift or competition
            auto faction_view = m_registry.view<FactionComponent, Layer4PoliticalComponent>();
            for (auto faction_entity : faction_view) {
                auto& faction = faction_view.get<FactionComponent>(faction_entity);
                auto& political = faction_view.get<Layer4PoliticalComponent>(faction_entity);
                
                // If the faction has high hard power (L4), influence (L4) drifts upward slowly
                if (political.hard_power > 0.8f) {
                    faction.influence += 0.001f;
                    if (faction.influence > 1.0f) faction.influence = 1.0f;
                }
                
                // Update aggregate opinion from Individual Loyalty (L4 propagation)
                float total_loyalty = 0.0f;
                int count = 0;
                auto citizen_view = m_registry.view<FactionAffiliationComponent>();
                for (auto citizen_entity : citizen_view) {
                    auto& affiliation = citizen_view.get<FactionAffiliationComponent>(citizen_entity);
                    if (affiliation.faction_id == faction_entity) {
                        total_loyalty += affiliation.loyalty;
                        count++;
                    }
                }
                
                if (count > 0) {
                    opinion.faction_approval[faction_entity] = total_loyalty / static_cast<float>(count);
                }
            }
        }
    }

    void handleCampaign(const CampaignEvent& event) {
        // Boost faction loyalty or candidate score
        if (m_registry.valid(event.target_faction_id) && m_registry.all_of<FactionComponent>(event.target_faction_id)) {
             auto& faction = m_registry.get<FactionComponent>(event.target_faction_id);
             faction.influence += 0.05f;
             if (faction.influence > 1.0f) faction.influence = 1.0f;
             
             // NPCs who are members of the faction or undecided might change loyalty
             auto citizen_view = m_registry.view<FactionAffiliationComponent>();
             for (auto entity : citizen_view) {
                 auto& affiliation = citizen_view.get<FactionAffiliationComponent>(entity);
                 if (affiliation.faction_id == event.target_faction_id) {
                     affiliation.loyalty += 0.01f;
                 }
             }
        }
    }

    void handlePolicyChange(const PolicyChangeEvent& event) {
        // Shift opinion based on policy impact
        m_dispatcher.enqueue<LogEvent>({"Policy Change: " + event.policy_name, LogSeverity::INFO, "PoliticalOpinionSystem"});
    }

    SimulationLayer simulation_layer() const override {
        return SimulationLayer::L4_Political;
    }

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_POLITICAL_OPINION_SYSTEM_H
