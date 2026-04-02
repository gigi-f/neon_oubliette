#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_PATHFINDING_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_PATHFINDING_SYSTEM_H

#include <algorithm>
#include <entt/entt.hpp>
#include <map>
#include <queue>

#include "../components/components.h"
#include "../event_declarations.h"
#include "../system_scheduler.h"

namespace NeonOubliette {

class PathfindingSystem : public ISystem {
public:
    struct Node {
        PositionComponent pos;
        int g_cost;
        int h_cost;
        Node* parent;

        Node(PositionComponent p, int g, int h, Node* prnt = nullptr) : pos(p), g_cost(g), h_cost(h), parent(prnt) {
        }

        int f_cost() const {
            return g_cost + h_cost;
        }

        bool operator>(const Node& other) const {
            return f_cost() > other.f_cost();
        }
    };

    PathfindingSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override {
    }
    void update(double delta_time) override {
    }

    void handlePathfindingRequestEvent(const PathfindingRequestEvent& event);

private:
    entt::registry& registry;
    entt::dispatcher& dispatcher;

    bool isTraversable(PositionComponent pos, entt::entity requester_entity) const;
    int getHeuristic(PositionComponent a, PositionComponent b) const;
    std::vector<PositionComponent> reconstructPath(Node* endNode) const;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_PATHFINDING_SYSTEM_H