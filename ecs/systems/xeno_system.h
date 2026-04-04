#pragma once
#include <entt/entt.hpp>
#include "../components/components.h"

#include "../simulation_coordinator.h"

namespace NeonOubliette {

class XenoSystem : public ISimulationSystem {
public:
    XenoSystem(entt::registry& registry, entt::dispatcher& dispatcher);
    void initialize() override {}
    void update(double delta_time) override;
    SimulationLayer simulation_layer() const override { return SimulationLayer::L2_Cognitive; }

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    
    void process_hierodule_auras();
    void process_cacogen_interferences();
};

} // namespace NeonOubliette
