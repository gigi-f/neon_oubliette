#include "grammar_engine.h"
#include "../../src/util/profiling.h"
#include <fstream>
#include <regex>
#include <iostream>
#include <nlohmann/json.hpp>
#include <entt/entt.hpp>
#include "../components/components.h"

#include "../components/simulation_layers.h"
#include "../components/lod_components.h"

namespace NeonOubliette::Systems {

GrammarEngine& GrammarEngine::instance() {
    static GrammarEngine inst;
    return inst;
}

void GrammarEngine::load_grammar(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "GrammarEngine: Could not open " << path << std::endl;
        return;
    }

    nlohmann::json j;
    try {
        file >> j;
        for (auto& [key, value] : j.items()) {
            if (value.is_array()) {
                m_rules[key] = value.get<std::vector<std::string>>();
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "GrammarEngine: Error parsing JSON: " << e.what() << std::endl;
    }
}

std::string GrammarEngine::expand(const std::string& tag, const entt::registry& registry, entt::entity entity) {
    ZoneScoped;
    if (m_rules.find(tag) == m_rules.end()) {
        return tag; // Return tag itself if not found
    }

    const auto& options = m_rules.at(tag);
    if (options.empty()) return "";

    std::uniform_int_distribution<size_t> dist(0, options.size() - 1);
    std::string template_str = options[dist(m_rng)];

    return process_slots(template_str, registry, entity);
}

std::string GrammarEngine::process_slots(const std::string& template_str, const entt::registry& registry, entt::entity entity) {
    std::string result = template_str;
    
    // First expand #tags# (recursive grammar)
    static const std::regex tag_regex("#([^#]+)#");
    std::smatch tag_match;
    while (std::regex_search(result, tag_match, tag_regex)) {
        std::string tag_name = tag_match[1].str();
        std::string replacement = expand(tag_name, registry, entity);
        result.replace(tag_match.position(0), tag_match.length(0), replacement);
    }

    // Then inject <ecs_slots> (live data)
    static const std::regex slot_regex("<([^>]+)>");
    std::smatch slot_match;
    while (std::regex_search(result, slot_match, slot_regex)) {
        std::string slot_name = slot_match[1].str();
        std::string replacement = get_ecs_value(slot_name, registry, entity);
        result.replace(slot_match.position(0), slot_match.length(0), replacement);
    }

    return result;
}

std::string GrammarEngine::get_ecs_value(const std::string& slot, const entt::registry& registry, entt::entity entity) {
    if (slot == "agent_name") {
        if (auto* name = registry.try_get<NameComponent>(entity)) {
            return name->name;
        }
        return "Someone";
    }
    
    if (slot == "workplace") {
        if (auto* work = registry.try_get<WorkplaceComponent>(entity)) {
            if (registry.valid(work->building_entity)) {
                if (auto* name = registry.try_get<NameComponent>(work->building_entity)) {
                    return name->name;
                }
                return "the industrial sector";
            }
        }
        return "nowhere";
    }
    
    if (slot == "faction_leader") {
        // 1. Get agent's political faction
        if (auto* pol = registry.try_get<Layer4PoliticalComponent>(entity)) {
            std::string faction_id = pol->primary_faction;
            
            // 2. Find leader where FactionComponent.faction_id matches
            auto leader_view = registry.view<FactionComponent, FactionLeaderComponent>();
            for (auto leader_ent : leader_view) {
                auto& f_comp = leader_view.get<FactionComponent>(leader_ent);
                if (f_comp.faction_id == faction_id) {
                    auto& leader = leader_view.get<FactionLeaderComponent>(leader_ent);
                    return leader.leader_name;
                }
            }
        }
        return "the AGI";
    }

    // [F.2] Work & Socio-Economic Slots
    if (slot == "shift_length") {
        if (auto* contract = registry.try_get<EmploymentContractComponent>(entity)) {
            return std::to_string(contract->shift_length);
        }
        return "eternal";
    }

    if (slot == "pay_rate") {
        float wage_index = 1.0f;
        auto market_view = registry.view<MacroMarketComponent>();
        if (!market_view.empty()) {
            wage_index = market_view.get<MacroMarketComponent>(market_view.front()).wage_index;
        }
        if (auto* contract = registry.try_get<EmploymentContractComponent>(entity)) {
            return std::to_string(static_cast<int>(contract->wage * wage_index));
        }
        return "0";
    }

    if (slot == "scarcity_food") {
        auto view = registry.view<PositionComponent>();
        if (view.contains(entity)) {
            auto& pos = view.get<PositionComponent>(entity);
            int cs = get_chunk_size(registry);
            int cx = pos.x / cs;
            int cy = pos.y / cs;
            auto chunk_view = registry.view<ChunkComponent, MarketDemandComponent>();
            for (auto chunk_ent : chunk_view) {
                auto& chunk = chunk_view.get<ChunkComponent>(chunk_ent);
                if (chunk.chunk_x == cx && chunk.chunk_y == cy) {
                    auto& market = chunk_view.get<MarketDemandComponent>(chunk_ent);
                    if (market.item_type_scarcity.count(1)) {
                        float s = market.item_type_scarcity.at(1);
                        if (s > 2.0f) return "severe";
                        if (s > 1.2f) return "moderate";
                        return "stable";
                    }
                }
            }
        }
        return "normal";
    }

    if (slot == "scarcity_water") {
        auto view = registry.view<PositionComponent>();
        if (view.contains(entity)) {
            auto& pos = view.get<PositionComponent>(entity);
            int cs = get_chunk_size(registry);
            int cx = pos.x / cs;
            int cy = pos.y / cs;
            auto chunk_view = registry.view<ChunkComponent, MarketDemandComponent>();
            for (auto chunk_ent : chunk_view) {
                auto& chunk = chunk_view.get<ChunkComponent>(chunk_ent);
                if (chunk.chunk_x == cx && chunk.chunk_y == cy) {
                    auto& market = chunk_view.get<MarketDemandComponent>(chunk_ent);
                    if (market.item_type_scarcity.count(2)) {
                        float s = market.item_type_scarcity.at(2);
                        if (s > 2.0f) return "critical";
                        if (s > 1.2f) return "low";
                        return "plentiful";
                    }
                }
            }
        }
        return "normal";
    }

    if (slot == "social_status") {
        if (auto* hierarchy = registry.try_get<SocialHierarchyComponent>(entity)) {
            if (hierarchy->status > 0.8f) return "elite";
            if (hierarchy->status > 0.5f) return "middle-tier";
            if (hierarchy->status > 0.2f) return "low-caste";
            return "dreg";
        }
        return "commoner";
    }

    if (slot == "boss_tier") {
        if (auto* contract = registry.try_get<EmploymentContractComponent>(entity)) {
            if (registry.valid(contract->boss_entity)) {
                if (auto* boss_hierarchy = registry.try_get<SocialHierarchyComponent>(contract->boss_entity)) {
                    return boss_hierarchy->class_title;
                }
                return "The Overseer";
            }
        }
        return "the algorithm";
    }

    if (slot == "unemployment_rate") {
        auto market_view = registry.view<MacroMarketComponent>();
        if (!market_view.empty()) {
            float rate = market_view.get<MacroMarketComponent>(market_view.front()).unemployment_rate;
            return std::to_string(static_cast<int>(rate * 100)) + "%";
        }
        return "unknown";
    }

    if (slot == "job_title") {
        if (auto* contract = registry.try_get<EmploymentContractComponent>(entity)) {
            return contract->job_title;
        }
        return "Scavenger";
    }

    if (slot == "coworker_name") {
        if (auto* work = registry.try_get<WorkplaceComponent>(entity)) {
            auto view = registry.view<WorkplaceComponent, NameComponent>();
            for (auto coworker_ent : view) {
                if (coworker_ent != entity && view.get<WorkplaceComponent>(coworker_ent).building_entity == work->building_entity) {
                    return view.get<NameComponent>(coworker_ent).name;
                }
            }
        }
        return "that other laborer";
    }

    if (slot == "friend_name") {
        if (auto* rel = registry.try_get<RelationshipComponent>(entity)) {
            auto mapping_view = registry.view<MacroIdMappingTag>();
            if (!mapping_view.empty()) {
                auto& mapping = registry.get<MacroIdMappingTag>(mapping_view.front()).mapping;
                for (auto const& [id, r] : rel->records) {
                    if (r.tier == RelationshipTier::FRIEND) {
                        auto it = mapping.find(id);
                        if (it != mapping.end() && registry.valid(it->second)) {
                            if (auto* name = registry.try_get<NameComponent>(it->second)) return name->name;
                        }
                    }
                }
            }
        }
        return "an old pal";
    }

    if (slot == "family_member_name") {
        if (auto* rel = registry.try_get<RelationshipComponent>(entity)) {
            auto mapping_view = registry.view<MacroIdMappingTag>();
            if (!mapping_view.empty()) {
                auto& mapping = registry.get<MacroIdMappingTag>(mapping_view.front()).mapping;
                for (auto const& [id, r] : rel->records) {
                    if (r.tier == RelationshipTier::FAMILY) {
                        auto it = mapping.find(id);
                        if (it != mapping.end() && registry.valid(it->second)) {
                            if (auto* name = registry.try_get<NameComponent>(it->second)) return name->name;
                        }
                    }
                }
            }
        }
        return "my kin";
    }

    if (slot == "quota") {
        // Synthetic quota based on entity ID for consistency
        return std::to_string(100 + (static_cast<uint32_t>(entity) % 89));
    }

    if (slot == "religion_name") {
        if (auto* rel = registry.try_get<ReligiosityComponent>(entity)) {
            auto* reg_ptr = registry.ctx().find<ReligionRegistryComponent>();
            if (reg_ptr && reg_ptr->religions.count(rel->religion_id)) {
                return reg_ptr->religions.at(rel->religion_id).name;
            }
        }
        return "the Truth";
    }

    if (slot == "deity") {
        if (auto* rel = registry.try_get<ReligiosityComponent>(entity)) {
            auto* reg_ptr = registry.ctx().find<ReligionRegistryComponent>();
            if (reg_ptr && reg_ptr->religions.count(rel->religion_id)) {
                return reg_ptr->religions.at(rel->religion_id).primary_deity;
            }
        }
        return "the Beyond";
    }

    if (slot == "rumor_tag") {
        if (auto* info = registry.try_get<InformationComponent>(entity)) {
            if (!info->records.empty()) {
                auto& rumor = info->records[rand() % info->records.size()];
                return rumor.content_tag;
            }
        }
        return "something big is happening";
    }
    
    return "<" + slot + ">";
}

std::string GrammarEngine::personality_tag_to_string(PersonalityTag tag) {
    switch(tag) {
        case PersonalityTag::LACONIC: return "LACONIC";
        case PersonalityTag::VERBOSE: return "VERBOSE";
        case PersonalityTag::PARANOID: return "PARANOID";
        case PersonalityTag::AGGRESSIVE: return "AGGRESSIVE";
        case PersonalityTag::SUBSERVIENT: return "SUBSERVIENT";
        default: return "";
    }
}

std::string GrammarEngine::relationship_tier_to_string(RelationshipTier tier) {
    switch(tier) {
        case RelationshipTier::STRANGER: return "STRANGER";
        case RelationshipTier::ACQUAINTANCE: return "ACQUAINTANCE";
        case RelationshipTier::COWORKER: return "COWORKER";
        case RelationshipTier::FRIEND: return "FRIEND";
        case RelationshipTier::FAMILY: return "FAMILY";
        default: return "";
    }
}

std::string GrammarEngine::expand_with_personality(const std::string& tag, const entt::registry& registry, entt::entity entity) {
    // 0. [F.3] Relationship-based Register (Relationship Modifier)
    // Checks relationship with the player to determine register.
    auto player_view = registry.view<PlayerComponent>();
    if (!player_view.empty()) {
        auto player_ent = player_view.front();
        uint64_t player_macro_id = registry.get<PlayerComponent>(player_ent).macro_id;
        
        RelationshipTier tier = RelationshipTier::STRANGER;
        if (auto* rel = registry.try_get<RelationshipComponent>(entity)) {
            auto it = rel->records.find(player_macro_id);
            if (it != rel->records.end()) {
                tier = it->second.tier;
            }
        }
        
        std::string tier_str = relationship_tier_to_string(tier);
        if (!tier_str.empty()) {
            std::string rel_tag = tag + "_" + tier_str;
            if (m_rules.find(rel_tag) != m_rules.end()) {
                return expand(rel_tag, registry, entity);
            }
        }
    }

    // 1. [F.3] Factional Dialect / Speech Profile Preference
    if (auto* profile = registry.try_get<SpeechProfileComponent>(entity)) {
        std::string profile_tag = tag + "_" + profile->profile_id;
        if (m_rules.find(profile_tag) != m_rules.end()) {
            return expand(profile_tag, registry, entity);
        }
    }

    // 2. Personality trait preference
    if (auto* personality = registry.try_get<PersonalityComponent>(entity)) {
        for (auto trait : personality->tags) {
            std::string trait_str = personality_tag_to_string(trait);
            if (!trait_str.empty()) {
                std::string specific_tag = tag + "_" + trait_str;
                if (m_rules.find(specific_tag) != m_rules.end()) {
                    return expand(specific_tag, registry, entity);
                }
            }
        }
    }
    return expand(tag, registry, entity);
}

std::string GrammarEngine::assemble(const std::vector<std::string>& tags, const entt::registry& registry, entt::entity entity) {
    ZoneScoped;
    if (tags.empty()) return "Wait, I don't have much time. What is it?";
    
    std::string result = "";
    for (size_t i = 0; i < tags.size(); ++i) {
        std::string part = expand_with_personality(tags[i], registry, entity);
        if (part.empty() || part == tags[i]) continue; // Avoid leaking raw tags if not found
        
        if (!result.empty()) {
            char last = result.back();
            if (last != '.' && last != '!' && last != '?') {
                result += ". ";
            } else {
                result += " ";
            }
        }
        result += part;
    }

    if (result.empty()) result = "Wait, I don't have much time. What is it?";
    
    // [F.3] Post-processing profile effects (e.g., glitch text)
    if (auto* profile = registry.try_get<SpeechProfileComponent>(entity)) {
        apply_profile_effects(result, profile->profile_id);
    }
    
    return result;
}

void GrammarEngine::apply_profile_effects(std::string& text, const std::string& profile_id) {
    if (profile_id == "CACOGEN") {
        // [F.3] Partially garbled / glitch-text artifacts
        std::string glitched = "";
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        const char* artifacts = "!@#$%^&*()_+-=[]{}|;':\",./<>?~`";
        size_t art_len = strlen(artifacts);

        for (char c : text) {
            float roll = dist(m_rng);
            if (roll < 0.05f) {
                // Phoneme substitution (simplified: random character)
                glitched += artifacts[m_rng() % art_len];
            } else if (roll < 0.08f) {
                // Glitch-text artifact
                glitched += "*static*";
            } else {
                glitched += c;
            }
        }
        text = glitched;
    }
}

} // namespace NeonOubliette::Systems
