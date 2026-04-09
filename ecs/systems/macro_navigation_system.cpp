#include "macro_navigation_system.h"
#include <queue>
#include <set>
#include <cmath>
#include <algorithm>
#include "../components/infrastructure_components.h"

namespace NeonOubliette {

MacroNavigationSystem::MacroNavigationSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {}

void MacroNavigationSystem::initialize() {
    rebuild_graph();
}

void MacroNavigationSystem::update(double delta_time) {
    (void)delta_time;
}

void MacroNavigationSystem::rebuild_graph() {
    auto* graph_ptr = m_registry.ctx().find<ArterialGraphComponent>();
    if (!graph_ptr) {
        m_registry.ctx().emplace<ArterialGraphComponent>();
        graph_ptr = m_registry.ctx().find<ArterialGraphComponent>();
    }
    auto& graph_comp = *graph_ptr;
    graph_comp.adj_list.clear();
    
    std::map<PositionComponent, entt::entity> node_pos_map;
    auto node_view = m_registry.view<InfrastructureNodeComponent, PositionComponent>();
    for (auto entity : node_view) {
        const auto& pos = node_view.get<PositionComponent>(entity);
        node_pos_map[pos] = entity;
    }

    // Use ArterialGrid for spatial lookup instead of per-tile entity map
    auto* grid = m_registry.ctx().find<ArterialGrid>();

    int dx[] = {0, 0, 1, -1};
    int dy[] = {1, -1, 0, 0};

    for (auto const& [pos, node_entity] : node_pos_map) {
        // [M.2] Link Vertical Transitions (Stairs & Portals)
        if (m_registry.all_of<StairsComponent>(node_entity)) {
            const auto& stairs = m_registry.get<StairsComponent>(node_entity);
            PositionComponent target_pos = pos;
            target_pos.layer_id = stairs.connects_to_layer;
            if (node_pos_map.count(target_pos)) {
                graph_comp.adj_list[node_entity].push_back({node_pos_map.at(target_pos), 5.0f, ArterialType::SIDEWALK});
            }
        }
        if (m_registry.all_of<PortalComponent>(node_entity)) {
            const auto& portal = m_registry.get<PortalComponent>(node_entity);
            PositionComponent target_pos(portal.target_x, portal.target_y, portal.target_layer);
            if (node_pos_map.count(target_pos)) {
                graph_comp.adj_list[node_entity].push_back({node_pos_map.at(target_pos), 10.0f, ArterialType::SIDEWALK});
            }
        }

        // Horizontal links — walk tile-by-tile using ArterialGrid for connectivity
        if (!grid) continue;
        for (int i = 0; i < 4; ++i) {
            PositionComponent cur_pos = pos;
            cur_pos.x += dx[i];
            cur_pos.y += dy[i];
            float distance = 1.0f;
            
            std::set<PositionComponent> visited;
            visited.insert(pos);

            while (true) {
                if (node_pos_map.count(cur_pos)) {
                    // Found another node on the same layer!
                    entt::entity target_node = node_pos_map.at(cur_pos);
                    graph_comp.adj_list[node_entity].push_back({target_node, distance, ArterialType::ROAD_PRIMARY});
                    break;
                }

                ArterialType dummy;
                if (!grid->type_at(cur_pos.x, cur_pos.y, cur_pos.layer_id, dummy)) break;

                // Move forward along the arterial
                visited.insert(cur_pos);
                bool moved = false;
                for (int j = 0; j < 4; ++j) {
                    PositionComponent next_pos = cur_pos;
                    next_pos.x += dx[j];
                    next_pos.y += dy[j];
                    if (visited.count(next_pos)) continue;
                    bool has_arterial = grid->type_at(next_pos.x, next_pos.y, next_pos.layer_id, dummy);
                    if (has_arterial || node_pos_map.count(next_pos)) {
                        cur_pos = next_pos;
                        distance += 1.0f;
                        moved = true;
                        break;
                    }
                }
                if (!moved) break;
            }
        }
    }
}

entt::entity MacroNavigationSystem::find_nearest_node(PositionComponent pos) {
    auto view = m_registry.view<InfrastructureNodeComponent, PositionComponent>();
    entt::entity nearest = entt::null;
    float min_dist = 1e9f;

    for (auto entity : view) {
        const auto& n_pos = view.get<PositionComponent>(entity);
        float d = (float)(std::abs(n_pos.x - pos.x) + std::abs(n_pos.y - pos.y));
        if (n_pos.layer_id != pos.layer_id) {
            d += 1000.0f; // Massive penalty for being on a different layer
        }
        if (d < min_dist) {
            min_dist = d;
            nearest = entity;
        }
    }
    return nearest;
}

float MacroNavigationSystem::get_heuristic(PositionComponent a, PositionComponent b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

std::vector<PositionComponent> MacroNavigationSystem::find_macro_path(PositionComponent start, PositionComponent goal) {
    auto* graph_comp = m_registry.ctx().find<ArterialGraphComponent>();
    if (!graph_comp) return {};

    entt::entity start_node = find_nearest_node(start);
    entt::entity goal_node = find_nearest_node(goal);

    if (start_node == entt::null || goal_node == entt::null) return {};
    if (start_node == goal_node) return { m_registry.get<PositionComponent>(start_node) };

    // A* on the graph
    struct Node {
        entt::entity entity;
        float g_cost;
        float h_cost;
        entt::entity parent;
        float f_cost() const { return g_cost + h_cost; }
        bool operator>(const Node& other) const { return f_cost() > other.f_cost(); }
    };

    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open_set;
    std::map<entt::entity, float> g_costs;
    std::map<entt::entity, entt::entity> parents;

    const auto& goal_pos = m_registry.get<PositionComponent>(goal_node);

    open_set.push({start_node, 0, get_heuristic(m_registry.get<PositionComponent>(start_node), goal_pos), entt::null});
    g_costs[start_node] = 0;

    bool found = false;
    while (!open_set.empty()) {
        Node current = open_set.top();
        open_set.pop();

        if (current.entity == goal_node) {
            found = true;
            break;
        }

        if (graph_comp->adj_list.count(current.entity)) {
            for (auto const& edge : graph_comp->adj_list.at(current.entity)) {
                float cost_multiplier = 1.0f;
                if (m_registry.all_of<CommerceHubComponent>(edge.target_node)) {
                    cost_multiplier = 0.4f; // Preferred pathing through commerce hubs
                }
                float new_g = current.g_cost + (edge.cost * cost_multiplier);
                if (!g_costs.count(edge.target_node) || new_g < g_costs[edge.target_node]) {
                    g_costs[edge.target_node] = new_g;
                    parents[edge.target_node] = current.entity;
                    open_set.push({edge.target_node, new_g, get_heuristic(m_registry.get<PositionComponent>(edge.target_node), goal_pos), current.entity});
                }
            }
        }
    }

    std::vector<PositionComponent> path;
    if (found) {
        entt::entity curr = goal_node;
        while (curr != entt::null) {
            path.push_back(m_registry.get<PositionComponent>(curr));
            curr = parents.count(curr) ? parents.at(curr) : entt::null;
        }
        std::reverse(path.begin(), path.end());
    }

    return path;
}

} // namespace NeonOubliette
