#ifndef NEON_OUBLIETTE_RENDERING_SYSTEM_H
#define NEON_OUBLIETTE_RENDERING_SYSTEM_H

#include <notcurses/notcurses.h>
#include <entt/entt.hpp>
#include "../../ecs/event_declarations.h"
#include "system_scheduler.h"
#include <string>
#include <vector>
#include <unordered_map>

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

    struct ncplane* world_plane_;            // [Layer 0] Terrain/Base
    struct ncplane* range_ring_plane_;       // [Layer 1] Interaction Rings [E.2]
    struct ncplane* entity_plane_;           // [Layer 2] Agents, Items, and Dynamic Entities
    struct ncplane* hud_plane_;              // UI Layer
    struct ncplane* inventory_plane_;        // Modal Layer
    struct ncplane* barter_plane_;           // Modal Layer [Phase T.1]
    struct ncplane* interior_overlay_plane_; // Modal Layer
    struct ncplane* minimap_plane_;          // HUD/Overlay Layer
    struct ncplane* speech_plane_;           // High-Z Floating Text [F.5]
    struct ncplane* dialogue_log_plane_;    // Dialogue History Layer [F.6]
    struct ncplane* crisis_dashboard_plane_; // God Mode Crisis Dashboard [L.6]
    struct ncplane* cursor_plane_;           // Top Layer
    struct ncplane* context_menu_plane_;     // Top Layer
    struct ncplane* debug_overlay_plane_;    // Debug Layer (topmost)

    bool inventory_visible_ = false;

    struct ActivePulse {
        int x, y, layer;
        int radius;
        int current_tick;
        int max_ticks;
        uint32_t color;
    };
    std::vector<ActivePulse> active_pulses_;

    void handleInventoryToggleEvent(const InventoryToggleEvent& event);
    void handleHUDNotificationEvent(const HUDNotificationEvent& event);
    void handleToggleCrisisDashboardEvent(const ToggleCrisisDashboardEvent& event);
    void handleToggleControlsHelpEvent(const ToggleControlsHelpEvent& event);
    void handleBroadcastPulseEvent(const BroadcastPulseEvent& event);
    uint32_t parse_hex_color(const std::string& hex);
    std::string room_tag_to_string(NeonOubliette::RoomTag tag);

    std::unordered_map<std::string, uint32_t> color_cache_;
};

} // namespace NeonOubliette::Systems

#endif // NEON_OUBLIETTE_RENDERING_SYSTEM_H
