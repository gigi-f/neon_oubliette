#include "inspection_system.h"
#include "../components/simulation_layers.h"
#include <chrono>
#include <entt/entt.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>

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

void InspectionSystem::initialize() {
}

void InspectionSystem::update(double delta_time) {
    (void)delta_time;
    if (m_window_visible) {
        draw_inspection_window();
    }
}

void InspectionSystem::create_inspection_plane() {
    if (m_inspection_plane) return;

    struct ncplane_options opts = {};
    opts.rows = 24;
    opts.cols = 60;
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

    for (auto entity : view) {
        const auto& pos = view.get<PositionComponent>(entity);
        if (pos.x == event.x && pos.y == event.y && pos.layer_id == event.layer_id) {
            if (entity != event.player_entity) {
                target = entity;
                break;
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
        event_dispatcher_.trigger(HUDNotificationEvent{"You see: " + name, 1.0f, "#AAAAAA"});
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
    
    ncplane_cursor_move_yx(m_inspection_plane, 0, 0);
    ncplane_box(m_inspection_plane, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0);

    std::string name = "Unknown Entity";
    if (registry_.all_of<NameComponent>(m_current_target)) {
        name = registry_.get<NameComponent>(m_current_target).name;
    }
    ncplane_putstr_yx(m_inspection_plane, 1, 2, ("INSPECTING: " + name).c_str());

    draw_tabs();
    draw_ascii_portrait();
    draw_content();

    ncplane_set_fg_rgb(m_inspection_plane, 0x888888);
    ncplane_putstr_yx(m_inspection_plane, 23, 2, "ESC: close");
}

void InspectionSystem::draw_tabs() {
    const char* tabs[] = {"[Physical]", "[Bio]", "[Mental]", "[Economy]", "[Political]", "[History]"};
    int current_tab_idx = 0;
    switch(m_current_mode) {
        case InspectionMode::GLANCE:
        case InspectionMode::SURFACE_SCAN: current_tab_idx = 0; break;
        case InspectionMode::BIOLOGICAL_AUDIT: current_tab_idx = 1; break;
        case InspectionMode::COGNITIVE_PROFILE: current_tab_idx = 2; break;
        case InspectionMode::FINANCIAL_FORENSICS: current_tab_idx = 3; break;
        case InspectionMode::STRUCTURAL_ANALYSIS: current_tab_idx = 4; break;
        case InspectionMode::FORENSIC: current_tab_idx = 5; break;
        case InspectionMode::SURVEILLANCE: current_tab_idx = 5; break;
        default: current_tab_idx = 0; break;
    }

    int x = 2;
    for (int i = 0; i < 6; ++i) {
        if (i == current_tab_idx) {
            ncplane_set_styles(m_inspection_plane, NCSTYLE_BOLD);
        } else {
            ncplane_set_styles(m_inspection_plane, NCSTYLE_NONE);
        }
        ncplane_putstr_yx(m_inspection_plane, 3, x, tabs[i]);
        x += (int)strlen(tabs[i]) + 1;
    }
    ncplane_set_styles(m_inspection_plane, NCSTYLE_NONE);
}

void InspectionSystem::draw_ascii_portrait() {
    ncplane_putstr_yx(m_inspection_plane, 5, 2, "      .---.    ");
    ncplane_putstr_yx(m_inspection_plane, 6, 2, "     / o o \\   ");
    ncplane_putstr_yx(m_inspection_plane, 7, 2, "     |  >  |   ");
    ncplane_putstr_yx(m_inspection_plane, 8, 2, "     | === |   ");
    ncplane_putstr_yx(m_inspection_plane, 9, 2, "      `---'    ");
}

void InspectionSystem::draw_content() {
    int start_y = 12;
    int x = 2;

    const EntityPrivacyComponent* privacy = registry_.try_get<EntityPrivacyComponent>(m_current_target);
    auto is_masked = [&](SimulationLayer layer) -> bool {
        if (!privacy) return false;
        if (privacy->masks.find(layer) == privacy->masks.end()) return false;
        return privacy->masks.at(layer).concealment_level != ConcealmentLevel::NONE;
    };

    auto draw_mask = [&]() {
        ncplane_putstr_yx(m_inspection_plane, start_y, x, "[DATA ENCRYPTED / MASKED]");
    };

    switch (m_current_mode) {
        case InspectionMode::SURFACE_SCAN: {
            if (is_masked(SimulationLayer::L0_Physics)) {
                draw_mask();
            } else if (registry_.all_of<Layer0PhysicsComponent>(m_current_target)) {
                auto& p = registry_.get<Layer0PhysicsComponent>(m_current_target);
                ncplane_printf_yx(m_inspection_plane, start_y++, x, "Integrity: %.1f%%", p.structural_integrity * 100.0f);
                ncplane_printf_yx(m_inspection_plane, start_y++, x, "Temperature: %.1f C", p.temperature_celsius);
            }
            break;
        }
        case InspectionMode::BIOLOGICAL_AUDIT: {
            if (is_masked(SimulationLayer::L1_Biology)) {
                draw_mask();
            } else if (registry_.all_of<Layer1BiologyComponent>(m_current_target)) {
                auto& bio = registry_.get<Layer1BiologyComponent>(m_current_target);
                ncplane_printf_yx(m_inspection_plane, start_y++, x, "Consciousness: %.1f%%", bio.consciousness_level * 100.0f);
                ncplane_printf_yx(m_inspection_plane, start_y++, x, "Pain: %d/10", bio.pain_level);
            }
            break;
        }
        case InspectionMode::COGNITIVE_PROFILE: {
            if (is_masked(SimulationLayer::L2_Cognitive)) {
                draw_mask();
            } else if (registry_.all_of<Layer2CognitiveComponent>(m_current_target)) {
                auto& cog = registry_.get<Layer2CognitiveComponent>(m_current_target);
                ncplane_printf_yx(m_inspection_plane, start_y++, x, "Dominance: %+.2f", cog.dominance);
            }
            break;
        }
        case InspectionMode::FINANCIAL_FORENSICS: {
            if (is_masked(SimulationLayer::L3_Economic)) {
                draw_mask();
            } else if (registry_.all_of<Layer3EconomicComponent>(m_current_target)) {
                auto& econ = registry_.get<Layer3EconomicComponent>(m_current_target);
                ncplane_printf_yx(m_inspection_plane, start_y++, x, "Cash: %d CR", econ.cash_on_hand);
            }
            break;
        }
        case InspectionMode::STRUCTURAL_ANALYSIS: {
             if (registry_.all_of<Layer4PoliticalComponent>(m_current_target)) {
                 auto& pol = registry_.get<Layer4PoliticalComponent>(m_current_target);
                 ncplane_printf_yx(m_inspection_plane, start_y++, x, "Faction: %s", pol.primary_faction.c_str());
             }
             break;
        }
        default: break;
    }
}

} // namespace NeonOubliette::Systems
