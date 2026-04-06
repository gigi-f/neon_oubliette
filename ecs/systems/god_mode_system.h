#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_GOD_MODE_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_GOD_MODE_SYSTEM_H

#include <entt/entt.hpp>
#include "../event_declarations.h"
#include "../system_scheduler.h"

namespace NeonOubliette::Systems {

class GodModeSystem : public ISystem {
public:
    GodModeSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override;
    void update(double delta_time) override;

    // Event Handlers
    void handleGodModeFollowAgentEvent(const GodModeFollowAgentEvent& event);
    void handleGodModeTeleportCursorEvent(const GodModeTeleportCursorEvent& event);
    void handleGodModeTagEntityEvent(const GodModeTagEntityEvent& event);
    void handleOpenContextMenuEvent(const OpenContextMenuEvent& event);
    void handleCloseContextMenuEvent(const CloseContextMenuEvent& event);
    void handleContextMenuSelectEvent(const ContextMenuSelectEvent& event);

private:
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;

    void execute_context_option(int index);
};

} // namespace NeonOubliette::Systems

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_GOD_MODE_SYSTEM_H
