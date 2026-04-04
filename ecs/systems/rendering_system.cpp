#include "rendering_system.h"
#include <iostream>
#include <string>
#include <fstream>
#include <cmath>
#include <random>
#include "../components/components.h"
#include "../components/simulation_layers.h"
#include "../components/physics_colors.h"
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

    // Determine mode and camera focus
    SimulationMode current_mode = SimulationMode::STANDARD;
    bool is_paused = false;
    float sim_speed = 1.0f;
    auto state_view = registry_.view<SimulationStateComponent>();
    if (!state_view.empty()) {
        auto& state = state_view.get<SimulationStateComponent>(*state_view.begin());
        current_mode = state.mode;
        is_paused = state.is_paused;
        sim_speed = state.god_mode_tps;
    }

    int current_layer = 0;
    int cam_x = 0;
    int cam_y = 0;
    bool show_help = true;

    if (current_mode == SimulationMode::GOD_MODE) {
        auto cursor_view = registry_.view<GodCursorComponent>();
        if (!cursor_view.empty()) {
            auto& cursor = cursor_view.get<GodCursorComponent>(*cursor_view.begin());
            cam_x = cursor.x;
            cam_y = cursor.y;
            current_layer = cursor.layer_id;
        }
        show_help = true; 
    } else {
        auto player_view = registry_.view<PlayerComponent, PositionComponent, PlayerCurrentLayerComponent, HUDComponent>();
        for (auto entity : player_view) {
            const auto& pos = player_view.get<PositionComponent>(entity);
            current_layer = player_view.get<PlayerCurrentLayerComponent>(entity).current_z;
            show_help = player_view.get<HUDComponent>(entity).show_controls_help;
            cam_x = pos.x;
            cam_y = pos.y;
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
        const auto& hud = hud_view.get<HUDComponent>(entity);
        
        if (current_mode == SimulationMode::GOD_MODE) {
            ncplane_set_fg_rgb(hud_plane_, 0xFFFF00);
            ncplane_printf_yx(hud_plane_, 0, 0, "MODE: SURVEILLANCE  [%s]", is_paused ? "PAUSED" : "RUNNING");
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
            ncplane_putstr_yx(hud_plane_, 3, 35, "i:Phys I:Bio c:Cogn f:Econ t:Pol");
        } else {
            ncplane_set_fg_rgb(hud_plane_, 0x00FFFF);
            ncplane_printf_yx(hud_plane_, 0, 0, "HEALTH: %.0f%%  CREDITS: %d", hud.health, hud.credits);
            ncplane_printf_yx(hud_plane_, 1, 0, "LAYER: %d", current_layer);

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

        // --- Time Display ---
        auto weather_view = registry_.view<WeatherComponent>();
        if (!weather_view.empty()) {
            const auto& weather = weather_view.get<WeatherComponent>(*weather_view.begin());
            std::string time_name = "Day";
            uint32_t t_color = 0xFFFFE0;
            switch(weather.time_of_day) {
                case TimeOfDay::DAWN: time_name = "Dawn"; t_color = 0xFF8C00; break;
                case TimeOfDay::DAY: time_name = "Day"; t_color = 0xFFFFE0; break;
                case TimeOfDay::DUSK: time_name = "Dusk"; t_color = 0x8A2BE2; break;
                case TimeOfDay::NIGHT: time_name = "Night"; t_color = 0x191970; break;
            }
            ncplane_set_fg_rgb(hud_plane_, t_color);
            ncplane_printf_yx(hud_plane_, 1, 15, "[ TIME: %s ]", time_name.c_str());
        }

        int row = 2;
        for (const auto& notif : hud.notifications) {
            ncplane_putstr_yx(hud_plane_, row++, 0, notif.c_str());
            if (row >= 6) break;
        }
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

    auto render_at = [&](int x, int y, char glyph, uint32_t color) {
        int screen_x = x + offset_x;
        int screen_y = y + offset_y;
        if (screen_x >= 0 && screen_x < (int)view_cols && screen_y >= 0 && screen_y < (int)view_rows) {
            // Apply time-of-day color shifts
            if (current_mode != SimulationMode::GOD_MODE) {
                auto weather_v = registry_.view<WeatherComponent>();
                if (!weather_v.empty()) {
                    const auto& weather = weather_v.get<WeatherComponent>(*weather_v.begin());
                    if (weather.time_of_day == TimeOfDay::NIGHT) {
                        uint32_t r = (color >> 16) & 0xFF;
                        uint32_t g = (color >> 8) & 0xFF;
                        uint32_t b = color & 0xFF;
                        r = (uint32_t)((float)r * 0.2f);
                        g = (uint32_t)((float)g * 0.3f);
                        b = (uint32_t)((float)b * 0.6f);
                        color = (r << 16) | (g << 8) | b;
                    } else if (weather.time_of_day == TimeOfDay::DAWN || weather.time_of_day == TimeOfDay::DUSK) {
                        uint32_t r = (color >> 16) & 0xFF;
                        uint32_t g = (color >> 8) & 0xFF;
                        uint32_t b = color & 0xFF;
                        r = (uint32_t)((float)r * 0.7f);
                        g = (uint32_t)((float)g * 0.5f);
                        b = (uint32_t)((float)b * 0.6f);
                        color = (r << 16) | (g << 8) | b;
                    }
                }
            }

            ncplane_set_fg_rgb(world_plane_, color);
            ncplane_putchar_yx(world_plane_, screen_y, screen_x, glyph);
        }
    };

    // 1. Render Memory (Fog of War) - Only in Standard Mode
    if (current_mode == SimulationMode::STANDARD && player_mem) {
        for (const auto& [m_pos, m_tile] : player_mem->remembered_tiles) {
            if (m_pos.layer_id != current_layer) continue;
            if (is_visible(m_pos)) continue; 

            uint32_t base_color = parse_hex_color(m_tile.color);
            uint32_t r = (base_color >> 16) & 0xFF;
            uint32_t g = (base_color >> 8) & 0xFF;
            uint32_t b = base_color & 0xFF;
            uint32_t dimmed_color = ((r / 4) << 16) | ((g / 4) << 8) | (b / 4);

            render_at(m_pos.x, m_pos.y, m_tile.glyph, dimmed_color);
        }
    }

    // 2. Render Terrain (with viewport culling)
    int vp_x_min = cam_x - (int)view_cols / 2 - 1;
    int vp_x_max = cam_x + (int)view_cols / 2 + 1;
    int vp_y_min = cam_y - (int)view_rows / 2 - 1;
    int vp_y_max = cam_y + (int)view_rows / 2 + 1;

    auto terrain_view = registry_.view<TerrainComponent, RenderableComponent, PositionComponent>();
    for (auto entity : terrain_view) {
        const auto& pos = terrain_view.get<PositionComponent>(entity);
        if (pos.layer_id != current_layer) continue;
        if (pos.x < vp_x_min || pos.x > vp_x_max || pos.y < vp_y_min || pos.y > vp_y_max) continue;
        if (!is_visible(pos)) continue;
        const auto& render = terrain_view.get<RenderableComponent>(entity);
        
        uint32_t color = parse_hex_color(render.color);
        if (registry_.all_of<Layer0PhysicsComponent>(entity)) {
            const auto& phys = registry_.get<Layer0PhysicsComponent>(entity);
            color = parse_hex_color(map_temperature_to_color(phys.temperature_celsius, render.color));
            if (phys.temperature_celsius > 800.0f) {
                static thread_local std::mt19937 fgen(666);
                if (std::uniform_real_distribution<>(0,1)(fgen) < 0.2) color = 0xFFFF00; 
            }
        }
        
        render_at(pos.x, pos.y, render.glyph, color);
    }

    // 3. Render Entities (with viewport culling)
    auto entity_view = registry_.view<RenderableComponent, PositionComponent>(entt::exclude<TerrainComponent>);
    for (auto entity : entity_view) {
        const auto& pos = entity_view.get<PositionComponent>(entity);
        if (pos.layer_id != current_layer) continue;
        if (pos.x < vp_x_min || pos.x > vp_x_max || pos.y < vp_y_min || pos.y > vp_y_max) continue;
        if (!is_visible(pos)) continue;
        const auto& render = entity_view.get<RenderableComponent>(entity);

        uint32_t color = parse_hex_color(render.color);
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
                        render_at(pos.x + dx, pos.y + dy, render.glyph, color);
                    }
                }
            }
        } else {
            render_at(pos.x, pos.y, render.glyph, color);
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
                        render_at(w_pos.x + dx, w_pos.y + dy, (dis(mist_gen) < 0.5 ? '~' : ','), 
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
                    ncplane_set_fg_rgb(world_plane_, w_color);
                    ncplane_putchar_yx(world_plane_, sy, sx, w_glyph);
                }
            }
        }
    }

    // 4. Render God Cursor (Top layer)
    if (current_mode == SimulationMode::GOD_MODE) {
        auto cursor_v = registry_.view<GodCursorComponent>();
        if (!cursor_v.empty()) {
            auto& cursor = cursor_v.get<GodCursorComponent>(*cursor_v.begin());
            if (cursor.layer_id == current_layer) {
                // Brackets
                uint32_t cur_color = 0x00FFFF;
                // Blinking effect
                static int frame = 0;
                if ((frame++ / 15) % 2 == 0) {
                    render_at(cursor.x, cursor.y, 'X', cur_color);
                }
            }
        }
    }

    ncplane_move_top(hud_plane_);
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

} // namespace NeonOubliette::Systems
