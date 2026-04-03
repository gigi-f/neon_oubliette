#include "rendering_system.h"
#include <iostream>
#include <string>
#include <fstream>
#include "../components/components.h"
#include "../event_declarations.h"

namespace NeonOubliette::Systems {

RenderingSystem::RenderingSystem(entt::registry& registry, struct notcurses* nc_context,
                                 entt::dispatcher& event_dispatcher)
    : registry_(registry), nc_context_(nc_context), event_dispatcher_(event_dispatcher), 
      world_plane_(nullptr), hud_plane_(nullptr), inventory_plane_(nullptr), inventory_visible_(false) {
}

RenderingSystem::~RenderingSystem() {
    if (hud_plane_) ncplane_destroy(hud_plane_);
    if (inventory_plane_) ncplane_destroy(inventory_plane_);
    if (world_plane_) ncplane_destroy(world_plane_);
}

void RenderingSystem::initialize() {
    if (nc_context_) {
        struct ncplane* stdp = notcurses_stdplane(nc_context_);
        unsigned term_y, term_x;
        ncplane_dim_yx(stdp, &term_y, &term_x);

        term_y = (term_y > 0) ? term_y : 24;
        term_x = (term_x > 0) ? term_x : 80;

        struct ncplane_options nopts = {};
        nopts.y = 0;
        nopts.x = 0;
        nopts.rows = term_y;
        nopts.cols = term_x;
        nopts.name = "World";
        world_plane_ = ncplane_create(stdp, &nopts);

        nopts.y = 0;
        nopts.x = 0;
        nopts.rows = 6;
        nopts.cols = term_x;
        nopts.name = "HUD";
        hud_plane_ = ncplane_create(stdp, &nopts);
        
        uint64_t channels = 0;
        ncchannels_set_fg_default(&channels);
        ncchannels_set_bg_default(&channels);
        if (hud_plane_) ncplane_set_base(hud_plane_, " ", 0, channels);

        nopts.name = "Inventory";
        nopts.y = 6;
        nopts.x = (term_x > 30) ? term_x - 30 : 0;
        nopts.rows = 10;
        nopts.cols = 30;
        inventory_plane_ = ncplane_create(stdp, &nopts);
        if (inventory_plane_) {
            uint64_t inv_channels = 0;
            ncchannels_set_bg_rgb(&inv_channels, 0x1A1A2E);
            ncchannels_set_fg_rgb(&inv_channels, 0xE0E0E0);
            ncplane_set_base(inventory_plane_, " ", 0, inv_channels);
            ncplane_set_scrolling(inventory_plane_, true);
            ncplane_move_below(inventory_plane_, world_plane_);
        }

        notcurses_cursor_disable(nc_context_);
    }

    event_dispatcher_.sink<InventoryToggleEvent>().connect<&RenderingSystem::handleInventoryToggleEvent>(this);
    event_dispatcher_.sink<HUDNotificationEvent>().connect<&RenderingSystem::handleHUDNotificationEvent>(this);
    event_dispatcher_.sink<ToggleControlsHelpEvent>().connect<&RenderingSystem::handleToggleControlsHelpEvent>(this);
}

void RenderingSystem::update(double delta_time) {
    if (!nc_context_ || !world_plane_ || !hud_plane_ || !inventory_plane_) {
        return;
    }

    ncplane_erase(world_plane_);
    ncplane_erase(hud_plane_);
    if (inventory_visible_) {
        ncplane_erase(inventory_plane_);
        ncplane_set_fg_rgb(inventory_plane_, 0x00FFFF);
        ncplane_putstr_yx(inventory_plane_, 0, 1, "[ INVENTORY ]");
        ncplane_set_fg_rgb(inventory_plane_, 0xAAAAAA);
        ncplane_putstr_yx(inventory_plane_, 2, 1, "(empty)");
        ncplane_putstr_yx(inventory_plane_, 9, 1, "B: close");
    }

    int current_layer = 0;
    int cam_x = 0;
    int cam_y = 0;
    bool show_help = true;

    auto player_view = registry_.view<PlayerComponent, PositionComponent, PlayerCurrentLayerComponent, HUDComponent>();
    for (auto entity : player_view) {
        const auto& pos = player_view.get<PositionComponent>(entity);
        current_layer = player_view.get<PlayerCurrentLayerComponent>(entity).current_z;
        show_help = player_view.get<HUDComponent>(entity).show_controls_help;
        cam_x = pos.x;
        cam_y = pos.y;
        break;
    }

    unsigned view_rows, view_cols;
    ncplane_dim_yx(world_plane_, &view_rows, &view_cols);
    
    int offset_x = (int)view_cols / 2 - cam_x;
    int offset_y = (int)view_rows / 2 - cam_y;

    auto hud_view = registry_.view<HUDComponent>();
    for (auto entity : hud_view) {
        const auto& hud = hud_view.get<HUDComponent>(entity);
        ncplane_set_fg_rgb(hud_plane_, 0x00FFFF);
        ncplane_printf_yx(hud_plane_, 0, 0, "HEALTH: %.0f%%  CREDITS: %d", hud.health, hud.credits);
        ncplane_printf_yx(hud_plane_, 1, 0, "LAYER: %d", current_layer);

        int row = 2;
        for (const auto& notif : hud.notifications) {
            ncplane_putstr_yx(hud_plane_, row++, 0, notif.c_str());
            if (row >= 6) break;
        }

        if (show_help) {
            ncplane_set_fg_rgb(hud_plane_, 0xFFFF00);
            ncplane_putstr_yx(hud_plane_, 0, 35, "CONTROLS [? hide]");
            ncplane_putstr_yx(hud_plane_, 1, 35, "WASD:Move  SPC:Wait  Q:Quit");
            ncplane_putstr_yx(hud_plane_, 2, 35, "E:Interact  B:Inventory  ESC:Close");
            ncplane_putstr_yx(hud_plane_, 3, 35, "</>:Floor");
            ncplane_putstr_yx(hud_plane_, 4, 35, "i:Scan  I:BioAudit  c:Cogn");
            ncplane_putstr_yx(hud_plane_, 5, 35, "f:Finance  t:Structure");
        }
    }

    auto render_at = [&](int x, int y, char glyph, uint32_t color) {
        int screen_x = x + offset_x;
        int screen_y = y + offset_y;
        if (screen_x >= 0 && screen_x < (int)view_cols && screen_y >= 0 && screen_y < (int)view_rows) {
            ncplane_set_fg_rgb(world_plane_, color);
            ncplane_putchar_yx(world_plane_, screen_y, screen_x, glyph);
        }
    };

    auto terrain_view = registry_.view<TerrainComponent, RenderableComponent, PositionComponent>();
    for (auto entity : terrain_view) {
        const auto& render = terrain_view.get<RenderableComponent>(entity);
        const auto& pos = terrain_view.get<PositionComponent>(entity);
        if (pos.layer_id != current_layer) continue;
        render_at(pos.x, pos.y, render.glyph, parse_hex_color(render.color));
    }

    auto entity_view = registry_.view<RenderableComponent, PositionComponent>(entt::exclude<TerrainComponent>);
    for (auto entity : entity_view) {
        const auto& render = entity_view.get<RenderableComponent>(entity);
        const auto& pos = entity_view.get<PositionComponent>(entity);
        if (pos.layer_id != current_layer) continue;

        uint32_t color = parse_hex_color(render.color);
        if (registry_.all_of<SizeComponent>(entity)) {
            const auto& size = registry_.get<SizeComponent>(entity);
            for (int dx = 0; dx < size.width; ++dx) {
                for (int dy = 0; dy < size.height; ++dy) {
                    render_at(pos.x + dx, pos.y + dy, render.glyph, color);
                }
            }
        } else {
            render_at(pos.x, pos.y, render.glyph, color);
        }
    }

    ncplane_move_top(hud_plane_);
    notcurses_render(nc_context_);
}

void RenderingSystem::handle_input(const struct ncinput& input) { (void)input; }

void RenderingSystem::handleInventoryToggleEvent(const InventoryToggleEvent& event) {
    (void)event;
    inventory_visible_ = !inventory_visible_;
    if (inventory_visible_) ncplane_move_top(inventory_plane_);
    else ncplane_move_below(inventory_plane_, world_plane_);
}

void RenderingSystem::handleHUDNotificationEvent(const HUDNotificationEvent& event) {
    auto view = registry_.view<HUDComponent, PlayerComponent>();
    for (auto entity : view) {
        auto& hud = view.get<HUDComponent>(entity);
        hud.notifications.insert(hud.notifications.begin(), event.message);
        if (hud.notifications.size() > 5) hud.notifications.pop_back();
    }
}

void RenderingSystem::handleToggleControlsHelpEvent(const ToggleControlsHelpEvent& event) {
    if (registry_.all_of<HUDComponent>(event.entity)) {
        registry_.get<HUDComponent>(event.entity).show_controls_help = !registry_.get<HUDComponent>(event.entity).show_controls_help;
    }
}

uint32_t RenderingSystem::parse_hex_color(const std::string& hex) {
    if (hex.empty()) return 0xFFFFFF;
    std::string s = (hex[0] == '#') ? hex.substr(1) : hex;
    try { return std::stoul(s, nullptr, 16); } catch (...) { return 0xFFFFFF; }
}

} // namespace NeonOubliette::Systems
