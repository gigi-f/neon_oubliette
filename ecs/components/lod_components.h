#ifndef NEON_OUBLIETTE_ECS_COMPONENTS_LOD_COMPONENTS_H
#define NEON_OUBLIETTE_ECS_COMPONENTS_LOD_COMPONENTS_H

#include "simulation_layers.h"
#include "components.h"
#include <string>
#include <vector>
#include <map>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/utility.hpp>

namespace NeonOubliette {

/**
 * @brief Statistical record for an agent that is currently out-of-range (Warm/Cold).
 *        Used to "materialize" the agent back into an ECS entity when the player approaches.
 */
struct MacroRelationshipRecord {
    std::string target_name; // Fallback
    uint64_t target_macro_id = 0;
    RelationshipTier tier = RelationshipTier::STRANGER;
    float affinity = 0.0f;
    bool shared_home = false;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("target_name", target_name),
           cereal::make_nvp("target_macro_id", target_macro_id),
           cereal::make_nvp("tier", tier),
           cereal::make_nvp("affinity", affinity),
           cereal::make_nvp("shared_home", shared_home));
    }
};

struct MacroAgentRecord {
    std::string name;
    uint64_t macro_id = 0;
    std::string archetype; // "Citizen", "Guard", etc.
    SpeciesType species = SpeciesType::HUMAN;
    int x = 0;
    int y = 0;
    int layer_id = 0;
    float hunger = 100.0f;
    float thirst = 100.0f;
    float frustration = 0.0f;
    float socialization = 100.0f;
    float consciousness = 1.0f;
    int cash_on_hand = 100;
    std::string faction_id;
    std::string speech_profile; // [F.3]
    uint64_t last_tick_turn = 0;
    
    // Age & Life Stage (Phase J.1)
    uint32_t age_years = 25;
    uint32_t age_ticks = 0;
    LifeStage life_stage = LifeStage::ADULT;
    float biological_wear = 0.0f;

    // Relationships (Phase 4.4 Social Graph)
    std::vector<MacroRelationshipRecord> relationships;

    // Xeno Data (Phase 4.5)
    bool is_xeno = false;
    XenoType xeno_type = XenoType::CACOGEN;
    std::string xeno_origin = "Orbital";

    // Social Hierarchy (Phase 4.4)
    float status = 0.5f;
    std::string class_title = "Citizen";
    bool is_autonomous = true;

    // Persistent locations for behavioral routine
    int home_x = -1;
    int home_y = -1;
    int home_layer = 0;
    int work_x = -1;
    int work_y = -1;
    int work_layer = 0;

    std::map<std::string, uint64_t> portfolio; // Ticker -> Shares
    std::vector<PersonalityTag> personality_tags; // [F.1]
    std::vector<InformationRecord> records; // [F.8]

    // Crime Data (Phase 6.1) [I.1]
    float boldness = 50.0f;
    bool is_active_criminal = false;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("name", name),
           cereal::make_nvp("macro_id", macro_id),
           cereal::make_nvp("archetype", archetype),
           cereal::make_nvp("species", species),
           cereal::make_nvp("x", x),
           cereal::make_nvp("y", y),
           cereal::make_nvp("layer_id", layer_id),
           cereal::make_nvp("hunger", hunger),
           cereal::make_nvp("thirst", thirst),
           cereal::make_nvp("frustration", frustration),
           cereal::make_nvp("socialization", socialization),
           cereal::make_nvp("consciousness", consciousness),
           cereal::make_nvp("cash_on_hand", cash_on_hand),
           cereal::make_nvp("faction_id", faction_id),
           cereal::make_nvp("speech_profile", speech_profile),
           cereal::make_nvp("last_tick_turn", last_tick_turn),
           cereal::make_nvp("age_years", age_years),
           cereal::make_nvp("age_ticks", age_ticks),
           cereal::make_nvp("life_stage", life_stage),
           cereal::make_nvp("biological_wear", biological_wear),
           cereal::make_nvp("relationships", relationships),
           cereal::make_nvp("is_xeno", is_xeno),
           cereal::make_nvp("xeno_type", xeno_type),
           cereal::make_nvp("xeno_origin", xeno_origin),
           cereal::make_nvp("status", status),
           cereal::make_nvp("class_title", class_title),
           cereal::make_nvp("is_autonomous", is_autonomous),
           cereal::make_nvp("home_x", home_x),
           cereal::make_nvp("home_y", home_y),
           cereal::make_nvp("home_layer", home_layer),
           cereal::make_nvp("work_x", work_x),
           cereal::make_nvp("work_y", work_y),
           cereal::make_nvp("work_layer", work_layer),
           cereal::make_nvp("portfolio", portfolio),
           cereal::make_nvp("personality_tags", personality_tags),
           cereal::make_nvp("records", records),
           cereal::make_nvp("boldness", boldness),
           cereal::make_nvp("is_active_criminal", is_active_criminal));
    }
};

