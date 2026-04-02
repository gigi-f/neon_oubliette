#include "traffic_flow_system.h" // Include our own header

#include <cmath> // For std::sqrt
#include <entt/entt.hpp>
#include <vector>

#include "../command_buffer.h"         // For NeonOubliette::CommandBuffer
#include "../component_declarations.h" // For all component and event declarations
#include "../events.h"                 // For MacroEventBus, MicroEventBus, and event structs

namespace NeonOubliette::Systems {

TrafficFlowSystem::TrafficFlowSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {
}

void TrafficFlowSystem::update(double delta_time) {
    float dt = static_cast<float>(delta_time);

    // --- Phase 1: Update Vehicle Positions and Detect Arrivals ---
    auto view = m_registry.view<VehicleComponent, PositionComponent>();
    for (auto [entity, vehicle, position] : view.each()) {
        if (!m_registry.valid(vehicle.destination_building_id)) {
            m_registry.destroy(entity);
            continue;
        }

        PositionComponent& dest_pos = m_registry.get<PositionComponent>(vehicle.destination_building_id);

        float dx = dest_pos.x - position.x;
        float dy = dest_pos.y - position.y;
        float distance = std::sqrt(dx * dx + dy * dy);

        float move_speed = 5.0f;

        if (distance > 0.1f) {
            position.x += (dx / distance) * move_speed * dt;
            position.y += (dy / distance) * move_speed * dt;
        } else {
            if (m_registry.valid(vehicle.destination_building_id)) {
                BuildingComponent& building_comp =
                    m_registry.get<BuildingComponent>(vehicle.destination_building_id);

                building_comp.command_buffer.push([resource_transport = vehicle.resource_transport,
                                                   source_node_id = vehicle.source_node_id](entt::registry& micro_reg) {
                    micro_reg.ctx().get<MicroEventBus>().trigger(RawMaterialDeliveryEvent{
                        source_node_id, entt::null, resource_transport.type, resource_transport.amount
                    });
                });
            }
            m_registry.destroy(entity);
        }
    }

    // --- Phase 2: Handle Congestion ---
    auto road_view = m_registry.view<RoadComponent>();
    for (auto [road_entity, road] : road_view.each()) {
        if (road.traffic_density > 0.8f) {
            uint64_t tick = 0;
            if (m_registry.ctx().contains<CityComponent>()) {
                tick = m_registry.ctx().get<CityComponent>().time_tick;
            }
            m_dispatcher.trigger(CongestionEvent{road_entity, road.traffic_density, tick});
        }
    }
}

} // namespace NeonOubliette::Systems
