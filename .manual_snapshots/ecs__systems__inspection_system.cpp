#include "inspection_system.h"
#include "../components/simulation_layers.h"
#include "../components/physics_colors.h"
#include <chrono>
#include <entt/entt.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cmath>

namespace NeonOubliette::Systems {

InspectionSystem::InspectionSystem(entt::registry& registry, struct notcurses* nc_context,
                                   entt::dispatcher& event_dispatcher)
    : registry_(registry), nc_context_(nc_context), event_dispatcher_(event_dispatcher) {
    event_dispatcher_.sink<InspectEvent>().connect<&InspectionSystem::handleInspectEvent>(this);
    event_dispatcher_.sink<CloseInspectionWindowEvent>().connect<&InspectionSystem::handleCloseInspectionWindowEvent>(this);
}

InspectionSystem::~InspectionSystem() {
    if (m_inspection_plane) {
        ncplane_destroy(m_inspection_plane);
    }
}

void InspectionSystem::initialize() {}

void InspectionSystem::update(double delta_time) {
    (void)delta_time;
    if (m_window_visible) {
        m_pulse_counter++;
        draw_inspection_window();
    }
}

void InspectionSystem::create_inspection_plane() {
    if (m_inspection_plane) return;

    struct ncplane_options opts = {};
    opts.rows = 26;
    opts.cols = 64;
    opts.y = 2;
    opts.x = 20; 
    opts.name = "Inspection Plane";

    m_inspection_plane = ncplane_create(notcurses_stdplane(nc_context_), &opts);
    
    uint64_t channels = 0;
    ncchannels_set_bg_rgb(&channels, 0x22252B);
    ncchannels_set_fg_rgb(&channels, 0xFFFFFF);
    ncplane_set_base(m_inspection_plane, " ", 0, channels);
}

void InspectionSystem::close_inspection_window() {
    if (m_inspection_plane) {
        ncplane_destroy(m_inspection_plane);
        m_inspection_plane = nullptr;
    }
    m_window_visible = false;
    m_current_target = entt::null;
}

void InspectionSystem::handleCloseInspectionWindowEvent(const CloseInspectionWindowEvent& event) {
    (void)event;
    close_inspection_window();
}

void InspectionSystem::handleInspectEvent(const InspectEvent& event) {
    auto view = registry_.view<PositionComponent>();
    entt::entity target = entt::null;

    std::vector<int> levels_to_check = {event.layer_id, 5, 0};
    if (event.layer_id >= 1000) levels_to_check = {event.layer_id};

    for (int lvl : levels_to_check) {
        for (auto entity : view) {
            if (entity == event.player_entity) continue;
            const auto& pos = view.get<PositionComponent>(entity);
            if (pos.layer_id == lvl) {
                int width = 1, height = 1;
                if (registry_.all_of<SizeComponent>(entity)) {
                    const auto& size = registry_.get<SizeComponent>(entity);
                    width = size.width;
                    height = size.height;
                }

                if (event.x >= pos.x && event.x < pos.x + width &&
                    event.y >= pos.y && event.y < pos.y + height) {
                    target = entity;
                    break; 
                }
            }
        }
        if (target != entt::null) break;
    }

    if (target == entt::null) {
        float best_dist = 999.0f;
        for (auto entity : view) {
            if (entity == event.player_entity) continue;
            const auto& pos = view.get<PositionComponent>(entity);
            if (pos.layer_id == event.layer_id) {
                int dx = pos.x - event.x;
                int dy = pos.y - event.y;
                float dist_sq = (float)(dx * dx + dy * dy);
                if (dist_sq <= 2.0f && dist_sq < best_dist) {
                    best_dist = dist_sq;
                    target = entity;
                }
            }
        }
    }

    if (target == entt::null) {
        if (event.mode != InspectionMode::GLANCE) {
            close_inspection_window();
        }
        return;
    }

    if (event.mode == InspectionMode::GLANCE) return;

    m_current_target = target;
    m_current_mode = event.mode;
    m_window_visible = true;
    create_inspection_plane();
}

void InspectionSystem::draw_inspection_window() {
    if (!m_inspection_plane || m_current_target == entt::null) return;

    ncplane_erase(m_inspection_plane);
    
    uint32_t accent_color = 0xFFFFFF;
    switch(m_current_mode) {
        case InspectionMode::SURFACE_SCAN: accent_color = 0x55FFFF; break;
        case InspectionMode::BIOLOGICAL_AUDIT: accent_color = 0x00FF00; break;
        case InspectionMode::COGNITIVE_PROFILE: accent_color = 0xFF00FF; break;
        case InspectionMode::FINANCIAL_FORENSICS: accent_color = 0xFFFF00; break;
        case InspectionMode::STRUCTURAL_ANALYSIS: accent_color = 0xFF5555; break;
        case InspectionMode::HISTORY: accent_color = 0xAAAAAA; break;
        default: break;
    }

    ncplane_set_fg_rgb(m_inspection_plane, accent_color);
    ncplane_cursor_move_yx(m_inspection_plane, 0, 0);
    ncplane_box(m_inspection_plane, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0);

    std::string name = "Unknown Entity";
    if (registry_.all_of<NameComponent>(m_current_target)) {
        name = registry_.get<NameComponent>(m_current_target).name;
    }
    
    ncplane_set_fg_rgb(m_inspection_plane, 0xFFFFFF);
    ncplane_putstr_yx(m_inspection_plane, 1, 2, ("INSPECTING: " + name).c_str());

    const char pulse_chars[] = {'-', '\\', '|', '/'};
    char pulse = pulse_chars[(m_pulse_counter / 10) % 4];
    ncplane_set_fg_rgb(m_inspection_plane, accent_color);
    ncplane_printf_yx(m_inspection_plane, 1, 60, "[%c]", pulse);

    draw_tabs();
    draw_ascii_portrait();
    draw_content();

    ncplane_set_fg_rgb(m_inspection_plane, 0x888888);
    ncplane_putstr_yx(m_inspection_plane, 25, 2, "ESC: close  [i]Phys [I]Bio [c]Mental [f]Econ [t]Pol");
}

void InspectionSystem::draw_tabs() {
    const char* tabs[] = {"[Physical]", "[Bio]", "[Mental]", "[Economy]", "[Political]", "[History]"};
    int current_tab_idx = 0;
    uint32_t colors[] = {0x55FFFF, 0x00FF00, 0xFF00FF, 0xFFFF00, 0xFF5555, 0xAAAAAA};

    switch(m_current_mode) {
        case InspectionMode::SURFACE_SCAN: current_tab_idx = 0; break;
        case InspectionMode::BIOLOGICAL_AUDIT: current_tab_idx = 1; break;
        case InspectionMode::COGNITIVE_PROFILE: current_tab_idx = 2; break;
        case InspectionMode::FINANCIAL_FORENSICS: current_tab_idx = 3; break;
        case InspectionMode::STRUCTURAL_ANALYSIS: current_tab_idx = 4; break;
        case InspectionMode::HISTORY: current_tab_idx = 5; break;
        default: current_tab_idx = 5; break;
    }

    int x = 2;
    for (int i = 0; i < 6; ++i) {
        if (i == current_tab_idx) {
            ncplane_set_fg_rgb(m_inspection_plane, colors[i]);
            ncplane_set_styles(m_inspection_plane, NCSTYLE_BOLD | NCSTYLE_UNDERLINE);
        } else {
            ncplane_set_fg_rgb(m_inspection_plane, 0x444444);
            ncplane_set_styles(m_inspection_plane, NCSTYLE_NONE);
        }
        ncplane_putstr_yx(m_inspection_plane, 3, x, tabs[i]);
        x += (int)strlen(tabs[i]) + 1;
    }
    ncplane_set_styles(m_inspection_plane, NCSTYLE_NONE);
}

void InspectionSystem::draw_ascii_portrait() {
    int start_y = 5;
    int x = 2;
    
    uint32_t color = 0xFFFFFF;
    switch(m_current_mode) {
        case InspectionMode::SURFACE_SCAN: color = 0x55FFFF; break;
        case InspectionMode::BIOLOGICAL_AUDIT: color = 0x00FF00; break;
        case InspectionMode::COGNITIVE_PROFILE: color = 0xFF00FF; break;
        case InspectionMode::FINANCIAL_FORENSICS: color = 0xFFFF00; break;
        case InspectionMode::STRUCTURAL_ANALYSIS: color = 0xFF5555; break;
        default: break;
    }
    ncplane_set_fg_rgb(m_inspection_plane, color);

    auto* xeno_ptr = registry_.try_get<XenoComponent>(m_current_target);
    if (xeno_ptr) {
        if (xeno_ptr->type == XenoType::HIERODULE) {
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "      .---.      ");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "     / / \\ \\     ");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "    | |/ \\| |    ");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "    | |\\ /| |    ");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "     \\ \\ / /     ");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "      '---'      ");
        } else {
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "     <><><>      ");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "    <  !!  >     ");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "     < VV >      ");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "    <      >     ");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "     <><><>      ");
        }
        return;
    }

    switch(m_current_mode) {
        case InspectionMode::SURFACE_SCAN:
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "  ┌──────────┐");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "  │  __  __  │");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "  │ |  ||  | │");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "  │ |__||__| │");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "  │  ______  │");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "  │ |______| │");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "  └──────────┘");
            break;
        case InspectionMode::BIOLOGICAL_AUDIT:
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "      .---.");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "     / @ @ \\");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "    (   ^   )");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "     \\  -  /");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "    .-\'---\'-.");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "   /    |    \\");
            break;
        case InspectionMode::COGNITIVE_PROFILE:
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "     .oooo.");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "   .oO()OOOo.");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "  .oOO()()OOo.");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "   'oOOOOOOo'");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "     'oooo'");
            break;
        case InspectionMode::FINANCIAL_FORENSICS:
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "  ________________");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, " |  BANK TRANS.  |");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, " | [3896738967389673896738967]  |");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, " | [3896738967      ]  |");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, " |________________|");
            break;
        case InspectionMode::STRUCTURAL_ANALYSIS:
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "      / \\");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "     / | \\");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "    /  |  \\");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "   |---|---|");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "   | [SEC] |");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "    \\  |  /");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "     \\ | /");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "      \\ /");
            break;
        default: break;
    }
}

