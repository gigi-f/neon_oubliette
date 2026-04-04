#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_TRANSIT_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_TRANSIT_SYSTEM_H

#include <entt/entt.hpp>
#include "../system_scheduler.h"
#include "../components/components.h"
#include "../components/transit_components.h"

namespace NeonOubliette {

/**
 * [NEW SYSTEM]
 * @brief Manages the lifecycle of transit vehicles, routes, and stations.
 *        Handles vehicle movement along infrastructure graphs and boarding mechanics.
 */
class TransitSystem : public ISystem {
public:
    TransitSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override;
    void update(double delta_time) override;

    /**
     * @brief Creates default transit routes and spawns vehicles.
     */
    void setup_transit_network();

private:
    void update_vehicle_movement(entt::entity vehicle, TransitVehicleComponent& transit, PositionComponent& pos, double dt);
    void handle_station_stop(entt::entity vehicle, TransitVehicleComponent& transit, entt::entity station);
    void process_boarding(entt::entity vehicle, entt::entity station);
    void process_unboarding(entt::entity vehicle, entt::entity station);

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    double m_accumulated_time = 0;
};

} // namespace NeonOubliette

#endif
