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
#include "systems/dialogue_system.h"
#include "systems/god_mode_system.h"
#include "systems/item_usage_system.h"
#include "systems/logging_system.h"
#include "systems/movement_system.h"
#include "systems/pathfinding_system.h"
#include "systems/macro_navigation_system.h"
#include "systems/rendering_system.h"
#include "systems/serialization_system.h"
#include "systems/turn_manager_system.h"
#include "systems/vertical_system.h"
#include "systems/building_generation_system.h"
#include "systems/population_system.h"
#include "systems/visibility_system.h"
#include "systems/sound_system.h"
#include "systems/activity_system.h"
#include "systems/barter_system.h"
#include "systems/faction_system.h"
#include "systems/city_generation_system.h"
#include "systems/zoning_solver_system.h"
#include "systems/infrastructure_network_system.h"
#include "systems/transit_system.h"
#include "systems/chunk_streaming_system.h"

// Multi-scalar Simulation Systems
#include "simulation_coordinator.h"
#include "systems/physics_system.h"
#include "systems/biology_system.h"
#include "systems/cognitive_system.h"
#include "systems/social_interaction_system.h"
#include "systems/economic_system.h"
#include "systems/stock_market_system.h"
#include "systems/political_system.h"
#include "systems/infrastructure_system.h"
#include "systems/environmental_system.h"
#include "systems/hazard_system.h"
#include "systems/power_grid_system.h"
#include "systems/ecosystem_system.h"
#include "systems/economic_market_system.h"
#include "systems/resource_distribution_system.h"
#include "systems/political_opinion_system.h"
#include "systems/infrastructure_influence_system.h"
#include "systems/xeno_system.h"
#include "systems/information_system.h"
#include "systems/conversation_system.h"
#include "systems/crisis_system.h"
#include "systems/crisis_dashboard_system.h"
#include "systems/drug_manufacturing_system.h"
#include "systems/milestone_system.h"
#include "systems/religion_system.h"
#include "systems/wanted_level_system.h"
#include "systems/guard_response_system.h"
#include "systems/inheritance_system.h"
#include "systems/urban_decay_system.h"
#include "systems/demolition_system.h"
#include "systems/rebuilding_system.h"
#include "systems/broadcast_tower_system.h"
#include "systems/underground_media_system.h"
#include "systems/supply_chain_system.h"
#include "systems/production_system.h"

