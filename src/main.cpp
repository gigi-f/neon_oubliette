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
#include "ecs/systems/city_generation_system.h"
#include "ecs/systems/agent_spawn_system.h"
#include "ecs/systems/zoning_solver_system.h"

static bool running = true;

void shutdown_handler(const NeonOubliette::ShutdownEvent& event) {
    running = false;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    std::setlocale(LC_ALL, "");

    struct notcurses_options ncopts = {}; 
    ncopts.loglevel = NCLOGLEVEL_SILENT;
    ncopts.flags = NCOPTION_SUPPRESS_BANNERS;
    
    struct notcurses* nc_context = notcurses_init(&ncopts, nullptr);
    if (nc_context == nullptr) {
        return 1;
    }

    entt::registry macro_registry;
    entt::dispatcher event_dispatcher;

    event_dispatcher.sink<NeonOubliette::ShutdownEvent>().connect<&shutdown_handler>();

    NeonOubliette::SystemScheduler scheduler(macro_registry, event_dispatcher);
    NeonOubliette::register_all_systems(scheduler, nc_context, macro_registry, event_dispatcher);
    
    NeonOubliette::SimulationCoordinator simulation_coordinator(macro_registry, event_dispatcher, scheduler);
    NeonOubliette::register_simulation_systems(simulation_coordinator, macro_registry, event_dispatcher);
    
    scheduler.initialize_all_systems();
    simulation_coordinator.initialize_all_systems();

    double delta_time = 0.016;

    // --- World Config ---
    auto config_entity = macro_registry.create();
    macro_registry.emplace<NeonOubliette::WorldConfigComponent>(config_entity, 100, 100, 20);

    // --- Zoning Solver (Phase 2.1) ---
    NeonOubliette::ZoningSolverSystem zoning_solver(macro_registry, event_dispatcher);
    zoning_solver.solve_zoning(5, 5); // 5x5 macro-cells of 20x20 tiles = 100x100 world

    // --- City Generation (Phase 2.2+) ---
    NeonOubliette::CityGenerationSystem city_gen(macro_registry, event_dispatcher);
    city_gen.generateCityFromZones();

    // --- Agent Spawning ---
    NeonOubliette::AgentSpawnSystem agent_spawn(macro_registry, event_dispatcher);
    agent_spawn.spawnAgents(60, 0); 

    // --- Entity Creation ---
    // Spawn player at (10, 13)
    auto player_entity = macro_registry.create();
    macro_registry.emplace<NeonOubliette::PositionComponent>(player_entity, 10, 13, 0); 
    macro_registry.emplace<NeonOubliette::PlayerCurrentLayerComponent>(player_entity, 0);
    macro_registry.emplace<NeonOubliette::PlayerComponent>(player_entity); 
    macro_registry.emplace<NeonOubliette::NameComponent>(player_entity, "Player One");
    macro_registry.emplace<NeonOubliette::InventoryComponent>(player_entity);
    macro_registry.emplace<NeonOubliette::RenderableComponent>(player_entity, '@', "#FFFFFF", 0);
    macro_registry.emplace<NeonOubliette::HUDComponent>(player_entity); 

    // Initial Multi-Layer Components
    macro_registry.emplace<NeonOubliette::Layer0PhysicsComponent>(player_entity);
    macro_registry.emplace<NeonOubliette::Layer1BiologyComponent>(player_entity);
    macro_registry.emplace<NeonOubliette::Layer2CognitiveComponent>(player_entity);
    macro_registry.emplace<NeonOubliette::Layer3EconomicComponent>(player_entity);
    macro_registry.emplace<NeonOubliette::Layer4PoliticalComponent>(player_entity);

    // Welcome message
    event_dispatcher.trigger(NeonOubliette::HUDNotificationEvent{"Neon Oubliette: Procedural Phase Active", 5.0f, "#00FFFF"});

    // --- Initial Render ---
    scheduler.run_phase(NeonOubliette::SystemScheduler::Phase::Output, macro_registry, event_dispatcher, delta_time);

    // --- Main Simulation Loop ---
    while (running) {
        simulation_coordinator.advance_turn(delta_time);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    notcurses_stop(nc_context);
    return 0;
}
