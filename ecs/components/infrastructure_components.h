#ifndef NEON_OUBLIETTE_ECS_COMPONENTS_INFRASTRUCTURE_COMPONENTS_H
#define NEON_OUBLIETTE_ECS_COMPONENTS_INFRASTRUCTURE_COMPONENTS_H

#include <entt/entt.hpp>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include "base_types.h"

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
    UNDERGROUND_TUNNEL,
    PEDESTRIAN_PATH
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

enum class TrafficLightState : uint8_t {
    RED,
    YELLOW,
    GREEN
};

/**
 * @brief [NEW CLASS] Manages intersection signal cycles.
 */
struct TrafficLightComponent {
    TrafficLightState state = TrafficLightState::RED;
    uint32_t timer = 0;
    uint32_t cycle_duration = 200; // Default cycle length

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("state", state),
           cereal::make_nvp("timer", timer),
           cereal::make_nvp("cycle_duration", cycle_duration));
    }
};

/**
 * @brief [NEW CLASS] Defines a high-resolution path for vehicles within a road segment.
 */
struct LaneComponent {
    std::vector<PositionComponent> waypoints;
    bool is_occupied = false;
    entt::entity occupying_vehicle = entt::null;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("waypoints", waypoints),
           cereal::make_nvp("is_occupied", is_occupied),
           cereal::make_nvp("occupying_vehicle", occupying_vehicle));
    }
};

/**
 * @brief Represents an axis-aligned line segment of infrastructure.
 *        Replaces per-tile InfrastructureArterialComponent entities.
 *        A single segment covers (x1,y1)→(x2,y2) inclusive.
 */
struct InfrastructureSegmentComponent {
    ArterialType type = ArterialType::ROAD_PRIMARY;
    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    int layer_id = 0;
    float flow_capacity = 2.0f;
    bool is_public = true;

    // Embedded conduit field properties (replaces separate ConduitFieldComponent)
    float field_radius = 2.0f;
    float temperature_offset = 0.0f;
    float economic_multiplier = 1.0f;
    float crime_modifier = 0.0f;

    bool is_horizontal() const { return y1 == y2; }
    bool is_vertical() const { return x1 == x2; }

    bool contains(int x, int y) const {
        if (is_horizontal()) return y == y1 && x >= std::min(x1, x2) && x <= std::max(x1, x2);
        if (is_vertical()) return x == x1 && y >= std::min(y1, y2) && y <= std::max(y1, y2);
        return false; // Non-axis-aligned segments not supported for point queries
    }

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("type", type),
           cereal::make_nvp("x1", x1), cereal::make_nvp("y1", y1),
           cereal::make_nvp("x2", x2), cereal::make_nvp("y2", y2),
           cereal::make_nvp("layer_id", layer_id),
           cereal::make_nvp("flow_capacity", flow_capacity),
           cereal::make_nvp("is_public", is_public),
           cereal::make_nvp("field_radius", field_radius),
           cereal::make_nvp("temperature_offset", temperature_offset),
           cereal::make_nvp("economic_multiplier", economic_multiplier),
           cereal::make_nvp("crime_modifier", crime_modifier));
    }
};

/**
 * @brief Spatial index for fast arterial type lookup at any (x, y, layer) position.
 *        Built from InfrastructureSegmentComponent entities after world generation.
 *        Stored as registry context (singleton).
 */
struct ArterialGrid {
    struct HInterval {
        int x_min, x_max;
        ArterialType type;
        float field_radius;
        float temperature_offset;
        float economic_multiplier;
        float crime_modifier;
    };
    struct VInterval {
        int y_min, y_max;
        ArterialType type;
        float field_radius;
        float temperature_offset;
        float economic_multiplier;
        float crime_modifier;
    };

    // layer → y → sorted intervals for horizontal segments
    std::unordered_map<int, std::unordered_map<int, std::vector<HInterval>>> h_intervals;
    // layer → x → sorted intervals for vertical segments
    std::unordered_map<int, std::unordered_map<int, std::vector<VInterval>>> v_intervals;

    void add_segment(const InfrastructureSegmentComponent& seg) {
        if (seg.is_horizontal()) {
            int y = seg.y1;
            int xmin = std::min(seg.x1, seg.x2);
            int xmax = std::max(seg.x1, seg.x2);
            h_intervals[seg.layer_id][y].push_back({xmin, xmax, seg.type,
                seg.field_radius, seg.temperature_offset, seg.economic_multiplier, seg.crime_modifier});
        } else if (seg.is_vertical()) {
            int x = seg.x1;
            int ymin = std::min(seg.y1, seg.y2);
            int ymax = std::max(seg.y1, seg.y2);
            v_intervals[seg.layer_id][x].push_back({ymin, ymax, seg.type,
                seg.field_radius, seg.temperature_offset, seg.economic_multiplier, seg.crime_modifier});
        }
    }

