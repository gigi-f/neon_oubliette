#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_POPULATION_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_POPULATION_SYSTEM_H

#include <entt/entt.hpp>
#include "../components/components.h"
#include "../components/simulation_layers.h"
#include "../event_declarations.h"
#include "../system_scheduler.h"

namespace NeonOubliette {

/**
 * @brief Manages the migration and persistent presence of citizens across simulation scales.
 */
class PopulationSystem : public ISystem {
public:
    PopulationSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {}

    void update(double delta_time) override {
        // Migration Logic: Macro to Micro
        // When a building is active (generated), move assigned citizens into it as NPCs
        auto active_buildings = m_registry.view<InteriorGeneratedComponent, BuildingComponent>();
        
        for (auto building : active_buildings) {
            auto const& interior = active_buildings.get<InteriorGeneratedComponent>(building);
            if (!interior.is_generated || interior.floor_entities.empty()) continue;

            // Find citizens whose home or workplace is this building
            auto citizens = m_registry.view<CitizenComponent>();
            for (auto citizen : citizens) {
                // If citizen already has a micro presence, skip
                if (m_registry.all_of<MicroPresenceComponent>(citizen)) {
                    if (m_registry.get<MicroPresenceComponent>(citizen).is_active) continue;
                }

                bool should_instantiate = false;
                int target_layer = -1;

                // Check Home
                if (m_registry.all_of<HomeComponent>(citizen)) {
                    if (m_registry.get<HomeComponent>(citizen).building_entity == building) {
                        should_instantiate = true;
                        target_layer = static_cast<int>(entt::to_integral(building)) * 100; // Floor 0
                    }
                }

                // Check Workplace
                if (!should_instantiate && m_registry.all_of<WorkplaceComponent>(citizen)) {
                    if (m_registry.get<WorkplaceComponent>(citizen).building_entity == building) {
                        should_instantiate = true;
                        target_layer = static_cast<int>(entt::to_integral(building)) * 100; // Work Floor
                    }
                }

                if (should_instantiate) {
                    instantiateMicroNPC(citizen, building, target_layer);
                }
            }
        }
    }

private:
    void instantiateMicroNPC(entt::entity macro_citizen, entt::entity building, int layer_id) {
        auto npc = m_registry.create();
        
        // Link NPC to its Macro counterpart
        m_registry.emplace<NPCComponent>(npc, 50, entt::to_integral(macro_citizen));
        m_registry.emplace<PositionComponent>(npc, 5, 5, layer_id);
        m_registry.emplace<RenderableComponent>(npc, 'n', "#00FF00", layer_id);
        
        // Cross-layer causality: Sync economic status
        if (m_registry.all_of<Layer3EconomicComponent>(macro_citizen)) {
            m_registry.emplace<Layer3EconomicComponent>(npc, m_registry.get<Layer3EconomicComponent>(macro_citizen));
        }

        // Mark macro entity as active in micro
        auto& presence = m_registry.get_or_emplace<MicroPresenceComponent>(macro_citizen);
        presence.is_active = true;
        presence.micro_entity = npc;

        m_dispatcher.enqueue<LogEvent>({"Citizen " + std::to_string(entt::to_integral(macro_citizen)) + " migrated to micro-sim.", LogSeverity::INFO, "Population"});
    }

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_POPULATION_SYSTEM_H
