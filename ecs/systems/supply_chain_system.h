#pragma once
#include <entt/entt.hpp>
#include "../event_declarations.h"
#include "../simulation_coordinator.h"

namespace NeonOubliette {

/**
 * @brief [O.3] Manages disruptions to the city's supply chains.
 *        Calculates logisitical bottlenecks, labor strikes, and industrial sabotage
 *        based on the city's infrastructure health, politics, and active crises.
 */
class SupplyChainSystem : public ISimulationSystem {
public:
    SupplyChainSystem(entt::registry& registry, entt::dispatcher& dispatcher);
    void initialize() override {}
    void update(double delta_time) override;

    SimulationLayer simulation_layer() const override { return SimulationLayer::L3_Economic; }

    void handleTurnEvent(const TurnEvent& event);
    void handleCrisisEffect(const CrisisEffectEvent& event);

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    
    // Macro metrics for the current turn
    float m_global_bottleneck_stress = 0.0f;
    float m_global_strike_stress = 0.0f;
    float m_global_sabotage_stress = 0.0f;
};

} // namespace NeonOubliette
