#include "pathfinding_system.h"
#include "macro_navigation_system.h"
#include "../components/infrastructure_components.h"
#include "../components/simulation_layers.h"
#include <algorithm>
#include <map>
#include <queue>

namespace NeonOubliette {

// Constructor
PathfindingSystem::PathfindingSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry(registry), dispatcher(dispatcher) {
    dispatcher.sink<PathfindingRequestEvent>().connect<&PathfindingSystem::handlePathfindingRequestEvent>(this);
    dispatcher.sink<ChunkChangedEvent>().connect<&PathfindingSystem::handleChunkChangedEvent>(this);
}

void PathfindingSystem::update(double delta_time) {
    (void)delta_time;

    size_t processed = 0;
    while (processed < kMaxPathRequestsPerTurn && !pending_requests_.empty()) {
        auto request = pending_requests_.front();
        pending_requests_.pop_front();
        if (registry.valid(request.entity)) {
            processPathfindingRequest(request);
        }
        processed++;
    }
}

void PathfindingSystem::handlePathfindingRequestEvent(const PathfindingRequestEvent& event) {
    pending_requests_.push_back(event);
}

void PathfindingSystem::handleChunkChangedEvent(const ChunkChangedEvent& event) {
    (void)event;
    invalidate_spatial_query_cache(registry);
}

// Helper to check if a position is traversable (no solid entities)
bool PathfindingSystem::isTraversable(PositionComponent pos, entt::entity requester_entity) {
    auto& spatial = get_spatial_query_cache(registry);

    // 1. Check World Bounds or Interior Nav Grid (Static Layout)
    if (pos.layer_id == 0) {
        auto config_view = registry.view<WorldConfigComponent>();
        if (config_view.empty()) return false;
        const auto& config = config_view.get<WorldConfigComponent>(config_view.front());

        if (pos.x < 0 || pos.x >= config.width || pos.y < 0 || pos.y >= config.height) {
            return false;
        }
    } else {
        // [A.8] Interior Nav Grid Check
        auto floor_view = registry.view<FloorComponent>();
        bool found_floor = false;
        for (auto floor_ent : floor_view) {
            const auto& floor = floor_view.get<FloorComponent>(floor_ent);
            if (floor.layer_id == pos.layer_id) {
                found_floor = true;
                if (!floor.nav_grid.is_passable(pos.x, pos.y)) return false;
                break;
            }
        }
        // [M.2] Dynamic Layer Traversal: If no FloorComponent, check for Terrain (e.g. Sewer -1, Rail 5)
        if (!found_floor) {
            if (spatial.terrain_at.find(pos) == spatial.terrain_at.end()) return false;
        }
    }

    // Let an obstacle entity path out of itself.
    if (registry.valid(requester_entity) && registry.all_of<PositionComponent, ObstacleComponent>(requester_entity)) {
        const auto& self_pos = registry.get<PositionComponent>(requester_entity);
        if (registry.all_of<SizeComponent>(requester_entity)) {
            const auto& self_size = registry.get<SizeComponent>(requester_entity);
            if (pos.layer_id == self_pos.layer_id &&
                pos.x >= self_pos.x && pos.x < self_pos.x + std::max(1, self_size.width) &&
                pos.y >= self_pos.y && pos.y < self_pos.y + std::max(1, self_size.height)) {
                return true;
            }
        } else if (self_pos == pos) {
            return true;
        }
    }

    // Broken windows are passable.
    if (spatial.passable_window_tiles.count(pos)) {
        return true;
    }

    auto obs_it = spatial.obstacle_at.find(pos);
    if (obs_it == spatial.obstacle_at.end()) {
        return true;
    }

    const entt::entity blocker = obs_it->second;
    if (!registry.valid(blocker) || blocker == requester_entity) {
        return true;
    }

    // [B.1] Exception: Building entrance positions are traversable for pathfinding.
    if (registry.all_of<BuildingComponent>(blocker) && spatial.entrance_tiles.count(pos)) {
        return true;
    }

    return false;
}

// Heuristic function (Manhattan distance for grid-based movement)
int PathfindingSystem::getHeuristic(PositionComponent a, PositionComponent b) const {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

// Helper to get movement cost at a position
int PathfindingSystem::getMovementCost(PositionComponent pos, entt::entity requester_entity) {
    (void)requester_entity;
    int cost = 10; // Default base cost

    auto& spatial = get_spatial_query_cache(registry);

    // Use ArterialGrid for O(1) arterial type lookup
    auto* grid = registry.ctx().find<ArterialGrid>();
    if (grid) {
        ArterialType art_type;
        if (grid->type_at(pos.x, pos.y, pos.layer_id, art_type)) {
            switch (art_type) {
                case ArterialType::SIDEWALK: return 5;
                case ArterialType::ROAD_ALLEY: return 8;
                case ArterialType::ROAD_PRIMARY:
                case ArterialType::ROAD_SECONDARY: return 15;
                case ArterialType::RAIL_ELEVATED: return 50;
                case ArterialType::WATERWAY_RIVER: return 200;
                case ArterialType::SEWER: return 12;
                case ArterialType::UNDERGROUND_TUNNEL: return 8;
                default: break;
            }
        }
    }

    auto terr_it = spatial.terrain_at.find(pos);
    if (terr_it != spatial.terrain_at.end()) {
        switch (terr_it->second) {
            case TerrainType::SIDEWALK: return 5;
            case TerrainType::STREET: return 15;
            case TerrainType::GRASS: return 12;
            case TerrainType::DIRT: return 10;
            case TerrainType::SEWER_FLOOR: return 8;
            case TerrainType::SEWER_WATER: return 15;
            default: break;
        }
    }

    return cost;
}

// Reconstruct path from end node
std::vector<PositionComponent> PathfindingSystem::reconstructPath(PathfindingSystem::Node* endNode) const {
    std::vector<PositionComponent> path;
    Node* current = endNode;
    while (current != nullptr) {
        path.push_back(current->pos);
        current = current->parent;
    }
    std::reverse(path.begin(), path.end());

    if (!path.empty()) {
        path.erase(path.begin()); // Remove the current position (start_pos) from the path
    }
    return path;
}

// --- Proximity helpers ---

PositionComponent PathfindingSystem::getPlayerPosition() const {
    auto player_view = registry.view<PlayerComponent, PositionComponent>();
    for (auto entity : player_view) {
        return player_view.get<PositionComponent>(entity);
    }
    return {0, 0, 0}; // Fallback – should never happen
}

bool PathfindingSystem::isNearPlayer(PositionComponent pos) const {
    PositionComponent player_pos = getPlayerPosition();
    if (pos.layer_id != player_pos.layer_id) return false;
    int dist = std::abs(pos.x - player_pos.x) + std::abs(pos.y - player_pos.y);
    return dist <= kFullPathfindingRadius;
}

// Cheap path for entities far from the player.
// Generates waypoints by stepping toward the goal in cardinal/diagonal
// directions, using the arterial graph when available and falling back to
// a straight-line walk.  No per-tile traversability checks.
std::vector<PositionComponent> PathfindingSystem::generateSimulatedPath(
        PositionComponent start, PositionComponent goal) const {
    std::vector<PositionComponent> path;

    // First, try to use the arterial graph for a high-level route.
    auto* graph_comp = registry.ctx().find<ArterialGraphComponent>();
    if (graph_comp) {
        auto find_node = [&](PositionComponent p) -> entt::entity {
            auto view = registry.view<InfrastructureNodeComponent, PositionComponent>();
            entt::entity nearest = entt::null;
            float min_d = 1e9f;
            for (auto e : view) {
                const auto& np = view.get<PositionComponent>(e);
                float d = static_cast<float>(std::abs(np.x - p.x) + std::abs(np.y - p.y));
                if (d < min_d) { min_d = d; nearest = e; }
            }
            return nearest;
        };

        entt::entity start_node = find_node(start);
        entt::entity goal_node  = find_node(goal);

        if (start_node != entt::null && goal_node != entt::null && start_node != goal_node) {
            // Lightweight BFS/greedy walk on the arterial graph (much cheaper than full A*)
            struct MNode {
                entt::entity entity; float g; float h; entt::entity p;
                float f() const { return g + h; }
                bool operator>(const MNode& o) const { return f() > o.f(); }
            };
            std::priority_queue<MNode, std::vector<MNode>, std::greater<MNode>> open;
            std::map<entt::entity, float> gs;
            std::map<entt::entity, entt::entity> ps;

            const auto& gp = registry.get<PositionComponent>(goal_node);
            open.push({start_node, 0, static_cast<float>(getHeuristic(registry.get<PositionComponent>(start_node), gp)), entt::null});
            gs[start_node] = 0;

            bool found = false;
            int iters = 0;
            while (!open.empty() && iters < 500) {
                ++iters;
                MNode curr = open.top(); open.pop();
                if (curr.entity == goal_node) { found = true; break; }
                if (graph_comp->adj_list.count(curr.entity)) {
                    for (auto const& edge : graph_comp->adj_list.at(curr.entity)) {
                        float ng = curr.g + edge.cost;
                        if (!gs.count(edge.target_node) || ng < gs[edge.target_node]) {
                            gs[edge.target_node] = ng;
                            ps[edge.target_node] = curr.entity;
                            open.push({edge.target_node, ng,
                                static_cast<float>(getHeuristic(registry.get<PositionComponent>(edge.target_node), gp)),
                                curr.entity});
                        }
                    }
                }
            }

            if (found) {
                // Reconstruct macro waypoints
                entt::entity c = goal_node;
                while (c != entt::null) {
                    path.push_back(registry.get<PositionComponent>(c));
                    c = ps.count(c) ? ps.at(c) : entt::null;
                }
                std::reverse(path.begin(), path.end());
                // Remove start position if present
                if (!path.empty() && path.front() == start) path.erase(path.begin());
                // Ensure goal is the final waypoint
                if (path.empty() || !(path.back() == goal)) path.push_back(goal);
                return path;
            }
        }
    }

    // Fallback: straight-line walk (skip intermediate points, just give the goal).
    // The agent action system already does simple direct movement when
    // CurrentPathComponent steps run out, so a sparse path is fine here.
    path.push_back(goal);
    return path;
}

void PathfindingSystem::processPathfindingRequest(const PathfindingRequestEvent& event) {
    std::vector<PositionComponent> path;
    std::vector<PositionComponent> macro_path;
    bool success = false;

    // --- Proximity gate: only run expensive A* for entities near the player ---
    bool entity_is_player = registry.all_of<PlayerComponent>(event.entity);
    if (!entity_is_player && !isNearPlayer(event.start)) {
        // Use cheap simulated path instead of full A*
        path = generateSimulatedPath(event.start, event.goal);
        dispatcher.enqueue<PathfindingResponseEvent>(
            {event.entity, path, {}, event.request_id, !path.empty()});
        return;
    }

    // 1. Check if we need Hierarchical Pathfinding
    int dist = getHeuristic(event.start, event.goal);
    if (dist > 60) {
        // Use Arterial Graph
        auto* graph_comp = registry.ctx().find<ArterialGraphComponent>();
        if (graph_comp) {
            // Find a way to get MacroNavigationSystem's find_macro_path logic
            // For now, we'll implement a local version of it or assume it's available.
            // Since we can't easily call other systems, we'll do the macro search here.
            
            auto find_node = [&](PositionComponent p) -> entt::entity {
                auto view = registry.view<InfrastructureNodeComponent, PositionComponent>();
                entt::entity nearest = entt::null;
                float min_d = 1e9f;
                for (auto e : view) {
                    const auto& np = view.get<PositionComponent>(e);
                    float d = std::abs(np.x - p.x) + std::abs(np.y - p.y);
                    if (d < min_d) { min_d = d; nearest = e; }
                }
                return nearest;
            };

            entt::entity start_node = find_node(event.start);
            entt::entity goal_node = find_node(event.goal);

            if (start_node != entt::null && goal_node != entt::null && start_node != goal_node) {
                // Perform A* on Arterial Graph
                struct MNode {
                    entt::entity entity;
                    float g; float h; entt::entity p;
                    float f() const { return g + h; }
                    bool operator>(const MNode& o) const { return f() > o.f(); }
                };
                std::priority_queue<MNode, std::vector<MNode>, std::greater<MNode>> open;
                std::map<entt::entity, float> gs;
                std::map<entt::entity, entt::entity> ps;

                const auto& gp = registry.get<PositionComponent>(goal_node);
                open.push({start_node, 0, (float)getHeuristic(registry.get<PositionComponent>(start_node), gp), entt::null});
                gs[start_node] = 0;

                bool found_macro = false;
                int macro_iters = 0;
                while (!open.empty() && macro_iters < 2000) {
                    ++macro_iters;
                    MNode curr = open.top(); open.pop();
                    if (curr.entity == goal_node) { found_macro = true; break; }
                    if (graph_comp->adj_list.count(curr.entity)) {
                        for (auto const& edge : graph_comp->adj_list.at(curr.entity)) {
                            float ng = curr.g + edge.cost;
                            if (!gs.count(edge.target_node) || ng < gs[edge.target_node]) {
                                gs[edge.target_node] = ng; ps[edge.target_node] = curr.entity;
                                open.push({edge.target_node, ng, (float)getHeuristic(registry.get<PositionComponent>(edge.target_node), gp), curr.entity});
                            }
                        }
                    }
                }

                if (found_macro) {
                    entt::entity c = goal_node;
                    while (c != entt::null) {
                        macro_path.push_back(registry.get<PositionComponent>(c));
                        c = ps.count(c) ? ps.at(c) : entt::null;
                    }
                    std::reverse(macro_path.begin(), macro_path.end());
                    
                    // The first goal for local A* is the first node in macro_path
                    PositionComponent local_goal = macro_path.front();
                    
                    // Run local A* to the first macro node
                    // (Implementation below)
                    // We'll set event.goal to local_goal for the rest of this function
                    // and then restore it or just use a local goal.
                }
            }
        }
    }

    // 2. Standard Local A* (either to final goal or to first macro node)
    PositionComponent target = (macro_path.empty()) ? event.goal : macro_path.front();
    if (!macro_path.empty()) {
        macro_path.erase(macro_path.begin()); // Remove the one we are pathfinding to now
    }

    // Handle case where start and target are the same
    if (event.start == target) {
        dispatcher.enqueue<PathfindingResponseEvent>({event.entity, {}, macro_path, event.request_id, true});
        return;
    }

    static constexpr int kMaxAStarIterations = 800;

    auto cmp = [](const Node* a, const Node* b) { return *a > *b; };
    std::priority_queue<Node*, std::vector<Node*>, decltype(cmp)> open_set(cmp);
    std::vector<Node*> all_nodes;
    std::map<PositionComponent, Node*> nodes_in_open_or_closed;

    Node* start_node = new Node(event.start, 0, getHeuristic(event.start, target));
    open_set.push(start_node);
    all_nodes.push_back(start_node);
    nodes_in_open_or_closed[event.start] = start_node;

    int iterations = 0;
    while (!open_set.empty() && iterations < kMaxAStarIterations) {
        ++iterations;
        Node* current_node = open_set.top();
        open_set.pop();

        if (current_node->pos == target) {
            path = reconstructPath(current_node);
            success = true;
            break;
        }

        int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
        int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};

        for (int i = 0; i < 8; ++i) {
            PositionComponent neighbor_pos = current_node->pos;
            neighbor_pos.x += dx[i];
            neighbor_pos.y += dy[i];

            if (isTraversable(neighbor_pos, event.entity)) {
                int move_cost = getMovementCost(neighbor_pos, event.entity);
                int new_g_cost = current_node->g_cost + move_cost;
                auto it = nodes_in_open_or_closed.find(neighbor_pos);
                if (it == nodes_in_open_or_closed.end() || new_g_cost < it->second->g_cost) {
                    Node* neighbor_node = new Node(neighbor_pos, new_g_cost, getHeuristic(neighbor_pos, target), current_node);
                    open_set.push(neighbor_node);
                    all_nodes.push_back(neighbor_node);
                    nodes_in_open_or_closed[neighbor_pos] = neighbor_node;
                }
            }
        }

        // [A.8] Layer Transitions (Stairs)
        auto stair_view = registry.view<PositionComponent, StairsComponent>();
        for (auto stair_ent : stair_view) {
            const auto& s_pos = stair_view.get<PositionComponent>(stair_ent);
            if (s_pos.x == current_node->pos.x && s_pos.y == current_node->pos.y && s_pos.layer_id == current_node->pos.layer_id) {
                const auto& stairs = stair_view.get<StairsComponent>(stair_ent);
                PositionComponent next_layer_pos = s_pos;
                next_layer_pos.layer_id = stairs.connects_to_layer;
                
                if (isTraversable(next_layer_pos, event.entity)) {
                    int move_cost = 15; // Increased cost for stairs
                    int new_g_cost = current_node->g_cost + move_cost;
                    auto it = nodes_in_open_or_closed.find(next_layer_pos);
                    if (it == nodes_in_open_or_closed.end() || new_g_cost < it->second->g_cost) {
                        Node* neighbor_node = new Node(next_layer_pos, new_g_cost, getHeuristic(next_layer_pos, target), current_node);
                        open_set.push(neighbor_node);
                        all_nodes.push_back(neighbor_node);
                        nodes_in_open_or_closed[next_layer_pos] = neighbor_node;
                    }
                }
            }
        }

        // [M.2] Layer Transitions (Portals / Manholes / Ladders)
        auto portal_view = registry.view<PositionComponent, PortalComponent>();
        for (auto portal_ent : portal_view) {
            const auto& p_pos = portal_view.get<PositionComponent>(portal_ent);
            if (p_pos.x == current_node->pos.x && p_pos.y == current_node->pos.y && p_pos.layer_id == current_node->pos.layer_id) {
                const auto& portal = portal_view.get<PortalComponent>(portal_ent);
                PositionComponent next_layer_pos(portal.target_x, portal.target_y, portal.target_layer);
                
                if (isTraversable(next_layer_pos, event.entity)) {
                    int move_cost = 20; // Higher cost for a sewer ladder
                    int new_g_cost = current_node->g_cost + move_cost;
                    auto it = nodes_in_open_or_closed.find(next_layer_pos);
                    if (it == nodes_in_open_or_closed.end() || new_g_cost < it->second->g_cost) {
                        Node* neighbor_node = new Node(next_layer_pos, new_g_cost, getHeuristic(next_layer_pos, target), current_node);
                        open_set.push(neighbor_node);
                        all_nodes.push_back(neighbor_node);
                        nodes_in_open_or_closed[next_layer_pos] = neighbor_node;
                    }
                }
            }
        }
    }

    for (Node* node : all_nodes) delete node;
    dispatcher.enqueue<PathfindingResponseEvent>({event.entity, path, macro_path, event.request_id, success});
}

} // namespace NeonOubliette