void InspectionSystem::draw_content() {
    int start_y = 14;
    int x = 2;
    int insight_y = 6;
    int insight_x = 22;

    const EntityPrivacyComponent* privacy = registry_.try_get<EntityPrivacyComponent>(m_current_target);
    auto is_masked = [&](SimulationLayer layer) -> bool {
        if (!privacy) return false;
        auto it = privacy->masks.find(layer);
        if (it == privacy->masks.end()) return false;
        return it->second.concealment_level != ConcealmentLevel::NONE;
    };

    auto draw_mask = [&]() {
        ncplane_set_fg_rgb(m_inspection_plane, 0xFF5555);
        ncplane_putstr_yx(m_inspection_plane, start_y, x, "[DATA ENCRYPTED / MASKED]");
    };

    SimulationLayer current_layer = SimulationLayer::L0_Physics;
    switch(m_current_mode) {
        case InspectionMode::SURFACE_SCAN: current_layer = SimulationLayer::L0_Physics; break;
        case InspectionMode::BIOLOGICAL_AUDIT: current_layer = SimulationLayer::L1_Biology; break;
        case InspectionMode::COGNITIVE_PROFILE: current_layer = SimulationLayer::L2_Cognitive; break;
        case InspectionMode::FINANCIAL_FORENSICS: current_layer = SimulationLayer::L3_Economic; break;
        case InspectionMode::STRUCTURAL_ANALYSIS: current_layer = SimulationLayer::L4_Political; break;
        default: break;
    }

    ncplane_set_fg_rgb(m_inspection_plane, 0x888888);
    ncplane_putstr_yx(m_inspection_plane, insight_y++, insight_x, "CAUSAL INSIGHTS:");
    auto insights = calculate_insights(m_current_target, current_layer);
    if (insights.empty()) {
        ncplane_putstr_yx(m_inspection_plane, insight_y, insight_x, "None detected.");
    } else {
        for (const auto& insight : insights) {
            ncplane_set_fg_rgb(m_inspection_plane, hex_to_rgb(insight.hex_color));
            ncplane_putstr_yx(m_inspection_plane, insight_y++, insight_x, insight.message.c_str());
        }
    }

    ncplane_set_fg_rgb(m_inspection_plane, 0xFFFFFF);

    switch (m_current_mode) {
        case InspectionMode::SURFACE_SCAN: {
            if (is_masked(SimulationLayer::L0_Physics)) {
                draw_mask();
            } else {
                if (auto* p = registry_.try_get<Layer0PhysicsComponent>(m_current_target)) {
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Material: %s", get_material_name(p->material).c_str());
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Integrity: %.1f%%", p->structural_integrity * 100.0f);
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Temp: %.1f C", p->temperature_celsius);
                }
                if (auto* haz = registry_.try_get<TileHazardComponent>(m_current_target)) {
                    ncplane_set_fg_rgb(m_inspection_plane, 0xFF5555);
                    ncplane_putstr_yx(m_inspection_plane, start_y++, x, "--- ACTIVE HAZARD ---");
                    ncplane_set_fg_rgb(m_inspection_plane, 0xFFFFFF);
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Type: %s", get_hazard_name(haz->type).c_str());
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Intensity: %.1f", haz->intensity);
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Status: %s", haz->is_active ? "ACTIVE" : "DORMANT");
                }
            }
            break;
        }
        case InspectionMode::BIOLOGICAL_AUDIT: {
            if (is_masked(SimulationLayer::L1_Biology)) {
                draw_mask();
            } else if (auto* bio = registry_.try_get<Layer1BiologyComponent>(m_current_target)) {
                ncplane_printf_yx(m_inspection_plane, start_y++, x, "Species: %s", get_species_name(bio->species).c_str());
                
                if (auto* age = registry_.try_get<AgeComponent>(m_current_target)) {
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Age: %u Standard Years", age->years);
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Life Stage: %s", get_life_stage_name(age->stage).c_str());
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Bio-Wear: %.1f%%", age->biological_wear * 100.0f);
                }

                ncplane_printf_yx(m_inspection_plane, start_y++, x, "Consciousness: %.1f%%", bio->consciousness_level * 100.0f);
                if (auto* needs = registry_.try_get<NeedsComponent>(m_current_target)) {
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Hunger: %.1f%%", needs->hunger);
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Thirst: %.1f%%", needs->thirst);
                }
            }
            break;
        }
        case InspectionMode::COGNITIVE_PROFILE: {
            if (is_masked(SimulationLayer::L2_Cognitive)) {
                draw_mask();
            } else if (auto* cog = registry_.try_get<Layer2CognitiveComponent>(m_current_target)) {
                ncplane_printf_yx(m_inspection_plane, start_y++, x, "Pleasure: %+.2f", cog->pleasure);
                ncplane_printf_yx(m_inspection_plane, start_y++, x, "Arousal:  %+.2f", cog->arousal);
            }
            break;
        }
        case InspectionMode::FINANCIAL_FORENSICS: {
            if (is_masked(SimulationLayer::L3_Economic)) {
                draw_mask();
            } else {
                if (auto* econ = registry_.try_get<Layer3EconomicComponent>(m_current_target)) {
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Cash: %d CR", econ->cash_on_hand);
                }
                if (auto* node = registry_.try_get<ResourceNodeComponent>(m_current_target)) {
                    ncplane_set_fg_rgb(m_inspection_plane, 0x00FFFF);
                    ncplane_putstr_yx(m_inspection_plane, start_y++, x, "--- RESOURCE NODE ---");
                    ncplane_set_fg_rgb(m_inspection_plane, 0xFFFFFF);
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Type:  %s", get_raw_material_name(node->material_type).c_str());
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Yield: %.2f", node->current_yield);
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Status: %s", node->is_exhausted ? "EXHAUSTED" : "AVAILABLE");
                }
                if (auto* lab = registry_.try_get<ClandestineLabComponent>(m_current_target)) {
                    ncplane_set_fg_rgb(m_inspection_plane, 0x00FF00);
                    ncplane_putstr_yx(m_inspection_plane, start_y++, x, "--- LAB INVENTORY ---");
                    ncplane_set_fg_rgb(m_inspection_plane, 0xFFFFFF);
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Raw: %d/%d", lab->raw_chemicals_stored, lab->max_raw_chemicals);
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Drug: %d/%d", lab->drugs_produced_stored, lab->max_drugs_produced);
                }

                if (auto* factory = registry_.try_get<FactoryComponent>(m_current_target)) {
                    ncplane_set_fg_rgb(m_inspection_plane, 0x55FFFF);
                    ncplane_putstr_yx(m_inspection_plane, start_y++, x, "--- INDUSTRIAL FACILITY ---");
                    ncplane_set_fg_rgb(m_inspection_plane, 0xFFFFFF);
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Owner: %s", factory->owning_faction.c_str());
                    
                    if (!factory->available_recipes.empty()) {
                        const auto& recipe = factory->available_recipes[factory->active_recipe_index];
                        ncplane_printf_yx(m_inspection_plane, start_y++, x, "Active: %s", recipe.recipe_name.c_str());
                        ncplane_printf_yx(m_inspection_plane, start_y++, x, "Progress: %.1f%%", (factory->current_progress / recipe.work_required) * 100.0f);
                        
                        ncplane_set_fg_rgb(m_inspection_plane, 0x888888);
                        std::string inputs = "Inputs: ";
                        for (auto const& [type, amt] : recipe.inputs) {
                            inputs += get_raw_material_name(type).substr(0, 3) + "(" + std::to_string((int)factory->input_stockpile[type]) + "/" + std::to_string((int)amt) + ") ";
                        }
                        ncplane_putstr_yx(m_inspection_plane, start_y++, x, inputs.c_str());
                    }
                    
                    if (auto* disruption = registry_.try_get<SupplyChainDisruptionComponent>(m_current_target)) {
                        ncplane_set_fg_rgb(m_inspection_plane, 0xFF5555);
                        if (disruption->labor_strike > 0.1f) ncplane_printf_yx(m_inspection_plane, start_y++, x, "STRIKE RISK: %.0f%%", disruption->labor_strike * 100.0f);
                        if (disruption->transport_bottleneck > 0.1f) ncplane_printf_yx(m_inspection_plane, start_y++, x, "LOGISTICS DELAY: %.0f%%", disruption->transport_bottleneck * 100.0f);
                    }
                }
                
                // [J.4] Show Zone Economic Data
                if (auto* pos = registry_.try_get<PositionComponent>(m_current_target)) {
                    draw_zone_data(pos->x, pos->y);
                }
            }
            break;
        }
        case InspectionMode::STRUCTURAL_ANALYSIS: {
            if (is_masked(SimulationLayer::L4_Political)) {
                draw_mask();
            } else {
                if (auto* health = registry_.try_get<BuildingHealthComponent>(m_current_target)) {
                    ncplane_set_fg_rgb(m_inspection_plane, 0xFF5555);
                    ncplane_putstr_yx(m_inspection_plane, start_y++, x, "--- STRUCTURAL INTEGRITY ---");
                    ncplane_set_fg_rgb(m_inspection_plane, 0xFFFFFF);
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Integrity: %.1f%%", health->integrity);
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Squatters: %d", health->num_squatters);
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Urgency:   %.1f%%", health->maintenance_urgency * 100.0f);
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Maint. Budget: %.0f CR", health->maintenance_budget);
                    if (health->is_condemned) {
                        ncplane_set_fg_rgb(m_inspection_plane, 0xFF0000);
                        ncplane_putstr_yx(m_inspection_plane, start_y++, x, "STATUS: CONDEMNED");
                        ncplane_set_fg_rgb(m_inspection_plane, 0xFFFFFF);
                    }
                }

                if (auto* pol = registry_.try_get<Layer4PoliticalComponent>(m_current_target)) {
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Faction: %s", pol->primary_faction.c_str());
                    
                    // [P.3] Reputation Feedback in Inspection
                    auto player_view = registry_.view<PlayerComponent>();
                    if (!player_view.empty()) {
                        auto player = player_view.front();
                        if (auto* rep = registry_.try_get<ReputationComponent>(player)) {
                            float standing = 0.0f;
                            if (rep->faction_standing.contains(pol->primary_faction)) {
                                standing = rep->faction_standing.at(pol->primary_faction);
                            }
                            ReputationTier tier = rep->get_tier(pol->primary_faction);
                            
                            ncplane_set_fg_rgb(m_inspection_plane, 0x888888);
                            ncplane_putstr_yx(m_inspection_plane, start_y, x, "Your Standing: ");
                            
                            uint32_t standing_color = 0xFFFFFF;
                            std::string tier_name = "NEUTRAL";
                            switch(tier) {
                                case ReputationTier::EXCOMMUNICATED: standing_color = 0xFF3333; tier_name = "EXCOMMUNICATED"; break;
                                case ReputationTier::HOSTILE: standing_color = 0xFF5555; tier_name = "HOSTILE"; break;
                                case ReputationTier::SUSPICIOUS: standing_color = 0xFFAA00; tier_name = "SUSPICIOUS"; break;
                                case ReputationTier::NEUTRAL: standing_color = 0xAAAAAA; tier_name = "NEUTRAL"; break;
                                case ReputationTier::FAVORED: standing_color = 0xAAFF00; tier_name = "FAVORED"; break;
                                case ReputationTier::FRIENDLY: standing_color = 0x55FF55; tier_name = "FRIENDLY"; break;
                                case ReputationTier::ALLY: standing_color = 0x00FFFF; tier_name = "ALLY"; break;
                            }
                            ncplane_set_fg_rgb(m_inspection_plane, standing_color);
                            ncplane_printf(m_inspection_plane, "%s (%.1f)", tier_name.c_str(), standing);
                            start_y++;
                            ncplane_set_fg_rgb(m_inspection_plane, 0xFFFFFF);
                        }
                    }

                    auto leader_view = registry_.view<FactionLeaderComponent, FactionComponent>();
                    for (auto leader_ent : leader_view) {
                        auto& f_comp = leader_view.get<FactionComponent>(leader_ent);
                        if (f_comp.faction_id == pol->primary_faction) {
                            auto& leader = leader_view.get<FactionLeaderComponent>(leader_ent);
                            ncplane_printf_yx(m_inspection_plane, start_y++, x, "Leader: %s", leader.leader_name.c_str());
                            break;
                        }
                    }
                }
                if (auto* lab = registry_.try_get<ClandestineLabComponent>(m_current_target)) {
                    ncplane_set_fg_rgb(m_inspection_plane, 0xFF55FF);
                    ncplane_putstr_yx(m_inspection_plane, start_y++, x, "--- CLANDESTINE OPERATION ---");
                    ncplane_set_fg_rgb(m_inspection_plane, 0xFFFFFF);
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Status: %s", lab->is_raided ? "RAIDED" : "ACTIVE");
                }

                // [J.4] Show Zone Demographic Data
                if (auto* pos = registry_.try_get<PositionComponent>(m_current_target)) {
                    draw_zone_data(pos->x, pos->y);
                }
            }
            break;
        }
        case InspectionMode::HISTORY: {
            ncplane_set_fg_rgb(m_inspection_plane, 0xAAAAAA);
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, "--- INFORMATION RECORDS ---");
            if (auto* info = registry_.try_get<InformationComponent>(m_current_target)) {
                if (info->records.empty()) {
                    ncplane_putstr_yx(m_inspection_plane, start_y++, x, "No records found.");
                } else {
                    for (const auto& record : info->records) {
                        std::string type_str = "RUMOR";
                        if (record.type == InformationType::PRICE_TIP) type_str = "TIP";
                        else if (record.type == InformationType::GOSSIP) type_str = "GOSSIP";
                        else if (record.type == InformationType::PROPAGANDA) type_str = "PROP";

                        ncplane_printf_yx(m_inspection_plane, start_y++, x, "[%s] %s", type_str.c_str(), record.content_tag.c_str());
                        ncplane_set_fg_rgb(m_inspection_plane, 0x666666);
                        ncplane_printf_yx(m_inspection_plane, start_y++, x, "  Veracity: %.0f%%  Hops: %d", record.veracity * 100.0f, record.hops);
                        ncplane_set_fg_rgb(m_inspection_plane, 0xAAAAAA);
                        if (start_y > 23) break; // Avoid overflow
                    }
                }
            } else {
                ncplane_putstr_yx(m_inspection_plane, start_y++, x, "No information profile.");
            }
            break;
        }
        default: break;
    }
}

