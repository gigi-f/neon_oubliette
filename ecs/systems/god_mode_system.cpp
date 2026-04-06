#include "god_mode_system.h"
#include "../components/components.h"
#include <algorithm>

namespace NeonOubliette::Systems {

GodModeSystem::GodModeSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry_(registry), dispatcher_(dispatcher) {
}

void GodModeSystem::initialize() {
    dispatcher_.sink<GodModeFollowAgentEvent>().connect<&GodModeSystem::handleGodModeFollowAgentEvent>(this);
    dispatcher_.sink<GodModeTeleportCursorEvent>().connect<&GodModeSystem::handleGodModeTeleportCursorEvent>(this);
    dispatcher_.sink<GodModeTagEntityEvent>().connect<&GodModeSystem::handleGodModeTagEntityEvent>(this);
    dispatcher_.sink<OpenContextMenuEvent>().connect<&GodModeSystem::handleOpenContextMenuEvent>(this);
    dispatcher_.sink<CloseContextMenuEvent>().connect<&GodModeSystem::handleCloseContextMenuEvent>(this);
    dispatcher_.sink<ContextMenuSelectEvent>().connect<&GodModeSystem::handleContextMenuSelectEvent>(this);
}

void GodModeSystem::update(double delta_time) {
    (void)delta_time;
    // Handle camera following logic if needed, or rely on RenderingSystem
}

void GodModeSystem::handleGodModeFollowAgentEvent(const GodModeFollowAgentEvent& event) {
    // Clear existing follow
    auto follow_view = registry_.view<GodModeFollowComponent>();
    registry_.destroy(follow_view.begin(), follow_view.end());

    if (registry_.valid(event.target)) {
        auto ent = registry_.create();
        registry_.emplace<GodModeFollowComponent>(ent, event.target);
        
        std::string name = "Agent";
        if (registry_.all_of<NameComponent>(event.target)) {
            name = registry_.get<NameComponent>(event.target).name;
        }
        dispatcher_.trigger(HUDNotificationEvent{"Following " + name + ".", 1.5f, "#FFFF00"});
    }
}

void GodModeSystem::handleGodModeTeleportCursorEvent(const GodModeTeleportCursorEvent& event) {
    auto cursor_view = registry_.view<GodCursorComponent>();
    for (auto ent : cursor_view) {
        auto& cursor = cursor_view.get<GodCursorComponent>(ent);
        cursor.x = event.x;
        cursor.y = event.y;
        cursor.layer_id = event.layer;
        
        // Break follow if teleporting
        auto follow_view = registry_.view<GodModeFollowComponent>();
        registry_.destroy(follow_view.begin(), follow_view.end());
    }
}

void GodModeSystem::handleGodModeTagEntityEvent(const GodModeTagEntityEvent& event) {
    if (registry_.valid(event.target)) {
        registry_.emplace_or_replace<TaggedComponent>(event.target, event.tag_label);
        dispatcher_.trigger(HUDNotificationEvent{"Entity Tagged: " + event.tag_label, 1.5f, "#00FFFF"});
    }
}

void GodModeSystem::handleOpenContextMenuEvent(const OpenContextMenuEvent& event) {
    // Clear existing menus
    auto menu_view = registry_.view<ContextMenuComponent>();
    registry_.destroy(menu_view.begin(), menu_view.end());

    auto ent = registry_.create();
    auto& menu = registry_.emplace<ContextMenuComponent>(ent);
    menu.open = true;
    menu.world_x = event.x;
    menu.world_y = event.y;
    menu.layer_id = event.layer_id;
    menu.target_entity = event.target_entity;
    
    // Find entity under cursor if target_entity is null
    if (menu.target_entity == entt::null) {
        auto pos_view = registry_.view<PositionComponent>(entt::exclude<TerrainComponent>);
        for (auto e : pos_view) {
            const auto& p = pos_view.get<PositionComponent>(e);
            if (p.layer_id == event.layer_id) {
                int w = 1, h = 1;
                if (registry_.all_of<SizeComponent>(e)) {
                    const auto& s = registry_.get<SizeComponent>(e);
                    w = s.width; h = s.height;
                }
                if (event.x >= p.x && event.x < p.x + w && event.y >= p.y && event.y < p.y + h) {
                    menu.target_entity = e;
                    break;
                }
            }
        }
    }

    menu.options = {"Inspect", "Follow", "Teleport Cursor", "Tag: GOAL", "Tag: SUSPECT", "Close"};
    menu.selected_index = 0;
}

void GodModeSystem::handleCloseContextMenuEvent(const CloseContextMenuEvent& event) {
    (void)event;
    auto menu_view = registry_.view<ContextMenuComponent>();
    registry_.destroy(menu_view.begin(), menu_view.end());
}

void GodModeSystem::handleContextMenuSelectEvent(const ContextMenuSelectEvent& event) {
    execute_context_option(event.selection_index);
    dispatcher_.trigger<CloseContextMenuEvent>();
}

void GodModeSystem::execute_context_option(int index) {
    auto menu_view = registry_.view<ContextMenuComponent>();
    if (menu_view.begin() == menu_view.end()) return;
    auto& menu = menu_view.get<ContextMenuComponent>(*menu_view.begin());

    std::string choice = menu.options[index];

    if (choice == "Inspect") {
        dispatcher_.trigger(InspectEvent{entt::null, menu.layer_id, menu.world_x, menu.world_y, InspectionMode::SURFACE_SCAN});
    } else if (choice == "Follow") {
        if (registry_.valid(menu.target_entity)) {
            dispatcher_.trigger(GodModeFollowAgentEvent{menu.target_entity});
        } else {
            dispatcher_.trigger(HUDNotificationEvent{"No target to follow.", 1.5f, "#FF0000"});
        }
    } else if (choice == "Teleport Cursor") {
        // Find player position
        auto p_view = registry_.view<PlayerComponent, PositionComponent>();
        if (p_view.begin() != p_view.end()) {
            const auto& p_pos = p_view.get<PositionComponent>(*p_view.begin());
            // This is actually teleporting the player to the cursor for God Mode shortcut
            registry_.patch<PositionComponent>(*p_view.begin(), [&](auto& pos) {
                pos.x = menu.world_x;
                pos.y = menu.world_y;
                pos.layer_id = menu.layer_id;
            });
            dispatcher_.trigger(HUDNotificationEvent{"Player teleported.", 1.5f, "#00FF00"});
        }
    } else if (choice.find("Tag:") == 0) {
        if (registry_.valid(menu.target_entity)) {
            std::string tag = choice.substr(5);
            dispatcher_.trigger(GodModeTagEntityEvent{menu.target_entity, tag});
        }
    }
}

} // namespace NeonOubliette::Systems
