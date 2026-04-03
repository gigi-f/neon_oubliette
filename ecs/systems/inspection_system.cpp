#include "inspection_system.h"
#include "../components/simulation_layers.h"
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
    // Proximity Targeting (Radius 1)
    auto view = registry_.view<PositionComponent>();
    entt::entity target = entt::null;
    float best_dist = 999.0f;

    for (auto entity : view) {
        if (entity == event.player_entity) continue;
        const auto& pos = view.get<PositionComponent>(entity);
        if (pos.layer_id == event.layer_id) {
            int dx = pos.x - event.x;
            int dy = pos.y - event.y;
            float dist_sq = (float)(dx * dx + dy * dy);
            
            // Allow targeting entities within radius 1 (including diagonals)
            if (dist_sq <= 2.0f) {
                if (dist_sq < best_dist) {
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

    if (event.mode == InspectionMode::GLANCE) {
        std::string name = "Something";
        if (registry_.all_of<NameComponent>(target)) {
            name = registry_.get<NameComponent>(target).name;
        }
        event_dispatcher_.trigger(HUDNotificationEvent{"Vague reading: " + name, 1.0f, "#AAAAAA"});
        return;
    }

    m_current_target = target;
    m_current_mode = event.mode;
    m_window_visible = true;
    create_inspection_plane();
}

void InspectionSystem::draw_inspection_window() {
    if (!m_inspection_plane || m_current_target == entt::null) return;

    ncplane_erase(m_inspection_plane);
    
    // Choose accent color based on mode
    uint32_t accent_color = 0xFFFFFF;
    switch(m_current_mode) {
        case InspectionMode::SURFACE_SCAN: accent_color = 0x55FFFF; break;
        case InspectionMode::BIOLOGICAL_AUDIT: accent_color = 0x00FF00; break;
        case InspectionMode::COGNITIVE_PROFILE: accent_color = 0xFF00FF; break;
        case InspectionMode::FINANCIAL_FORENSICS: accent_color = 0xFFFF00; break;
        case InspectionMode::STRUCTURAL_ANALYSIS: accent_color = 0xFF5555; break;
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

    // Simulation Pulse
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
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, " | [$$$$$$$$$$]  |");
            ncplane_putstr_yx(m_inspection_plane, start_y++, x, " | [$$$$      ]  |");
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

    // Draw Cross-Layer Insights
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
            } else if (auto* p = registry_.try_get<Layer0PhysicsComponent>(m_current_target)) {
                ncplane_printf_yx(m_inspection_plane, start_y++, x, "Material: %s", get_material_name(p->material).c_str());
                ncplane_printf_yx(m_inspection_plane, start_y++, x, "Integrity: %.1f%%", p->structural_integrity * 100.0f);
                ncplane_printf_yx(m_inspection_plane, start_y++, x, "Temp: %.1f C", p->temperature_celsius);
                
                // Add Local Weather
                auto weather_view = registry_.view<WeatherComponent>();
                if (!weather_view.empty()) {
                    auto& weather = weather_view.get<WeatherComponent>(weather_view.front());
                    std::string state_str = "Unknown";
                    switch(weather.state) {
                        case WeatherState::CLEAR: state_str = "Clear Skies"; break;
                        case WeatherState::OVERCAST: state_str = "Overcast"; break;
                        case WeatherState::RAIN: state_str = "Acidic Rain"; break;
                        case WeatherState::HEAVY_RAIN: state_str = "Heavy Acid Downpour"; break;
                        case WeatherState::ACID_RAIN: state_str = "ACID STORM"; break;
                        case WeatherState::SMOG: state_str = "Industrial Smog"; break;
                        case WeatherState::ELECTRICAL_STORM: state_str = "Lightning Grid"; break;
                    }
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Weather: %s (Intens:%.2f)", state_str.c_str(), weather.intensity);
                }
            }
            break;
        }
        case InspectionMode::BIOLOGICAL_AUDIT: {
            if (is_masked(SimulationLayer::L1_Biology)) {
                draw_mask();
            } else if (auto* bio = registry_.try_get<Layer1BiologyComponent>(m_current_target)) {
                ncplane_printf_yx(m_inspection_plane, start_y++, x, "Species: %s", get_species_name(bio->species).c_str());
                ncplane_printf_yx(m_inspection_plane, start_y++, x, "Consciousness: %.1f%%", bio->consciousness_level * 100.0f);
                ncplane_printf_yx(m_inspection_plane, start_y++, x, "Pain: %d/10", bio->pain_level);
                ncplane_printf_yx(m_inspection_plane, start_y++, x, "Vitals: HR:%d BPM | O2:%.1f%%", (int)bio->vitals.heart_rate, bio->vitals.oxygen_saturation);
                
                if (auto* needs = registry_.try_get<NeedsComponent>(m_current_target)) {
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Hydration: %.1f%%", needs->thirst);
                    ncplane_printf_yx(m_inspection_plane, start_y++, x, "Nutrition: %.1f%%", needs->hunger);
                }
            }
            break;
        }
        case InspectionMode::COGNITIVE_PROFILE: {
            if (is_masked(SimulationLayer::L2_Cognitive)) {
                draw_mask();
            } else if (auto* cog = registry_.try_get<Layer2CognitiveComponent>(m_current_target)) {
                ncplane_printf_yx(m_inspection_plane, start_y++, x, "Pleasure:   %+.2f", cog->pleasure);
                ncplane_printf_yx(m_inspection_plane, start_y++, x, "Arousal:    %+.2f", cog->arousal);
                ncplane_printf_yx(m_inspection_plane, start_y++, x, "Dominance:   %+.2f", cog->dominance);
            }
            break;
        }
        case InspectionMode::FINANCIAL_FORENSICS: {
            if (is_masked(SimulationLayer::L3_Economic)) {
                draw_mask();
            } else if (auto* econ = registry_.try_get<Layer3EconomicComponent>(m_current_target)) {
                ncplane_printf_yx(m_inspection_plane, start_y++, x, "Liquid Cash: %d CR", econ->cash_on_hand);
                ncplane_printf_yx(m_inspection_plane, start_y++, x, "Credit Score: %d", econ->credit_score);
            }
            break;
        }
        case InspectionMode::STRUCTURAL_ANALYSIS: {
             if (is_masked(SimulationLayer::L4_Political)) {
                 draw_mask();
             } else if (auto* pol = registry_.try_get<Layer4PoliticalComponent>(m_current_target)) {
                 ncplane_printf_yx(m_inspection_plane, start_y++, x, "Primary Faction: %s", pol->primary_faction.c_str());
                 ncplane_printf_yx(m_inspection_plane, start_y++, x, "Loyalty: %.1f%%", pol->faction_loyalty * 100.0f);
                 ncplane_printf_yx(m_inspection_plane, start_y++, x, "Clearance Rank: %s", pol->rank.empty() ? "NONE" : pol->rank.c_str());
             }
             break;
        }
        default: break;
    }
}

