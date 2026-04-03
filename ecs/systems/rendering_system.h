#ifndef NEON_OUBLIETTE_RENDERING_SYSTEM_H
#define NEON_OUBLIETTE_RENDERING_SYSTEM_H

#include <notcurses/notcurses.h>
#include <entt/entt.hpp>
#include "../../ecs/event_declarations.h"
#include "system_scheduler.h"
#include <string>
#include <vector>

namespace NeonOubliette::Systems {

class RenderingSystem : public ISystem {
public:
    RenderingSystem(entt::registry& registry, struct notcurses* nc_context, entt::dispatcher& event_dispatcher);
    ~RenderingSystem();

    void initialize() override;
    void update(double delta_time) override;

    void handle_input(const struct ncinput& input);

private:
    entt::registry& registry_;
    struct notcurses* nc_context_;
    entt::dispatcher& event_dispatcher_;

    struct ncplane* world_plane_;
    struct ncplane* hud_plane_;
    struct ncplane* inventory_plane_;

    bool inventory_visible_ = false;

    void handleInventoryToggleEvent(const InventoryToggleEvent& event);
    void handleHUDNotificationEvent(const HUDNotificationEvent& event);
    void handleToggleControlsHelpEvent(const ToggleControlsHelpEvent& event);
    uint32_t parse_hex_color(const std::string& hex);
};

} // namespace NeonOubliette::Systems

#endif // NEON_OUBLIETTE_RENDERING_SYSTEM_H
