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
    void carve_secondary_roads();

    void create_arterial_segment(int x, int y, ArterialType type);

    void subdivide_block_corporate(int start_x, int start_y, int end_x, int end_y);
    void subdivide_block_slum(int start_x, int start_y, int end_x, int end_y);
    void subdivide_block_industrial(int start_x, int start_y, int end_x, int end_y);
    void subdivide_block_residential(int start_x, int start_y, int end_x, int end_y);
    void subdivide_block_park(int start_x, int start_y, int end_x, int end_y);
    void subdivide_block_airport(int start_x, int start_y, int end_x, int end_y);

    void link_arterial_to_zone(entt::entity arterial, int x, int y);

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    std::map<std::pair<int, int>, entt::entity> m_zone_cache;
};

} // namespace NeonOubliette

#endif
