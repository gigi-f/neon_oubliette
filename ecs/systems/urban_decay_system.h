#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_URBAN_DECAY_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_URBAN_DECAY_SYSTEM_H

#include "simulation_coordinator.h"
#include "../components/components.h"
#include "../components/zoning_components.h"

namespace NeonOubliette {

/**
 * @brief [NEW CLASS] Simulates the entropy of the city. 
 *        Buildings deteriorate over time based on environmental factors 
 *        (weather, pollution) and are repaired based on economic factors.
 */
class UrbanDecaySystem : public ISimulationSystem {
public:
    UrbanDecaySystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {}
    void update(double delta_time) override;

    SimulationLayer simulation_layer() const override {
        // [K.1] Urban Decay is a Layer 4 (Macro/Political/Structural) simulation concern.
        return SimulationLayer::L4_Political;
    }

private:
    void apply_decay(entt::entity building, BuildingHealthComponent& health, const WeatherComponent& weather, float local_pollution);
    void apply_repairs(entt::entity building, BuildingHealthComponent& health, PropertyComponent& property);
    void trigger_environmental_effects(entt::entity building, BuildingHealthComponent& health);
    void process_graffiti(entt::entity building_ent, BuildingHealthComponent& health, const BuildingComponent& building_comp, const PositionComponent& b_pos, const SizeComponent& b_size);

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_URBAN_DECAY_SYSTEM_H
