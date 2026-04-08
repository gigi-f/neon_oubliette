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

enum class DialogueTopicCategory {
    GREETING,
    WORK,
    FAMILY,
    LORE,
    RUMOR,
    LEAVE,
    INK_CHOICE,
    BUY_INFO,
    FAMILY_CHECKIN,
    THREATEN,
    SOLIDARITY,
    LIE,
    MEMORY_RECALL,
    RELIGION,
    LEAK_INFO,
    LEAK_SELECT,
    VERIFY_INFO,
    VERIFY_SELECT,
    SET_BROADCAST,
    BROADCAST_SELECT,
    GIVE_CREDITS,
    GIVE_ITEM,
    PARDON
};

struct DialogueChoice {
    char key;
    std::string text;
    DialogueTopicCategory category;
    int ink_choice_index = -1;
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
    int m_selected_rumor_index = -1;

    void handleDialogueEvent(const DialogueEvent& event);
    void handleDialogueChoiceEvent(const DialogueChoiceEvent& event);
    void handleCloseDialogueWindowEvent(const CloseDialogueWindowEvent& event);
    
    void create_dialogue_plane();
    void close_dialogue_window();
    void draw_dialogue_window();
    
    void generate_content(DialogueTopicCategory category = DialogueTopicCategory::GREETING);
    void record_interaction(DialogueTopicCategory category);
    std::string category_to_string(DialogueTopicCategory cat);
    void draw_ascii_portrait();
    uint32_t hex_to_rgb(const std::string& hex);

    DialogueTopicCategory get_atom_category(const std::string& tag);
    std::string atom_tag_to_grammar_tag(const std::string& tag);
};

} // namespace Systems
} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_DIALOGUE_SYSTEM_H
