#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_ECONOMIC_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_ECONOMIC_SYSTEM_H

#include <entt/entt.hpp>
#include "../simulation_coordinator.h"
#include "../components/components.h"
#include "../components/simulation_layers.h"
#include "../event_declarations.h"

namespace NeonOubliette {

class EconomicSystem : public ISimulationSystem {
public:
    EconomicSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override;
    void update(double delta_time) override;
    SimulationLayer simulation_layer() const override { return SimulationLayer::L3_Economic; }

    void handlePurchase(const PurchaseEvent& event);
    void handleResourceExtracted(const ResourceExtractedEvent& event);
    void handleCrisisEffect(const CrisisEffectEvent& event);
    void handleCrisisResolved(const CrisisResolvedEvent& event);

private:
    void calculateLocalMarketConditions();
    void processAgentWages();
    void updatePropertyValues();

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    
    float m_active_economic_crisis_severity = 0.0f;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_ECONOMIC_SYSTEM_H