std::vector<LayerInsight> InspectionSystem::calculate_insights(entt::entity target, SimulationLayer current_view) {
    std::vector<LayerInsight> insights;
    
    auto* l0 = registry_.try_get<Layer0PhysicsComponent>(target);
    auto* l1 = registry_.try_get<Layer1BiologyComponent>(target);
    auto* l2 = registry_.try_get<Layer2CognitiveComponent>(target);
    auto* l3 = registry_.try_get<Layer3EconomicComponent>(target);
    auto* l4 = registry_.try_get<Layer4PoliticalComponent>(target);

    switch (current_view) {
        case SimulationLayer::L0_Physics:
            if (l1 && l1->pain_level > 5) 
                insights.push_back({"[STRESS: NEURO-TRAUMA DETECTED]", "#FF5555"});
            if (l0 && l0->temperature_celsius > 40.0f)
                insights.push_back({"[THERMAL OVERLOAD: AMBIENT HEAT]", "#FF5555"});
            break;
        case SimulationLayer::L1_Biology:
            if (l1 && l1->pain_level > 7)
                insights.push_back({"[SHOCK: L1 TRAUMATIC INTERFACE]", "#FF5555"});
            if (l3 && l3->cash_on_hand < 10)
                insights.push_back({"[MALNUTRITION: L3 RESOURCE DEFICIT]", "#FFAA00"});
            if (l0 && l0->temperature_celsius > 42.0f)
                insights.push_back({"[PROTEIN DENATURATION: L0 HEAT]", "#FF0000"});
            break;
        case SimulationLayer::L2_Cognitive:
            if (l1 && l1->pain_level > 3)
                insights.push_back({"[COGNITIVE LOAD: L1 CHRONIC PAIN]", "#FFAA00"});
            if (l3 && l3->credit_score < 400)
                insights.push_back({"[ANXIETY: L3 DEBT BURDEN]", "#FF00FF"});
            if (l4 && l4->faction_loyalty < 0.2f)
                insights.push_back({"[DISSENT: L4 IDEOLOGICAL SHIFT]", "#FF00FF"});
            break;
        case SimulationLayer::L3_Economic:
            if (l2 && l2->pleasure < -0.5f)
                insights.push_back({"[STAGNATION: L2 DEPRESSIVE STATE]", "#AAAAAA"});
            if (l4 && l4->rank.empty())
                insights.push_back({"[REVENUE DEFICIT: L4 SYSTEMIC EXCLUSION]", "#FF5555"});
            break;
        case SimulationLayer::L4_Political:
            if (l3 && l3->cash_on_hand > 1000)
                insights.push_back({"[POWER: L3 CAPITAL DENSITY]", "#FFFF00"});
            if (l2 && l2->arousal > 0.6f)
                insights.push_back({"[VOLATILITY: L2 EMOTIONAL INSTABILITY]", "#FF5555"});
            break;
        default: break;
    }
    return insights;
}

std::string InspectionSystem::get_material_name(MaterialType type) {
    switch(type) {
        case MaterialType::CONCRETE: return "Concrete";
        case MaterialType::FLESH: return "Biological Matter";
        case MaterialType::STEEL: return "Hardened Steel";
        case MaterialType::PLASTIC: return "Polymer";
        case MaterialType::GLASS: return "Glass";
        case MaterialType::ELECTRONICS: return "Silicon/Components";
        default: return "Unknown";
    }
}

std::string InspectionSystem::get_species_name(SpeciesType type) {
    switch(type) {
        case SpeciesType::HUMAN: return "Human";
        case SpeciesType::RAT: return "Rodent";
        case SpeciesType::SYNTHETIC: return "Android";
        case SpeciesType::CANINE: return "Canine";
        case SpeciesType::FELINE: return "Feline";
        default: return "Unclassified";
    }
}

uint32_t InspectionSystem::hex_to_rgb(const std::string& hex) {
    if (hex.empty() || hex[0] != '#') return 0xFFFFFF;
    try {
        return std::stoul(hex.substr(1), nullptr, 16);
    } catch (...) {
        return 0xFFFFFF;
    }
}

} // namespace NeonOubliette::Systems
