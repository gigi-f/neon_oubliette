#ifndef NEON_OUBLIETTE_ECS_COMPONENTS_LOD_COMPONENTS_H
#define NEON_OUBLIETTE_ECS_COMPONENTS_LOD_COMPONENTS_H

#include "simulation_layers.h"
#include <string>
#include <vector>
#include <map>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/map.hpp>

namespace NeonOubliette {

/**
 * @brief Statistical record for an agent that is currently out-of-range (Warm/Cold).
 *        Used to "materialize" the agent back into an ECS entity when the player approaches.
 */
struct MacroAgentRecord {
    std::string name;
    std::string archetype; // "Citizen", "Guard", etc.
    SpeciesType species = SpeciesType::HUMAN;
    int x = 0;
    int y = 0;
    int layer_id = 0;
    float hunger = 100.0f;
    float thirst = 100.0f;
    float frustration = 0.0f;
    float consciousness = 1.0f;
    int cash_on_hand = 100;
    std::string faction_id;
    uint64_t last_tick_turn = 0;
    
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

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("name", name),
           cereal::make_nvp("archetype", archetype),
           cereal::make_nvp("species", species),
           cereal::make_nvp("x", x),
           cereal::make_nvp("y", y),
           cereal::make_nvp("layer_id", layer_id),
           cereal::make_nvp("hunger", hunger),
           cereal::make_nvp("thirst", thirst),
           cereal::make_nvp("frustration", frustration),
           cereal::make_nvp("consciousness", consciousness),
           cereal::make_nvp("cash_on_hand", cash_on_hand),
           cereal::make_nvp("faction_id", faction_id),
           cereal::make_nvp("last_tick_turn", last_tick_turn),
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
           cereal::make_nvp("portfolio", portfolio));
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
    std::vector<entt::entity> macro_zones; // Macro-zones belonging to this 40x40 chunk

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
           cereal::make_nvp("macro_zones", macro_zones));
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