namespace NeonOubliette {

void register_all_systems(SystemScheduler& scheduler, struct notcurses* nc_context, entt::registry& registry,
                          entt::dispatcher& event_dispatcher) {
    // --- Input Phase ---
    scheduler.add_system(SystemScheduler::Phase::Input,
                         std::make_unique<Systems::InputSystem>(registry, nc_context, event_dispatcher), "Input");

    // --- Macro Phase ---
    scheduler.add_system(SystemScheduler::Phase::Macro,
                         std::make_unique<MacroNavigationSystem>(registry, event_dispatcher), "MacroNav");
    scheduler.add_system(SystemScheduler::Phase::Macro,
                         std::make_unique<PathfindingSystem>(registry, event_dispatcher), "Pathfinding");
    scheduler.add_system(SystemScheduler::Phase::Macro,
                         std::make_unique<AgentDecisionSystem>(registry, event_dispatcher), "AgentDecision");
    scheduler.add_system(SystemScheduler::Phase::Macro,
                         std::make_unique<AgentActionSystem>(registry, event_dispatcher), "AgentAction");
    scheduler.add_system(SystemScheduler::Phase::Macro,
                         std::make_unique<GuardResponseSystem>(registry, event_dispatcher), "GuardResponse");
    scheduler.add_system(SystemScheduler::Phase::Macro,
                         std::make_unique<AgentSpawnSystem>(registry, event_dispatcher), "AgentSpawn");
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<MovementSystem>(registry, event_dispatcher), "Movement");
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<VerticalSystem>(registry, event_dispatcher), "Vertical");
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<BuildingGenerationSystem>(registry, event_dispatcher), "BuildingGen");
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<PopulationSystem>(registry, event_dispatcher), "Population");
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<VisibilitySystem>(registry, event_dispatcher), "Visibility");
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<Systems::SoundSystem>(registry, event_dispatcher), "Sound");

    // Interaction/Inspection systems
    scheduler.add_system(SystemScheduler::Phase::Macro,
                         std::make_unique<Systems::InteractionSystem>(registry, nc_context, event_dispatcher), "Interaction");
    scheduler.add_system(SystemScheduler::Phase::Macro,
                         std::make_unique<Systems::InspectionSystem>(registry, nc_context, event_dispatcher), "Inspection");
    scheduler.add_system(SystemScheduler::Phase::Macro,
                         std::make_unique<Systems::DialogueSystem>(registry, nc_context, event_dispatcher), "Dialogue");
    scheduler.add_system(SystemScheduler::Phase::Macro,
                         std::make_unique<Systems::GodModeSystem>(registry, event_dispatcher), "GodMode");

    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<ActivitySystem>(registry, event_dispatcher), "Activity");
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<BarterSystem>(registry, event_dispatcher), "Barter");

    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<CraftingSystem>(registry, event_dispatcher), "Crafting");
    scheduler.add_system(SystemScheduler::Phase::Macro,
                         std::make_unique<ConsumptionSystem>(registry, event_dispatcher), "Consumption");
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<ContainerSystem>(registry, event_dispatcher), "Container");
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<ItemUsageSystem>(registry, event_dispatcher), "ItemUsage");
    scheduler.add_system(SystemScheduler::Phase::Macro,
                         std::make_unique<SerializationSystem>(registry, event_dispatcher), "Serialization");
    scheduler.add_system(SystemScheduler::Phase::Macro,
                         std::make_unique<TurnManagerSystem>(registry, event_dispatcher), "TurnManager");
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<LoggingSystem>(registry, event_dispatcher), "Logging");
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<Systems::CrisisSystem>(registry, event_dispatcher), "Crisis");
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<Systems::CrisisDashboardSystem>(registry, event_dispatcher), "CrisisDashboard");
    
    // Generation, Zoning & Streaming
    auto city_gen = std::make_unique<CityGenerationSystem>(registry, event_dispatcher);
    CityGenerationSystem& city_gen_ref = *city_gen;
    scheduler.add_system(SystemScheduler::Phase::Macro, std::move(city_gen), "CityGen");
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<ZoningSolverSystem>(registry, event_dispatcher), "ZoningSolver");
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<InfrastructureNetworkSystem>(registry, event_dispatcher), "InfraNetwork");
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<TransitSystem>(registry, event_dispatcher), "Transit");
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<ChunkStreamingSystem>(registry, event_dispatcher), "ChunkStream");

    // [K.3] Demolition & Rebuilding
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<DemolitionSystem>(registry, event_dispatcher), "Demolition");
    scheduler.add_system(SystemScheduler::Phase::Macro, std::make_unique<RebuildingSystem>(registry, event_dispatcher, city_gen_ref), "Rebuilding");

    // --- Output Phase ---
    scheduler.add_system(SystemScheduler::Phase::Output,
                         std::make_unique<Systems::RenderingSystem>(registry, nc_context, event_dispatcher), "Rendering");
}

void register_simulation_systems(SimulationCoordinator& coordinator, entt::registry& registry,
                                 entt::dispatcher& event_dispatcher) {
    // Register systems for each simulation layer (L0-L4)
    coordinator.add_simulation_system(std::make_unique<PhysicsSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<InfrastructureSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<EnvironmentalSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<HazardSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<InfrastructureInfluenceSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<Systems::PowerGridSystem>(registry, event_dispatcher));
    
    coordinator.add_simulation_system(std::make_unique<BiologySystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<EcosystemSystem>(registry, event_dispatcher));
    
    coordinator.add_simulation_system(std::make_unique<CognitiveSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<XenoSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<SocialInteractionSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<ConversationSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<Systems::InformationSystem>(registry, event_dispatcher));
    
    coordinator.add_simulation_system(std::make_unique<EconomicSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<SupplyChainSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<ProductionSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<DrugManufacturingSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<StockMarketSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<EconomicMarketSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<ResourceDistributionSystem>(registry, event_dispatcher));
    
    coordinator.add_simulation_system(std::make_unique<FactionSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<ReligionSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<PoliticalSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<PoliticalOpinionSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<Systems::BroadcastTowerSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<Systems::UndergroundMediaSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<MilestoneSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<WantedLevelSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<InheritanceSystem>(registry, event_dispatcher));
    coordinator.add_simulation_system(std::make_unique<UrbanDecaySystem>(registry, event_dispatcher));
}

} // namespace NeonOubliette
