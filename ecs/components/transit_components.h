#ifndef NEON_OUBLIETTE_ECS_COMPONENTS_TRANSIT_COMPONENTS_H
#define NEON_OUBLIETTE_ECS_COMPONENTS_TRANSIT_COMPONENTS_H

#include <entt/entt.hpp>
#include <vector>
#include <string>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include "../components/infrastructure_components.h"

namespace NeonOubliette {

enum class TransitVehicleType : uint8_t {
    TRAIN,
    BUS,
    SCI_FI_SHUTTLE
};

/**
 * @brief [NEW CLASS] Marks an entity as a transit vehicle.
 */
struct TransitVehicleComponent {
    TransitVehicleType type = TransitVehicleType::TRAIN;
    int capacity = 20;
    float current_speed = 0.0f;
    float max_speed = 2.0f;
    entt::entity current_route = entt::null;
    size_t next_stop_index = 0;
    bool is_stopped = false;
    uint32_t stop_timer = 0;
    uint32_t stop_duration = 5; // Ticks to stay at a station

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("type", type),
           cereal::make_nvp("capacity", capacity),
           cereal::make_nvp("current_speed", current_speed),
           cereal::make_nvp("max_speed", max_speed),
           cereal::make_nvp("current_route", current_route),
           cereal::make_nvp("next_stop_index", next_stop_index),
           cereal::make_nvp("is_stopped", is_stopped),
           cereal::make_nvp("stop_timer", stop_timer),
           cereal::make_nvp("stop_duration", stop_duration));
    }
};

/**
 * @brief [NEW CLASS] Defines a route for transit vehicles.
 */
struct TransitRouteComponent {
    std::string route_name;
    TransitVehicleType vehicle_type = TransitVehicleType::TRAIN;
    std::vector<entt::entity> stops; // Sequence of InfrastructureNode entities
    bool is_circular = true;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("route_name", route_name),
           cereal::make_nvp("vehicle_type", vehicle_type),
           cereal::make_nvp("stops", stops),
           cereal::make_nvp("is_circular", is_circular));
    }
};

/**
 * @brief [NEW CLASS] Marks an entity (InfrastructureNode) as a station.
 */
struct TransitStationComponent {
    TransitVehicleType type = TransitVehicleType::TRAIN;
    std::vector<entt::entity> serving_routes;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("type", type),
           cereal::make_nvp("serving_routes", serving_routes));
    }
};

/**
 * @brief [NEW CLASS] Attached to a vehicle, lists entities currently riding.
 */
struct TransitOccupantsComponent {
    std::vector<entt::entity> occupants;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("occupants", occupants));
    }
};

/**
 * @brief [NEW CLASS] Attached to an agent/player currently on a vehicle.
 */
struct RidingComponent {
    entt::entity vehicle = entt::null;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(cereal::make_nvp("vehicle", vehicle));
    }
};

} // namespace NeonOubliette

#endif
