#ifndef NEON_OUBLIETTE_INTERACTION_SYSTEM_H
#define NEON_OUBLIETTE_INTERACTION_SYSTEM_H

#include <notcurses/notcurses.h>

#include <entt/entt.hpp>

#include "../components/components.h"
#include "../event_declarations.h"
#include "../system_scheduler.h"

namespace NeonOubliette {
namespace Systems {

class InteractionSystem : public ISystem {
public:
    InteractionSystem(entt::registry& registry, struct notcurses* nc_context, entt::dispatcher& event_dispatcher);
    ~InteractionSystem() = default;

    void initialize() override;
    void update(double delta_time) override;

private:
    entt::registry& registry_;
    struct notcurses* nc_context_;
    entt::dispatcher& event_dispatcher_;

    void handlePickupItemEvent(const PickupItemEvent& event);
};

} // namespace Systems
} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_INTERACTION_SYSTEM_H
