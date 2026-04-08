#ifndef NEON_OUBLIETTE_ECS_COMPONENTS_RELIGION_COMPONENTS_H
#define NEON_OUBLIETTE_ECS_COMPONENTS_RELIGION_COMPONENTS_H

#include <string>
#include <vector>
#include <map>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/map.hpp>
#include <entt/entt.hpp>
#include "nlohmann/json.hpp"
#include "base_types.h"

namespace NeonOubliette {

enum class DogmaTag : uint8_t {
    ASCETIC,
    HEDONIST,
    HIERARCHICAL,
    EGALITARIAN,
    XENO_REVERENT,
    TECHNOLOGICAL,
    NATURALISTIC,
    NIHILISTIC
};

enum class ReligionStance : uint8_t {
    PATRON,
    NEUTRAL,
    SUPPRESSOR
};

// nlohmann/json conversion for DogmaTag
NLOHMANN_JSON_SERIALIZE_ENUM(DogmaTag, {
    {DogmaTag::ASCETIC, "ASCETIC"},
    {DogmaTag::HEDONIST, "HEDONIST"},
    {DogmaTag::HIERARCHICAL, "HIERARCHICAL"},
    {DogmaTag::EGALITARIAN, "EGALITARIAN"},
    {DogmaTag::XENO_REVERENT, "XENO_REVERENT"},
    {DogmaTag::TECHNOLOGICAL, "TECHNOLOGICAL"},
    {DogmaTag::NATURALISTIC, "NATURALISTIC"},
    {DogmaTag::NIHILISTIC, "NIHILISTIC"}
})

NLOHMANN_JSON_SERIALIZE_ENUM(ReligionStance, {
    {ReligionStance::PATRON, "PATRON"},
    {ReligionStance::NEUTRAL, "NEUTRAL"},
    {ReligionStance::SUPPRESSOR, "SUPPRESSOR"}
})

/**
 * @brief [H.5] Defines a faction's stance toward specific religions.
 */
struct FactionReligionStanceComponent {
    std::map<std::string, ReligionStance> stances; // ReligionID -> Stance
    
    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("stances", stances));
    }
};

/**
 * @brief [H.5] Tracks a religion's aggregate political influence and suppressed tension.
 */
struct ReligionInfluenceComponent {
    float global_influence = 0.0f;
    float suppression_tension = 0.0f; // Accumulates if suppressed, can spawn a new faction
    
    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("global_influence", global_influence),
           cereal::make_nvp("suppression_tension", suppression_tension));
    }
};

/**
 * @brief Singleton component for global religion political state.
 */
struct ReligionInfluenceRegistry {
    std::map<std::string, ReligionInfluenceComponent> religions; // ReligionID -> Stats
    
    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("religions", religions));
    }
};

struct ReligionRecord {
    std::string religion_id;
    std::string name;
    std::string primary_deity; // or principle
    std::vector<DogmaTag> dogma_tags;
    std::map<std::string, float> faction_affinity_modifiers; // FactionID -> modifier
    std::vector<uint32_t> holy_day_ticks; // Simple schedule (e.g., modulo world ticks)
    std::string home_building_type; // e.g., "TEMPLE", "SHRINE"
    std::string color_hex = "#FFFFFF";
    char glyph = '+';

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("religion_id", religion_id),
           cereal::make_nvp("name", name),
           cereal::make_nvp("primary_deity", primary_deity),
           cereal::make_nvp("dogma_tags", dogma_tags),
           cereal::make_nvp("faction_affinity_modifiers", faction_affinity_modifiers),
           cereal::make_nvp("holy_day_ticks", holy_day_ticks),
           cereal::make_nvp("home_building_type", home_building_type),
           cereal::make_nvp("color_hex", color_hex),
           cereal::make_nvp("glyph", glyph));
    }
};

// nlohmann/json conversion for ReligionRecord
inline void from_json(const nlohmann::json& j, ReligionRecord& r) {
    j.at("religion_id").get_to(r.religion_id);
    j.at("name").get_to(r.name);
    j.at("primary_deity").get_to(r.primary_deity);
    j.at("dogma_tags").get_to(r.dogma_tags);
    if (j.contains("faction_affinity_modifiers")) {
        j.at("faction_affinity_modifiers").get_to(r.faction_affinity_modifiers);
    }
    if (j.contains("holy_day_ticks")) {
        j.at("holy_day_ticks").get_to(r.holy_day_ticks);
    }
    j.at("home_building_type").get_to(r.home_building_type);
    if (j.contains("color_hex")) {
        j.at("color_hex").get_to(r.color_hex);
    }
    if (j.contains("glyph")) {
        std::string g = j.at("glyph").get<std::string>();
        if (!g.empty()) r.glyph = g[0];
    }
}

/**
 * @brief [H.2] Per-agent religiosity data.
 */
struct ReligiosityComponent {
    std::string religion_id;
    float devotion = 0.0f; // 0 to 100
    uint64_t last_worship_tick = 0;
    bool is_schismatic = false;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("religion_id", religion_id),
           cereal::make_nvp("devotion", devotion),
           cereal::make_nvp("last_worship_tick", last_worship_tick),
           cereal::make_nvp("is_schismatic", is_schismatic));
    }
};

/**
 * @brief [H.3] Tag for buildings that serve as places of worship.
 */
struct WorshipPlaceComponent {
    std::string religion_id;
    int capacity = 10;
    
    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("religion_id", religion_id),
           cereal::make_nvp("capacity", capacity));
    }
};

/**
 * @brief [H.4] Tracks an active religious procession.
 */
struct ProcessionComponent {
    std::string religion_id;
    uint32_t holy_day_id = 0;
    entt::entity leader = entt::null;
    std::vector<entt::entity> followers;
    std::vector<PositionComponent> route;
    bool is_active = true;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("religion_id", religion_id),
           cereal::make_nvp("holy_day_id", holy_day_id),
           cereal::make_nvp("leader", leader),
           cereal::make_nvp("followers", followers),
           cereal::make_nvp("route", route),
           cereal::make_nvp("is_active", is_active));
    }
};

/**
 * @brief [H.4] Tag for an agent currently participating in a procession.
 */
struct InProcessionComponent {
    std::string religion_id;
    entt::entity procession_entity = entt::null;
    bool is_leader = false;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("religion_id", religion_id),
           cereal::make_nvp("procession_entity", procession_entity),
           cereal::make_nvp("is_leader", is_leader));
    }
};

/**
 * @brief Singleton component to store all religion definitions loaded from JSON.
 */
struct ReligionRegistryComponent {
    std::map<std::string, ReligionRecord> religions;
    
    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("religions", religions));
    }
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_COMPONENTS_RELIGION_COMPONENTS_H
