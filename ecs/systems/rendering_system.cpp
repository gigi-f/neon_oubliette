#include "rendering_system.h"

#include <iostream>
#include <string>

#include "../../ecs/component_declarations.h"
#include "../../ecs/events.h"

namespace NeonOubliette::Systems {

RenderingSystem::RenderingSystem(entt::registry& registry, struct notcurses* nc_context,
                                 entt::dispatcher& event_dispatcher)
    : registry_(registry), nc_context_(nc_context), event_dispatcher_(event_dispatcher), world_plane_(nullptr),
      hud_plane_(nullptr), inventory_plane_(nullptr), inventory_visible_(false) {
    std::cout << "RenderingSystem constructed." << std::endl;
}

RenderingSystem::~RenderingSystem() {
    if (world_plane_) {
        ncplane_destroy(world_plane_);
    }
    if (hud_plane_) {
        ncplane_destroy(hud_plane_);
    }
    if (inventory_plane_) {
        ncplane_destroy(inventory_plane_);
    }
}

void RenderingSystem::initialize() {
    if (nc_context_) {
        world_plane_ = notcurses_stdplane(nc_context_);

        // HUD: 4 rows at the top, full width
        struct ncplane_options nopts = {.y = 1,
                                        .x = 2,
                                        .rows = 5,
                                        .cols = 40,
                                        .userptr = nullptr,
                                        .name = "HUD",
                                        .resizecb = nullptr,
                                        .flags = 0,
                                        .margin_b = 0,
                                        .margin_r = 0};
        hud_plane_ = ncplane_create(world_plane_, &nopts);

        // Inventory: Right column
        nopts.name = "Inventory";
        nopts.y = 1;
        nopts.x = -30; // 30 columns from the right
        nopts.rows = 20;
        nopts.cols = 28;
        inventory_plane_ = ncplane_create(world_plane_, &nopts);

        if (inventory_plane_) {
            ncplane_set_scrolling(inventory_plane_, true);
        }
    }

    event_dispatcher_.sink<ECS::InventoryToggleEvent>().connect<&RenderingSystem::handleInventoryToggleEvent>(this);
    // std::cout << "RenderingSystem initialized." << std::endl;
}

void RenderingSystem::update(double delta_time) {
    if (!nc_context_ || !world_plane_ || !hud_plane_ || !inventory_plane_) {
        return;
    }

    ncplane_erase(world_plane_);
    ncplane_erase(hud_plane_);
    ncplane_erase(inventory_plane_);

    ncplane_set_fg_rgb(world_plane_, 0xAAAAAA);
    ncplane_putstr_aligned(world_plane_, 0, NCALIGN_CENTER, "Neon Oubliette World View");

    auto hud_view = registry_.view<HUDComponent>();
    for (auto entity : hud_view) {
        const auto& hud = hud_view.get<HUDComponent>(entity);
        std::string health_str = "Health: " + std::to_string(static_cast<int>(hud.health)) + "%";
        std::string credits_str = "Credits: " + std::to_string(hud.credits);
        std::string layer_str = "Layer: " + std::to_string(hud.current_layer_display);

        ncplane_set_fg_rgb(hud_plane_, 0x00FFFF);
        ncplane_putstr_aligned(hud_plane_, 0, NCALIGN_LEFT, health_str.c_str());
        ncplane_putstr_aligned(hud_plane_, 1, NCALIGN_LEFT, credits_str.c_str());
        ncplane_putstr_aligned(hud_plane_, 2, NCALIGN_LEFT, layer_str.c_str());

        int notification_line = 3;
        for (const auto& notif : hud.notifications) {
            ncplane_putstr_aligned(hud_plane_, notification_line++, NCALIGN_LEFT, notif.c_str());
        }
    }

    // Render World Entities
    auto world_view = registry_.view<RenderableComponent, PositionComponent>();
    for (auto entity : world_view) {
        const auto& render = world_view.get<RenderableComponent>(entity);
        const auto& pos = world_view.get<PositionComponent>(entity);

        uint32_t color = parse_hex_color(render.color);
        ncplane_set_fg_rgb(world_plane_, color);
        ncplane_putchar_yx(world_plane_, pos.y, pos.x, render.glyph);
    }

    notcurses_render(nc_context_);
}

void RenderingSystem::handle_input(const ncinput& input) {
    (void)input;
}

void RenderingSystem::handleInventoryToggleEvent(const ECS::InventoryToggleEvent& event) {
    (void)event;
    inventory_visible_ = !inventory_visible_;
}

uint32_t RenderingSystem::parse_hex_color(const std::string& hex) {
    if (hex.empty()) return 0xFFFFFF;
    std::string s = hex;
    if (s[0] == '#') s = s.substr(1);
    try {
        return std::stoul(s, nullptr, 16);
    } catch (...) {
        return 0xFFFFFF;
    }
}

} // namespace NeonOubliette::Systems