void InspectionSystem::draw_zone_data(int world_x, int world_y) {
    auto config_view = registry_.view<WorldConfigComponent>();
    if (config_view.empty()) return;
    int macro_cell_size = config_view.get<WorldConfigComponent>(config_view.front()).macro_cell_size;

    int mx = world_x / macro_cell_size;
    int my = world_y / macro_cell_size;

    auto zone_view = registry_.view<MacroZoneComponent, MacroMarketComponent>();
    for (auto entity : zone_view) {
        const auto& zone = zone_view.get<MacroZoneComponent>(entity);
        if (zone.macro_x == mx && zone.macro_y == my) {
            const auto& market = zone_view.get<MacroMarketComponent>(entity);
            int start_y = 20;
            int x_pos = 2;
            ncplane_set_fg_rgb(m_inspection_plane, 0x00FFCC);
            ncplane_printf_yx(m_inspection_plane, start_y++, x_pos, "DISTRICT: %s", zone.district_name.c_str());
            ncplane_set_fg_rgb(m_inspection_plane, 0x88FFFF);
            ncplane_printf_yx(m_inspection_plane, start_y++, x_pos, "Pressure: %.1f  Attraction: %.1f", zone.pressure, zone.attractiveness);
            ncplane_set_fg_rgb(m_inspection_plane, 0xFFFF88);
            ncplane_printf_yx(m_inspection_plane, start_y++, x_pos, "Avg Wealth: %.1f CR  Crime: %.1f", market.average_wealth, market.crime_rate);
            
            if (!market.material_scarcity.empty()) {
                ncplane_set_fg_rgb(m_inspection_plane, 0xFFAA55);
                std::string scarcity_str = "Scarcity: ";
                for (auto const& [type, val] : market.material_scarcity) {
                    if (val > 1.2f) { 
                        scarcity_str += get_raw_material_name(type).substr(0, 3) + " ";
                    }
                }
                ncplane_putstr_yx(m_inspection_plane, start_y++, x_pos, scarcity_str.c_str());
            }
            break;
        }
    }
}

