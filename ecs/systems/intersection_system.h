#ifndef NEON_OUBLIETTE_INTERSECTION_SYSTEM_H
#define NEON_OUBLIETTE_INTERSECTION_SYSTEM_H

#include <entt/entt.hpp>
#include "../system_scheduler.h"

namespace NeonOubliette::Systems {

/**
 * @brief Manages traffic light cycles and intersection logic (FIFO).
 */
class IntersectionSystem : public ISystem {
public:
    IntersectionSystem(entt::registry& registry, entt::dispatcher& event_dispatcher);

    void initialize() override {}
    void update(double delta_time) override;

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;

    void update_traffic_lights(double delta_time);
    void handle_unprotected_intersections();
};

} // namespace NeonOubliette::Systems

#endif // NEON_OUBLIETTE_INTERSECTION_SYSTEM_H