    /// Returns true and fills out_type if an arterial exists at (x, y, layer).
    bool type_at(int x, int y, int layer, ArterialType& out_type) const {
        // Check horizontal intervals at this y
        auto h_layer_it = h_intervals.find(layer);
        if (h_layer_it != h_intervals.end()) {
            auto h_row_it = h_layer_it->second.find(y);
            if (h_row_it != h_layer_it->second.end()) {
                for (const auto& iv : h_row_it->second) {
                    if (x >= iv.x_min && x <= iv.x_max) { out_type = iv.type; return true; }
                }
            }
        }
        // Check vertical intervals at this x
        auto v_layer_it = v_intervals.find(layer);
        if (v_layer_it != v_intervals.end()) {
            auto v_col_it = v_layer_it->second.find(x);
            if (v_col_it != v_layer_it->second.end()) {
                for (const auto& iv : v_col_it->second) {
                    if (y >= iv.y_min && y <= iv.y_max) { out_type = iv.type; return true; }
                }
            }
        }
        return false;
    }

    /// Collect all arterial types at (x, y, layer) into out_types.
    void types_at(int x, int y, int layer, std::vector<ArterialType>& out_types) const {
        auto h_layer_it = h_intervals.find(layer);
        if (h_layer_it != h_intervals.end()) {
            auto h_row_it = h_layer_it->second.find(y);
            if (h_row_it != h_layer_it->second.end()) {
                for (const auto& iv : h_row_it->second) {
                    if (x >= iv.x_min && x <= iv.x_max) out_types.push_back(iv.type);
                }
            }
        }
        auto v_layer_it = v_intervals.find(layer);
        if (v_layer_it != v_intervals.end()) {
            auto v_col_it = v_layer_it->second.find(x);
            if (v_col_it != v_layer_it->second.end()) {
                for (const auto& iv : v_col_it->second) {
                    if (y >= iv.y_min && y <= iv.y_max) out_types.push_back(iv.type);
                }
            }
        }
    }

    /// Check if a specific type exists anywhere in given zone bounds.
    bool has_type_in_rect(int sx, int sy, int ex, int ey, int layer, ArterialType type) const {
        auto h_layer_it = h_intervals.find(layer);
        if (h_layer_it != h_intervals.end()) {
            for (int y = sy; y <= ey; ++y) {
                auto row_it = h_layer_it->second.find(y);
                if (row_it == h_layer_it->second.end()) continue;
                for (const auto& iv : row_it->second) {
                    if (iv.type == type && iv.x_max >= sx && iv.x_min <= ex) return true;
                }
            }
        }
        auto v_layer_it = v_intervals.find(layer);
        if (v_layer_it != v_intervals.end()) {
            for (int x = sx; x <= ex; ++x) {
                auto col_it = v_layer_it->second.find(x);
                if (col_it == v_layer_it->second.end()) continue;
                for (const auto& iv : col_it->second) {
                    if (iv.type == type && iv.y_max >= sy && iv.y_min <= ey) return true;
                }
            }
        }
        return false;
    }

    /// Expand all segments overlapping [sx,sy]-[ex,ey] into a position→type map.
    void expand_to_map(int sx, int sy, int ex, int ey, int layer,
                       std::map<std::pair<int,int>, ArterialType>& out) const {
        auto h_layer_it = h_intervals.find(layer);
        if (h_layer_it != h_intervals.end()) {
            for (int y = sy; y <= ey; ++y) {
                auto row_it = h_layer_it->second.find(y);
                if (row_it == h_layer_it->second.end()) continue;
                for (const auto& iv : row_it->second) {
                    int lo = std::max(iv.x_min, sx);
                    int hi = std::min(iv.x_max, ex);
                    for (int x = lo; x <= hi; ++x) out[{x, y}] = iv.type;
                }
            }
        }
        auto v_layer_it = v_intervals.find(layer);
        if (v_layer_it != v_intervals.end()) {
            for (int x = sx; x <= ex; ++x) {
                auto col_it = v_layer_it->second.find(x);
                if (col_it == v_layer_it->second.end()) continue;
                for (const auto& iv : col_it->second) {
                    int lo = std::max(iv.y_min, sy);
                    int hi = std::min(iv.y_max, ey);
                    for (int y = lo; y <= hi; ++y) out[{x, y}] = iv.type;
                }
            }
        }
    }

    void clear() { h_intervals.clear(); v_intervals.clear(); }
};

} // namespace NeonOubliette

#endif 
