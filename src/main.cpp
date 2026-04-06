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
#include <cstring>

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
#include "ecs/systems/city_planner_system.h"
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

    // --- Loading Screen ---
    struct ncplane* stdplane = notcurses_stdplane(nc_context);
    unsigned term_rows, term_cols;
    ncplane_dim_yx(stdplane, &term_rows, &term_cols);

    int loading_step = 0;
    constexpr int LOADING_TOTAL = 8;
    constexpr int BAR_WIDTH = 30;

    auto show_loading = [&](const char* phase) {
        loading_step++;
        ncplane_move_top(stdplane);
        ncplane_erase(stdplane);
        int cy = (int)term_rows / 2;
        int cx = (int)term_cols / 2;

        // Title
        ncplane_set_fg_rgb(stdplane, 0x00FFFF);
        const char* title = "NEON OUBLIETTE";
        ncplane_putstr_yx(stdplane, cy - 3, cx - 7, title);

        // Phase description
        ncplane_set_fg_rgb(stdplane, 0xAAAAAA);
        ncplane_putstr_yx(stdplane, cy - 1, cx - (int)(strlen(phase) / 2), phase);

        // Progress bar
        int filled = (loading_step * BAR_WIDTH) / LOADING_TOTAL;
        int pct = (loading_step * 100) / LOADING_TOTAL;
        char bar[64];
        int bi = 0;
        bar[bi++] = '[';
        for (int i = 0; i < BAR_WIDTH; i++)
            bar[bi++] = (i < filled) ? '#' : '.';
        bar[bi++] = ']';
        bar[bi] = '\0';

        ncplane_set_fg_rgb(stdplane, 0x00FF88);
        ncplane_putstr_yx(stdplane, cy + 1, cx - (BAR_WIDTH + 2) / 2, bar);

        char pct_str[8];
        snprintf(pct_str, sizeof(pct_str), "%3d%%", pct);
        ncplane_set_fg_rgb(stdplane, 0xFFFFFF);
        ncplane_putstr_yx(stdplane, cy + 1, cx + (BAR_WIDTH + 2) / 2 + 1, pct_str);

        notcurses_render(nc_context);
    };

    show_loading("Initializing systems...");

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
    macro_registry.emplace<NeonOubliette::StandardCursorComponent>(config_entity);

    // --- Phase 2: Global Infrastructure Skeleton ---
    show_loading("Carving infrastructure...");
    NeonOubliette::InfrastructureNetworkSystem infra_gen(macro_registry, event_dispatcher);
    infra_gen.generate_skeleton(WORLD_WIDTH, WORLD_HEIGHT);

    // --- Phase 2: Zoning Solver ---
    show_loading("Solving zoning (WFC)...");
    NeonOubliette::ZoningSolverSystem zoning_solver(macro_registry, event_dispatcher);
    zoning_solver.solve_zoning(MACRO_COLS, MACRO_ROWS);

    // --- Phase A.1: City Planner (Block & Lot Subdivision) ---
    show_loading("Planning city layout...");
    NeonOubliette::CityPlannerSystem city_planner(macro_registry, event_dispatcher);
    city_planner.plan_city_layout();

    // --- Phase 2: Capillaries & Junctions ---
    show_loading("Resolving junctions...");
    infra_gen.initialize(); // Populate zone cache
    // Phase A.1: CityPlanner now handles block subdivision and secondary roads
    // infra_gen.generate_capillaries(); 
    infra_gen.resolve_junctions();

    // --- Phase 3.3: Hierarchical Pathfinding Graph ---
    show_loading("Building navigation graph...");
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
    macro_registry.emplace<NeonOubliette::PlayerInteractionComponent>(player_entity);

    // Initial Multi-Layer Components
    macro_registry.emplace<NeonOubliette::Layer0PhysicsComponent>(player_entity);
    macro_registry.emplace<NeonOubliette::Layer1BiologyComponent>(player_entity);
    macro_registry.emplace<NeonOubliette::Layer2CognitiveComponent>(player_entity);
    macro_registry.emplace<NeonOubliette::Layer3EconomicComponent>(player_entity);
    macro_registry.emplace<NeonOubliette::Layer4PoliticalComponent>(player_entity);

    // --- Phase 3: Chunk Streaming System Initial Trigger ---
    show_loading("Streaming initial chunks...");
    NeonOubliette::ChunkStreamingSystem streaming(macro_registry, event_dispatcher);
    streaming.initialize();
    // Manual first update to populate initial area around player
    streaming.update(0.0);

    // --- Agent Spawning (Distributed into Chunks) ---
    show_loading("Spawning population...");
    NeonOubliette::AgentSpawnSystem agent_spawn(macro_registry, event_dispatcher);
    agent_spawn.spawnAgentsIntoChunks(1000); // Massive world population
    agent_spawn.spawnAgents(50, 0); // Local starting population

    // Welcome message
    event_dispatcher.trigger(NeonOubliette::HUDNotificationEvent{"Neon Oubliette: Infrastructure Active (40x40)", 5.0f, "#00FFFF"});

    // Move loading plane back to bottom so game planes are visible
    ncplane_move_bottom(stdplane);

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
