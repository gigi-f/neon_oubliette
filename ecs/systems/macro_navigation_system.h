#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_MACRO_NAVIGATION_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_MACRO_NAVIGATION_SYSTEM_H

#include <entt/entt.hpp>
#include <vector>
#include <map>
#include "../system_scheduler.h"
#include "../components/components.h"
#include "../components/infrastructure_components.h"

namespace NeonOubliette {

/**
 * @brief Manages the global Arterial Graph for hierarchical pathfinding.
 *        Constructs a high-level network of Infrastructure Nodes (Junctions) 
 *        connected by Arterial edges.
 */
class MacroNavigationSystem : public ISystem {
public:
    MacroNavigationSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override;
    void update(double delta_time) override;

    /**
     * @brief Finds a high-level path between two positions using the Arterial Graph.
     * @return A sequence of positions corresponding to Infrastructure Nodes.
     */
    std::vector<PositionComponent> find_macro_path(PositionComponent start, PositionComponent goal);

    /**
     * @brief Rebuilds the Arterial Graph from current Infrastructure entities.
     */
    void rebuild_graph();

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;

    entt::entity find_nearest_node(PositionComponent pos);
    float get_heuristic(PositionComponent a, PositionComponent b);
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_MACRO_NAVIGATION_SYSTEM_H
