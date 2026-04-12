#ifndef NEON_OUBLIETTE_ECS_COMPONENTS_ZONING_COMPONENTS_H
#define NEON_OUBLIETTE_ECS_COMPONENTS_ZONING_COMPONENTS_H

#include <string>
#include <vector>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include "base_types.h"

namespace NeonOubliette {

/**
 * @brief [NEW CLASS] Defines a surveyed lot within a city block.
 */
struct LotComponent {
    int x = 0; // World-space AABB
    int y = 0;
    int width = 0;
    int height = 0;
    ZoneType zone_class = ZoneType::VOID;
    entt::entity ownership_entity = entt::null;
    entt::entity building_entity = entt::null; // [K.3] The active building on this lot
    StreetFacingSide facing = StreetFacingSide::NONE;
    StreetFacingSide alley_facing = StreetFacingSide::NONE;
    entt::entity parent_block = entt::null;
    uint64_t building_id = 0; // [NEW] Stable ID for consistency

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("x", x),
           cereal::make_nvp("y", y),
           cereal::make_nvp("width", width),
           cereal::make_nvp("height", height),
           cereal::make_nvp("zone_class", zone_class),
           cereal::make_nvp("ownership_entity", ownership_entity),
           cereal::make_nvp("building_entity", building_entity),
           cereal::make_nvp("facing", facing),
           cereal::make_nvp("alley_facing", alley_facing),
           cereal::make_nvp("parent_block", parent_block),
           cereal::make_nvp("building_id", building_id));
    }
};

/**
 * @brief [NEW CLASS] Defines a rectangular city block composed of multiple lots.
 */
struct BlockComponent {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    std::vector<entt::entity> lots;
    entt::entity zone_entity = entt::null;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("x", x),
           cereal::make_nvp("y", y),
           cereal::make_nvp("width", width),
           cereal::make_nvp("height", height),
           cereal::make_nvp("lots", lots),
           cereal::make_nvp("zone_entity", zone_entity));
    }
};

/**
 * @brief Represents a macro-scale urban zone (e.g., a 20x20 tile area).
 *        Used by the generation system to decide what to build and by 
 *        simulation systems to apply district-wide modifiers.
 */
struct MacroZoneComponent {
    ZoneType type = ZoneType::VOID;
    int macro_x = 0; // Coordinates in macro-grid space
    int macro_y = 0;
    float density = 0.5f; // 0.0 to 1.0 (building vs open space)
    std::string district_name;
    std::vector<entt::entity> arterial_entities; // Links to global skeleton entities in this zone
    std::vector<entt::entity> block_entities; // [NEW] Link to city blocks in this zone
    
    // [J.4] Demographic Pressure & Attractiveness
    float attractiveness = 0.0f;
    float pressure = 0.0f;
    
    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("type", type),
           cereal::make_nvp("macro_x", macro_x),
           cereal::make_nvp("macro_y", macro_y),
           cereal::make_nvp("density", density),
           cereal::make_nvp("district_name", district_name),
           cereal::make_nvp("arterial_entities", arterial_entities),
           cereal::make_nvp("block_entities", block_entities),
           cereal::make_nvp("attractiveness", attractiveness),
           cereal::make_nvp("pressure", pressure));
    }
};

/**
 * @brief Attached to buildings to track economic value influenced by zoning.
 */
struct PropertyComponent {
    int base_value = 1000;
    int current_market_value = 1000;
    std::string owner_faction;
    uint64_t last_tax_payment_turn = 0;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("base_value", base_value),
           cereal::make_nvp("current_market_value", current_market_value),
           cereal::make_nvp("owner_faction", owner_faction),
           cereal::make_nvp("last_tax_payment_turn", last_tax_payment_turn));
    }
};

/**
 * @brief [NEW CLASS] Tracks the physical health and maintenance state of a building.
 *        Part of Phase K.1 - Building Health & Decay.
 */
struct BuildingHealthComponent {
    float integrity = 100.0f;    // 0.0 to 100.0
    float max_integrity = 100.0f;
    float base_decay_rate = 0.01f; // Per L4 tick (20 simulation steps)
    float maintenance_budget = 0.0f; // Credits assigned for repair
    uint64_t last_repair_turn = 0;
    bool is_condemned = false;
    int num_squatters = 0; // [K.2]
    float maintenance_urgency = 0.0f; // [K.2] 0-1, higher means more attractive for REPAIR task

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("integrity", integrity),
           cereal::make_nvp("max_integrity", max_integrity),
           cereal::make_nvp("base_decay_rate", base_decay_rate),
           cereal::make_nvp("maintenance_budget", maintenance_budget),
           cereal::make_nvp("last_repair_turn", last_repair_turn),
           cereal::make_nvp("is_condemned", is_condemned),
           cereal::make_nvp("num_squatters", num_squatters),
           cereal::make_nvp("maintenance_urgency", maintenance_urgency));
    }
};

/**
 * @brief Applied to Layer 0 Physics to simulate the "Heat Island Effect" in dense zones.
 */
struct HeatIslandComponent {
    float heat_retention_multiplier = 1.0f;
    float ambient_temp_offset = 0.0f;

    template <class Archive> void serialize(Archive& ar) {
        ar(cereal::make_nvp("heat_retention_multiplier", heat_retention_multiplier),
           cereal::make_nvp("ambient_temp_offset", ambient_temp_offset));
    }
};

/**
 * @brief [NEW CLASS] Marks a location as a commerce anchor (e.g. train station).
 */
struct CommerceHubComponent {
    float influence_radius = 10.0f;
    float economic_boost = 1.5f;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("influence_radius", influence_radius),
           cereal::make_nvp("economic_boost", economic_boost));
    }
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_COMPONENTS_ZONING_COMPONENTS_H
