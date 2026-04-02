#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_BARTER_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_BARTER_SYSTEM_H

#include <entt/entt.hpp>

#include "../components/components.h"
#include "../event_declarations.h"
#include "../system_scheduler.h"

namespace NeonOubliette {

class BarterSystem : public ISystem {
public:
    BarterSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override {}
    void update(double delta_time) override {}

    void handleBarterEvent(const BarterEvent& event);

private:
    int calculateTotalValue(const std::vector<entt::entity>& items);
    
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_BARTER_SYSTEM_H
