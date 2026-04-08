#ifndef NEON_OUBLIETTE_ECS_COMPONENTS_INFRASTRUCTURE_COMPONENTS_H
#define NEON_OUBLIETTE_ECS_COMPONENTS_INFRASTRUCTURE_COMPONENTS_H

#include <entt/entt.hpp>
#include <vector>
#include <string>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

namespace NeonOubliette {

enum class ArterialType : uint8_t {
    ROAD_PRIMARY,
    ROAD_SECONDARY,
    ROAD_ALLEY,
    SIDEWALK,
    RAIL_ELEVATED,
    RAIL_SUBWAY,
    WATERWAY_CANAL,
    WATERWAY_RIVER,
    ELECTRIC_GRID,
    SEWER,
    UNDERGROUND_TUNNEL
};

/**
 * @brief [NEW CLASS] Tracks power state for a chunk or building.
 */
struct PowerGridComponent {
    float power_level = 1.0f; // 0.0 to 1.0
    float voltage_stability = 1.0f;
    bool is_grid_source = false; // Power plant or major substation
    uint32_t ticks_since_failure = 0;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("power_level", power_level),
           cereal::make_nvp("voltage_stability", voltage_stability),
           cereal::make_nvp("is_grid_source", is_grid_source),
           cereal::make_nvp("ticks_since_failure", ticks_since_failure));
    }
};

/**
 * @brief Defines a global conduit that spans multiple macro-tiles.
 */
struct InfrastructureArterialComponent {
    ArterialType type = ArterialType::ROAD_PRIMARY;
    float flow_capacity = 1.0f; // Throughput for L3/L4 simulation
    bool is_public = true;
    std::vector<entt::entity> connected_nodes; // Graph connectivity

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("type", type),
           cereal::make_nvp("flow_capacity", flow_capacity),
           cereal::make_nvp("is_public", is_public),
           cereal::make_nvp("connected_nodes", connected_nodes));
    }
};

/**
 * @brief Projects simulation modifiers to adjacent cells (Causal Conductivity).
 */
struct ConduitFieldComponent {
    float radius = 2.0f;
    float temperature_offset = 0.0f; // L0 influence
    float economic_multiplier = 1.0f; // L3 influence
    float crime_modifier = 0.0f; // L4 influence

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("radius", radius),
           cereal::make_nvp("temperature_offset", temperature_offset),
           cereal::make_nvp("economic_multiplier", economic_multiplier),
           cereal::make_nvp("crime_modifier", crime_modifier));
    }
};

/**
 * @brief Represents a junction, bridge, or station in the arterial network.
 */
struct InfrastructureNodeComponent {
    std::string node_name;
    bool is_bridge = false;
    bool is_substation = false; // [L.5] Power distribution point
    float power_load = 0.0f; // Cumulative demand downstream
    entt::entity controlling_faction = entt::null; // L4 link

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("node_name", node_name),
           cereal::make_nvp("is_bridge", is_bridge),
           cereal::make_nvp("is_substation", is_substation),
           cereal::make_nvp("power_load", power_load),
           cereal::make_nvp("controlling_faction", controlling_faction));
    }
};

/**
 * @brief Global singleton storing the pre-calculated graph of the Arterial Network.
 */
struct ArterialGraphComponent {
    struct Edge {
        entt::entity target_node;
        float cost;
        ArterialType type;
        
        template <class Archive>
        void serialize(Archive& ar) {
            ar(cereal::make_nvp("target_node", target_node),
               cereal::make_nvp("cost", cost),
               cereal::make_nvp("type", type));
        }
    };

    std::map<entt::entity, std::vector<Edge>> adj_list;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("adj_list", adj_list));
    }
};

} // namespace NeonOubliette

#endif 
