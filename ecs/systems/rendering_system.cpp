#include "rendering_system.h"
#include <iostream>
#include <string>
#include <fstream>
#include <cmath>
#include <random>
#include "../components/components.h"
#include "../components/simulation_layers.h"
#include "../components/lod_components.h"
#include "../components/physics_colors.h"
#include "../event_declarations.h"

namespace NeonOubliette::Systems {

// [L.6] Forward declaration for internal dashboard rendering
static void render_sparkline(struct ncplane* plane, int y, int x, const std::vector<float>& history, uint32_t color);

RenderingSystem::RenderingSystem(entt::registry& registry, struct notcurses* nc_context,
                                 entt::dispatcher& event_dispatcher)
    : registry_(registry), nc_context_(nc_context), event_dispatcher_(event_dispatcher), 
      world_plane_(nullptr), entity_plane_(nullptr), range_ring_plane_(nullptr), hud_plane_(nullptr), 
      inventory_plane_(nullptr), barter_plane_(nullptr), interior_overlay_plane_(nullptr), 
      minimap_plane_(nullptr), speech_plane_(nullptr), dialogue_log_plane_(nullptr), cursor_plane_(nullptr), context_menu_plane_(nullptr),
      debug_overlay_plane_(nullptr), inventory_visible_(false) {
}

RenderingSystem::~RenderingSystem() {
    if (hud_plane_) ncplane_destroy(hud_plane_);
    if (inventory_plane_) ncplane_destroy(inventory_plane_);
    if (barter_plane_) ncplane_destroy(barter_plane_);
    if (world_plane_) ncplane_destroy(world_plane_);
    if (entity_plane_) ncplane_destroy(entity_plane_);
    if (range_ring_plane_) ncplane_destroy(range_ring_plane_);
    if (interior_overlay_plane_) ncplane_destroy(interior_overlay_plane_);
    if (minimap_plane_) ncplane_destroy(minimap_plane_);
    if (speech_plane_) ncplane_destroy(speech_plane_);
    if (dialogue_log_plane_) ncplane_destroy(dialogue_log_plane_);
    if (crisis_dashboard_plane_) ncplane_destroy(crisis_dashboard_plane_);
    if (cursor_plane_) ncplane_destroy(cursor_plane_);
    if (context_menu_plane_) ncplane_destroy(context_menu_plane_);
    if (debug_overlay_plane_) ncplane_destroy(debug_overlay_plane_);
}

void RenderingSystem::initialize() {
    if (nc_context_) {
        struct ncplane* stdp = notcurses_stdplane(nc_context_);
        unsigned term_y, term_x;
        ncplane_dim_yx(stdp, &term_y, &term_x);

        term_y = (term_y > 0) ? term_y : 24;
        term_x = (term_x > 0) ? term_x : 80;

        uint64_t transp_channels = 0;
        ncchannels_set_fg_alpha(&transp_channels, NCALPHA_TRANSPARENT);
        ncchannels_set_bg_alpha(&transp_channels, NCALPHA_TRANSPARENT);

        struct ncplane_options nopts = {};
        nopts.y = 0;
        nopts.x = 0;
        nopts.rows = term_y;
        nopts.cols = term_x;
        
        nopts.name = "Terrain";
        world_plane_ = ncplane_create(stdp, &nopts);
        if (world_plane_) ncplane_set_base(world_plane_, "", 0, transp_channels);

        nopts.name = "RangeRing";
        range_ring_plane_ = ncplane_create(stdp, &nopts);
        if (range_ring_plane_) {
            ncplane_set_base(range_ring_plane_, "", 0, transp_channels);
            ncplane_move_above(range_ring_plane_, world_plane_);
        }

        nopts.name = "Entities";
        entity_plane_ = ncplane_create(stdp, &nopts);
        if (entity_plane_) {
            ncplane_set_base(entity_plane_, "", 0, transp_channels);
            ncplane_move_above(entity_plane_, range_ring_plane_);
        }

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

        // Barter Plane [Phase T.1]
        nopts.name = "BarterPanel";
        nopts.rows = (unsigned)(term_y * 0.8);
        nopts.cols = (unsigned)(term_x * 0.8);
        nopts.y = (term_y - nopts.rows) / 2;
        nopts.x = (term_x - nopts.cols) / 2;
        barter_plane_ = ncplane_create(stdp, &nopts);
        if (barter_plane_) {
            uint64_t barter_channels = 0;
            ncchannels_set_bg_rgb(&barter_channels, 0x101010);
            ncchannels_set_fg_rgb(&barter_channels, 0xFFD700);
            ncplane_set_base(barter_plane_, " ", 0, barter_channels);
            ncplane_move_below(barter_plane_, world_plane_);
        }

        // Interior Overlay Plane [B.3]
        nopts.name = "InteriorOverlay";
        nopts.rows = 20;
        nopts.cols = 50;
        nopts.y = (term_y - nopts.rows) / 2;
        nopts.x = (term_x - nopts.cols) / 2;
        interior_overlay_plane_ = ncplane_create(stdp, &nopts);
        if (interior_overlay_plane_) {
            uint64_t int_channels = 0;
            ncchannels_set_bg_rgb(&int_channels, 0x101010);
            ncchannels_set_fg_rgb(&int_channels, 0x00FF00);
            ncplane_set_base(interior_overlay_plane_, " ", 0, int_channels);
            ncplane_move_below(interior_overlay_plane_, world_plane_);
        }

        // Minimap disabled — not needed

        // Speech Plane [F.5]
        nopts.name = "Speech";
        nopts.rows = term_y;
        nopts.cols = term_x;
        nopts.y = 0;
        nopts.x = 0;
        speech_plane_ = ncplane_create(stdp, &nopts);
        if (speech_plane_) {
            ncplane_set_base(speech_plane_, "", 0, transp_channels);
            ncplane_move_above(speech_plane_, entity_plane_);
        }

        // Dialogue Log Plane [F.6]
        nopts.name = "DialogueLog";
        nopts.rows = (unsigned)(term_y * 0.7);
        nopts.cols = (unsigned)(term_x * 0.7);
        nopts.y = (term_y - nopts.rows) / 2;
        nopts.x = (term_x - nopts.cols) / 2;
        dialogue_log_plane_ = ncplane_create(stdp, &nopts);
        if (dialogue_log_plane_) {
            uint64_t log_channels = 0;
            ncchannels_set_bg_rgb(&log_channels, 0x101010);
            ncchannels_set_fg_rgb(&log_channels, 0xAAAAAA);
            ncplane_set_base(dialogue_log_plane_, " ", 0, log_channels);
            ncplane_move_below(dialogue_log_plane_, world_plane_);
        }

        // Crisis Dashboard Plane [L.6]
        nopts.name = "CrisisDashboard";
        nopts.rows = (unsigned)(term_y * 0.85);
        nopts.cols = (unsigned)(term_x * 0.9);
        nopts.y = (term_y - nopts.rows) / 2;
        nopts.x = (term_x - nopts.cols) / 2;
        crisis_dashboard_plane_ = ncplane_create(stdp, &nopts);
        if (crisis_dashboard_plane_) {
            uint64_t dash_channels = 0;
            ncchannels_set_bg_rgb(&dash_channels, 0x050510);
            ncchannels_set_fg_rgb(&dash_channels, 0x00FF00);
            ncplane_set_base(crisis_dashboard_plane_, " ", 0, dash_channels);
            ncplane_move_below(crisis_dashboard_plane_, world_plane_);
        }

        // Cursor Plane [D.1]
        nopts.name = "Cursor";
        nopts.rows = 1;
        nopts.cols = 1;
        nopts.y = 0;
        nopts.x = 0;
        cursor_plane_ = ncplane_create(stdp, &nopts);
        if (cursor_plane_) {
            ncplane_move_top(cursor_plane_);
        }

        // Context Menu Plane [D.3]
        nopts.name = "ContextMenu";
        nopts.rows = 6;
        nopts.cols = 20;
        context_menu_plane_ = ncplane_create(stdp, &nopts);
        if (context_menu_plane_) {
            uint64_t ctx_channels = 0;
            ncchannels_set_bg_rgb(&ctx_channels, 0x1A1A1A);
            ncchannels_set_fg_rgb(&ctx_channels, 0xFFFFFF);
            ncplane_set_base(context_menu_plane_, " ", 0, ctx_channels);
            ncplane_move_below(context_menu_plane_, world_plane_);
        }

        // Debug Overlay Plane — disabled for now (stderr logging still active)
        // To re-enable: uncomment this block and the rendering block below
        /*
        nopts.name = "DebugOverlay";
        nopts.rows = 12;
        nopts.cols = 44;
        nopts.y = term_y - nopts.rows;
        nopts.x = 0;
        debug_overlay_plane_ = ncplane_create(stdp, &nopts);
        if (debug_overlay_plane_) {
            uint64_t dbg_channels = 0;
            ncchannels_set_bg_rgb(&dbg_channels, 0x0A0A1A);
            ncchannels_set_fg_rgb(&dbg_channels, 0x00FF00);
            ncplane_set_base(debug_overlay_plane_, " ", 0, dbg_channels);
            ncplane_move_top(debug_overlay_plane_);
        }
        */

        notcurses_cursor_disable(nc_context_);
        // Mouse controls removed
    }

    event_dispatcher_.sink<InventoryToggleEvent>().connect<&RenderingSystem::handleInventoryToggleEvent>(this);
    event_dispatcher_.sink<HUDNotificationEvent>().connect<&RenderingSystem::handleHUDNotificationEvent>(this);
    event_dispatcher_.sink<ToggleControlsHelpEvent>().connect<&RenderingSystem::handleToggleControlsHelpEvent>(this);
    event_dispatcher_.sink<ToggleCrisisDashboardEvent>().connect<&RenderingSystem::handleToggleCrisisDashboardEvent>(this);
    event_dispatcher_.sink<BroadcastPulseEvent>().connect<&RenderingSystem::handleBroadcastPulseEvent>(this);
}

void RenderingSystem::handleBroadcastPulseEvent(const BroadcastPulseEvent& event) {
    uint32_t color = 0xAAAAAA; // Default Neutral Gray
    
    bool is_pirate = false;
    if (registry_.valid(event.tower_entity) && registry_.all_of<PirateNodeComponent>(event.tower_entity)) {
        is_pirate = true;
        color = 0xFF00FF; // Magenta for Pirate Node
    } else if (registry_.valid(event.faction_entity) && registry_.all_of<FactionComponent>(event.faction_entity)) {
        const auto& faction = registry_.get<FactionComponent>(event.faction_entity);
        if (faction.faction_id == "CONSENSUS") color = 0x55AAFF;
        else if (faction.faction_id == "ENTROPIC_DRIFT") color = 0xFF5555;
        else if (faction.faction_id == "SILICON_MAW") color = 0x55FF55;
        else if (faction.faction_id == "VOID_WALKERS") color = 0xAA55FF;
        else if (faction.faction_id == "SYNDICATE") color = 0xFFCC33;
    }

    int max_ticks = is_pirate ? 6 : 10; // Pirate pulses are shorter/sharper
    active_pulses_.push_back({event.x, event.y, event.layer, event.radius, 0, max_ticks, color});
}

void RenderingSystem::update(double delta_time) {
    if (!nc_context_ || !world_plane_ || !hud_plane_ || !inventory_plane_) {
        return;
    }
    ncplane_erase(world_plane_);
    if (range_ring_plane_) ncplane_erase(range_ring_plane_);
    if (entity_plane_) ncplane_erase(entity_plane_);
    ncplane_erase(hud_plane_);
    if (minimap_plane_) ncplane_erase(minimap_plane_); // plane not created; noop guard

    // Determine mode and camera focus
    SimulationMode current_mode = SimulationMode::STANDARD;
    bool is_paused = false;
    float sim_speed = 1.0f;
    auto state_view = registry_.view<SimulationStateComponent>();
    if (state_view.begin() != state_view.end()) {
        auto& state = state_view.get<SimulationStateComponent>(*state_view.begin());
        current_mode = state.mode;
        is_paused = state.is_paused;
        sim_speed = state.god_mode_tps;
    }

    int current_layer = 0;
    int cam_x = 0;
    int cam_y = 0;
    bool show_help = true;

    // --- Contextual Indicators [B.4] ---
    std::string context_label = "[OVERWORLD]";
    std::string room_label = "";
    RoomData current_room_aabb = {0, 0, 0, 0, RoomTag::VOID};
    bool in_room = false;

    if (current_mode == SimulationMode::GOD_MODE) {
        bool following = false;
        auto follow_view = registry_.view<GodModeFollowComponent>();
        if (follow_view.begin() != follow_view.end()) {
            auto target = follow_view.get<GodModeFollowComponent>(*follow_view.begin()).target;
            if (registry_.valid(target) && registry_.all_of<PositionComponent>(target)) {
                const auto& f_pos = registry_.get<PositionComponent>(target);
                cam_x = f_pos.x;
                cam_y = f_pos.y;
                current_layer = f_pos.layer_id;
                following = true;
                
                // Sync cursor to followed target
                auto g_view = registry_.view<GodCursorComponent>();
                if (g_view.begin() != g_view.end()) {
                    auto& gc = g_view.get<GodCursorComponent>(*g_view.begin());
                    gc.x = cam_x; gc.y = cam_y; gc.layer_id = current_layer;
                }
            }
        }

        if (!following) {
            auto cursor_view = registry_.view<GodCursorComponent>();
            if (cursor_view.begin() != cursor_view.end()) {
                auto& cursor = cursor_view.get<GodCursorComponent>(*cursor_view.begin());
                cam_x = cursor.x;
                cam_y = cursor.y;
                current_layer = cursor.layer_id;
            }
        }
        show_help = true; 

        if (state_view.begin() != state_view.end()) {
            auto& state = state_view.get<SimulationStateComponent>(*state_view.begin());
            if (state.is_inside_view && registry_.valid(state.focused_building)) {
                std::string b_name = "Building " + std::to_string(registry_.get<BuildingComponent>(state.focused_building).building_id);
                if (registry_.all_of<NameComponent>(state.focused_building)) {
                    b_name = registry_.get<NameComponent>(state.focused_building).name;
                }
                context_label = "[GOD: " + b_name + "]";
            } else {
                context_label = "[GOD: SURVEILLANCE]";
            }
        }
    } else {
        auto player_view = registry_.view<PlayerComponent, PositionComponent, PlayerCurrentLayerComponent, HUDComponent>();
        for (auto entity : player_view) {
            const auto& pos = player_view.get<PositionComponent>(entity);
            current_layer = player_view.get<PlayerCurrentLayerComponent>(entity).current_z;
            show_help = player_view.get<HUDComponent>(entity).show_controls_help;
            cam_x = pos.x;
            cam_y = pos.y;

            if (registry_.all_of<InteriorStateComponent>(entity)) {
                const auto& interior_state = registry_.get<InteriorStateComponent>(entity);
                if (registry_.valid(interior_state.building_entity) && registry_.all_of<BuildingComponent>(interior_state.building_entity)) {
                    std::string b_name = "Building " + std::to_string(registry_.get<BuildingComponent>(interior_state.building_entity).building_id);
                    if (registry_.all_of<NameComponent>(interior_state.building_entity)) {
                        b_name = registry_.get<NameComponent>(interior_state.building_entity).name;
                    }
                    context_label = "[INTERIOR: " + b_name + " L" + std::to_string(current_layer) + "]";
                    
                    // Room logic
                    if (registry_.all_of<BuildingInteriorComponent>(interior_state.building_entity)) {
                        const auto& interior = registry_.get<BuildingInteriorComponent>(interior_state.building_entity);
                        for (const auto& room : interior.rooms) {
                            if (pos.x >= room.x && pos.x < room.x + room.width &&
                                pos.y >= room.y && pos.y < room.y + room.height) {
                                room_label = "[ROOM: " + room_tag_to_string(room.tag) + "]";
                                current_room_aabb = room;
                                in_room = true;
                                break;
                            }
                        }
                    }
                }
            }
            break;
        }
    }

    if (inventory_visible_) {
        ncplane_erase(inventory_plane_);
        ncplane_set_fg_rgb(inventory_plane_, 0x00FFFF);
        ncplane_putstr_yx(inventory_plane_, 0, 1, "[ INVENTORY ]");
        
        auto player_inv_view = registry_.view<PlayerComponent, InventoryComponent, HUDComponent>();
        for (auto p_ent : player_inv_view) {
            const auto& inv = player_inv_view.get<InventoryComponent>(p_ent);
            const auto& hud = player_inv_view.get<HUDComponent>(p_ent);
            
            if (inv.contained_items.empty()) {
                ncplane_set_fg_rgb(inventory_plane_, 0xAAAAAA);
                ncplane_putstr_yx(inventory_plane_, 2, 1, "(empty)");
            } else {
                int row = 2;
                for (size_t i = 0; i < inv.contained_items.size(); ++i) {
                    auto item_ent = inv.contained_items[i];
                    std::string item_name = "Unknown Item";
                    if (registry_.valid(item_ent) && registry_.all_of<ItemComponent>(item_ent)) {
                        item_name = registry_.get<ItemComponent>(item_ent).name;
                    }
                    
                    if (static_cast<int>(i) == hud.selected_inventory_index) {
                        ncplane_set_fg_rgb(inventory_plane_, 0xFFFF00);
                        ncplane_printf_yx(inventory_plane_, row++, 1, "> %s", item_name.c_str());
                    } else {
                        ncplane_set_fg_rgb(inventory_plane_, 0xAAAAAA);
                        ncplane_printf_yx(inventory_plane_, row++, 1, "  %s", item_name.c_str());
                    }
                    if (row >= 9) break;
                }
            }
        }
        ncplane_set_fg_rgb(inventory_plane_, 0x00FFFF);
        ncplane_putstr_yx(inventory_plane_, 9, 1, "ENTR:Use B:Close ARW:Nav");
    }

    unsigned view_rows, view_cols;
    ncplane_dim_yx(world_plane_, &view_rows, &view_cols);
    
    int offset_x = (int)view_cols / 2 - cam_x;
    int offset_y = (int)view_rows / 2 - cam_y;

    auto hud_view = registry_.view<HUDComponent>();
    for (auto entity : hud_view) {
        auto& hud = hud_view.get<HUDComponent>(entity);
        
        // --- Decrement timers [C.2] ---
        if (hud.held_item_flash_timer > 0.0f) {
            hud.held_item_flash_timer -= (float)delta_time;
            if (hud.held_item_flash_timer < 0.0f) hud.held_item_flash_timer = 0.0f;
        }

        if (current_mode == SimulationMode::GOD_MODE) {
            ncplane_set_fg_rgb(hud_plane_, 0xFFFF00);
            ncplane_printf_yx(hud_plane_, 0, 0, "MODE: %s  [%s]", context_label.c_str(), is_paused ? "PAUSED" : "RUNNING");
            ncplane_printf_yx(hud_plane_, 1, 0, "LAYER: %d  TPS: %.1f", current_layer, sim_speed);

            // God Mode Overview (Global Telemetry)
            auto npc_view = registry_.view<NPCComponent>();
            auto citizen_view = registry_.view<CitizenComponent>();
            auto xeno_view = registry_.view<XenoComponent>();
            ncplane_set_fg_rgb(hud_plane_, 0xAAAAAA);
            ncplane_printf_yx(hud_plane_, 2, 0, "CITIZENS: %zu | AGENTS: %zu", citizen_view.size(), npc_view.size());
            ncplane_printf_yx(hud_plane_, 3, 0, "XENO DETECTED: %zu", xeno_view.size());
            
            ncplane_set_fg_rgb(hud_plane_, 0x00FFFF);
            ncplane_putstr_yx(hud_plane_, 0, 35, "GOD CONTROLS [G:Exit]");
            ncplane_putstr_yx(hud_plane_, 1, 35, "WASD:Cursor  P/SPC:Pause");
            ncplane_putstr_yx(hud_plane_, 2, 35, "+/-:Speed    </>:Floor");
            ncplane_putstr_yx(hud_plane_, 3, 35, "i:Phys I:Bio c:Cogn f:Econ t:Pol v:Crisis");

            // --- God Mode Hover Info [B.3] ---
            auto cursor_v = registry_.view<GodCursorComponent>();
            if (cursor_v.begin() != cursor_v.end()) {
                auto& cursor = cursor_v.get<GodCursorComponent>(*cursor_v.begin());
                auto b_view = registry_.view<BuildingComponent, PositionComponent, SizeComponent>();
                for (auto b_ent : b_view) {
                    const auto& b_pos = b_view.get<PositionComponent>(b_ent);
                    const auto& b_size = b_view.get<SizeComponent>(b_ent);
                    if (cursor.layer_id == b_pos.layer_id &&
                        cursor.x >= b_pos.x && cursor.x < b_pos.x + b_size.width &&
                        cursor.y >= b_pos.y && cursor.y < b_pos.y + b_size.height) {
                        
                        const auto& b_comp = b_view.get<BuildingComponent>(b_ent);
                        ncplane_set_fg_rgb(hud_plane_, 0x00FF00);
                        ncplane_printf_yx(hud_plane_, 4, 0, "BUILDING: ID %u | ZONE: %d | HEIGHT: %d", 
                                          b_comp.building_id, (int)b_comp.zone_type, b_comp.height);
                        
                        if (registry_.all_of<BuildingInteriorComponent>(b_ent)) {
                            const auto& interior = registry_.get<BuildingInteriorComponent>(b_ent);
                            ncplane_printf_yx(hud_plane_, 5, 0, "ROOMS: %zu | GEN: %s [G:Inspect]", 
                                              interior.rooms.size(), interior.is_generated ? "Y" : "N");
                        }
                        break;
                    }
                }
            }
        } else {
            ncplane_set_fg_rgb(hud_plane_, 0x00FFFF);
            ncplane_printf_yx(hud_plane_, 0, 0, "HP: %3.0f%%  CR: %d  L: %d", hud.health, hud.credits, current_layer);

            // [I.5] Wanted Level Display & Notoriety
            int w_col = 45;
            for (auto const& [faction, level] : hud.faction_wanted_levels) {
                if (level > 0) {
                    uint32_t f_color = 0xFFFFFF;
                    if (faction == "CONSENSUS") f_color = 0x5555FF; // Blue
                    else if (faction == "ENTROPIC_DRIFT") f_color = 0xFF5555; // Red
                    else if (faction == "SILICON_MAW") f_color = 0x55FF55; // Green
                    else if (faction == "VOID_WALKERS") f_color = 0xAA55FF; // Purple
                    else if (faction == "SYNDICATE") f_color = 0xFFCC33; // Gold
                    else if (faction == "CITY_WATCH") f_color = 0xAAAAAA; // Grey

                    ncplane_set_fg_rgb(hud_plane_, f_color);
                    std::string f_label = faction.substr(0, 3);
                    ncplane_printf_yx(hud_plane_, 0, w_col, "[%s:", f_label.c_str());
                    w_col += 5;
                    for (int i = 0; i < 5; ++i) {
                        if (i < level) ncplane_putchar_yx(hud_plane_, 0, w_col++, '*');
                        else ncplane_putchar_yx(hud_plane_, 0, w_col++, '.');
                    }
                    ncplane_putchar_yx(hud_plane_, 0, w_col++, ']');
                    w_col += 2;
                }
            }
            
            // Global Notoriety Bar
            if (hud.global_notoriety > 0) {
                ncplane_set_fg_rgb(hud_plane_, 0xFF00FF);
                ncplane_printf_yx(hud_plane_, 1, 55, "NOTORIETY: ");
                int bar_len = 10;
                int filled = (int)((hud.global_notoriety / 100.0f) * (float)bar_len);
                for (int i = 0; i < bar_len; ++i) {
                    if (i < filled) ncplane_putchar_yx(hud_plane_, 1, 66 + i, '=');
                    else ncplane_putchar_yx(hud_plane_, 1, 66 + i, '-');
                }
            }

            // [E.1] Interaction Mode Indicator
            if (registry_.all_of<PlayerInteractionComponent>(entity)) {
                auto mode = registry_.get<PlayerInteractionComponent>(entity).current_mode;
                std::string mode_label = "OBSERVE";
                uint32_t mode_color = 0xFFFFFF; // White
                if (mode == InteractionMode::SPEAK) { mode_label = "SPEAK"; mode_color = 0x0000FF; } // Blue
                else if (mode == InteractionMode::TRADE) { mode_label = "TRADE"; mode_color = 0xFFD700; } // Gold

                ncplane_set_fg_rgb(hud_plane_, mode_color);
                ncplane_printf_yx(hud_plane_, 1, 30, "[MODE: %s]", mode_label.c_str());
            }

            // Context Indicators [B.4]
            ncplane_set_fg_rgb(hud_plane_, 0x00FF00);
            ncplane_putstr_yx(hud_plane_, 0, 30, context_label.c_str());
            if (!room_label.empty()) {
                ncplane_set_fg_rgb(hud_plane_, 0xFFA500);
                ncplane_putstr_yx(hud_plane_, 1, 30, room_label.c_str());
            }

            // [L.2] Economic Telemetry
            uint32_t econ_color = 0x00FF00; // GREEN
            if (hud.economy_status_label == "!!! MARKET CRASH !!!") econ_color = 0xFF0000; // RED
            else if (hud.economy_status_label == "VOLATILE") econ_color = 0xFFFF00; // YELLOW
            
            ncplane_set_fg_rgb(hud_plane_, econ_color);
            ncplane_printf_yx(hud_plane_, 1, 60, "ECON: %s", hud.economy_status_label.c_str());

            // --- [C.1] Held Item Slot ---
            ncplane_set_fg_rgb(hud_plane_, 0xAAAAAA);
            ncplane_putstr_yx(hud_plane_, 1, 0, "HELD: ");
            if (registry_.valid(hud.held_item)) {
                std::string item_name = "[unknown]";
                char item_glyph = '?';
                uint32_t item_color = 0xFFFFFF;

                if (registry_.all_of<ItemComponent>(hud.held_item)) {
                    item_name = registry_.get<ItemComponent>(hud.held_item).name;
                    if (item_name.length() > 12) item_name = item_name.substr(0, 11) + ".";
                }
                
                // Color-coding [C.2]
                if (registry_.all_of<ConsumableComponent>(hud.held_item)) {
                    item_color = 0x00FF00; // Green
                } else if (registry_.all_of<WeaponComponent>(hud.held_item)) {
                    item_color = 0xFF0000; // Red
                } else if (registry_.all_of<KeyComponent>(hud.held_item)) {
                    item_color = 0xFFFF00; // Yellow
                } else if (registry_.all_of<UsableComponent>(hud.held_item)) {
                    item_color = 0x00FFFF; // Cyan
                }

                if (registry_.all_of<RenderableComponent>(hud.held_item)) {
                    item_glyph = registry_.get<RenderableComponent>(hud.held_item).glyph;
                }

                // Flash effect [C.2]
                if (hud.held_item_flash_timer > 0.0f) {
                    ncplane_set_bg_rgb(hud_plane_, 0xFFFFFF);
                    ncplane_set_fg_rgb(hud_plane_, 0x000000);
                } else {
                    ncplane_set_bg_default(hud_plane_);
                    ncplane_set_fg_rgb(hud_plane_, item_color);
                }
                
                ncplane_printf_yx(hud_plane_, 1, 6, "[%c] %s", item_glyph, item_name.c_str());
                ncplane_set_bg_default(hud_plane_);

                // Durability Bar [C.2]
                if (registry_.all_of<DurabilityComponent>(hud.held_item)) {
                    const auto& dur = registry_.get<DurabilityComponent>(hud.held_item);
                    int bar_len = 5;
                    int filled = (int)((dur.current / dur.max) * (float)bar_len);
                    ncplane_set_fg_rgb(hud_plane_, 0x555555);
                    ncplane_putstr_yx(hud_plane_, 1, 6 + 4 + (int)item_name.length(), " (");
                    for (int i = 0; i < bar_len; ++i) {
                        if (i < filled) ncplane_set_fg_rgb(hud_plane_, 0x00FF00);
                        else ncplane_set_fg_rgb(hud_plane_, 0x555555);
                        ncplane_putchar_yx(hud_plane_, 1, 6 + 4 + (int)item_name.length() + 2 + i, '|');
                    }
                    ncplane_set_fg_rgb(hud_plane_, 0x555555);
                    ncplane_putchar_yx(hud_plane_, 1, 6 + 4 + (int)item_name.length() + 2 + bar_len, ')');
                }
            } else {
                ncplane_set_fg_rgb(hud_plane_, 0x555555);
                ncplane_putstr_yx(hud_plane_, 1, 6, "[empty]");
            }
            // --- End [C.1] ---

            if (show_help) {
                // Controls (left column)
                ncplane_set_fg_rgb(hud_plane_, 0xFFFF00);
                ncplane_putstr_yx(hud_plane_, 0, 35, "WASD:Move  E/Click:Interact");
                ncplane_putstr_yx(hud_plane_, 1, 35, "B:Inventory  Q:Quit  ?:Legend");
                // ASCII Legend (right column)
                ncplane_set_fg_rgb(hud_plane_, 0xAAAAAA);
                ncplane_putstr_yx(hud_plane_, 2, 35, "LEGEND:");
                ncplane_set_fg_rgb(hud_plane_, 0xFFFFFF); ncplane_putstr_yx(hud_plane_, 2, 42, "@ You");
                ncplane_set_fg_rgb(hud_plane_, 0x00FF00); ncplane_putstr_yx(hud_plane_, 3, 35, "n NPC");
                ncplane_set_fg_rgb(hud_plane_, 0xFFAA00); ncplane_putstr_yx(hud_plane_, 3, 41, "K Kiosk");
                ncplane_set_fg_rgb(hud_plane_, 0xFFFF00); ncplane_putstr_yx(hud_plane_, 4, 35, "+ Door");
                ncplane_set_fg_rgb(hud_plane_, 0xFF00FF); ncplane_putstr_yx(hud_plane_, 4, 41, "E Elev");
                ncplane_set_fg_rgb(hud_plane_, 0xAAAAAA); ncplane_putstr_yx(hud_plane_, 5, 35, "# Bld  . Road");
                ncplane_set_fg_rgb(hud_plane_, 0x55FFFF); ncplane_putstr_yx(hud_plane_, 5, 49, "* Item");
            }

            int row = 2;
            for (const auto& notif : hud.notifications) {
                ncplane_putstr_yx(hud_plane_, row++, 0, notif.c_str());
                if (row >= 6) break;
            }
        }

        // --- Time Display ---
    }

    // --- Visibility & Memory state ---
    const VisibilityComponent* player_vis = nullptr;
    const MemoryComponent* player_mem = nullptr;
    auto player_vis_view = registry_.view<PlayerComponent, VisibilityComponent>();
    if (player_vis_view.begin() != player_vis_view.end()) {
        player_vis = &player_vis_view.get<VisibilityComponent>(*player_vis_view.begin());
    }
    auto player_mem_view = registry_.view<PlayerComponent, MemoryComponent>();
    if (player_mem_view.begin() != player_mem_view.end()) {
        player_mem = &player_mem_view.get<MemoryComponent>(*player_mem_view.begin());
    }

    auto is_visible = [&](const PositionComponent& p) {
        if (current_mode == SimulationMode::GOD_MODE) return true; 
        if (!player_vis) return true; 
        return player_vis->visible_tiles.count(p) > 0;
    };

    auto render_at = [&](struct ncplane* target_plane, int x, int y, char glyph, uint32_t color) {
        int screen_x = x + offset_x;
        int screen_y = y + offset_y;
        if (screen_x >= 0 && screen_x < (int)view_cols && screen_y >= 0 && screen_y < (int)view_rows) {
            // [L.5] Apply Power Grid Failure shifts
            float power_mult = 1.0f;
            if (current_mode != SimulationMode::GOD_MODE) {
                // Determine chunk for (x, y)
                // Assuming 40x40 chunks as per roadmap Phase 3.1
                int cx = x / 40;
                int cy = y / 40;
                
                auto grid_view = registry_.view<PowerGridComponent, ChunkComponent>();
                for (auto g_ent : grid_view) {
                    const auto& chunk = grid_view.get<ChunkComponent>(g_ent);
                    if (chunk.chunk_x == cx && chunk.chunk_y == cy) {
                        power_mult = grid_view.get<PowerGridComponent>(g_ent).power_level;
                        break;
                    }
                }
            }

            // Apply time-of-day color shifts
            if (current_mode != SimulationMode::GOD_MODE) {
                auto weather_v = registry_.view<WeatherComponent>();
                if (weather_v.begin() != weather_v.end()) {
                    const auto& weather = weather_v.get<WeatherComponent>(*weather_v.begin());
                    float darkness = 1.0f;
                    if (weather.time_of_day == TimeOfDay::NIGHT) {
                        darkness = 0.3f;
                    } else if (weather.time_of_day == TimeOfDay::DAWN || weather.time_of_day == TimeOfDay::DUSK) {
                        darkness = 0.7f;
                    }

                    // Combined darkness from time and power failure
                    // Buildings might have emergency lights (min 0.1 brightness)
                    float final_mult = std::max(0.1f, darkness * power_mult);

                    uint32_t r = (color >> 16) & 0xFF;
                    uint32_t g = (color >> 8) & 0xFF;
                    uint32_t b = color & 0xFF;

                    if (weather.time_of_day == TimeOfDay::NIGHT) {
                         // Blue tint for night
                         r = (uint32_t)((float)r * 0.2f * power_mult);
                         g = (uint32_t)((float)g * 0.3f * power_mult);
                         b = (uint32_t)((float)b * 0.6f * power_mult);
                    } else {
                         r = (uint32_t)((float)r * final_mult);
                         g = (uint32_t)((float)g * final_mult);
                         b = (uint32_t)((float)b * final_mult);
                    }
                    color = (r << 16) | (g << 8) | b;
                }
            }

            ncplane_set_fg_rgb(target_plane, color);
            ncplane_putchar_yx(target_plane, screen_y, screen_x, glyph);
        }
    };

    // Viewport bounds for culling
    int vp_x_min = cam_x - (int)view_cols / 2 - 1;
    int vp_x_max = cam_x + (int)view_cols / 2 + 1;
    int vp_y_min = cam_y - (int)view_rows / 2 - 1;
    int vp_y_max = cam_y + (int)view_rows / 2 + 1;

    // 1. Render Memory (Fog of War) - Only in Standard Mode (viewport-culled)
    if (current_mode == SimulationMode::STANDARD && player_mem) {
        for (const auto& [m_pos, m_tile] : player_mem->remembered_tiles) {
            if (m_pos.layer_id != current_layer) continue;
            if (m_pos.x < vp_x_min || m_pos.x > vp_x_max || m_pos.y < vp_y_min || m_pos.y > vp_y_max) continue;
            if (is_visible(m_pos)) continue; 

            uint32_t base_color = parse_hex_color(m_tile.color);
            uint32_t r = (base_color >> 16) & 0xFF;
            uint32_t g = (base_color >> 8) & 0xFF;
            uint32_t b = base_color & 0xFF;
            uint32_t dimmed_color = ((r / 4) << 16) | ((g / 4) << 8) | (b / 4);

            render_at(world_plane_, m_pos.x, m_pos.y, m_tile.glyph, dimmed_color);
        }
    }

    // 2. Render Terrain (with viewport culling)

    auto terrain_view = registry_.view<TerrainComponent, RenderableComponent, PositionComponent>();
    for (auto entity : terrain_view) {
        const auto& pos = terrain_view.get<PositionComponent>(entity);
        if (pos.layer_id != current_layer) continue;
        if (pos.x < vp_x_min || pos.x > vp_x_max || pos.y < vp_y_min || pos.y > vp_y_max) continue;
        if (!is_visible(pos)) continue;
        const auto& render = terrain_view.get<RenderableComponent>(entity);
        
        uint32_t color = parse_hex_color(render.color);
        bool has_physics = registry_.all_of<Layer0PhysicsComponent>(entity);
        if (has_physics) {
            const auto& phys = registry_.get<Layer0PhysicsComponent>(entity);
            // Terrain keeps authored palette; thermal tinting is reserved for non-terrain entities.
            if (phys.temperature_celsius > 800.0f) {
                static thread_local std::mt19937 fgen(666);
                if (std::uniform_real_distribution<>(0,1)(fgen) < 0.2) color = 0xFFFF00; 
            }
        }
        
        render_at(world_plane_, pos.x, pos.y, render.glyph, color);
    }

    // --- [K.4] Render Graffiti ---
    auto graffiti_view = registry_.view<GraffitiComponent, PositionComponent>();
    for (auto entity : graffiti_view) {
        const auto& pos = graffiti_view.get<PositionComponent>(entity);
        if (pos.layer_id != current_layer) continue;
        if (pos.x < vp_x_min || pos.x > vp_x_max || pos.y < vp_y_min || pos.y > vp_y_max) continue;
        if (!is_visible(pos)) continue;
        
        const auto& graffiti = graffiti_view.get<GraffitiComponent>(entity);
        
        // Faction Color Logic
        uint32_t color = graffiti.color;
        
        // Density affects brightness (0.0 to 1.0)
        uint32_t r = (color >> 16) & 0xFF;
        uint32_t g = (color >> 8) & 0xFF;
        uint32_t b = color & 0xFF;
        r = (uint32_t)(r * graffiti.density);
        g = (uint32_t)(g * graffiti.density);
        b = (uint32_t)(b * graffiti.density);
        color = (r << 16) | (g << 8) | b;

        render_at(world_plane_, pos.x, pos.y, graffiti.glyph, color);
    }

    // --- [E.2] Render Range Ring ---
    if (range_ring_plane_) {
        // Find player and interaction mode
        auto player_inter_view = registry_.view<PlayerComponent, PositionComponent, PlayerInteractionComponent>();
        for (auto p_ent : player_inter_view) {
            const auto& p_pos = player_inter_view.get<PositionComponent>(p_ent);
            const auto& p_inter = player_inter_view.get<PlayerInteractionComponent>(p_ent);
            
            if (p_pos.layer_id == current_layer) {
                int range = 6;
                // High-visibility orange for player in overworld
                render_at(world_plane_, p_pos.x, p_pos.y, '@', 0xFFA500); // [MOD] Vibrant Orange
                std::string ring_color = Colors::RANGE_OBSERVE;
                if (p_inter.current_mode == InteractionMode::SPEAK) {
                    range = 3;
                    ring_color = Colors::RANGE_SPEAK;
                } else if (p_inter.current_mode == InteractionMode::TRADE) {
                    range = 1;
                    ring_color = Colors::RANGE_TRADE;
                }

                uint32_t color = parse_hex_color(ring_color);
                
                // Set low opacity for the ring (B.4)
                uint64_t ring_channels = 0;
                ncchannels_set_fg_rgb(&ring_channels, color & 0xFFFFFF);
                ncchannels_set_fg_alpha(&ring_channels, NCALPHA_BLEND);
                ncchannels_set_bg_alpha(&ring_channels, NCALPHA_TRANSPARENT);
                ncplane_set_base(range_ring_plane_, "", 0, ring_channels);
                
                // Draw square border into range_ring_plane_ (occluded by walls [B.4])
                for (int dx = -range; dx <= range; ++dx) {
                    for (int dy = -range; dy <= range; ++dy) {
                        if (std::abs(dx) == range || std::abs(dy) == range) {
                            int tx = p_pos.x + dx;
                            int ty = p_pos.y + dy;
                            
                            // Simple occlusion check: is there a wall/window here?
                            bool occluded = false;
                            auto terrain_view_ring = registry_.view<PositionComponent, TerrainComponent>();
                            for (auto t_ent : terrain_view_ring) {
                                const auto& tp = terrain_view_ring.get<PositionComponent>(t_ent);
                                if (tp.x == tx && tp.y == ty && tp.layer_id == current_layer) {
                                    auto type = terrain_view_ring.get<TerrainComponent>(t_ent).type;
                                    if (type == TerrainType::WALL || type == TerrainType::WINDOW) {
                                        occluded = true;
                                    }
                                    break;
                                }
                            }
                            
                            if (!occluded) {
                                render_at(range_ring_plane_, tx, ty, '.', color);
                            }
                        }
                    }
                }
            }
            break; 
        }
    }

    // 3. Render Entities (with viewport culling)
    auto entity_view = registry_.view<RenderableComponent, PositionComponent>(entt::exclude<TerrainComponent>);
    for (auto entity : entity_view) {
        const auto& pos = entity_view.get<PositionComponent>(entity);
        if (pos.layer_id != current_layer) continue;
        if (pos.x < vp_x_min || pos.x > vp_x_max || pos.y < vp_y_min || pos.y > vp_y_max) continue;
        if (!is_visible(pos)) continue;
        const auto& render = entity_view.get<RenderableComponent>(entity);

        char glyph = render.glyph;
        uint32_t color = parse_hex_color(render.color);

        // [J.1] Age-Based Visual Metaphor
        if (registry_.all_of<AgeComponent>(entity)) {
            const auto& age = registry_.get<AgeComponent>(entity);
            
            // Glyph Metaphor
            if (age.stage == LifeStage::INFANT) {
                glyph = '.'; // Metaphor: Small, proto-agent
            } else if (age.stage == LifeStage::CHILD) {
                glyph = (glyph >= 'A' && glyph <= 'Z') ? (char)(glyph + 32) : glyph; // Force lowercase
            } else if (age.stage == LifeStage::ANCIENT) {
                // Xeno-Ancient Metaphor: specialized Omega or shimmering
                if (registry_.all_of<XenoComponent>(entity)) glyph = (char)224; // Greek Alpha/Omega style if supported
            }

            // Color Metaphor (Brightness/Saturation shifts)
            uint32_t r = (color >> 16) & 0xFF;
            uint32_t g = (color >> 8) & 0xFF;
            uint32_t b = color & 0xFF;

            if (age.stage == LifeStage::INFANT || age.stage == LifeStage::CHILD) {
                // Vibrant, high saturation for youth
                r = std::min(255u, r + 50); g = std::min(255u, g + 50); b = std::min(255u, b + 50);
            } else if (age.stage == LifeStage::ELDER) {
                // Desaturated/Dimmed for elders
                float grey_factor = 0.5f;
                uint32_t grey = (uint32_t)((r + g + b) / 3);
                r = (uint32_t)(r * (1.0f - grey_factor) + grey * grey_factor);
                g = (uint32_t)(g * (1.0f - grey_factor) + grey * grey_factor);
                b = (uint32_t)(b * (1.0f - grey_factor) + grey * grey_factor);
                // Further dim
                r /= 2; g /= 2; b /= 2;
            } else if (age.stage == LifeStage::ANCIENT) {
                // Mythic shimmering (flicker)
                static thread_local std::mt19937 flicker_gen(42);
                if (std::uniform_real_distribution<>(0, 1)(flicker_gen) < 0.3f) {
                    r = 255; g = 255; b = 255; // White flash
                }
            }
            color = (r << 16) | (g << 8) | b;
        }

        if (registry_.all_of<Layer0PhysicsComponent>(entity)) {
            const auto& phys = registry_.get<Layer0PhysicsComponent>(entity);
            color = parse_hex_color(map_temperature_to_color(phys.temperature_celsius, render.color));
            if (phys.temperature_celsius > 800.0f) {
                static thread_local std::mt19937 fgen(777);
                if (std::uniform_real_distribution<>(0,1)(fgen) < 0.2) color = 0xFFFF00; 
            }
        }

        if (registry_.all_of<SizeComponent>(entity)) {
            const auto& size = registry_.get<SizeComponent>(entity);
            for (int dx = 0; dx < size.width; ++dx) {
                for (int dy = 0; dy < size.height; ++dy) {
                    if (is_visible(PositionComponent(pos.x + dx, pos.y + dy, current_layer))) {
                        render_at(entity_plane_, pos.x + dx, pos.y + dy, glyph, color);
                    }
                }
            }
        } else {
            render_at(entity_plane_, pos.x, pos.y, glyph, color);
        }

        // --- Render Tags [D.3] ---
        if (current_mode == SimulationMode::GOD_MODE && registry_.all_of<TaggedComponent>(entity)) {
            const auto& tag = registry_.get<TaggedComponent>(entity);
            int sx = pos.x + offset_x;
            int sy = pos.y + offset_y;
            if (sx >= 0 && sx < (int)view_cols && sy >= 0 && sy < (int)view_rows) {
                ncplane_set_fg_rgb(entity_plane_, 0x00FFFF);
                ncplane_set_styles(entity_plane_, NCSTYLE_BOLD);
                ncplane_putstr_yx(entity_plane_, sy - 1, sx, tag.tag_label.c_str());
                ncplane_set_styles(entity_plane_, NCSTYLE_NONE);
            }
        }
    }

    // --- [N.2] Render Broadcast Pulses ---
    for (auto it = active_pulses_.begin(); it != active_pulses_.end(); ) {
        if (it->layer == current_layer) {
            int radius = it->current_tick;
            if (radius > it->radius) radius = it->radius;
            
            char glyph = '.';
            if (it->current_tick < 2) glyph = '.';
            else if (it->current_tick < 4) glyph = ',';
            else if (it->current_tick < 6) glyph = '~';
            else if (it->current_tick < 8) glyph = '*';
            else glyph = ' ';

            // Draw a circle of pulses
            for (int i = 0; i < 360; i += 10) {
                float theta = (float)i * 3.14159f / 180.0f;
                int px = it->x + (int)(radius * std::cos(theta));
                int py = it->y + (int)(radius * std::sin(theta));
                
                if (px >= vp_x_min && px <= vp_x_max && py >= vp_y_min && py <= vp_y_max) {
                    if (is_visible(PositionComponent(px, py, current_layer))) {
                        render_at(entity_plane_, px, py, glyph, it->color);
                    }
                }
            }
        }
        
        it->current_tick++;
        if (it->current_tick >= it->max_ticks) {
            it = active_pulses_.erase(it);
        } else {
            ++it;
        }
    }

    // --- Environmental Overlays (River Cooling & Weather) ---
    // 1. River Cooling Field - Only render mist near visible water within viewport
    {
        // Collect visible water positions efficiently
        auto water_view = registry_.view<PositionComponent, Layer0PhysicsComponent, RenderableComponent>();
        static thread_local std::mt19937 mist_gen(1337);
        std::uniform_real_distribution<> dis(0, 1);

        int vp_min_x = cam_x - (int)view_cols / 2 - 3;
        int vp_max_x = cam_x + (int)view_cols / 2 + 3;
        int vp_min_y = cam_y - (int)view_rows / 2 - 3;
        int vp_max_y = cam_y + (int)view_rows / 2 + 3;

        for (auto water_ent : water_view) {
            const auto& w_phys = water_view.get<Layer0PhysicsComponent>(water_ent);
            if (w_phys.material != MaterialType::WATER) continue;
            const auto& w_pos = water_view.get<PositionComponent>(water_ent);
            if (w_pos.layer_id != current_layer) continue;
            // Skip water tiles completely outside viewport
            if (w_pos.x < vp_min_x || w_pos.x > vp_max_x || w_pos.y < vp_min_y || w_pos.y > vp_max_y) continue;

            for (int dx = -3; dx <= 3; ++dx) {
                for (int dy = -3; dy <= 3; ++dy) {
                    if (dx == 0 && dy == 0) continue;
                    int dist = std::abs(dx) + std::abs(dy);
                    if (dist > 3) continue;
                    if (dis(mist_gen) < 0.15f / (float)dist) {
                        PositionComponent mist_pos(w_pos.x + dx, w_pos.y + dy, current_layer);
                        if (!is_visible(mist_pos)) continue;
                        render_at(entity_plane_, w_pos.x + dx, w_pos.y + dy, (dis(mist_gen) < 0.5 ? '~' : ','), 
                                  parse_hex_color(Colors::COOLING_MIST));
                    }
                }
            }
        }
    }

    // 2. Weather Overlay
    auto weather_v = registry_.view<WeatherComponent>();
    if (weather_v.begin() != weather_v.end()) {
        const auto& weather = weather_v.get<WeatherComponent>(*weather_v.begin());
        if (weather.state != WeatherState::CLEAR) {
            static thread_local std::mt19937 gen(42);
            std::uniform_real_distribution<> dis(0, 1);
            std::uniform_int_distribution<> disX(0, (int)view_cols-1);
            std::uniform_int_distribution<> disY(0, (int)view_rows-1);

            float intensity = weather.intensity;
            int particle_count = (int)(intensity * 50.0f);
            
            char w_glyph = ' ';
            uint32_t w_color = 0xFFFFFF;
            bool do_weather = true;

            switch(weather.state) {
                case WeatherState::RAIN: w_glyph = '\''; w_color = parse_hex_color(Colors::RAIN); break;
                case WeatherState::HEAVY_RAIN: w_glyph = '|'; w_color = parse_hex_color(Colors::RAIN); particle_count *= 2; break;
                case WeatherState::ACID_RAIN: w_glyph = ','; w_color = parse_hex_color(Colors::ACID_RAIN); break;
                case WeatherState::SMOG: w_glyph = '.'; w_color = parse_hex_color(Colors::SMOG); break;
                case WeatherState::ELECTRICAL_STORM: 
                    if (dis(gen) < 0.05f) {
                        w_glyph = (dis(gen) < 0.5 ? 'Z' : '/');
                        w_color = parse_hex_color(Colors::LIGHTNING);
                    } else { do_weather = false; }
                    break;
                default: do_weather = false; break;
            }

            if (do_weather) {
                for (int i = 0; i < particle_count; ++i) {
                    int sx = disX(gen);
                    int sy = disY(gen);
                    ncplane_set_fg_rgb(entity_plane_, w_color);
                    ncplane_putchar_yx(entity_plane_, sy, sx, w_glyph);
                }
            }
        }
    }

    // --- [L.3] Environmental Hazard Overlay ---
    auto hazard_view_ren = registry_.view<EnvironmentalHazardComponent>();
    if (!hazard_view_ren.empty()) {
        const auto& hazard = hazard_view_ren.get<EnvironmentalHazardComponent>(hazard_view_ren.front());
        if (hazard.is_active) {
            static thread_local std::mt19937 hgen(888);
            std::uniform_int_distribution<> h_disX(0, (int)view_cols-1);
            std::uniform_int_distribution<> h_disY(0, (int)view_rows-1);
            
            int p_count = (int)(hazard.toxicity_level * 40.0f);
            for (int i = 0; i < p_count; ++i) {
                int sx = h_disX(hgen);
                int sy = h_disY(hgen);
                ncplane_set_fg_rgb(entity_plane_, 0xADFF2F); // GreenYellow
                ncplane_putchar_yx(entity_plane_, sy, sx, (i % 3 == 0 ? 'x' : (i % 3 == 1 ? '`' : 'v')));
            }
        }
    }

    // 4. Render Interior Overlay [B.3]
    if (state_view.begin() != state_view.end()) {
        auto& state = state_view.get<SimulationStateComponent>(*state_view.begin());
        if (state.mode == SimulationMode::GOD_MODE && state.is_inside_view && registry_.valid(state.focused_building)) {
            ncplane_erase(interior_overlay_plane_);
            ncplane_move_top(interior_overlay_plane_);
            
            // Draw Border
            uint64_t border_channels = 0;
            ncchannels_set_fg_rgb(&border_channels, 0x00FF00);
            ncchannels_set_bg_rgb(&border_channels, 0x000000);
            ncplane_set_base(interior_overlay_plane_, " ", 0, border_channels);
            ncplane_cursor_move_yx(interior_overlay_plane_, 0, 0);
            ncplane_perimeter_double(interior_overlay_plane_, 0, border_channels, 0);
            
            ncplane_set_fg_rgb(interior_overlay_plane_, 0xFFFF00);
            ncplane_putstr_yx(interior_overlay_plane_, 0, 2, "[ INTERIOR INSPECTION ]");
            
            const auto& interior = registry_.get<BuildingInteriorComponent>(state.focused_building);
            const auto& b_comp = registry_.get<BuildingComponent>(state.focused_building);
            ncplane_set_fg_rgb(interior_overlay_plane_, 0x00FFFF);
            ncplane_printf_yx(interior_overlay_plane_, 1, 2, "Building: %u | Floor: %d/%d | Rooms: %zu", 
                              b_comp.building_id, state.focus_floor, b_comp.height - 1, interior.rooms.size());
            ncplane_putstr_yx(interior_overlay_plane_, 1, 35, "</>:Nav  ESC:Exit");

            // Render rooms and entities of the focused floor into the overlay
            int base_layer = 1000 + b_comp.building_id * 10;
            int target_layer = base_layer + state.focus_floor;
            
            auto int_ent_view = registry_.view<PositionComponent, RenderableComponent>();
            for (auto ent : int_ent_view) {
                const auto& pos = int_ent_view.get<PositionComponent>(ent);
                if (pos.layer_id == target_layer) {
                    const auto& render = int_ent_view.get<RenderableComponent>(ent);
                    int px = pos.x + 4; // offset
                    int py = pos.y + 3;
                    // Fix clipping for larger buildings (expand bounds check)
                    if (px >= 0 && px < 50 && py >= 0 && py < 20) {
                        ncplane_set_fg_rgb(interior_overlay_plane_, parse_hex_color(render.color));
                        ncplane_putchar_yx(interior_overlay_plane_, py, px, render.glyph);
                    }
                }
            }
        } else {
            ncplane_move_below(interior_overlay_plane_, world_plane_);
        }
    }

    // 6. Render Crisis Dashboard [L.6]
    auto dash_view = registry_.view<CrisisDashboardComponent>();
    if (dash_view.begin() != dash_view.end()) {
        auto& dash = dash_view.get<CrisisDashboardComponent>(*dash_view.begin());
        if (dash.visible) {
            ncplane_erase(crisis_dashboard_plane_);
            ncplane_move_top(crisis_dashboard_plane_);
            
            // Border
            uint64_t border_channels = 0;
            ncchannels_set_fg_rgb(&border_channels, 0x00FF00);
            ncchannels_set_bg_rgb(&border_channels, 0x050510);
            ncplane_perimeter_double(crisis_dashboard_plane_, 0, border_channels, 0);
            
            ncplane_set_fg_rgb(crisis_dashboard_plane_, 0xFFFF00);
            ncplane_putstr_yx(crisis_dashboard_plane_, 0, 2, " GOD MODE: SYSTEMIC CRISIS DASHBOARD ");
            
            // 1. Global Stress Meters
            ncplane_set_fg_rgb(crisis_dashboard_plane_, 0xAAAAAA);
            ncplane_putstr_yx(crisis_dashboard_plane_, 2, 2, "GLOBAL SIMULATION STRESS:");
            
            auto draw_meter = [&](int y, int x, const std::string& label, const std::vector<float>& history, uint32_t color) {
                float current = history.empty() ? 0.0f : history.back();
                ncplane_set_fg_rgb(crisis_dashboard_plane_, 0xAAAAAA);
                ncplane_printf_yx(crisis_dashboard_plane_, y, x, "%s:", label.c_str());
                
                int bar_len = 10;
                int filled = (int)(current * (float)bar_len);
                for (int i = 0; i < bar_len; ++i) {
                    if (i < filled) ncplane_set_fg_rgb(crisis_dashboard_plane_, color);
                    else ncplane_set_fg_rgb(crisis_dashboard_plane_, 0x333333);
                    ncplane_putchar_yx(crisis_dashboard_plane_, y, x + (int)label.length() + 2 + i, '#');
                }
                ncplane_set_fg_rgb(crisis_dashboard_plane_, color);
                ncplane_printf_yx(crisis_dashboard_plane_, y, x + (int)label.length() + 2 + bar_len + 1, "%3.0f%%", current * 100.0f);
                
                // Sparkline
                render_sparkline(crisis_dashboard_plane_, y, x + (int)label.length() + 2 + bar_len + 7, history, color);
            };

            draw_meter(4, 2, "ECONOMIC     ", dash.economic_stress_history, 0x00FFFF);
            draw_meter(5, 2, "POLITICAL    ", dash.political_stress_history, 0xFF00FF);
            draw_meter(6, 2, "BIOLOGICAL   ", dash.biological_stress_history, 0x00FF00);
            draw_meter(7, 2, "ENVIRONMENTAL", dash.environmental_stress_history, 0xFF8800);

            // 2. Active Crises
            ncplane_set_fg_rgb(crisis_dashboard_plane_, 0xFFFF00);
            ncplane_putstr_yx(crisis_dashboard_plane_, 9, 2, "ACTIVE CITY-WIDE CRISES:");
            
            auto crisis_v = registry_.view<CrisisComponent>();
            if (!crisis_v.empty()) {
                const auto& crisis_comp = crisis_v.get<CrisisComponent>(crisis_v.front());
                if (crisis_comp.active_crises.empty()) {
                    ncplane_set_fg_rgb(crisis_dashboard_plane_, 0x00FF00);
                    ncplane_putstr_yx(crisis_dashboard_plane_, 11, 4, "NO ACTIVE CRISES DETECTED. SIMULATION STABLE.");
                } else {
                    int crow = 11;
                    for (const auto& c : crisis_comp.active_crises) {
                        ncplane_set_fg_rgb(crisis_dashboard_plane_, 0xFF0000);
                        ncplane_printf_yx(crisis_dashboard_plane_, crow++, 4, "> %s (SEVERITY: %.2f) - %u turns remaining", 
                                          c.description.c_str(), c.severity, c.ticks_remaining);
                        if (crow >= 16) break;
                    }
                }
            }

            // 3. Propagation Vectors
            ncplane_set_fg_rgb(crisis_dashboard_plane_, 0xFFFF00);
            ncplane_putstr_yx(crisis_dashboard_plane_, 17, 2, "PROPAGATION VECTORS (CAUSAL CONDUCTIVITY):");
            int vrow = 19;
            for (const auto& vec : dash.propagation_vectors) {
                ncplane_set_fg_rgb(crisis_dashboard_plane_, 0xAAAAAA);
                ncplane_putstr_yx(crisis_dashboard_plane_, vrow++, 4, vec.c_str());
                if (vrow >= 24) break;
            }

            ncplane_set_fg_rgb(crisis_dashboard_plane_, 0x00FFFF);
            unsigned dy, dx;
            ncplane_dim_yx(crisis_dashboard_plane_, &dy, &dx);
            ncplane_putstr_yx(crisis_dashboard_plane_, (int)dy - 1, 2, " [C: Close Dashboard] ");

        } else {
            ncplane_move_below(crisis_dashboard_plane_, world_plane_);
        }
    }

    // --- Render Barter UI [T.1/T.3] ---
    auto barter_view = registry_.view<BarterUIComponent>();
    if (barter_view.begin() != barter_view.end()) {
        auto& ui = barter_view.get<BarterUIComponent>(*barter_view.begin());
        if (ui.is_open && registry_.valid(ui.target_agent)) {
            ncplane_erase(barter_plane_);
            ncplane_move_top(barter_plane_);
            
            uint64_t border_channels = 0;
            ncchannels_set_fg_rgb(&border_channels, 0xFFD700);
            ncchannels_set_bg_rgb(&border_channels, 0x101010);
            ncplane_perimeter_double(barter_plane_, 0, border_channels, 0);
            
            ncplane_set_fg_rgb(barter_plane_, 0xFFFF00);
            ncplane_putstr_yx(barter_plane_, 0, 2, " BARTER: ");
            if (registry_.all_of<NameComponent>(ui.target_agent)) {
                ncplane_putstr(barter_plane_, registry_.get<NameComponent>(ui.target_agent).name.c_str());
            }
            
            // --- [T.3] Negotiation Stats ---
            uint32_t pat_color = 0x00FF00;
            const char* pat_glyph = "☺";
            if (ui.npc_patience <= 0.3f) { pat_color = 0xFF0000; pat_glyph = "⚠"; }
            else if (ui.npc_patience <= 0.7f) { pat_color = 0xFFFF00; pat_glyph = "⚄"; }
            
            ncplane_set_fg_rgb(barter_plane_, 0xAAAAAA);
            ncplane_putstr_yx(barter_plane_, 1, 2, "PATIENCE: ");
            ncplane_set_fg_rgb(barter_plane_, pat_color);
            ncplane_putstr(barter_plane_, pat_glyph);
            
            ncplane_set_fg_rgb(barter_plane_, 0xAAAAAA);
            ncplane_printf_yx(barter_plane_, 1, 18, "GREED: %.2f  LEVERAGE: %.2f", ui.npc_greed_margin, ui.current_leverage);
            
            // NPC Feedback
            ncplane_set_fg_rgb(barter_plane_, 0x00FFFF);
            ncplane_printf_yx(barter_plane_, 3, 2, "\"%s\"", ui.npc_feedback.c_str());

            // --- [T.1] Split Screen Layout ---
            unsigned by, bx;
            ncplane_dim_yx(barter_plane_, &by, &bx);
            int mid_x = (int)bx / 2;
            
            // Player side (Left)
            ncplane_set_fg_rgb(barter_plane_, 0xFFFFFF);
            ncplane_putstr_yx(barter_plane_, 5, 2, "YOUR OFFER:");
            int row = 7;
            for (auto item : ui.player_offer) {
                if (registry_.valid(item) && registry_.all_of<ItemComponent>(item))
                    ncplane_printf_yx(barter_plane_, row++, 4, "- %s", registry_.get<ItemComponent>(item).name.c_str());
                if (row >= (int)by - 4) break;
            }
            for (const auto& info : ui.player_info_offer) {
                ncplane_printf_yx(barter_plane_, row++, 4, "* INTEL: %s", info.content_tag.c_str());
                if (row >= (int)by - 4) break;
            }

            // NPC side (Right)
            ncplane_set_fg_rgb(barter_plane_, 0xFFFFFF);
            ncplane_putstr_yx(barter_plane_, 5, mid_x + 2, "THEIR OFFER:");
            row = 7;
            for (auto item : ui.npc_offer) {
                if (registry_.valid(item) && registry_.all_of<ItemComponent>(item))
                    ncplane_printf_yx(barter_plane_, row++, mid_x + 4, "- %s", registry_.get<ItemComponent>(item).name.c_str());
                if (row >= (int)by - 4) break;
            }
            for (const auto& info : ui.npc_info_offer) {
                ncplane_printf_yx(barter_plane_, row++, mid_x + 4, "* INTEL: %s", info.content_tag.c_str());
                if (row >= (int)by - 4) break;
            }

            // Footer controls
            ncplane_set_fg_rgb(barter_plane_, 0xFFFF00);
            ncplane_putstr_yx(barter_plane_, (int)by - 1, 2, " [S:OFFER] [P:PRESSURE] [X:INTEL] [TAB:SWAP] [ENTER:TOGGLE] [ESC:CLOSE] ");
        } else {
            ncplane_move_below(barter_plane_, world_plane_);
        }
    }

    // 5. Render Cursor [D.1]
    if (cursor_plane_) {
        ncplane_erase(cursor_plane_);
        ncplane_move_below(cursor_plane_, world_plane_); // Hide by default

        int cur_x = -1, cur_y = -1, cur_layer = -1;
        bool cur_active = false;

        if (current_mode == SimulationMode::GOD_MODE) {
            auto cursor_view = registry_.view<GodCursorComponent>();
            if (cursor_view.begin() != cursor_view.end()) {
                auto& cursor = cursor_view.get<GodCursorComponent>(*cursor_view.begin());
                cur_x = cursor.x; cur_y = cursor.y; cur_layer = cursor.layer_id;
                cur_active = true;
            }
            // Allow Standard (Mouse) Cursor to override in God Mode for hover effects [D.1]
            auto std_view = registry_.view<StandardCursorComponent>();
            if (std_view.begin() != std_view.end()) {
                auto& sc = std_view.get<StandardCursorComponent>(*std_view.begin());
                if (sc.active && sc.mouse_driven) {
                    cur_x = sc.x; cur_y = sc.y; cur_layer = sc.layer_id;
                    cur_active = true;
                }
            }
        } else {
            auto cursor_view = registry_.view<StandardCursorComponent>();
            if (cursor_view.begin() != cursor_view.end()) {
                auto& cursor = cursor_view.get<StandardCursorComponent>(*cursor_view.begin());
                if (cursor.active) {
                    cur_x = cursor.x; cur_y = cursor.y; cur_layer = cursor.layer_id;
                    cur_active = true;
                }
            }
        }

        if (cur_active && cur_layer == current_layer) {
            int sx, sy;
            // Use raw screen coords for mouse-driven cursor so X tracks actual pointer
            {
                auto raw_sc_view = registry_.view<StandardCursorComponent>();
                bool mouse_driven = false;
                if (raw_sc_view.begin() != raw_sc_view.end()) {
                    const auto& rsc = raw_sc_view.get<StandardCursorComponent>(*raw_sc_view.begin());
                    if (rsc.mouse_driven && rsc.screen_x >= 0) {
                        sx = rsc.screen_x;
                        sy = rsc.screen_y;
                        mouse_driven = true;
                    }
                }
                if (!mouse_driven) {
                    sx = cur_x + offset_x;
                    sy = cur_y + offset_y;
                }
            }
            if (sx >= 0 && sx < (int)view_cols && sy >= 0 && sy < (int)view_rows) {
                ncplane_move_yx(cursor_plane_, sy, sx);
                ncplane_move_top(cursor_plane_);

                uint32_t cur_color = 0xAAAAAA; // Dim white default [D.1]

                // Check what's under the cursor for color highlight [D.1]
                bool found = false;
                
                // Agents (Yellow)
                auto npc_view = registry_.view<NPCComponent, PositionComponent>();
                for (auto ent : npc_view) {
                    const auto& pos = npc_view.get<PositionComponent>(ent);
                    if (pos.x == cur_x && pos.y == cur_y && pos.layer_id == cur_layer) {
                        cur_color = 0xFFFF00;
                        found = true; break;
                    }
                }
                if (!found) {
                    auto player_view = registry_.view<PlayerComponent, PositionComponent>();
                    for (auto ent : player_view) {
                        const auto& pos = player_view.get<PositionComponent>(ent);
                        if (pos.x == cur_x && pos.y == cur_y && pos.layer_id == cur_layer) {
                            cur_color = 0xFFFF00;
                            found = true; break;
                        }
                    }
                }

                // Items (Cyan)
                if (!found) {
                    auto item_view = registry_.view<ItemComponent, PositionComponent>();
                    for (auto ent : item_view) {
                        const auto& pos = item_view.get<PositionComponent>(ent);
                        if (pos.x == cur_x && pos.y == cur_y && pos.layer_id == cur_layer) {
                            cur_color = 0x00FFFF;
                            found = true; break;
                        }
                    }
                }

                // Doors (Green)
                if (!found) {
                    auto door_view = registry_.view<DoorComponent, PositionComponent>();
                    for (auto ent : door_view) {
                        const auto& pos = door_view.get<PositionComponent>(ent);
                        if (pos.x == cur_x && pos.y == cur_y && pos.layer_id == cur_layer) {
                            cur_color = 0x00FF00;
                            found = true; break;
                        }
                    }
                    if (!found) {
                        auto entrance_view = registry_.view<BuildingEntranceComponent, PositionComponent>();
                        for (auto ent : entrance_view) {
                            const auto& pos = entrance_view.get<PositionComponent>(ent);
                            if (pos.x == cur_x && pos.y == cur_y && pos.layer_id == cur_layer) {
                                cur_color = 0x00FF00;
                                found = true; break;
                            }
                        }
                    }
                }

                if (current_mode == SimulationMode::GOD_MODE && !found) {
                    cur_color = 0x00FFFF; // God Mode default Cyan for focus point
                }

                // [D.2] Standard Mode Range Enforcement
                if (current_mode == SimulationMode::STANDARD) {
                    auto player_view = registry_.view<PlayerComponent, PositionComponent>();
                    for (auto p_ent : player_view) {
                        const auto& p_pos = player_view.get<PositionComponent>(p_ent);
                        int dist = std::max(std::abs(cur_x - p_pos.x), std::abs(cur_y - p_pos.y));
                        
                        int max_range = 6; // Default OBSERVE
                        if (registry_.all_of<PlayerInteractionComponent>(p_ent)) {
                            auto mode = registry_.get<PlayerInteractionComponent>(p_ent).current_mode;
                            if (mode == InteractionMode::SPEAK) max_range = 3;
                            else if (mode == InteractionMode::TRADE) max_range = 1;
                        }

                        if (dist > max_range) {
                            cur_color = 0xFF0000; // RED
                        }
                        break;
                    }
                }

                static int cur_frame = 0;
                if ((cur_frame++ / 10) % 2 == 0) {
                    ncplane_set_fg_rgb(cursor_plane_, cur_color);
                    ncplane_putstr_yx(cursor_plane_, 0, 0, "X");
                }
            }
        }
    }

    // --- Minimap disabled ---
    if (false && minimap_plane_) {
        // ... (minimap code)
    }

    // Barter Panel [Phase T.1]
    auto barter_ui_view = registry_.view<BarterUIComponent>();
    bool barter_open = false;
    for (auto ent : barter_ui_view) {
        auto& ui = barter_ui_view.get<BarterUIComponent>(ent);
        if (ui.is_open && registry_.valid(ui.target_agent)) {
            barter_open = true;
            ncplane_erase(barter_plane_);
            ncplane_move_top(barter_plane_);
            
            // Draw Border
            uint64_t border_channels = 0;
            ncchannels_set_fg_rgb(&border_channels, 0xFFD700);
            ncchannels_set_bg_rgb(&border_channels, 0x000000);
            ncplane_perimeter_double(barter_plane_, 0, border_channels, 0);
            
            unsigned bw, bh;
            ncplane_dim_yx(barter_plane_, &bh, &bw);
            
            // Title
            ncplane_set_fg_rgb(barter_plane_, 0xFFFF00);
            ncplane_putstr_yx(barter_plane_, 0, 2, "[ BARTER PANEL ]");
            
            // NPC Header (Left)
            std::string npc_name = "Agent";
            if (registry_.all_of<NameComponent>(ui.target_agent))
                npc_name = registry_.get<NameComponent>(ui.target_agent).name;
            
            ncplane_set_fg_rgb(barter_plane_, ui.focusing_npc_inventory ? 0xFFFF00 : 0xAAAAAA);
            ncplane_printf_yx(barter_plane_, 1, 2, " %s'S STOCK ", npc_name.c_str());

            // Player Header (Right)
            ncplane_set_fg_rgb(barter_plane_, !ui.focusing_npc_inventory ? 0xFFFF00 : 0xAAAAAA);
            ncplane_printf_yx(barter_plane_, 1, (int)bw / 2 + 2, " YOUR INVENTORY ");

            // Middle Divider
            for (unsigned i = 1; i < bh - 1; ++i) {
                ncplane_set_fg_rgb(barter_plane_, 0x333333);
                ncplane_putchar_yx(barter_plane_, (int)i, (int)bw / 2, '|');
            }

            // Draw NPC Inventory (Left)
            if (registry_.all_of<InventoryComponent>(ui.target_agent)) {
                auto& inv = registry_.get<InventoryComponent>(ui.target_agent).contained_items;
                for (size_t i = 0; i < inv.size() && i < (bh - 4); ++i) {
                    auto item = inv[i];
                    std::string iname = registry_.all_of<ItemComponent>(item) ? registry_.get<ItemComponent>(item).name : "Item";
                    bool selected = (ui.focusing_npc_inventory && (int)i == ui.selected_inventory_index);
                    bool in_offer = (std::find(ui.npc_offer.begin(), ui.npc_offer.end(), item) != ui.npc_offer.end());
                    
                    if (selected) ncplane_set_fg_rgb(barter_plane_, 0xFFFF00);
                    else ncplane_set_fg_rgb(barter_plane_, in_offer ? 0x00FFFF : 0xAAAAAA);
                    
                    ncplane_printf_yx(barter_plane_, (int)i + 2, 2, "%s [%s] %s", selected ? ">" : " ", in_offer ? "X" : " ", iname.c_str());
                }
            }

            // Draw Player Inventory (Right)
            auto player_v = registry_.view<PlayerComponent, InventoryComponent>();
            if (player_v.begin() != player_v.end()) {
                auto& inv = registry_.get<InventoryComponent>(*player_v.begin()).contained_items;
                for (size_t i = 0; i < inv.size() && i < (bh - 4); ++i) {
                    auto item = inv[i];
                    std::string iname = registry_.all_of<ItemComponent>(item) ? registry_.get<ItemComponent>(item).name : "Item";
                    bool selected = (!ui.focusing_npc_inventory && (int)i == ui.selected_inventory_index);
                    bool in_offer = (std::find(ui.player_offer.begin(), ui.player_offer.end(), item) != ui.player_offer.end());
                    
                    if (selected) ncplane_set_fg_rgb(barter_plane_, 0xFFFF00);
                    else ncplane_set_fg_rgb(barter_plane_, in_offer ? 0x00FFFF : 0xAAAAAA);
                    
                    ncplane_printf_yx(barter_plane_, (int)i + 2, (int)bw / 2 + 2, "%s [%s] %s", selected ? ">" : " ", in_offer ? "X" : " ", iname.c_str());
                }
            }
            
            // Footer Info
            ncplane_set_fg_rgb(barter_plane_, 0x00FFFF);
            ncplane_putstr_yx(barter_plane_, (int)bh - 1, 2, "TAB:Side | ENTR:Toggle | S:Confirm | ESC:Cancel");
        }
    }
    if (!barter_open && barter_plane_) {
        ncplane_move_below(barter_plane_, world_plane_);
    }

    // 6. Render Speech (Overhead) [B.5 / F.2 / F.5]
    if (speech_plane_) ncplane_erase(speech_plane_);
    auto speech_view = registry_.view<SpeechComponent, PositionComponent>();
    for (auto ent : speech_view) {
        const auto& speech = speech_view.get<SpeechComponent>(ent);
        const auto& s_pos = speech_view.get<PositionComponent>(ent);
        
        if (s_pos.layer_id != current_layer) continue;
        if (!is_visible(s_pos)) continue;
        if (speech.audibility == AudibilityLevel::INAUDIBLE) continue;
        if (speech.chunks.empty() || speech.current_chunk_index >= speech.chunks.size()) continue;

        const std::string& current_text = speech.chunks[speech.current_chunk_index];

        // Determine Faction Color [F.5]
        uint32_t speech_color = 0xFFFFFF; // White default
        if (auto* pol = registry_.try_get<Layer4PoliticalComponent>(ent)) {
            if (pol->primary_faction == "GOVERNMENT") speech_color = 0x5555FF; // Blue
            else if (pol->primary_faction == "REBEL") speech_color = 0xFF5555; // Red
            else if (pol->primary_faction == "MAW") speech_color = 0x55FF55; // Green
            else if (pol->primary_faction == "VOID") speech_color = 0xAA55FF; // Purple
            else if (pol->primary_faction == "SYNDICATE") speech_color = 0xFFCC33; // Gold
        }

        bool overheard = (speech.audibility == AudibilityLevel::OVERHEARD);
        bool muffled = (speech.audibility == AudibilityLevel::MUFFLED);

        if (overheard || muffled) {
            speech_color = 0x888888; // dim color override
        }

        std::string text_to_draw = current_text;
        if (muffled) {
            // Asterisks replace some characters [F.6]
            for (size_t i = 0; i < text_to_draw.length(); ++i) {
                if (i % 3 == 0 && text_to_draw[i] != ' ' && text_to_draw[i] != '.') {
                    text_to_draw[i] = '*';
                }
            }
        }

        int sx = s_pos.x + offset_x;
        int sy = s_pos.y + offset_y - 1; // 1 tile above speaker

        if (sx >= 0 && sx < (int)view_cols && sy >= 0 && sy < (int)view_rows && speech_plane_) {
            ncplane_set_fg_rgb(speech_plane_, speech_color);
            
            // Alpha ramping [F.5] - Scale RGB for terminal dimming
            if (speech.alpha < 1.0f) {
                uint32_t r = (speech_color >> 16) & 0xFF;
                uint32_t g = (speech_color >> 8) & 0xFF;
                uint32_t b = speech_color & 0xFF;
                speech_color = (((uint32_t)((float)r * speech.alpha) & 0xFF) << 16) | 
                               (((uint32_t)((float)g * speech.alpha) & 0xFF) << 8) | 
                                ((uint32_t)((float)b * speech.alpha) & 0xFF);
                ncplane_set_fg_rgb(speech_plane_, speech_color);
            }
            
            if (overheard || muffled) ncplane_set_styles(speech_plane_, NCSTYLE_ITALIC);
            else ncplane_set_styles(speech_plane_, NCSTYLE_NONE);
            
            int text_x = sx - (int)text_to_draw.length() / 2;
            if (text_x < 0) text_x = 0;
            if (text_x + (int)text_to_draw.length() >= (int)view_cols) text_x = (int)view_cols - (int)text_to_draw.length();
            
            if (text_x >= 0) {
                ncplane_putstr_yx(speech_plane_, sy, text_x, text_to_draw.c_str());
            }
            ncplane_set_styles(speech_plane_, NCSTYLE_NONE);
        }
    }

    // 7. Render Context Menu [D.3]
    auto menu_view = registry_.view<ContextMenuComponent>();
    bool menu_open = false;
    for (auto ent : menu_view) {
        auto& menu = menu_view.get<ContextMenuComponent>(ent);
        if (menu.open) {
            menu_open = true;
            ncplane_erase(context_menu_plane_);
            ncplane_resize_simple(context_menu_plane_, (unsigned)menu.options.size() + 2, 20);
            
            // Position menu at cursor
            int sx = menu.world_x + offset_x + 1;
            int sy = menu.world_y + offset_y + 1;
            
            // Clamp to screen
            if (sx + 20 >= (int)view_cols) sx = (int)view_cols - 20;
            if (sy + (int)menu.options.size() + 2 >= (int)view_rows) sy = (int)view_rows - (int)menu.options.size() - 2;
            if (sx < 0) sx = 0;
            if (sy < 0) sy = 0;

            ncplane_move_yx(context_menu_plane_, sy, sx);
            ncplane_move_top(context_menu_plane_);
            
            ncplane_perimeter_double(context_menu_plane_, 0, 0, 0);
            
            for (size_t i = 0; i < menu.options.size(); ++i) {
                if ((int)i == menu.selected_index) {
                    ncplane_set_fg_rgb(context_menu_plane_, 0xFFFF00);
                    ncplane_printf_yx(context_menu_plane_, (int)i + 1, 1, "> %s", menu.options[i].c_str());
                } else {
                    ncplane_set_fg_rgb(context_menu_plane_, 0xFFFFFF);
                    ncplane_printf_yx(context_menu_plane_, (int)i + 1, 1, "  %s", menu.options[i].c_str());
                }
            }
        }
    }
    if (!menu_open && context_menu_plane_) {
        ncplane_move_below(context_menu_plane_, world_plane_);
    }

    // 8. Render Dialogue Log [F.6]
    auto log_view = registry_.view<DialogueLogComponent>();
    bool log_open = false;
    for (auto ent : log_view) {
        auto& log = log_view.get<DialogueLogComponent>(ent);
        if (log.visible) {
            log_open = true;
            ncplane_erase(dialogue_log_plane_);
            ncplane_move_top(dialogue_log_plane_);
            
            // Draw Border
            uint64_t border_channels = 0;
            ncchannels_set_fg_rgb(&border_channels, 0x00AAAA);
            ncchannels_set_bg_rgb(&border_channels, 0x000000);
            ncplane_perimeter_double(dialogue_log_plane_, 0, border_channels, 0);
            
            unsigned lw, lh;
            ncplane_dim_yx(dialogue_log_plane_, &lh, &lw);
            
            ncplane_set_fg_rgb(dialogue_log_plane_, 0x00FFFF);
            ncplane_putstr_yx(dialogue_log_plane_, 0, 2, "[ DIALOGUE HISTORY ]");

            int row = 2;
            for (const auto& entry : log.entries) {
                if (row >= (int)lh - 2) break;
                
                uint32_t entry_color = 0xAAAAAA;
                if (entry.audibility == AudibilityLevel::CLEAR) entry_color = 0xFFFFFF;
                else if (entry.audibility == AudibilityLevel::MUFFLED) entry_color = 0x666666;

                std::string log_text = entry.text;
                if (entry.audibility == AudibilityLevel::MUFFLED) {
                    for (size_t i = 0; i < log_text.length(); ++i) {
                        if (i % 3 == 0 && log_text[i] != ' ' && log_text[i] != '.') log_text[i] = '*';
                    }
                }

                ncplane_set_fg_rgb(dialogue_log_plane_, 0x00AAAA);
                ncplane_printf_yx(dialogue_log_plane_, row, 2, "%s:", entry.speaker_name.c_str());
                
                ncplane_set_fg_rgb(dialogue_log_plane_, entry_color);
                if (entry.audibility != AudibilityLevel::CLEAR) ncplane_set_styles(dialogue_log_plane_, NCSTYLE_ITALIC);
                else ncplane_set_styles(dialogue_log_plane_, NCSTYLE_NONE);
                
                ncplane_printf_yx(dialogue_log_plane_, row + 1, 4, "\"%s\"", log_text.c_str());
                ncplane_set_styles(dialogue_log_plane_, NCSTYLE_NONE);
                
                row += 3;
            }
            
            ncplane_set_fg_rgb(dialogue_log_plane_, 0x00AAAA);
            ncplane_putstr_yx(dialogue_log_plane_, (int)lh - 1, 2, "L:Close");
        }
    }
    if (!log_open && dialogue_log_plane_) {
        ncplane_move_below(dialogue_log_plane_, world_plane_);
    }

    ncplane_move_top(hud_plane_);

    // --- Debug Overlay --- (disabled, stderr logging still active)
    // Uncomment to re-enable in-game debug panel

    notcurses_render(nc_context_);
}

void RenderingSystem::handle_input(const struct ncinput& input) { (void)input; }

void RenderingSystem::handleInventoryToggleEvent(const InventoryToggleEvent& event) {
    if (registry_.all_of<HUDComponent>(event.entity)) {
        auto& hud = registry_.get<HUDComponent>(event.entity);
        hud.inventory_open = !hud.inventory_open;
        inventory_visible_ = hud.inventory_open;
        if (inventory_visible_) ncplane_move_top(inventory_plane_);
        else ncplane_move_below(inventory_plane_, world_plane_);
    }
}

void RenderingSystem::handleHUDNotificationEvent(const HUDNotificationEvent& event) {
    auto view = registry_.view<HUDComponent, PlayerComponent>();
    for (auto entity : view) {
        auto& hud = view.get<HUDComponent>(entity);
        hud.notifications.insert(hud.notifications.begin(), event.message);
        if (hud.notifications.size() > 5) hud.notifications.pop_back();
    }
}

void RenderingSystem::handleToggleCrisisDashboardEvent(const ToggleCrisisDashboardEvent& event) {
    (void)event;
    // Visiblity is handled by the component flag, but we can use this to force a plane move
}

void RenderingSystem::handleToggleControlsHelpEvent(const ToggleControlsHelpEvent& event) {
    if (registry_.all_of<HUDComponent>(event.entity)) {
        registry_.get<HUDComponent>(event.entity).show_controls_help = !registry_.get<HUDComponent>(event.entity).show_controls_help;
    }
}

uint32_t RenderingSystem::parse_hex_color(const std::string& hex) {
    if (hex.empty()) return 0xFFFFFF;
    auto it = color_cache_.find(hex);
    if (it != color_cache_.end()) return it->second;
    const char* s = hex.c_str();
    if (*s == '#') ++s;
    uint32_t val = 0;
    for (; *s; ++s) {
        char c = *s;
        val <<= 4;
        if (c >= '0' && c <= '9') val |= (c - '0');
        else if (c >= 'a' && c <= 'f') val |= (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
    }
    color_cache_[hex] = val;
    return val;
}

std::string RenderingSystem::room_tag_to_string(NeonOubliette::RoomTag tag) {
    switch (tag) {
        case RoomTag::LOBBY: return "LOBBY";
        case RoomTag::OFFICE: return "OFFICE";
        case RoomTag::SERVER_ROOM: return "SERVER ROOM";
        case RoomTag::EXECUTIVE_SUITE: return "EXECUTIVE SUITE";
        case RoomTag::BEDROOM: return "BEDROOM";
        case RoomTag::KITCHEN: return "KITCHEN";
        case RoomTag::BATHROOM: return "BATHROOM";
        case RoomTag::LIVING_ROOM: return "LIVING ROOM";
        case RoomTag::FACTORY_FLOOR: return "FACTORY FLOOR";
        case RoomTag::STORAGE: return "STORAGE";
        case RoomTag::SUPERVISOR_OFFICE: return "SUPERVISOR OFFICE";
        case RoomTag::HALLWAY: return "HALLWAY";
        default: return "UNKNOWN";
    }
}


// Added for [L.6]
static void render_sparkline(struct ncplane* plane, int y, int x, const std::vector<float>& history, uint32_t color) {
    if (history.empty()) return;
    ncplane_set_fg_rgb(plane, color);
    // ASCII Metaphor for trends
    const char* blocks[] = {" ", ".", "-", "=", "o", "x", "X", "#", "@"};
    for (size_t i = 0; i < history.size(); ++i) {
        int idx = (int)(history[i] * 8);
        idx = std::clamp(idx, 0, 8);
        ncplane_putstr_yx(plane, y, x + (int)i, blocks[idx]);
    }
}

} // namespace NeonOubliette::Systems
