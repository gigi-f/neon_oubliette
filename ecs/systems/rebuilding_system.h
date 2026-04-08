#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_REBUILDING_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_REBUILDING_SYSTEM_H

#include <entt/entt.hpp>
#include "../system_scheduler.h"
#include "../event_declarations.h"
#include <random>

namespace NeonOubliette {

class CityGenerationSystem;

/**
 * @brief [K.3] Handles the reconstruction of buildings on vacant lots.
 */
class RebuildingSystem : public ISystem {
public:
    RebuildingSystem(entt::registry& registry, entt::dispatcher& dispatcher, CityGenerationSystem& gen_system);

    void initialize() override {}
    void update(double delta_time) override { (void)delta_time; }

    void on_rebuild_event(const RebuildEvent& event);

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    CityGenerationSystem& m_gen_system;
    std::mt19937 m_gen;
};

} // namespace NeonOubliette

#endif
