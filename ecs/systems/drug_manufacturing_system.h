#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_DRUG_MANUFACTURING_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_DRUG_MANUFACTURING_SYSTEM_H

#include <entt/entt.hpp>
#include "../components/components.h"
#include "../event_declarations.h"
#include "../system_scheduler.h"
#include "../simulation_coordinator.h"

namespace NeonOubliette {

/**
 * @brief [I.4] Handles the simulation of clandestine labs producing contraband.
 *        Labs consume raw chemical inputs and produce drugs on a tick cycle.
 */
class DrugManufacturingSystem : public ISimulationSystem {
public:
    DrugManufacturingSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override;
    void update(double delta_time) override;
    SimulationLayer simulation_layer() const override { return SimulationLayer::L3_Economic; }

private:
    void handleRawMaterialDelivery(const RawMaterialDeliveryEvent& event);
    void handleRaidEvent(const RaidEvent& event);
    void handleTurnEvent(const TurnEvent& event);

    void processLabTick();

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_DRUG_MANUFACTURING_SYSTEM_H