std::vector<LayerInsight> InspectionSystem::calculate_insights(entt::entity target, SimulationLayer current_view) {
    std::vector<LayerInsight> insights;
    (void)target; (void)current_view;
    return insights;
}

std::string InspectionSystem::get_material_name(MaterialType type) {
    switch(type) {
        case MaterialType::CONCRETE: return "Concrete";
        case MaterialType::FLESH: return "Flesh";
        case MaterialType::STEEL: return "Steel";
        default: return "Unknown";
    }
}

std::string InspectionSystem::get_raw_material_name(RawMaterialType type) {
    switch(type) {
        case RawMaterialType::METAL: return "Metal";
        case RawMaterialType::CHEMICAL: return "Chemical";
        case RawMaterialType::BIOMASS: return "Biomass";
        case RawMaterialType::ELECTRONIC: return "Electronic";
        case RawMaterialType::ENERGY: return "Energy";
        default: return "Unknown";
    }
}

std::string InspectionSystem::get_species_name(SpeciesType type) {
    switch(type) {
        case SpeciesType::HUMAN: return "Human";
        case SpeciesType::SYNTHETIC: return "Synthetic";
        case SpeciesType::CACOGEN: return "Cacogen";
        case SpeciesType::HIERODULE: return "Hierodule";
        default: return "Unknown";
    }
}

