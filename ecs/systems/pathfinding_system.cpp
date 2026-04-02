#include "pathfinding_system.h"

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
    // Check if position is within conceptual map bounds (e.g., 0-99 for x/y)
    if (pos.x < 0 || pos.x >= 100 || pos.y < 0 || pos.y >= 100 || pos.layer_id != 0) {
        // For now, only layer 0 is considered traversable for pathfinding.
        // Will expand to multi-layer pathfinding later.
        return false;
    }

    // Iterate through all entities that have a PositionComponent and an ObstacleComponent
    // If any such entity (other than the requester itself) is at 'pos',
    // then the position is not traversable.
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
    // For now, only considers X and Y. Layer changes are assumed to be a separate movement.
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

    // The first step in the path is the starting position, which the agent is already on.
    // We want the path to represent the sequence of *moves* needed to reach the target.
    // So, we remove the starting position from the reconstructed path.
    if (!path.empty()) {
        path.erase(path.begin()); // Remove the current position (start_pos) from the path
    }
    return path;
}

void PathfindingSystem::handlePathfindingRequestEvent(const PathfindingRequestEvent& event) {
    std::vector<PositionComponent> path;
    bool success = false;

    // Check if start or target positions are valid and traversable
    if (!isTraversable(event.start, event.entity)) {
        // Start position is an obstacle
        dispatcher.enqueue<PathfindingResponseEvent>({event.entity, path, event.request_id, false});
        return;
    }
    if (!isTraversable(event.goal, entt::null)) { // Target can be an entity, so don't block by itself
        // Target position is an obstacle that cannot be moved onto.
        dispatcher.enqueue<PathfindingResponseEvent>({event.entity, path, event.request_id, false});
        return;
    }

    // Handle case where start and target are the same
    if (event.start == event.goal) {
        // Path is already at the target, so an empty path means success.
        dispatcher.enqueue<PathfindingResponseEvent>({event.entity, path, event.request_id, true});
        return;
    }

    // Use a priority queue for A* algorithm
    auto cmp = [](const Node* a, const Node* b) { return *a > *b; };
    std::priority_queue<Node*, std::vector<Node*>, decltype(cmp)> open_set(cmp);

    // Store all dynamically allocated nodes to clean up later
    std::vector<Node*> all_nodes;

    // Map to store the node with the best g_cost found so far for each position
    std::map<PositionComponent, Node*> nodes_in_open_or_closed;

    PathfindingSystem::Node* start_node =
        new PathfindingSystem::Node(event.start, 0, getHeuristic(event.start, event.goal));
    open_set.push(start_node);
    all_nodes.push_back(start_node);
    nodes_in_open_or_closed[event.start] = start_node;

    while (!open_set.empty()) {
        Node* current_node = open_set.top();
        open_set.pop();

        // If we reached the target, reconstruct path and exit
        if (current_node->pos == event.goal) {
            path = reconstructPath(current_node);
            success = true;
            break;
        }

        // Define possible movements (8 directions for x,y + same layer_id for now)
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
                    PathfindingSystem::Node* neighbor_node = new PathfindingSystem::Node(
                        neighbor_pos, new_g_cost, getHeuristic(neighbor_pos, event.goal), current_node);
                    open_set.push(neighbor_node);
                    all_nodes.push_back(neighbor_node);
                    nodes_in_open_or_closed[neighbor_pos] = neighbor_node;
                }
            }
        }
    }

    // Clean up dynamically allocated nodes
    for (PathfindingSystem::Node* node : all_nodes) {
        delete node;
    }

    dispatcher.enqueue<PathfindingResponseEvent>({event.entity, path, event.request_id, success});
}

} // namespace NeonOubliette
