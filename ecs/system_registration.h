#ifndef NEON_OUBLIETTE_ECS_SYSTEM_REGISTRATION_H
#define NEON_OUBLIETTE_ECS_SYSTEM_REGISTRATION_H

#include "system_scheduler.h"
#include <entt/entt.hpp>
#include <notcurses/notcurses.h>

namespace NeonOubliette {

class SimulationCoordinator; // Forward declaration

/**
 * @brief Registers all standard systems with the scheduler.
 */
void register_all_systems(SystemScheduler& scheduler, struct notcurses* nc_context, entt::registry& registry,
                          entt::dispatcher& event_dispatcher);

/**
 * @brief Registers all multi-scalar simulation systems with the coordinator.
 */
void register_simulation_systems(SimulationCoordinator& coordinator, entt::registry& registry,
                                 entt::dispatcher& event_dispatcher);

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEM_REGISTRATION_H
