#ifndef NEON_OUBLIETTE_DIALOGUE_SYSTEM_H
#define NEON_OUBLIETTE_DIALOGUE_SYSTEM_H

#include <notcurses/notcurses.h>
#include <entt/entt.hpp>
#include "../component_declarations.h"
#include "../events.h"
#include "system_scheduler.h"
#include <string>
#include <vector>

namespace NeonOubliette {
namespace Systems {

struct DialogueChoice {
    char key;
    std::string text;
};

class DialogueSystem : public ISystem {
public:
    DialogueSystem(entt::registry& registry, struct notcurses* nc_context, entt::dispatcher& event_dispatcher);
    ~DialogueSystem();

    void initialize() override;
    void update(double delta_time) override;

private:
    entt::registry& registry_;
    struct notcurses* nc_context_;
    entt::dispatcher& event_dispatcher_;
    
    struct ncplane* m_dialogue_plane = nullptr;
    bool m_window_visible = false;
    entt::entity m_target_agent = entt::null;
    
    std::string m_agent_name;
    std::string m_agent_title;
    std::string m_current_utterance;
    std::vector<DialogueChoice> m_choices;
    
    uint64_t m_pulse_counter = 0;

    void handleDialogueEvent(const DialogueEvent& event);
    void handleCloseDialogueWindowEvent(const CloseDialogueWindowEvent& event);
    
    void create_dialogue_plane();
    void close_dialogue_window();
    void draw_dialogue_window();
    
    void generate_content();
    void draw_ascii_portrait();
    uint32_t hex_to_rgb(const std::string& hex);
};

} // namespace Systems
} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_DIALOGUE_SYSTEM_H
