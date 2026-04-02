#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <entt/entt.hpp>
#include "nlohmann/json.hpp"
#include <notcurses/notcurses.h>
#include <thread>
#include <chrono>
#include <clocale>

#include "config/ConfigLoader.h"
#include "ecs/component_registration.h"
#include "ecs/system_registration.h"
#include "ecs/system_scheduler.h"
#include "ecs/simulation_coordinator.h"
#include "ecs/components/components.h"
#include "ecs/components/simulation_layers.h"
#include "ecs/event_declarations.h"
#include "ecs/systems/serialization_system.h"

static bool running = true;

void shutdown_handler(const NeonOubliette::ShutdownEvent& event) {
    running = false;
    // Direct log for tracing
    std::ofstream log_file("game.log", std::ios::app);
    if (log_file.is_open()) {
        log_file << "2026-03-30 -- [CORE] SHUTDOWN SIGNAL RECEIVED BY MAIN LOOP" << std::endl;
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    std::setlocale(LC_ALL, "");

    struct notcurses_options ncopts = {}; // Zero initialize
    ncopts.loglevel = NCLOGLEVEL_SILENT;
    ncopts.flags = NCOPTION_SUPPRESS_BANNERS;
    
    struct notcurses* nc_context = notcurses_init(&ncopts, nullptr);
    if (nc_context == nullptr) {
        return 1;
    }

    std::string baseConfigPath = "data/configs/";
    std::string baseSchemaPath = "data/schemas/";
    NeonOubliette::Config::ConfigLoader configLoader(baseConfigPath, baseSchemaPath);

    try {
        auto worldGenParams = configLoader.loadConfig<NeonOubliette::Config::WorldGeneratorParams>("world_generator_params.json", "world_generator_params_schema.json");
    } catch (const std::exception& e) {
        notcurses_stop(nc_context);
        return 1;
    }

    entt::registry macro_registry;
    entt::dispatcher event_dispatcher;

    event_dispatcher.sink<NeonOubliette::ShutdownEvent>().connect<&shutdown_handler>();

    // Initial Registration
    NeonOubliette::SystemScheduler scheduler(macro_registry, event_dispatcher);
    NeonOubliette::register_all_systems(scheduler, nc_context, macro_registry, event_dispatcher);
    
    NeonOubliette::SimulationCoordinator simulation_coordinator(macro_registry, event_dispatcher, scheduler);
    NeonOubliette::register_simulation_systems(simulation_coordinator, macro_registry, event_dispatcher);
    
    // Initialize all systems (Phase-based and Layer-based)
    scheduler.initialize_all_systems();
    simulation_coordinator.initialize_all_systems();

    double delta_time = 0.016;

    // --- Entity Creation ---
    auto player_entity = macro_registry.create();
    macro_registry.emplace<NeonOubliette::PositionComponent>(player_entity, 10, 10, 0); 
    macro_registry.emplace<NeonOubliette::PlayerCurrentLayerComponent>(player_entity);
    macro_registry.emplace<NeonOubliette::PlayerComponent>(player_entity); 
    macro_registry.emplace<NeonOubliette::NameComponent>(player_entity, "Player One");
    macro_registry.emplace<NeonOubliette::InventoryComponent>(player_entity);
    macro_registry.emplace<NeonOubliette::RenderableComponent>(player_entity, '@', "#FFFFFF");
    macro_registry.emplace<NeonOubliette::HUDComponent>(player_entity); 

    // Initial Multi-Layer Components for Player
    macro_registry.emplace<NeonOubliette::Layer0PhysicsComponent>(player_entity);
    macro_registry.emplace<NeonOubliette::Layer1BiologyComponent>(player_entity);
    macro_registry.emplace<NeonOubliette::Layer2CognitiveComponent>(player_entity);
    macro_registry.emplace<NeonOubliette::Layer3EconomicComponent>(player_entity);
    macro_registry.emplace<NeonOubliette::Layer4PoliticalComponent>(player_entity);

    // --- Test Building ---
    auto building_entity = macro_registry.create();
    macro_registry.emplace<NeonOubliette::NameComponent>(building_entity, "Neon Oubliette HQ");
    macro_registry.emplace<NeonOubliette::BuildingComponent>(building_entity, 5, "Corporate", 0);
    macro_registry.emplace<NeonOubliette::PositionComponent>(building_entity, 12, 10, 0);
    macro_registry.emplace<NeonOubliette::RenderableComponent>(building_entity, 'B', "#FF00FF");
    
    // Entrance to the building
    auto entrance_entity = macro_registry.create();
    macro_registry.emplace<NeonOubliette::PositionComponent>(entrance_entity, 12, 10, 0);
    macro_registry.emplace<NeonOubliette::BuildingEntranceComponent>(entrance_entity, building_entity, 0);
    macro_registry.emplace<NeonOubliette::RenderableComponent>(entrance_entity, 'E', "#00FF00");

    auto agent_entity = macro_registry.create();
    macro_registry.emplace<NeonOubliette::PositionComponent>(agent_entity, 1, 1, 0); 
    macro_registry.emplace<NeonOubliette::RenderableComponent>(agent_entity, 'A', "#00FF00"); 
    macro_registry.emplace<NeonOubliette::NameComponent>(agent_entity, "Wanderer");
    macro_registry.emplace<NeonOubliette::AgentComponent>(agent_entity);
    macro_registry.emplace<NeonOubliette::AgentTaskComponent>(agent_entity);
    macro_registry.emplace<NeonOubliette::GoalComponent>(agent_entity, 20, 5, 0);
    macro_registry.emplace<NeonOubliette::NeedsComponent>(agent_entity, 45, 100); 
    macro_registry.emplace<NeonOubliette::InventoryComponent>(agent_entity);

    // Initial Multi-Layer Components for Agent
    macro_registry.emplace<NeonOubliette::Layer0PhysicsComponent>(agent_entity);
    macro_registry.emplace<NeonOubliette::Layer1BiologyComponent>(agent_entity);
    macro_registry.emplace<NeonOubliette::Layer2CognitiveComponent>(agent_entity);

    // --- Initial Render ---
    // Ensure the screen is drawn immediately upon startup
    scheduler.run_phase(NeonOubliette::SystemScheduler::Phase::Output, macro_registry, event_dispatcher, delta_time);

    // --- Main Simulation Loop ---
    while (running) {
        // Tick advancement handles phase execution via the coordinator
        simulation_coordinator.advance_turn(delta_time);

        // Regulate frame rate (~60 FPS)
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    notcurses_stop(nc_context);
    return 0;
}
