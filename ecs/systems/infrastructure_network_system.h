#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_INFRASTRUCTURE_NETWORK_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_INFRASTRUCTURE_NETWORK_SYSTEM_H

#include <entt/entt.hpp>
#include "../system_scheduler.h"
#include "../components/infrastructure_components.h"
#include "../components/components.h"
#include <map>

namespace NeonOubliette {

/**
 * @brief Generates the global skeletal infrastructure (Rivers, Rails, Arterials)
 *        across the entire world grid and resolves junctions.
 */
class InfrastructureNetworkSystem : public ISystem {
public:
    InfrastructureNetworkSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override;
    void update(double delta_time) override;

    /**
     * @brief Generates the global infrastructure skeleton.
     * @param world_width Total world width in tiles
     * @param world_height Total world height in tiles
     */
    void generate_skeleton(int world_width, int world_height);

    /**
     * @brief Generates secondary street connectivity (capillaries) based on zoning.
     */
    void generate_capillaries();

    /**
     * @brief Finalizes the infrastructure graph by resolving junctions.
     */
    void resolve_junctions();

private:
    void carve_river(int width, int height);
    void carve_primary_roads(int width, int height);
    void carve_rail_line(int width, int height);
    void carve_electric_grid(int width, int height);
    void carve_sewers(int width, int height);
    void carve_secondary_roads();

    /// Create a single segment entity covering an axis-aligned line.
    entt::entity create_line_segment(int x1, int y1, int x2, int y2, ArterialType type, int layer_id = 0);

    /// Link a segment to all zones it crosses.
    void link_segment_to_zones(entt::entity seg_entity, int x1, int y1, int x2, int y2);

    /// Build the ArterialGrid spatial index from all segment entities (call after all generation).
    void build_arterial_grid();

    void subdivide_block_corporate(int start_x, int start_y, int end_x, int end_y);
    void subdivide_block_slum(int start_x, int start_y, int end_x, int end_y);
    void subdivide_block_industrial(int start_x, int start_y, int end_x, int end_y);
    void subdivide_block_residential(int start_x, int start_y, int end_x, int end_y);
    void subdivide_block_park(int start_x, int start_y, int end_x, int end_y);
    void subdivide_block_airport(int start_x, int start_y, int end_x, int end_y);

    /// Link a single point entity (node/junction) to its zone.
    void link_node_to_zone(entt::entity node, int x, int y);

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    std::map<std::pair<int, int>, entt::entity> m_zone_cache;
};

} // namespace NeonOubliette

#endif
