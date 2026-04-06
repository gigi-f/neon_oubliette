#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_CITY_PLANNER_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_CITY_PLANNER_SYSTEM_H

#include <entt/entt.hpp>
#include "../system_scheduler.h"
#include "../components/zoning_components.h"
#include "../components/components.h"
#include <random>

namespace NeonOubliette {

/**
 * @brief [NEW SYSTEM] Subdivides the urban space into blocks and lots.
 *        Defines the legal urban fabric that other generation systems respect.
 */
class CityPlannerSystem : public ISystem {
public:
    CityPlannerSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override;
    void update(double delta_time) override;

    /**
     * @brief Performs the initial urban subdivision for the entire world.
     */
    void plan_city_layout();

private:
    void apply_commerce_hub_upzoning();
    void subdivide_zone_into_blocks(entt::entity zone_entity);
    void subdivide_block_into_lots(entt::entity block_entity, ZoneType zone_type);

    /**
     * @brief Creates a block entity and registers its bounds.
     */
    entt::entity create_block(int x, int y, int w, int h, entt::entity zone_entity);

    /**
     * @brief Creates a lot entity within a block and determines its facing.
     */
    entt::entity create_lot(int x, int y, int w, int h, ZoneType zone_class, entt::entity block_entity, StreetFacingSide alley_facing = StreetFacingSide::NONE);

    /**
     * @brief Infers street-facing sides based on block boundary proximity.
     */
    StreetFacingSide determine_facing(int x, int y, int w, int h, const BlockComponent& block);

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    std::mt19937 m_gen;
};

} // namespace NeonOubliette

#endif
