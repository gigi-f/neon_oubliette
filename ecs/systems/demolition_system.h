#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_DEMOLITION_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_DEMOLITION_SYSTEM_H

#include <entt/entt.hpp>
#include "../system_scheduler.h"
#include "../event_declarations.h"

namespace NeonOubliette {

/**
 * @brief [K.3] Handles the removal of building entities and resetting of their lot footprints.
 */
class DemolitionSystem : public ISystem {
public:
    DemolitionSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override {}
    void update(double delta_time) override { (void)delta_time; }

    void on_demolition_event(const DemolitionEvent& event);

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif
