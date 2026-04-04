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
#include "ecs/components/lod_components.h"
#include "ecs/event_declarations.h"
#include "ecs/systems/serialization_system.h"
#include "ecs/systems/city_generation_system.h"
#include "ecs/systems/agent_spawn_system.h"
#include "ecs/systems/zoning_solver_system.h"
#include "ecs/systems/infrastructure_network_system.h"
#include "ecs/systems/macro_navigation_system.h"
#include "ecs/systems/chunk_streaming_system.h"

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

    // --- World Dimensions (single source of truth) ---
    constexpr int MACRO_COLS       = 40;
    constexpr int MACRO_ROWS       = 40;
    constexpr int MACRO_CELL_SIZE  = 20;
    constexpr int WORLD_WIDTH      = MACRO_COLS * MACRO_CELL_SIZE;  // 800
    constexpr int WORLD_HEIGHT     = MACRO_ROWS * MACRO_CELL_SIZE;  // 800

    // --- World Config ---
    auto config_entity = macro_registry.create();
    macro_registry.emplace<NeonOubliette::WorldConfigComponent>(config_entity, WORLD_WIDTH, WORLD_HEIGHT, MACRO_CELL_SIZE, 99999u);
    macro_registry.emplace<NeonOubliette::SimulationStateComponent>(config_entity);
    macro_registry.emplace<NeonOubliette::GodCursorComponent>(config_entity);

    // --- Phase 2: Global Infrastructure Skeleton ---
    NeonOubliette::InfrastructureNetworkSystem infra_gen(macro_registry, event_dispatcher);
    infra_gen.generate_skeleton(WORLD_WIDTH, WORLD_HEIGHT);

    // --- Phase 2: Zoning Solver ---
    NeonOubliette::ZoningSolverSystem zoning_solver(macro_registry, event_dispatcher);
    zoning_solver.solve_zoning(MACRO_COLS, MACRO_ROWS);

    // --- Phase 2: Capillaries & Junctions ---
    infra_gen.initialize(); // Populate zone cache
    infra_gen.generate_capillaries();
    infra_gen.resolve_junctions();

    // --- Phase 3.3: Hierarchical Pathfinding Graph ---
    NeonOubliette::MacroNavigationSystem macro_nav(macro_registry, event_dispatcher);
    macro_nav.rebuild_graph();

    // --- Entity Creation (Player) ---
    auto player_entity = macro_registry.create();
    macro_registry.emplace<NeonOubliette::PositionComponent>(player_entity, WORLD_WIDTH / 2, WORLD_HEIGHT / 2, 0);
    macro_registry.emplace<NeonOubliette::PlayerCurrentLayerComponent>(player_entity, 0);
    macro_registry.emplace<NeonOubliette::PlayerComponent>(player_entity); 
    macro_registry.emplace<NeonOubliette::NameComponent>(player_entity, "Neon Operator");
    macro_registry.emplace<NeonOubliette::InventoryComponent>(player_entity);
    macro_registry.emplace<NeonOubliette::RenderableComponent>(player_entity, '@', "#FFFFFF", 0);
    macro_registry.emplace<NeonOubliette::HUDComponent>(player_entity); 
    macro_registry.emplace<NeonOubliette::PersistentEntityComponent>(player_entity, true);
    macro_registry.emplace<NeonOubliette::VisibilityComponent>(player_entity, 18);
    macro_registry.emplace<NeonOubliette::MemoryComponent>(player_entity);
    macro_registry.emplace<NeonOubliette::VerticalViewComponent>(player_entity, 1);

    // Initial Multi-Layer Components
    macro_registry.emplace<NeonOubliette::Layer0PhysicsComponent>(player_entity);
    macro_registry.emplace<NeonOubliette::Layer1BiologyComponent>(player_entity);
    macro_registry.emplace<NeonOubliette::Layer2CognitiveComponent>(player_entity);
    macro_registry.emplace<NeonOubliette::Layer3EconomicComponent>(player_entity);
    macro_registry.emplace<NeonOubliette::Layer4PoliticalComponent>(player_entity);

    // --- Phase 3: Chunk Streaming System Initial Trigger ---
    NeonOubliette::ChunkStreamingSystem streaming(macro_registry, event_dispatcher);
    streaming.initialize();
    // Manual first update to populate initial area around player
    streaming.update(0.0);

    // --- Agent Spawning (Distributed into Chunks) ---
    NeonOubliette::AgentSpawnSystem agent_spawn(macro_registry, event_dispatcher);
    agent_spawn.spawnAgentsIntoChunks(1000); // Massive world population
    agent_spawn.spawnAgents(20, 0); // Local starting population

    // Welcome message
    event_dispatcher.trigger(NeonOubliette::HUDNotificationEvent{"Neon Oubliette: Infrastructure Active (40x40)", 5.0f, "#00FFFF"});

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
