#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_BIOLOGY_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_BIOLOGY_SYSTEM_H

#include "simulation_coordinator.h"
#include "../components/components.h"
#include "../components/simulation_layers.h"
#include <iostream>
#include <algorithm>
#include <cstdlib>

namespace NeonOubliette {

/**
 * @brief Processes Layer 1 Biological simulation.
 *        Links physical environment (L0) and agent needs to internal vitals.
 */
class BiologySystem : public ISimulationSystem {
public:
    BiologySystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {
    }

    void update(double delta_time) override {
        m_turn_counter++;
        auto view = m_registry.view<Layer1BiologyComponent, Layer0PhysicsComponent, NeedsComponent>();
        std::vector<entt::entity> dead_agents;

        for (auto entity : view) {
            auto& bio = view.get<Layer1BiologyComponent>(entity);
            auto& physics = view.get<Layer0PhysicsComponent>(entity);
            auto& needs = view.get<NeedsComponent>(entity);
            
            // 1. Environmental Effects (L0 -> L1)
            // Hyperthermia / Hypothermia
            if (physics.temperature_celsius > 45.0f) {
                bio.pain_level = std::min(10, bio.pain_level + 1);
                bio.vitals.heart_rate += 5.0f;
                bio.consciousness_level -= 0.05f;
            } else if (physics.temperature_celsius < 5.0f) {
                bio.vitals.core_temperature -= 0.1f;
            } else {
                // Homeostasis
                if (bio.vitals.core_temperature < 37.0f) bio.vitals.core_temperature += 0.05f;
                if (bio.vitals.core_temperature > 37.0f) bio.vitals.core_temperature -= 0.05f;
            }
            
            // 2. Needs Effects (Needs -> L1)
            // Severe thirst drops consciousness
            if (needs.thirst < 10.0f) {
                bio.consciousness_level -= 0.02f;
                bio.vitals.oxygen_saturation -= 0.1f;
            }
            
            // 3. Recovery
            if (bio.consciousness_level < 1.0f && needs.thirst > 20.0f && bio.pain_level < 5) {
                bio.consciousness_level = std::min(1.0f, bio.consciousness_level + 0.01f);
            }
            
            if (bio.pain_level > 0 && (m_turn_counter % 10 == 0)) {
                bio.pain_level--; // Natural pain decay
            }

            // Clamp values
            bio.consciousness_level = std::clamp(bio.consciousness_level, 0.0f, 1.0f);
            bio.vitals.heart_rate = std::clamp(bio.vitals.heart_rate, 0.0f, 220.0f);
            bio.vitals.oxygen_saturation = std::clamp(bio.vitals.oxygen_saturation, 0.0f, 100.0f);

            // [NEW] Death Check
            auto* age = m_registry.try_get<AgeComponent>(entity);
            bool natural_death = (age && age->biological_wear >= 1.0f);
            
            if (natural_death || (bio.consciousness_level <= 0.0f && bio.vitals.oxygen_saturation < 5.0f)) {
                dead_agents.push_back(entity);
            }
        }

        // [L.3] Biological Crisis: Infection & Contagion
        auto infected_view = m_registry.view<InfectionComponent, PositionComponent, Layer1BiologyComponent>();
        auto susceptible_view = m_registry.view<Layer1BiologyComponent, PositionComponent>(entt::exclude<InfectionComponent>);
        std::vector<entt::entity> new_infections;

        for (auto infected_entity : infected_view) {
            auto& infection = infected_view.get<InfectionComponent>(infected_entity);
            auto& infected_pos = infected_view.get<PositionComponent>(infected_entity);
            auto& infected_bio = infected_view.get<Layer1BiologyComponent>(infected_entity);
            
            // Progress disease
            infection.progress += 0.001f * (infection.severity + 0.5f);
            infected_bio.pain_level = std::max(infected_bio.pain_level, (int)(infection.progress * 7.0f));
            infected_bio.consciousness_level -= infection.progress * 0.005f;

            // Visual feedback: Sickly green-shift
            if (auto* render = m_registry.try_get<RenderableComponent>(infected_entity)) {
                if (m_turn_counter % 2 == 0) render->color = "#7FFF00"; 
                else render->color = "#228B22";
            }

            // Contagion
            if (infection.is_contagious && infection.progress > 0.05f) {
                for (auto susceptible_entity : susceptible_view) {
                    auto& sus_pos = susceptible_view.get<PositionComponent>(susceptible_entity);
                    
                    int dx = std::abs(infected_pos.x - sus_pos.x);
                    int dy = std::abs(infected_pos.y - sus_pos.y);
                    if (dx <= 2 && dy <= 2 && infected_pos.layer_id == sus_pos.layer_id) {
                        float chance = infection.virulence;
                        
                        // Social contact multiplier [Phase G]
                        if (auto* rels = m_registry.try_get<RelationshipComponent>(susceptible_entity)) {
                            uint64_t m_id = 0;
                            if (auto* npc = m_registry.try_get<NPCComponent>(infected_entity)) m_id = npc->macro_id;
                            if (rels->records.contains(m_id) && rels->records.at(m_id).affinity > 30.0f) {
                                chance *= 3.0f;
                            }
                        }

                        if (((double)rand() / (double)RAND_MAX) < (double)chance) {
                            new_infections.push_back(susceptible_entity);
                        }
                    }
                }
            }
            
            // Recovery check
            if (infection.progress >= 1.0f) {
                if (infected_bio.consciousness_level > 0.3f && (rand() % 100 < 2)) {
                    m_registry.remove<InfectionComponent>(infected_entity);
                    if (auto* render = m_registry.try_get<RenderableComponent>(infected_entity)) {
                        render->color = "#FFFFFF"; // Reset to default (or specific archetype if we had it stored)
                    }
                }
            }
        }
        for (auto entity : new_infections) {
            if (!m_registry.all_of<InfectionComponent>(entity)) {
                m_registry.emplace<InfectionComponent>(entity, 0.0f, 0.02f, 0.5f, static_cast<uint64_t>(m_turn_counter), true);
            }
        }

        // [L.3] Environmental Hazard Impact
        auto hazard_view = m_registry.view<EnvironmentalHazardComponent>();
        if (!hazard_view.empty()) {
            auto& hazard = hazard_view.get<EnvironmentalHazardComponent>(hazard_view.front());
            if (hazard.is_active) {
                auto agents_view = m_registry.view<Layer1BiologyComponent>();
                for (auto entity : agents_view) {
                    bool is_inside = m_registry.all_of<InteriorStateComponent>(entity);
                    if (!is_inside) {
                        auto& bio = agents_view.get<Layer1BiologyComponent>(entity);
                        bio.pain_level = std::min(10, bio.pain_level + (int)(hazard.toxicity_level * 1.5f));
                        bio.consciousness_level -= hazard.toxicity_level * 0.01f;
                    }
                }
            }
        }

        // Process deaths
        for (auto entity : dead_agents) {
            uint64_t m_id = 0;
            if (auto* npc = m_registry.try_get<NPCComponent>(entity)) m_id = npc->macro_id;
            
            m_dispatcher.trigger(AgentDeathEvent{entity, m_id});
            
            // Transform to corpse
            m_registry.remove<AgentComponent>(entity);
            m_registry.emplace<ObstacleComponent>(entity);
            if (auto* render = m_registry.try_get<RenderableComponent>(entity)) {
                render->glyph = '%';
                render->color = "#550000";
            }
            m_registry.remove<Layer1BiologyComponent>(entity);
            
            if (m_registry.all_of<PlayerComponent>(entity)) {
                m_dispatcher.trigger(HUDNotificationEvent{"YOU HAVE DIED.", 10.0f, "#FF0000"});
            }
        }
    }

    SimulationLayer simulation_layer() const override {
        return SimulationLayer::L1_Biology;
    }

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    uint64_t m_turn_counter = 0;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_BIOLOGY_SYSTEM_H
