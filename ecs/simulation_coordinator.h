#ifndef NEON_OUBLIETTE_ECS_SIMULATION_COORDINATOR_H
#define NEON_OUBLIETTE_ECS_SIMULATION_COORDINATOR_H

#include <entt/entt.hpp>
#include <memory>
#include <vector>
#include <map>
#include "system_scheduler.h"
#include "components/simulation_layers.h"

namespace NeonOubliette {

/**
 * @brief Extension of ISystem specifically for simulation layers.
 */
class ISimulationSystem {
public:
    virtual ~ISimulationSystem() = default;
    virtual void initialize() = 0;
    virtual void update(double delta_time) = 0;
    virtual SimulationLayer simulation_layer() const = 0;
};

/**
 * @brief Orchestrates the multi-scalar simulation loop across L0-L4.
 *        Implements 'Top-Down Pressure' and 'Bottom-Up Causality' protocols.
 */
class SimulationCoordinator {
public:
    SimulationCoordinator(entt::registry& registry, entt::dispatcher& dispatcher, SystemScheduler& scheduler);

    void add_simulation_system(std::unique_ptr<ISimulationSystem> system);

    /**
     * @brief Initializes all registered simulation systems.
     */
    void initialize_all_systems();

    /**
     * @brief Ticks all active simulation layers.
     *        Enforces the causality hierarchy: Macro (L4) -> Micro (L0).
     */
    void advance_turn(double delta_time);

private:
    void tick_layer(SimulationLayer layer, double delta_time);
    bool should_layer_tick(SimulationLayer layer, uint64_t turn) const;

    uint64_t m_turn_counter = 0;
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    SystemScheduler& m_scheduler;
    std::map<SimulationLayer, std::vector<std::unique_ptr<ISimulationSystem>>> m_layer_systems;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SIMULATION_COORDINATOR_H
