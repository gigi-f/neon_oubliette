#ifndef NEON_OUBLIETTE_AMBIENT_SPAWN_SYSTEM_H
#define NEON_OUBLIETTE_AMBIENT_SPAWN_SYSTEM_H

#include <entt/entt.hpp>
#include "../system_scheduler.h"
#include <random>

namespace NeonOubliette::Systems {

/**
 * @brief Spawns and despawns ambient "filler" entities (vehicles, pedestrians) 
 *        based on player proximity and local density.
 */
class AmbientSpawnSystem : public ISystem {
public:
    AmbientSpawnSystem(entt::registry& registry, entt::dispatcher& event_dispatcher);

    void initialize() override;
    void update(double delta_time) override;

private:
    void spawn_ambient_entities();
    void despawn_distant_entities();

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    std::mt19937 m_gen;
    double m_accumulated_time = 0;
};

} // namespace NeonOubliette::Systems

#endif
