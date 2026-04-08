#pragma once
#include <entt/entt.hpp>
#include "../event_declarations.h"
#include "../simulation_coordinator.h"

namespace NeonOubliette {

/**
 * @brief [O.2] Manages building-scale production cycles.
 *        Consumes raw materials and produces finished items based on agent labor.
 */
class ProductionSystem : public ISimulationSystem {
public:
    ProductionSystem(entt::registry& registry, entt::dispatcher& dispatcher);
    void initialize() override {}
    void update(double delta_time) override;

    SimulationLayer simulation_layer() const override { return SimulationLayer::L3_Economic; }

    void handleTurnEvent(const TurnEvent& event);

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette
