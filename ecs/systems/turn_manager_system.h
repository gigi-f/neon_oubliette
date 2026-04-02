#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_TURN_MANAGER_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_TURN_MANAGER_SYSTEM_H

#include <entt/entt.hpp>

#include "../event_declarations.h"
#include "../system_scheduler.h"

namespace NeonOubliette {

class TurnManagerSystem : public ISystem {
public:
    TurnManagerSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override {
    }
    void update(double delta_time) override {
    }

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    uint64_t m_currentTurnNumber = 0;

    void handleAdvanceTurnRequest(const AdvanceTurnRequestEvent& event);
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_TURN_MANAGER_SYSTEM_H