std::string InspectionSystem::get_life_stage_name(LifeStage stage) {
    switch(stage) {
        case LifeStage::INFANT: return "Infant";
        case LifeStage::CHILD: return "Child";
        case LifeStage::YOUNG_ADULT: return "Young Adult";
        case LifeStage::ADULT: return "Adult";
        case LifeStage::ELDER: return "Elder";
        case LifeStage::ANCIENT: return "Ancient";
        case LifeStage::AGELESS: return "Ageless";
        default: return "Unknown";
    }
}

uint32_t InspectionSystem::hex_to_rgb(const std::string& hex) {
    if (hex.empty() || hex[0] != '#') return 0xFFFFFF;
    try { return std::stoul(hex.substr(1), nullptr, 16); } catch (...) { return 0xFFFFFF; }
}

std::string InspectionSystem::get_hazard_name(HazardType type) {
    switch(type) {
        case HazardType::TOXIC_GAS: return "Toxic Gas";
        case HazardType::ELECTRICAL: return "Electrical Arc";
        case HazardType::STEAM_VENT: return "Steam Vent";
        case HazardType::BIO_HAZARD: return "Bio-Hazard";
        case HazardType::RAD_ZONE: return "Radiation Zone";
        case HazardType::FIRE: return "Fire";
        default: return "Unknown";
    }
}

} // namespace NeonOubliette::Systems