struct MacroShopRecord {
    std::string name = "Shop";
    int x = 0;
    int y = 0;
    int layer_id = 0;

    bool has_renderable = false;
    char glyph = '$';
    std::string color = "#FFFF00";
    bool has_obstacle = false;

    bool has_building = false;
    BuildingComponent building;
    bool has_size = false;
    SizeComponent size;

    bool has_container = false;
    bool container_is_open = false;
    bool container_is_locked = false;
    std::vector<MacroObjectRecord> stock_items;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("name", name),
           cereal::make_nvp("x", x),
           cereal::make_nvp("y", y),
           cereal::make_nvp("layer_id", layer_id),
           cereal::make_nvp("has_renderable", has_renderable),
           cereal::make_nvp("glyph", glyph),
           cereal::make_nvp("color", color),
           cereal::make_nvp("has_obstacle", has_obstacle),
           cereal::make_nvp("has_building", has_building),
           cereal::make_nvp("building", building),
           cereal::make_nvp("has_size", has_size),
           cereal::make_nvp("size", size),
           cereal::make_nvp("has_container", has_container),
           cereal::make_nvp("container_is_open", container_is_open),
           cereal::make_nvp("container_is_locked", container_is_locked),
           cereal::make_nvp("stock_items", stock_items));
    }
};

/**
 * @brief Attached to a macro-tile entity to manage its streaming state.
 */
struct ChunkComponent {
    int chunk_x = 0;
    int chunk_y = 0;
    bool is_hot = false; // Fully materialized (entities exist)
    bool is_warm = false; // Statistical (only MacroAgentRecords exist)
    std::vector<MacroAgentRecord> stored_agents;
    std::vector<MacroShopRecord> stored_shops;
    std::vector<entt::entity> macro_zones; // Macro-zones belonging to this 40x40 chunk
    
    /**
     * @brief [NEW] Interior map caching. Maps building stable (x,y) to its interior state.
     */
    std::map<std::pair<int, int>, BuildingInteriorComponent> building_interiors;

    ChunkComponent() = default;
    ChunkComponent(int x, int y, bool hot, bool warm) 
        : chunk_x(x), chunk_y(y), is_hot(hot), is_warm(warm) {}

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("chunk_x", chunk_x),
           cereal::make_nvp("chunk_y", chunk_y),
           cereal::make_nvp("is_hot", is_hot),
           cereal::make_nvp("is_warm", is_warm),
           cereal::make_nvp("stored_agents", stored_agents),
           cereal::make_nvp("stored_shops", stored_shops),
           cereal::make_nvp("macro_zones", macro_zones),
           cereal::make_nvp("building_interiors", building_interiors));
    }
};

/**
 * @brief Prevents an entity from being destroyed during chunk dematerialization.
 */
struct PersistentEntityComponent {
    bool persistent = true;
    template <class Archive> void serialize(Archive& ar) { ar(cereal::make_nvp("persistent", persistent)); }
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_COMPONENTS_LOD_COMPONENTS_H
