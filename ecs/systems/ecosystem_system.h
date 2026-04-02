#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_ECOSYSTEM_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_ECOSYSTEM_SYSTEM_H

#include "simulation_coordinator.h"
#include "components/components.h"
#include <random>

namespace NeonOubliette {

/**
 * @brief Manages macro-level ecosystem factors like wildlife behavior and survival (L1).
 */
class EcosystemSystem : public ISimulationSystem {
public:
    EcosystemSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {}

    void update(double delta_time) override {
        // Wildlife (Creatures) survival and foraging
        auto creature_view = m_registry.view<CreatureComponent, PositionComponent, Layer1BiologyComponent>();
        auto forage_view = m_registry.view<ForageNodeComponent, PositionComponent>();
        auto waste_view = m_registry.view<WasteComponent, PositionComponent>();

        for (auto entity : creature_view) {
            auto& creature = creature_view.get<CreatureComponent>(entity);
            auto& pos = creature_view.get<PositionComponent>(entity);
            auto& bio = m_registry.get<Layer1BiologyComponent>(entity);

            // Health decay (biological metabolic rate)
            bio.metabolic_rate = 0.5f; // Standard metabolic rate
            creature.health -= 1; // Basic hunger decay
            
            // Foraging logic: look for food in the vicinity
            bool found_food = false;
            for (auto f_entity : forage_view) {
                auto& f_pos = forage_view.get<PositionComponent>(f_entity);
                auto& forage = forage_view.get<ForageNodeComponent>(f_entity);
                
                if (f_pos.x == pos.x && f_pos.y == pos.y && f_pos.layer_id == pos.layer_id) {
                    if (forage.current_amount > 0) {
                        forage.current_amount -= 1;
                        creature.health += 5;
                        found_food = true;
                        break;
                    }
                }
            }
            
            // Look for waste as food (for urban scavengers)
            if (!found_food) {
                for (auto w_entity : waste_view) {
                    auto& w_pos = waste_view.get<PositionComponent>(w_entity);
                    auto& waste = waste_view.get<WasteComponent>(w_entity);
                    
                    if (w_pos.x == pos.x && w_pos.y == pos.y && w_pos.layer_id == pos.layer_id) {
                        if (waste.waste_level > 5.0f) {
                            waste.waste_level -= 1.0f;
                            creature.health += 2;
                            found_food = true;
                            break;
                        }
                    }
                }
            }

            // Cap health and check for death
            if (creature.health > 100) creature.health = 100;
            if (creature.health <= 0) {
                m_dispatcher.enqueue<LogEvent>({"Wildlife death: " + creature.species, LogSeverity::INFO, "EcosystemSystem"});
                // Deferred destruction or event emission
            }
        }
    }

    SimulationLayer simulation_layer() const override {
        return SimulationLayer::L1_Biology;
    }

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_ECOSYSTEM_SYSTEM_H
