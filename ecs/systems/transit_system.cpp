#include "transit_system.h"
#include "../components/infrastructure_components.h"
#include "../events.h"
#include <random>
#include <algorithm>

namespace NeonOubliette {

TransitSystem::TransitSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {}

void TransitSystem::initialize() {
    setup_transit_network();
}

void TransitSystem::update(double delta_time) {
    m_accumulated_time += delta_time;
    if (m_accumulated_time < 0.2) return; // Process at a fixed rate (L1-like)
    m_accumulated_time = 0;

    auto vehicle_view = m_registry.view<TransitVehicleComponent, PositionComponent, RenderableComponent>();
    for (auto vehicle : vehicle_view) {
        auto& transit = vehicle_view.get<TransitVehicleComponent>(vehicle);
        auto& pos = vehicle_view.get<PositionComponent>(vehicle);
        
        if (transit.is_stopped) {
            transit.stop_timer++;
            if (transit.stop_timer >= transit.stop_duration) {
                transit.is_stopped = false;
                transit.stop_timer = 0;
            }
        } else {
            update_vehicle_movement(vehicle, transit, pos, delta_time);
        }

        // --- Synchronize Occupant Positions ---
        if (m_registry.all_of<TransitOccupantsComponent>(vehicle)) {
            auto& occupants = m_registry.get<TransitOccupantsComponent>(vehicle).occupants;
            for (auto occupant : occupants) {
                if (m_registry.valid(occupant)) {
                    auto& occ_pos = m_registry.get<PositionComponent>(occupant);
                    occ_pos.x = pos.x;
                    occ_pos.y = pos.y;
                    occ_pos.layer_id = pos.layer_id;
                }
            }
        }
    }
}

void TransitSystem::setup_transit_network() {
    // 1. Mark some InfrastructureNodes as Stations
    auto node_view = m_registry.view<InfrastructureNodeComponent, PositionComponent>();
    std::vector<entt::entity> rail_nodes;
    std::vector<entt::entity> road_nodes;

    for (auto node_ent : node_view) {
        // Simple heuristic: If it's a station node or bus stop
        const auto& node = node_view.get<InfrastructureNodeComponent>(node_ent);
        if (node.node_name.find("Rail") != std::string::npos) {
            rail_nodes.push_back(node_ent);
        } else if (node.node_name.find("Bus") != std::string::npos || node.is_bridge) {
            road_nodes.push_back(node_ent);
        }
    }

    // Assign stations to rail line
    if (!rail_nodes.empty()) {
        auto route_ent = m_registry.create();
        auto& route = m_registry.emplace<TransitRouteComponent>(route_ent);
        route.route_name = "Blue Line Rail";
        route.vehicle_type = TransitVehicleType::TRAIN;
        route.stops = rail_nodes;
        route.is_circular = true;

        for (auto sn : rail_nodes) {
            m_registry.get_or_emplace<TransitStationComponent>(sn, TransitVehicleType::TRAIN).serving_routes.push_back(route_ent);
            m_registry.get<RenderableComponent>(sn).glyph = 'S'; // Mark Station
            m_registry.get<RenderableComponent>(sn).color = "#00FFFF";
        }

        // Spawn a train
        auto train = m_registry.create();
        m_registry.emplace<NameComponent>(train, "Blue Line Train");
        auto const& start_pos = m_registry.get<PositionComponent>(rail_nodes[0]);
        m_registry.emplace<PositionComponent>(train, start_pos.x, start_pos.y, start_pos.layer_id);
        m_registry.emplace<RenderableComponent>(train, 'T', "#FFFFFF", 2);
        auto& tv = m_registry.emplace<TransitVehicleComponent>(train);
        tv.type = TransitVehicleType::TRAIN;
        tv.current_route = route_ent;
        tv.next_stop_index = 1 % rail_nodes.size();
        m_registry.emplace<TransitOccupantsComponent>(train);
    }

    // Assign stations to bus route
    if (road_nodes.size() >= 2) {
        auto route_ent = m_registry.create();
        auto& route = m_registry.emplace<TransitRouteComponent>(route_ent);
        route.route_name = "City Express Bus";
        route.vehicle_type = TransitVehicleType::BUS;
        route.stops = road_nodes;
        route.is_circular = true;

        for (auto sn : road_nodes) {
            m_registry.get_or_emplace<TransitStationComponent>(sn, TransitVehicleType::BUS).serving_routes.push_back(route_ent);
            m_registry.get<RenderableComponent>(sn).glyph = 'B'; // Mark Bus Stop
            m_registry.get<RenderableComponent>(sn).color = "#FFFF00";
        }

        // Spawn a bus
        auto bus = m_registry.create();
        m_registry.emplace<NameComponent>(bus, "City Bus #1");
        auto const& start_pos = m_registry.get<PositionComponent>(road_nodes[0]);
        m_registry.emplace<PositionComponent>(bus, start_pos.x, start_pos.y, start_pos.layer_id);
        m_registry.emplace<RenderableComponent>(bus, 'V', "#FFAA00", 2);
        auto& tv = m_registry.emplace<TransitVehicleComponent>(bus);
        tv.type = TransitVehicleType::BUS;
        tv.current_route = route_ent;
        tv.next_stop_index = 1 % road_nodes.size();
        m_registry.emplace<TransitOccupantsComponent>(bus);
    }
}

void TransitSystem::update_vehicle_movement(entt::entity vehicle, TransitVehicleComponent& transit, PositionComponent& pos, double dt) {
    if (transit.current_route == entt::null) return;
    const auto& route = m_registry.get<TransitRouteComponent>(transit.current_route);
    if (transit.next_stop_index >= route.stops.size()) return;

    entt::entity target_station = route.stops[transit.next_stop_index];
    const auto& target_pos = m_registry.get<PositionComponent>(target_station);

    // Simple Grid-based movement: move towards target node tile by tile
    int dx = (target_pos.x > pos.x) ? 1 : (target_pos.x < pos.x ? -1 : 0);
    int dy = (target_pos.y > pos.y) ? 1 : (target_pos.y < pos.y ? -1 : 0);

    pos.x += dx;
    pos.y += dy;

    // Arrived at station?
    if (pos.x == target_pos.x && pos.y == target_pos.y) {
        handle_station_stop(vehicle, transit, target_station);
        transit.next_stop_index = (transit.next_stop_index + 1) % route.stops.size();
    }
}

void TransitSystem::handle_station_stop(entt::entity vehicle, TransitVehicleComponent& transit, entt::entity station) {
    transit.is_stopped = true;
    process_unboarding(vehicle, station);
    process_boarding(vehicle, station);
}

void TransitSystem::process_boarding(entt::entity vehicle, entt::entity station) {
    auto& occupants = m_registry.get<TransitOccupantsComponent>(vehicle).occupants;
    const auto& transit = m_registry.get<TransitVehicleComponent>(vehicle);
    const auto& station_pos = m_registry.get<PositionComponent>(station);

    // Find entities at the same tile waiting for transit
    auto waiting_view = m_registry.view<PositionComponent, AgentTaskComponent>();
    for (auto candidate : waiting_view) {
        if (occupants.size() >= (size_t)transit.capacity) break;

        const auto& c_pos = waiting_view.get<PositionComponent>(candidate);
        auto& task = waiting_view.get<AgentTaskComponent>(candidate);

        if (c_pos.x == station_pos.x && c_pos.y == station_pos.y && c_pos.layer_id == station_pos.layer_id) {
            // Check if player or agent waiting for transit
            bool is_player = m_registry.all_of<PlayerComponent>(candidate);
            bool is_waiting = (task.task_type == AgentTaskType::WAIT_FOR_TRANSIT);

            if (is_player || is_waiting) {
                // Board!
                occupants.push_back(candidate);
                m_registry.emplace_or_replace<RidingComponent>(candidate, vehicle);
                task.task_type = AgentTaskType::RIDE_TRANSIT;
                
                // If it's the player, we should hide them or attach them to vehicle movement
                // For now, RidingSystem (implied) will handle updating riding positions
            }
        }
    }
}

void TransitSystem::process_unboarding(entt::entity vehicle, entt::entity station) {
    auto& occupants_comp = m_registry.get<TransitOccupantsComponent>(vehicle);
    auto& occupants = occupants_comp.occupants;
    const auto& station_pos = m_registry.get<PositionComponent>(station);

    for (auto it = occupants.begin(); it != occupants.end(); ) {
        entt::entity occupant = *it;
        bool should_unboard = false;

        if (m_registry.all_of<PlayerComponent>(occupant)) {
            // Player unboards manually or if they reach destination
            // For now, player unboards if they press a key (handled in input)
            // Or maybe always unboard at every stop? No, manual is better.
        } else if (m_registry.all_of<GoalComponent>(occupant)) {
            const auto& goal = m_registry.get<GoalComponent>(occupant);
            // Check if station is near goal
            float dist = std::abs(goal.target_x - station_pos.x) + std::abs(goal.target_y - station_pos.y);
            if (dist < 10) { // arbitrary unboard threshold
                should_unboard = true;
            }
        }

        if (should_unboard) {
            m_registry.remove<RidingComponent>(occupant);
            auto& pos = m_registry.get<PositionComponent>(occupant);
            pos.x = station_pos.x;
            pos.y = station_pos.y;
            pos.layer_id = station_pos.layer_id;

            if (m_registry.all_of<AgentTaskComponent>(occupant)) {
                m_registry.get<AgentTaskComponent>(occupant).task_type = AgentTaskType::IDLE;
            }

            it = occupants.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace NeonOubliette
