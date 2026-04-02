#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_FACTION_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_FACTION_SYSTEM_H

#include <entt/entt.hpp>

#include "../components/components.h"
#include "../event_declarations.h"
#include "../system_scheduler.h"

namespace NeonOubliette {

class FactionSystem : public ISystem {
public:
    FactionSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override {}
    void update(double delta_time) override {}

    void handleChangeFactionStanding(const ChangeFactionStandingEvent& event);

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_FACTION_SYSTEM_H
