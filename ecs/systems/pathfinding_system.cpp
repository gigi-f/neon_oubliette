#include "pathfinding_system.h"
#include "macro_navigation_system.h"
#include "../components/infrastructure_components.h"
#include <algorithm>
#include <map>
#include <queue>

namespace NeonOubliette {

// Constructor
PathfindingSystem::PathfindingSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry(registry), dispatcher(dispatcher) {
    dispatcher.sink<PathfindingRequestEvent>().connect<&PathfindingSystem::handlePathfindingRequestEvent>(this);
}

// Helper to check if a position is traversable (no solid entities)
bool PathfindingSystem::isTraversable(PositionComponent pos, entt::entity requester_entity) const {
    auto config_view = registry.view<WorldConfigComponent>();
    if (config_view.empty()) return false;
    const auto& config = config_view.get<WorldConfigComponent>(config_view.front());

    if (pos.x < 0 || pos.x >= config.width || pos.y < 0 || pos.y >= config.height || pos.layer_id != 0) {
        return false;
    }

    bool is_obstacle = false;
    auto view = registry.view<PositionComponent, ObstacleComponent>();
    for (auto entity : view) {
        if (entity != requester_entity) {
            const auto& p_pos = view.get<PositionComponent>(entity);
            if (p_pos == pos) {
                is_obstacle = true;
                break;
            }
        }
    }
    return !is_obstacle;
}

// Heuristic function (Manhattan distance for grid-based movement)
int PathfindingSystem::getHeuristic(PositionComponent a, PositionComponent b) const {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
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

void PathfindingSystem::handlePathfindingRequestEvent(const PathfindingRequestEvent& event) {
    std::vector<PositionComponent> path;
    std::vector<PositionComponent> macro_path;
    bool success = false;

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
                while (!open.empty()) {
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

    auto cmp = [](const Node* a, const Node* b) { return *a > *b; };
    std::priority_queue<Node*, std::vector<Node*>, decltype(cmp)> open_set(cmp);
    std::vector<Node*> all_nodes;
    std::map<PositionComponent, Node*> nodes_in_open_or_closed;

    Node* start_node = new Node(event.start, 0, getHeuristic(event.start, target));
    open_set.push(start_node);
    all_nodes.push_back(start_node);
    nodes_in_open_or_closed[event.start] = start_node;

    while (!open_set.empty()) {
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
                int new_g_cost = current_node->g_cost + 1;
                auto it = nodes_in_open_or_closed.find(neighbor_pos);
                if (it == nodes_in_open_or_closed.end() || new_g_cost < it->second->g_cost) {
                    Node* neighbor_node = new Node(neighbor_pos, new_g_cost, getHeuristic(neighbor_pos, target), current_node);
                    open_set.push(neighbor_node);
                    all_nodes.push_back(neighbor_node);
                    nodes_in_open_or_closed[neighbor_pos] = neighbor_node;
                }
            }
        }
    }

    for (Node* node : all_nodes) delete node;
    dispatcher.enqueue<PathfindingResponseEvent>({event.entity, path, macro_path, event.request_id, success});
}

} // namespace NeonOubliette
