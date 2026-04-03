#include "system_registration.h"

#include "systems/agent_action_system.h"
#include "systems/agent_decision_system.h"
#include "systems/agent_spawn_system.h"
#include "systems/consumption_system.h"
#include "systems/container_system.h"
#include "systems/crafting_system.h"
#include "systems/input_system.h"
#include "systems/inspection_system.h"
#include "systems/interaction_system.h"
#include "systems/item_usage_system.h"
#include "systems/logging_system.h"
#include "systems/movement_system.h"
#include "systems/pathfinding_system.h"
#include "systems/rendering_system.h"
#include "systems/serialization_system.h"
#include "systems/turn_manager_system.h"
#include "systems/vertical_system.h"
#include "systems/building_generation_system.h"
#include "systems/population_system.h"
#include "systems/visibility_system.h"
#include "systems/activity_system.h"
#include "systems/barter_system.h"
#include "systems/faction_system.h"
#include "systems/city_generation_system.h"
#include "systems/zoning_solver_system.h"

// Multi-scalar Simulation Systems
#include "simulation_coordinator.h"
#include "systems/physics_system.h"
#include "systems/biology_system.h"
#include "systems/cognitive_system.h"
#include "systems/economic_system.h"
#include "systems/political_system.h"
#include "systems/infrastructure_system.h"
#include "systems/environmental_system.h"
#include "systems/ecosystem_system.h"
#include "systems/economic_market_system.h"
#include "systems/political_opinion_system.h"

namespace NeonOubliette {

void register_all_systems(SystemScheduler& scheduler, struct notcurses* nc_context, entt::registry& registry,
                          entt::dispatcher& event_dispatcher) {
    // --- Input Phase ---
    scheduler.add_system(SystemScheduler::Phase::Input,
                         std::make_unique<Systems::InputSystem>(registry, nc_context, event_dispatcher));

    // --- Macro Phase ---
    scheduler.add_system(SystemScheduler::Phase::Macro,
                         std::make_unique<PathfindingSystem>(registry, event_dispatcher));
    scheduler.add_system(SystemScheduler::Phase::Macro,
                         std::make_unique<AgentDecisionSystem>(registry, event_dispatcher));
    scheduler.add_system(SystemScheduler::Phase::Macro,
                         std::make_unique<AgentActionSystem>(registry, event_dispatcher));
    scheduler.add_system(SystemScheduler::Phase::Macro,
                         std::make_unique<AgentSpawnSystem>(registry, event_dispatcher));
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<MovementSystem>(registry, event_dispatcher));
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<VerticalSystem>(registry, event_dispatcher));
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<BuildingGenerationSystem>(registry, event_dispatcher));
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<PopulationSystem>(registry, event_dispatcher));
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<VisibilitySystem>(registry, event_dispatcher));

    // Interaction/Inspection systems
    scheduler.add_system(SystemScheduler::Phase::Macro,
                         std::make_unique<Systems::InteractionSystem>(registry, nc_context, event_dispatcher));
    scheduler.add_system(SystemScheduler::Phase::Macro,
                         std::make_unique<Systems::InspectionSystem>(registry, nc_context, event_dispatcher));

    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<ActivitySystem>(registry, event_dispatcher));
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<BarterSystem>(registry, event_dispatcher));
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<FactionSystem>(registry, event_dispatcher));

    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<CraftingSystem>(registry, event_dispatcher));
    scheduler.add_system(SystemScheduler::Phase::Macro,
                         std::make_unique<ConsumptionSystem>(registry, event_dispatcher));
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<ContainerSystem>(registry, event_dispatcher));
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<ItemUsageSystem>(registry, event_dispatcher));
    scheduler.add_system(SystemScheduler::Phase::Macro,
                         std::make_unique<SerializationSystem>(registry, event_dispatcher));
    scheduler.add_system(SystemScheduler::Phase::Macro,
                         std::make_unique<TurnManagerSystem>(registry, event_dispatcher));
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<LoggingSystem>(registry, event_dispatcher));
    
    // Generation & Zoning Systems
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<CityGenerationSystem>(registry, event_dispatcher));
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<ZoningSolverSystem>(registry, event_dispatcher));

    // --- Output Phase ---
    scheduler.add_system(SystemScheduler::Phase::Output,
                         std::make_unique<Systems::RenderingSystem>(registry, nc_context, event_dispatcher));
}

void register_simulation_systems(SimulationCoordinator& coordinator, entt::registry& registry,
                                 entt::dispatcher& event_dispatcher) {
    // Register systems for each simulation layer (L0-L4)
    coordinator.add_simulation_system(std::make_unique<PhysicsSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<InfrastructureSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<EnvironmentalSystem>(registry, event_dispatcher));
    
    coordinator.add_simulation_system(std::make_unique<BiologySystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<EcosystemSystem>(registry, event_dispatcher));
    
    coordinator.add_simulation_system(std::make_unique<CognitiveSystem>(registry, event_dispatcher));
    
    coordinator.add_simulation_system(std::make_unique<EconomicSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<EconomicMarketSystem>(registry, event_dispatcher));
    
    coordinator.add_simulation_system(std::make_unique<PoliticalSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<PoliticalOpinionSystem>(registry, event_dispatcher));
}

} // namespace NeonOubliette
