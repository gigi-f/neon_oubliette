#ifndef NEON_OUBLIETTE_TRAFFIC_FLOW_SYSTEM_H
#define NEON_OUBLIETTE_TRAFFIC_FLOW_SYSTEM_H

#include <entt/entt.hpp>
#include <functional>
#include <vector>

#include "../command_buffer.h" // Include the global CommandBuffer definition
#include "../events.h"         // Include event definitions for RawMaterialDeliveryEvent, CongestionEvent
#include "../system_scheduler.h"

// Forward declarations of components (to be defined in entt_component_types.h / component_declarations.h)
// These are typically all in a single 'component_declarations.h' for convenience.
struct CityComponent;
struct RoadComponent;
struct VehicleComponent;
struct PositionComponent;
struct ResourceNodeComponent;
struct BuildingComponent; // To get CommandBuffer from macro_registry

/**
 * @brief The TrafficFlowSystem is responsible for simulating vehicle movement,
 *        handling congestion, and managing resource delivery at the macro-level.
 *
 * This system operates on the macro_registry during the Macro Simulation Phase.
 *
 * Architectural References:
 * - Macro-Micro Design: /home/gianfiorenzo/Documents/ag2_workspace/neon_oubliette/architecture/macro_micro_design.md
 * - System Scheduler: /home/gianfiorenzo/Documents/ag2_workspace/neon_oubliette/ecs/system_scheduler.md
 * - Component Definitions: /home/gianfiorenzo/Documents/ag2_workspace/neon_oubliette/ecs/entt_component_types.md
 */
namespace NeonOubliette::Systems {

class TrafficFlowSystem : public ISystem {
public:
    TrafficFlowSystem(entt::registry& registry, entt::dispatcher& event_dispatcher);

    void initialize() override {
    }
    void update(double delta_time) override;

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette::Systems

#endif // NEON_OUBLIETTE_TRAFFIC_FLOW_SYSTEM_H
