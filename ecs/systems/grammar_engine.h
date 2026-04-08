#ifndef NEON_OUBLIETTE_GRAMMAR_ENGINE_H
#define NEON_OUBLIETTE_GRAMMAR_ENGINE_H

#include <string>
#include <vector>
#include <map>
#include <random>
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>
#include "../components/components.h"

namespace NeonOubliette::Systems {

/**
 * @brief [NEW CLASS] Tracery-style recursive grammar engine.
 * Supports #tag# for expansion and <ecs_slot> for data injection.
 */
class GrammarEngine {
public:
    static GrammarEngine& instance();

    /**
     * @brief Load grammar rules from a JSON file.
     * @param path Path to the grammar.json file.
     */
    void load_grammar(const std::string& path);

    /**
     * @brief Recursively expand a tag into a string, injecting ECS data.
     * @param tag The starting tag (e.g., "work_complaint").
     * @param registry The ECS registry.
     * @param entity The entity for which the dialogue is being generated.
     * @return The expanded string.
     */
    std::string expand(const std::string& tag, const entt::registry& registry, entt::entity entity);

    /**
     * @brief Personality-aware expansion. Prefers tag_TAGNAME if the entity has that trait.
     */
    std::string expand_with_personality(const std::string& tag, const entt::registry& registry, entt::entity entity);

    /**
     * @brief [F.1] Assemble multiple topics into a single coherent utterance.
     */
    std::string assemble(const std::vector<std::string>& tags, const entt::registry& registry, entt::entity entity);

private:
    GrammarEngine() : m_rng(std::random_device{}()) {}
    
    std::map<std::string, std::vector<std::string>> m_rules;
    std::mt19937 m_rng;

    std::string process_slots(const std::string& template_str, const entt::registry& registry, entt::entity entity);
    std::string get_ecs_value(const std::string& slot, const entt::registry& registry, entt::entity entity);
    std::string personality_tag_to_string(PersonalityTag tag);
    std::string relationship_tier_to_string(RelationshipTier tier);
    
    /**
     * @brief [F.3] Apply post-processing effects based on speech profile (e.g., glitched text for CACOGEN).
     */
    void apply_profile_effects(std::string& text, const std::string& profile_id);
};

} // namespace NeonOubliette::Systems

#endif // NEON_OUBLIETTE_GRAMMAR_ENGINE_H
