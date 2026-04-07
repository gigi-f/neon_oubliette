#include "input_system.h"
#include "../components/components.h"
#include "../event_declarations.h"
#include <cstdlib>

namespace NeonOubliette {
namespace Systems {

InputSystem::InputSystem(entt::registry& registry, struct notcurses* nc_context, entt::dispatcher& dispatcher)
    : m_registry(registry), m_ncContext(nc_context), m_dispatcher(dispatcher) {
}

void InputSystem::initialize() {
}

void InputSystem::update(double delta_time) {
    ncinput input;
    struct timespec ts = {0, 0};
    uint32_t key_id;

    // Get Simulation State
    SimulationMode current_mode = SimulationMode::STANDARD;
    auto state_view = m_registry.view<SimulationStateComponent>();
    if (state_view.begin() != state_view.end()) {
        auto& state = state_view.get<SimulationStateComponent>(*state_view.begin());
        current_mode = state.mode;
    }

    // Check if Context Menu is open
    auto menu_view = m_registry.view<ContextMenuComponent>();
    ContextMenuComponent* active_menu = nullptr;
    if (menu_view.begin() != menu_view.end()) {
        active_menu = &menu_view.get<ContextMenuComponent>(*menu_view.begin());
    }

    // Check if Dialogue is open
    auto dialogue_view = m_registry.view<DialogueStateComponent>();
    DialogueStateComponent* active_dialogue = nullptr;
    if (dialogue_view.begin() != dialogue_view.end()) {
        active_dialogue = &dialogue_view.get<DialogueStateComponent>(*dialogue_view.begin());
    }

    while ((key_id = notcurses_get(m_ncContext, &ts, &input)) > 0) {
        // --- Handle Mouse [D.1 / D.3] ---
        if (nckey_mouse_p(key_id) || key_id == NCKEY_MOTION) {
            unsigned term_y, term_x;
            notcurses_term_dim_yx(m_ncContext, &term_y, &term_x);

            int cam_x = 0, cam_y = 0, cam_z = 0;
            if (current_mode == SimulationMode::GOD_MODE) {
                // Determine if we're following or using cursor
                bool following = false;
                auto follow_view = m_registry.view<GodModeFollowComponent>();
                if (follow_view.begin() != follow_view.end()) {
                    auto target = follow_view.get<GodModeFollowComponent>(*follow_view.begin()).target;
                    if (m_registry.valid(target) && m_registry.all_of<PositionComponent>(target)) {
                        const auto& tp = m_registry.get<PositionComponent>(target);
                        cam_x = tp.x; cam_y = tp.y; cam_z = tp.layer_id;
                        following = true;
                    }
                }
                if (!following) {
                    auto g_view = m_registry.view<GodCursorComponent>();
                    if (g_view.begin() != g_view.end()) {
                        auto& gc = g_view.get<GodCursorComponent>(*g_view.begin());
                        cam_x = gc.x; cam_y = gc.y; cam_z = gc.layer_id;
                    }
                }
            } else {
                auto p_view = m_registry.view<PlayerComponent, PositionComponent, PlayerCurrentLayerComponent>();
                if (p_view.begin() != p_view.end()) {
                    auto ent = *p_view.begin();
                    cam_x = p_view.get<PositionComponent>(ent).x;
                    cam_y = p_view.get<PositionComponent>(ent).y;
                    cam_z = p_view.get<PlayerCurrentLayerComponent>(ent).current_z;
                }
            }

            int wx = input.x - ((int)term_x / 2) + cam_x;
            int wy = input.y - ((int)term_y / 2) + cam_y;
            int wz = cam_z;

            auto cursor_view = m_registry.view<StandardCursorComponent>();
            if (cursor_view.begin() != cursor_view.end()) {
                auto& cursor = cursor_view.get<StandardCursorComponent>(*cursor_view.begin());
                cursor.x = wx; cursor.y = wy; cursor.layer_id = wz;
                cursor.active = true; cursor.mouse_driven = true;
                cursor.screen_x = input.x; cursor.screen_y = input.y;
            }

            if (input.evtype == NCTYPE_PRESS) {
                if (key_id == NCKEY_BUTTON1) { // Left Click
                    if (current_mode == SimulationMode::GOD_MODE) {
                         m_dispatcher.trigger(InspectEvent{entt::null, wz, wx, wy, InspectionMode::SURFACE_SCAN});
                         if (active_menu) m_dispatcher.trigger<CloseContextMenuEvent>();
                    } else {
                        // Standard mode: left-click = interact at cursor world position
                        m_dispatcher.trigger(InteractEvent{entt::null, wz, wx, wy});
                        m_dispatcher.trigger<AdvanceTurnRequestEvent>();
                    }
                } else if (key_id == NCKEY_BUTTON3) { // Right Click
                    if (current_mode == SimulationMode::GOD_MODE) {
                        m_dispatcher.trigger(OpenContextMenuEvent{wx, wy, wz});
                    }
                } else if (key_id == NCKEY_BUTTON2) { // Middle Click [D.3]
                    if (current_mode == SimulationMode::GOD_MODE) {
                        entt::entity target = entt::null;
                        auto pos_view = m_registry.view<PositionComponent>(entt::exclude<TerrainComponent>);
                        for (auto ent : pos_view) {
                            const auto& p = pos_view.get<PositionComponent>(ent);
                            if (p.layer_id == wz) {
                                int w = 1, h = 1;
                                if (m_registry.all_of<SizeComponent>(ent)) {
                                    const auto& s = m_registry.get<SizeComponent>(ent);
                                    w = s.width; h = s.height;
                                }
                                if (wx >= p.x && wx < p.x + w && wy >= p.y && wy < p.y + h) {
                                    target = ent; break;
                                }
                            }
                        }
                        if (m_registry.valid(target)) m_dispatcher.trigger(GodModeFollowAgentEvent{target});
                    }
                }
            }
            if (key_id == NCKEY_MOTION) continue;
        }

        if (input.evtype != NCTYPE_PRESS && input.evtype != NCTYPE_UNKNOWN) continue;

        // Context Menu Navigation
        if (active_menu && active_menu->open) {
            // ... (existing menu logic)
            if (key_id == NCKEY_UP) {
                active_menu->selected_index = std::max(0, active_menu->selected_index - 1);
                continue;
            } else if (key_id == NCKEY_DOWN) {
                active_menu->selected_index = std::min((int)active_menu->options.size() - 1, active_menu->selected_index + 1);
                continue;
            } else if (key_id == NCKEY_ENTER || key_id == '\r' || key_id == '\n') {
                m_dispatcher.trigger(ContextMenuSelectEvent{active_menu->selected_index});
                continue;
            } else if (key_id == NCKEY_ESC) {
                m_dispatcher.trigger<CloseContextMenuEvent>();
                continue;
            }
            // Swallow other keys if menu is open
            continue;
        }

        // Dialogue Navigation
        if (active_dialogue && active_dialogue->is_open) {
            if (key_id == 'a' || key_id == 'b' || key_id == 'c') {
                m_dispatcher.trigger(HUDNotificationEvent{"Dialogue choice acknowledged.", 1.5f, "#00FFFF"});
                m_dispatcher.trigger<CloseDialogueWindowEvent>();
                m_dispatcher.trigger<AdvanceTurnRequestEvent>();
                continue;
            } else if (key_id == NCKEY_ESC) {
                m_dispatcher.trigger<CloseDialogueWindowEvent>();
                continue;
            }
            // Swallow other keys if dialogue is open
            continue;
        }

        // 1. Global Commands
        if (key_id == 'q' || key_id == 'Q') {
            m_dispatcher.trigger<NeonOubliette::ShutdownEvent>();
            return;
        }

        // Toggle Debug Overlay (F12) — disabled for now
        // if (key_id == NCKEY_F12) { ... }

        // Toggle God Mode / Focus Building
        if (key_id == 'g' || key_id == 'G') {
            // ... (existing code)
        }

        // --- Cycle Interaction Mode [E.1] ---
        if (key_id == NCKEY_TAB || key_id == '\t') {
            auto player_view = m_registry.view<PlayerComponent>();
            for (auto entity : player_view) {
                if (!m_registry.all_of<PlayerInteractionComponent>(entity)) {
                    m_registry.emplace<PlayerInteractionComponent>(entity);
                }
                auto& pi = m_registry.get<PlayerInteractionComponent>(entity);
                pi.current_mode = static_cast<InteractionMode>((static_cast<int>(pi.current_mode) + 1) % 3);
                
                std::string mode_name = "OBSERVE";
                if (pi.current_mode == InteractionMode::SPEAK) mode_name = "SPEAK";
                else if (pi.current_mode == InteractionMode::TRADE) mode_name = "TRADE";
                
                m_dispatcher.trigger(HUDNotificationEvent{"Mode: " + mode_name, 1.5f, "#FFFFFF"});
            }
            continue;
        }

        // Toggle Pause
        if (key_id == 'p' || key_id == 'P' || (current_mode == SimulationMode::GOD_MODE && key_id == ' ')) {
            m_dispatcher.trigger<TogglePauseEvent>();
            continue;
        }

        // Adjust Speed (God Mode Only)
        if (current_mode == SimulationMode::GOD_MODE) {
            if (key_id == '+' || key_id == '=') {
                m_dispatcher.trigger(AdjustGodModeSpeedEvent{0.5f});
                continue;
            } else if (key_id == '-' || key_id == '_') {
                m_dispatcher.trigger(AdjustGodModeSpeedEvent{-0.5f});
                continue;
            }
        }

        // 2. Mode-Specific Commands
        if (current_mode == SimulationMode::GOD_MODE) {
            auto cursor_view = m_registry.view<GodCursorComponent>();
            if (cursor_view.begin() != cursor_view.end()) {
                auto& cursor = cursor_view.get<GodCursorComponent>(*cursor_view.begin());
                
                auto break_follow = [&]() {
                    auto follow_view = m_registry.view<GodModeFollowComponent>();
                    m_registry.destroy(follow_view.begin(), follow_view.end());
                };

                if (key_id == 'w' || key_id == 'W' || key_id == NCKEY_UP) { cursor.y--; break_follow(); }
                else if (key_id == 's' || key_id == 'S' || key_id == NCKEY_DOWN) { cursor.y++; break_follow(); }
                else if (key_id == 'a' || key_id == 'A' || key_id == NCKEY_LEFT) { cursor.x--; break_follow(); }
                else if (key_id == 'd' || key_id == 'D' || key_id == NCKEY_RIGHT) { cursor.x++; break_follow(); }
                
                // Vertical Navigation in God Mode
                else if (key_id == '>') {
                    auto state_view = m_registry.view<SimulationStateComponent>();
                    if (state_view.begin() != state_view.end()) {
                        auto& state = state_view.get<SimulationStateComponent>(*state_view.begin());
                        if (state.is_inside_view && m_registry.valid(state.focused_building)) {
                            auto& b_comp = m_registry.get<BuildingComponent>(state.focused_building);
                            state.focus_floor = std::min(state.focus_floor + 1, b_comp.height - 1);
                        } else {
                            cursor.layer_id++;
                            break_follow();
                        }
                    } else {
                        cursor.layer_id++;
                        break_follow();
                    }
                }
                else if (key_id == '<') {
                    auto state_view = m_registry.view<SimulationStateComponent>();
                    if (state_view.begin() != state_view.end()) {
                        auto& state = state_view.get<SimulationStateComponent>(*state_view.begin());
                        if (state.is_inside_view && m_registry.valid(state.focused_building)) {
                            state.focus_floor = std::max(state.focus_floor - 1, 0);
                        } else {
                            cursor.layer_id--;
                            break_follow();
                        }
                    } else {
                        cursor.layer_id--;
                        break_follow();
                    }
                }
                
                // UI Toggles
                else if (key_id == NCKEY_ESC) {
                    auto state_view = m_registry.view<SimulationStateComponent>();
                    if (state_view.begin() != state_view.end()) {
                        auto& state = state_view.get<SimulationStateComponent>(*state_view.begin());
                        if (state.is_inside_view) {
                            m_dispatcher.trigger<GodModeExitFocusEvent>();
                        } else {
                            m_dispatcher.trigger<CloseInspectionWindowEvent>();
                            m_dispatcher.trigger<CloseDialogueWindowEvent>();
                        }
                    } else {
                        m_dispatcher.trigger<CloseInspectionWindowEvent>();
                        m_dispatcher.trigger<CloseDialogueWindowEvent>();
                    }
                }

                // Inspection in God Mode (uses cursor pos)
                if (key_id == 'i') m_dispatcher.trigger(InspectEvent{entt::null, cursor.layer_id, cursor.x, cursor.y, InspectionMode::SURFACE_SCAN});
                else if (key_id == 'I') m_dispatcher.trigger(InspectEvent{entt::null, cursor.layer_id, cursor.x, cursor.y, InspectionMode::BIOLOGICAL_AUDIT});
                else if (key_id == 'c') m_dispatcher.trigger(InspectEvent{entt::null, cursor.layer_id, cursor.x, cursor.y, InspectionMode::COGNITIVE_PROFILE});
                else if (key_id == 'F') m_dispatcher.trigger(InspectEvent{entt::null, cursor.layer_id, cursor.x, cursor.y, InspectionMode::FINANCIAL_FORENSICS});
                else if (key_id == 't') m_dispatcher.trigger(InspectEvent{entt::null, cursor.layer_id, cursor.x, cursor.y, InspectionMode::STRUCTURAL_ANALYSIS});
                else if (key_id == 'f') { // [D.3] Keyboard Follow
                    entt::entity target = entt::null;
                    auto pos_view = m_registry.view<PositionComponent>(entt::exclude<TerrainComponent>);
                    for (auto ent : pos_view) {
                        const auto& p = pos_view.get<PositionComponent>(ent);
                        if (p.layer_id == cursor.layer_id) {
                            int w = 1, h = 1;
                            if (m_registry.all_of<SizeComponent>(ent)) {
                                const auto& s = m_registry.get<SizeComponent>(ent);
                                w = s.width; h = s.height;
                            }
                            if (cursor.x >= p.x && cursor.x < p.x + w && 
                                cursor.y >= p.y && cursor.y < p.y + h) {
                                target = ent; break;
                            }
                        }
                    }
                    if (m_registry.valid(target)) m_dispatcher.trigger(GodModeFollowAgentEvent{target});
                }
            }
            continue;
        }

        // STANDARD MODE (Player-centric)
        auto player_view = m_registry.view<PlayerComponent, PositionComponent>();
        entt::entity player_entity = entt::null;
        for (auto entity : player_view) {
            player_entity = entity;
            break;
        }

        if (player_entity == entt::null)
            continue;
        
        auto& pos = player_view.get<PositionComponent>(player_entity);

        // Inventory UI Focus
        if (m_registry.all_of<HUDComponent>(player_entity)) {
            auto& hud = m_registry.get<HUDComponent>(player_entity);
            if (hud.inventory_open) {
                if (key_id == NCKEY_UP) {
                    hud.selected_inventory_index = std::max(0, hud.selected_inventory_index - 1);
                    continue;
                } else if (key_id == NCKEY_DOWN) {
                    if (m_registry.all_of<InventoryComponent>(player_entity)) {
                        auto& inv = m_registry.get<InventoryComponent>(player_entity);
                        if (!inv.contained_items.empty()) {
                            hud.selected_inventory_index = std::min(static_cast<int>(inv.contained_items.size()) - 1, hud.selected_inventory_index + 1);
                        }
                    }
                    continue;
                } else if (key_id == NCKEY_ENTER || key_id == '\r' || key_id == '\n') {
                    if (m_registry.all_of<InventoryComponent>(player_entity)) {
                        auto& inv = m_registry.get<InventoryComponent>(player_entity);
                        if (!inv.contained_items.empty() && hud.selected_inventory_index < (int)inv.contained_items.size()) {
                            entt::entity item_ent = inv.contained_items[hud.selected_inventory_index];
                            
                            // [C.1] Set as held item
                            hud.held_item = item_ent;
                            hud.held_item_flash_timer = 0.5f; // [C.2] Flash on swap
                            std::string item_name = "item";
                            if (m_registry.all_of<ItemComponent>(item_ent)) {
                                item_name = m_registry.get<ItemComponent>(item_ent).name;
                            }
                            m_dispatcher.trigger(HUDNotificationEvent{"Holding " + item_name, 1.5f, "#00FFFF"});

                            m_dispatcher.trigger(UseItemEvent{player_entity, item_ent});
                            m_dispatcher.trigger<AdvanceTurnRequestEvent>();
                        }
                    }
                    continue;
                } else if (key_id == 'b' || key_id == 'B') {
                    m_dispatcher.trigger(InventoryToggleEvent{player_entity});
                    continue;
                } else if (key_id == NCKEY_ESC) {
                    m_dispatcher.trigger(InventoryToggleEvent{player_entity});
                    continue;
                }
                // Swallow other keys if inventory is open
                continue;
            }
        }

        // Determine target for movement (Direct or via Personal Vehicle)
        entt::entity move_target = player_entity;
        if (m_registry.all_of<RidingComponent>(player_entity)) {
            auto vehicle = m_registry.get<RidingComponent>(player_entity).vehicle;
            if (m_registry.valid(vehicle) && m_registry.all_of<PersonalVehicleComponent>(vehicle)) {
                if (m_registry.get<PersonalVehicleComponent>(vehicle).driver == player_entity) {
                    move_target = vehicle;
                }
            }
        }

        // Movement (Horizontal)
        if (key_id == 'w' || key_id == 'W') {
            m_dispatcher.trigger(MoveEvent{move_target, 0, -1, pos.layer_id});
            m_dispatcher.trigger<AdvanceTurnRequestEvent>();
            // Snap back [D.1]
            auto cv = m_registry.view<StandardCursorComponent>();
            if (cv.begin() != cv.end()) cv.get<StandardCursorComponent>(*cv.begin()).active = false;
        } else if (key_id == 's' || key_id == 'S') {
            m_dispatcher.trigger(MoveEvent{move_target, 0, 1, pos.layer_id});
            m_dispatcher.trigger<AdvanceTurnRequestEvent>();
            // Snap back [D.1]
            auto cv = m_registry.view<StandardCursorComponent>();
            if (cv.begin() != cv.end()) cv.get<StandardCursorComponent>(*cv.begin()).active = false;
        } else if (key_id == 'a' || key_id == 'A') {
            m_dispatcher.trigger(MoveEvent{move_target, -1, 0, pos.layer_id});
            m_dispatcher.trigger<AdvanceTurnRequestEvent>();
            // Snap back [D.1]
            auto cv = m_registry.view<StandardCursorComponent>();
            if (cv.begin() != cv.end()) cv.get<StandardCursorComponent>(*cv.begin()).active = false;
        } else if (key_id == 'd' || key_id == 'D') {
            m_dispatcher.trigger(MoveEvent{move_target, 1, 0, pos.layer_id});
            m_dispatcher.trigger<AdvanceTurnRequestEvent>();
            // Snap back [D.1]
            auto cv = m_registry.view<StandardCursorComponent>();
            if (cv.begin() != cv.end()) cv.get<StandardCursorComponent>(*cv.begin()).active = false;
        } 
        
        // --- Keyboard Cursor Movement [D.1] ---
        else if (key_id == NCKEY_UP) {
            auto cv = m_registry.view<StandardCursorComponent>();
            if (cv.begin() != cv.end()) {
                auto& sc = cv.get<StandardCursorComponent>(*cv.begin());
                if (!sc.active) { sc.x = pos.x; sc.y = pos.y; sc.active = true; }
                sc.y--; sc.mouse_driven = false;
            }
        } else if (key_id == NCKEY_DOWN) {
            auto cv = m_registry.view<StandardCursorComponent>();
            if (cv.begin() != cv.end()) {
                auto& sc = cv.get<StandardCursorComponent>(*cv.begin());
                if (!sc.active) { sc.x = pos.x; sc.y = pos.y; sc.active = true; }
                sc.y++; sc.mouse_driven = false;
            }
        } else if (key_id == NCKEY_LEFT) {
            auto cv = m_registry.view<StandardCursorComponent>();
            if (cv.begin() != cv.end()) {
                auto& sc = cv.get<StandardCursorComponent>(*cv.begin());
                if (!sc.active) { sc.x = pos.x; sc.y = pos.y; sc.active = true; }
                sc.x--; sc.mouse_driven = false;
            }
        } else if (key_id == NCKEY_RIGHT) {
            auto cv = m_registry.view<StandardCursorComponent>();
            if (cv.begin() != cv.end()) {
                auto& sc = cv.get<StandardCursorComponent>(*cv.begin());
                if (!sc.active) { sc.x = pos.x; sc.y = pos.y; sc.active = true; }
                sc.x++; sc.mouse_driven = false;
            }
        }
        
        else if (key_id == ' ') {
            // Spacebar = interact at cursor position (same as E), or wait if nothing to interact with
            int tx = pos.x; int ty = pos.y; int tl = pos.layer_id;
            auto cursor_view_sp = m_registry.view<StandardCursorComponent>();
            if (cursor_view_sp.begin() != cursor_view_sp.end()) {
                auto& sc = cursor_view_sp.get<StandardCursorComponent>(*cursor_view_sp.begin());
                if (sc.active) { tx = sc.x; ty = sc.y; tl = sc.layer_id; }
            }
            m_dispatcher.trigger(InteractEvent{entt::null, tl, tx, ty});
            m_dispatcher.trigger<AdvanceTurnRequestEvent>();
        }
        
        // Interaction
        else if (key_id == 'e' || key_id == 'E') {
            int tx = pos.x; int ty = pos.y; int tl = pos.layer_id;
            auto cursor_view = m_registry.view<StandardCursorComponent>();
            if (cursor_view.begin() != cursor_view.end()) {
                auto& sc = cursor_view.get<StandardCursorComponent>(*cursor_view.begin());
                if (sc.active) { tx = sc.x; ty = sc.y; tl = sc.layer_id; }
            }

            // [D.2] Determine range for Interaction (currently mostly Trade (1) or Speak (3) related things)
            int dist = std::max(std::abs(tx - pos.x), std::abs(ty - pos.y));
            int max_range = 1; // Default TRADE
            if (m_registry.all_of<PlayerInteractionComponent>(player_entity)) {
                auto mode = m_registry.get<PlayerInteractionComponent>(player_entity).current_mode;
                if (mode == InteractionMode::SPEAK) max_range = 3;
                else if (mode == InteractionMode::OBSERVE) max_range = 6;
            }

            if (dist > max_range) {
                std::string mode_str = "interact";
                if (m_registry.all_of<PlayerInteractionComponent>(player_entity)) {
                    auto mode = m_registry.get<PlayerInteractionComponent>(player_entity).current_mode;
                    if (mode == InteractionMode::SPEAK) mode_str = "speak";
                    else if (mode == InteractionMode::TRADE) mode_str = "trade";
                }
                m_dispatcher.trigger(HUDNotificationEvent{"Too far to " + mode_str + ".", 1.5f, "#FF0000"});
                continue;
            }

            m_dispatcher.trigger(InteractEvent{player_entity, tl, tx, ty});
            m_dispatcher.trigger<AdvanceTurnRequestEvent>();
        }

        // Movement (Vertical)
        else if (key_id == '>') {
            m_dispatcher.trigger(PlayerLayerChangeEvent{1});
            m_dispatcher.trigger<AdvanceTurnRequestEvent>();
            auto cv = m_registry.view<StandardCursorComponent>();
            if (cv.begin() != cv.end()) cv.get<StandardCursorComponent>(*cv.begin()).layer_id++;
        } else if (key_id == '<') {
            m_dispatcher.trigger(PlayerLayerChangeEvent{-1});
            m_dispatcher.trigger<AdvanceTurnRequestEvent>();
            auto cv = m_registry.view<StandardCursorComponent>();
            if (cv.begin() != cv.end()) cv.get<StandardCursorComponent>(*cv.begin()).layer_id--;
        }
        
        // UI Toggles
        else if (key_id == '?') {
            m_dispatcher.trigger(ToggleControlsHelpEvent{player_entity});
        } else if (key_id == 'b' || key_id == 'B') {
            m_dispatcher.trigger(InventoryToggleEvent{player_entity});
        } else if (key_id == NCKEY_ESC) {
            m_dispatcher.trigger<CloseInspectionWindowEvent>();
            m_dispatcher.trigger<CloseDialogueWindowEvent>();
        }

        // Inspection
        else if (key_id == 'i' || key_id == 'I' || key_id == 'c' || key_id == 'f' || key_id == 't') { 
            int tx = pos.x; int ty = pos.y; int tl = pos.layer_id;
            auto cursor_view = m_registry.view<StandardCursorComponent>();
            if (cursor_view.begin() != cursor_view.end()) {
                auto& sc = cursor_view.get<StandardCursorComponent>(*cursor_view.begin());
                if (sc.active) { tx = sc.x; ty = sc.y; tl = sc.layer_id; }
            }

            // [D.2] Enforce Observe Range (6)
            int dist = std::max(std::abs(tx - pos.x), std::abs(ty - pos.y));
            if (dist > 6) {
                m_dispatcher.trigger(HUDNotificationEvent{"Too far to observe.", 1.5f, "#FF0000"});
                continue;
            }

            InspectionMode mode = InspectionMode::SURFACE_SCAN;
            if (key_id == 'I') mode = InspectionMode::BIOLOGICAL_AUDIT;
            else if (key_id == 'c') mode = InspectionMode::COGNITIVE_PROFILE;
            else if (key_id == 'f') mode = InspectionMode::FINANCIAL_FORENSICS;
            else if (key_id == 't') mode = InspectionMode::STRUCTURAL_ANALYSIS;
            
            m_dispatcher.trigger(InspectEvent{player_entity, tl, tx, ty, mode});
        }
    }
}

} // namespace Systems
} // namespace NeonOubliette
