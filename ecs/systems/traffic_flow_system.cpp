#include "traffic_flow_system.h" // Include our own header

#include <cmath> // For std::sqrt
#include <entt/entt.hpp>
#include <vector>
#include <algorithm>

#include "../command_buffer.h"         // For NeonOubliette::CommandBuffer
#include "../component_declarations.h" // For all component and event declarations
#include "../events.h"                 // For MacroEventBus, MicroEventBus, and event structs
#include "../components/infrastructure_components.h"

namespace NeonOubliette::Systems {

TrafficFlowSystem::TrafficFlowSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {
}

void TrafficFlowSystem::update(double delta_time) {
    float dt = static_cast<float>(delta_time);

    // --- Phase 1: Update Vehicle Positions and Detect Arrivals ---
    auto view = m_registry.view<VehicleComponent, PositionComponent>();
    auto light_view = m_registry.view<TrafficLightComponent, PositionComponent>();

    for (auto [entity, vehicle, position] : view.each()) {
        if (!m_registry.valid(vehicle.destination_building_id)) {
            m_registry.destroy(entity);
            continue;
        }

        // Check for traffic lights at current position
        bool stopped_at_light = false;
        for (auto [light_ent, light, light_pos] : light_view.each()) {
            if (std::abs(position.x - light_pos.x) < 0.5f && std::abs(position.y - light_pos.y) < 0.5f) {
                if (light.state == TrafficLightState::RED) {
                    stopped_at_light = true;
                    break;
                }
            }
        }

        if (stopped_at_light) {
            continue; // Wait for green
        }

        PositionComponent& dest_pos = m_registry.get<PositionComponent>(vehicle.destination_building_id);

        // Movement Logic: Prefer Lane waypoints if available
        float dx, dy;
        float move_speed = 5.0f;

        if (auto* lane = m_registry.try_get<LaneComponent>(entity); lane && !lane->waypoints.empty()) {
            const auto& target = lane->waypoints[0];
            dx = target.x - position.x;
            dy = target.y - position.y;
            
            float dist_to_waypoint = std::sqrt(dx * dx + dy * dy);
            if (dist_to_waypoint < 0.1f) {
                lane->waypoints.erase(lane->waypoints.begin());
                if (lane->waypoints.empty()) continue; // Wait for next tick or move to next
                
                const auto& next_target = lane->waypoints[0];
                dx = next_target.x - position.x;
                dy = next_target.y - position.y;
            }
        } else {
            dx = dest_pos.x - position.x;
            dy = dest_pos.y - position.y;
        }

        float distance = std::sqrt(dx * dx + dy * dy);

        if (distance > 0.1f) {
            position.x += (dx / distance) * move_speed * dt;
            position.y += (dy / distance) * move_speed * dt;
        } else {
            // Arrival logic (same as before)
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
