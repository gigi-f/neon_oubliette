#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_CRAFTING_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_CRAFTING_SYSTEM_H

#include <entt/entt.hpp>

#include "../components/components.h"
#include "../event_declarations.h"
#include "../system_scheduler.h"

namespace NeonOubliette {

class CraftingSystem : public ISystem {
public:
    CraftingSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override {
    }
    void update(double delta_time) override {
    }

    void handleCraftItemEvent(const CraftItemEvent& event);

private:
    entt::registry& registry;
    entt::dispatcher& dispatcher;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_CRAFTING_SYSTEM_H