#include "dialogue_system.h"
#include "../components/components.h"
#include "../components/simulation_layers.h"
#include <chrono>
#include <entt/entt.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <cstring>

namespace NeonOubliette::Systems {

DialogueSystem::DialogueSystem(entt::registry& registry, struct notcurses* nc_context,
                                   entt::dispatcher& event_dispatcher)
    : registry_(registry), nc_context_(nc_context), event_dispatcher_(event_dispatcher) {
    event_dispatcher_.sink<DialogueEvent>().connect<&DialogueSystem::handleDialogueEvent>(this);
    event_dispatcher_.sink<CloseDialogueWindowEvent>().connect<&DialogueSystem::handleCloseDialogueWindowEvent>(this);
}

DialogueSystem::~DialogueSystem() {
    if (m_dialogue_plane) {
        ncplane_destroy(m_dialogue_plane);
    }
}

void DialogueSystem::initialize() {}

void DialogueSystem::update(double delta_time) {
    (void)delta_time;
    if (m_window_visible) {
        m_pulse_counter++;
        draw_dialogue_window();
    }
}

void DialogueSystem::create_dialogue_plane() {
    if (m_dialogue_plane) return;

    struct ncplane_options opts = {};
    opts.rows = 26;
    opts.cols = 64;
    opts.y = 2;
    opts.x = 20; 
    opts.name = "Dialogue Plane";

    m_dialogue_plane = ncplane_create(notcurses_stdplane(nc_context_), &opts);
    
    uint64_t channels = 0;
    ncchannels_set_bg_rgb(&channels, 0x1A1C22);
    ncchannels_set_fg_rgb(&channels, 0xFFFFFF);
    ncplane_set_base(m_dialogue_plane, " ", 0, channels);
}

void DialogueSystem::close_dialogue_window() {
    if (m_dialogue_plane) {
        ncplane_destroy(m_dialogue_plane);
        m_dialogue_plane = nullptr;
    }
    m_window_visible = false;
    m_target_agent = entt::null;

    // Update singleton state
    auto view = registry_.view<DialogueStateComponent>();
    if (!view.empty()) {
        auto& state = view.get<DialogueStateComponent>(view.front());
        state.is_open = false;
        state.target_agent = entt::null;
    }
}

void DialogueSystem::handleCloseDialogueWindowEvent(const CloseDialogueWindowEvent& event) {
    (void)event;
    close_dialogue_window();
}

void DialogueSystem::handleDialogueEvent(const DialogueEvent& event) {
    if (!registry_.valid(event.target_agent)) return;

    m_target_agent = event.target_agent;
    m_window_visible = true;

    // Set singleton state
    auto view = registry_.view<DialogueStateComponent>();
    if (view.empty()) {
        auto singleton = registry_.create();
        registry_.emplace<DialogueStateComponent>(singleton, true, m_target_agent);
    } else {
        auto& state = view.get<DialogueStateComponent>(view.front());
        state.is_open = true;
        state.target_agent = m_target_agent;
    }

    generate_content();
    create_dialogue_plane();
}

void DialogueSystem::generate_content() {
    m_agent_name = "Citizen";
    if (registry_.all_of<NameComponent>(m_target_agent)) {
        m_agent_name = registry_.get<NameComponent>(m_target_agent).name;
    }
    
    m_agent_title = "Unknown Status";
    if (auto* hierarchy = registry_.try_get<SocialHierarchyComponent>(m_target_agent)) {
        m_agent_title = hierarchy->class_title;
    }

    // Determine utterance based on needs/faction/status
    m_current_utterance = "Wait, I don't have much time. What is it?";
    
    if (auto* needs = registry_.try_get<NeedsComponent>(m_target_agent)) {
        if (needs->hunger < 30.0f) {
            m_current_utterance = "I'm starving. Do you have any credits for food?";
        } else if (needs->thirst < 30.0f) {
            m_current_utterance = "The water reclamation is down again. My throat is parched.";
        } else if (needs->frustration > 70.0f) {
            m_current_utterance = "Get out of my face before I do something we both regret.";
        }
    }
    
    if (auto* xeno = registry_.try_get<XenoComponent>(m_target_agent)) {
        if (xeno->type == XenoType::HIERODULE) {
            m_current_utterance = "The stars whisper of your arrival, traveler. Why seek the hollow truth?";
        } else if (xeno->type == XenoType::CACOGEN) {
            m_current_utterance = "[UNINTELLIGIBLE CHITTERING / FEEDBACK]";
        }
    }

    // Default Choices
    m_choices.clear();
    m_choices.push_back({'a', "Who are you?"});
    m_choices.push_back({'b', "What's the word on the street?"});
    m_choices.push_back({'c', "Never mind."});
}

void DialogueSystem::draw_dialogue_window() {
    if (!m_dialogue_plane || m_target_agent == entt::null) return;

    ncplane_erase(m_dialogue_plane);
    
    uint32_t accent_color = 0x55AAFF; // Dialogue Blue
    
    ncplane_set_fg_rgb(m_dialogue_plane, accent_color);
    ncplane_cursor_move_yx(m_dialogue_plane, 0, 0);
    ncplane_box(m_dialogue_plane, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0);

    // Title
    ncplane_set_fg_rgb(m_dialogue_plane, 0xFFFFFF);
    ncplane_putstr_yx(m_dialogue_plane, 1, 2, ("DIALOGUE: " + m_agent_name).c_str());
    ncplane_set_fg_rgb(m_dialogue_plane, 0x888888);
    ncplane_putstr_yx(m_dialogue_plane, 2, 2, ("[" + m_agent_title + "]").c_str());

    // Simulation Pulse
    const char pulse_chars[] = {'-', '\\', '|', '/'};
    char pulse = pulse_chars[(m_pulse_counter / 10) % 4];
    ncplane_set_fg_rgb(m_dialogue_plane, accent_color);
    ncplane_printf_yx(m_dialogue_plane, 1, 60, "[%c]", pulse);

    draw_ascii_portrait();
    
    // Draw Utterance (with wrapping)
    ncplane_set_fg_rgb(m_dialogue_plane, 0x00FF00); // NPC Green
    int start_y = 12;
    int x = 4;
    int max_width = 56;
    
    // Very simple wrapping
    std::string text = "\"" + m_current_utterance + "\"";
    size_t pos = 0;
    while (pos < text.length()) {
        size_t len = std::min((size_t)max_width, text.length() - pos);
        ncplane_putnstr_yx(m_dialogue_plane, start_y++, x, len, text.substr(pos, len).c_str());
        pos += len;
    }

    // Draw Choices
    start_y++;
    for (const auto& choice : m_choices) {
        ncplane_set_fg_rgb(m_dialogue_plane, 0xFFFFFF);
        ncplane_printf_yx(m_dialogue_plane, start_y++, x, "[%c] %s", choice.key, choice.text.c_str());
    }

    ncplane_set_fg_rgb(m_dialogue_plane, 0x888888);
    ncplane_putstr_yx(m_dialogue_plane, 25, 2, "ESC: close");
}

void DialogueSystem::draw_ascii_portrait() {
    int start_y = 4;
    int x = 4;
    
    uint32_t color = 0x00FF00; // Agent color
    ncplane_set_fg_rgb(m_dialogue_plane, color);

    auto* xeno_ptr = registry_.try_get<XenoComponent>(m_target_agent);
    if (xeno_ptr) {
        if (xeno_ptr->type == XenoType::HIERODULE) {
            ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "      .---.      ");
            ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "     / / \\ \\     ");
            ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "    | |/ \\| |    ");
            ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "    | |\\ /| |    ");
            ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "     \\ \\ / /     ");
            ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "      '---'      ");
        } else {
            ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "     <><><>      ");
            ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "    <  !!  >     ");
            ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "     < VV >      ");
            ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "    <      >     ");
            ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "     <><><>      ");
        }
        return;
    }

    // Default Human/Citizen portrait
    ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "      .---.");
    ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "     / @ @ \\");
    ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "    (   ^   )");
    ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "     \\  -  /");
    ncplane_putstr_yx(m_dialogue_plane, start_y++, x, "    .-\'---\'-.");
}

uint32_t DialogueSystem::hex_to_rgb(const std::string& hex) {
    if (hex.empty() || hex[0] != '#') return 0xFFFFFF;
    try {
        return std::stoul(hex.substr(1), nullptr, 16);
    } catch (...) {
        return 0xFFFFFF;
    }
}

} // namespace NeonOubliette::Systems
