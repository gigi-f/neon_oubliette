#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <unordered_set>
#include <climits>
#include <fstream>
#include <entt/entt.hpp>
#include "nlohmann/json.hpp"
#include <notcurses/notcurses.h>
#include <thread>
#include <chrono>
#include <clocale>
#include <cstring>
#include <csignal>
#include <cstdio>
#include <cstdlib>

#include <backward.hpp>
#include "util/profiling.h"
#include "feature_flags.h"

#include "config/ConfigLoader.h"
#include "ecs/component_registration.h"
#include "ecs/system_registration.h"
#include "ecs/system_scheduler.h"
#include "ecs/simulation_coordinator.h"
#include "ecs/components/components.h"
#include "ecs/components/milestone_components.h"
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
#include "ecs/systems/visibility_system.h"

// ────────────────────────────────────────────────────────
// Crash handler: writes a rich stack trace to crash.txt
// and restores the terminal before dying.
// ────────────────────────────────────────────────────────
static struct notcurses* g_nc_context = nullptr;
static const char* g_crash_file = "crash.txt";
static const char* g_startup_phase = "pre-init";

static void crash_signal_handler(int sig, siginfo_t* info, void* ctx) {
    (void)info;
    (void)ctx;

    // 1. Restore terminal ASAP so the user sees output
    if (g_nc_context) {
        notcurses_stop(g_nc_context);
        g_nc_context = nullptr;
    }

    // 2. Capture stack trace
    backward::StackTrace st;
    st.load_here(48);
    st.skip_n_firsts(2); // skip this handler + signal trampoline

    // 3. Write to crash file
    {
        std::ofstream crash_file(g_crash_file, std::ios::trunc);
        if (crash_file.is_open()) {
            crash_file << "=== NEON OUBLIETTE CRASH REPORT ===" << std::endl;
            crash_file << "Signal: " << sig << " (" << strsignal(sig) << ")" << std::endl;
            crash_file << "Last startup phase: " << g_startup_phase << std::endl;
            crash_file << std::endl;

            backward::Printer printer;
            printer.snippet = true;   // show source code context if debug info available
            printer.color_mode = backward::ColorMode::never;
            printer.address = true;
            printer.object = true;
            printer.print(st, crash_file);
            crash_file.close();
        }
    }

    // 4. Also print to stderr (terminal is restored now)
    fprintf(stderr, "\n╔══════════════════════════════════════════╗\n");
    fprintf(stderr, "║     NEON OUBLIETTE — FATAL CRASH         ║\n");
    fprintf(stderr, "╠══════════════════════════════════════════╣\n");
    fprintf(stderr, "║ Signal: %d (%s)\n", sig, strsignal(sig));
    fprintf(stderr, "║ Phase:  %s\n", g_startup_phase);
    fprintf(stderr, "╠══════════════════════════════════════════╣\n");

    backward::Printer printer;
    printer.snippet = false;
    printer.color_mode = backward::ColorMode::automatic;
    printer.address = true;
    printer.object = true;
    printer.print(st, stderr);

    fprintf(stderr, "╠══════════════════════════════════════════╣\n");
    fprintf(stderr, "║ Full report saved to: %s\n", g_crash_file);
    fprintf(stderr, "╚══════════════════════════════════════════╝\n");

    // 5. Re-raise with default handler to get proper exit code
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(sig, &sa, nullptr);
    raise(sig);
}

static void install_crash_handler() {
    const int signals[] = { SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL };
    struct sigaction sa;
    sa.sa_sigaction = crash_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND; // one-shot to avoid loops
    for (int sig : signals) {
        sigaction(sig, &sa, nullptr);
    }
}

static bool running = true;

void shutdown_handler(const NeonOubliette::ShutdownEvent& event) {
    running = false;
}

// Diagnostic: test registry health by creating+destroying an entity
static void probe_registry(entt::registry& reg, const char* phase) {
    FILE* f = fopen("/tmp/neon_probe.log", "a");
    if (f) {
        fprintf(f, "[PROBE] After %-20s: alive=%zu\n", phase, reg.storage<entt::entity>().size());
        fclose(f);
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    // Load feature flags
    NeonOubliette::g_feature_flags.load_from_file("data/configs/feature_flags.json");

    bool headless = (getenv("NEON_HEADLESS") != nullptr);
    if (headless) {
        NeonOubliette::g_feature_flags.rendering = false;
    }
    
    // Sync headless with feature flag
    headless = !NeonOubliette::g_feature_flags.rendering;

    // Install crash handler FIRST, before anything else
    install_crash_handler();
    g_startup_phase = "locale-init";

    std::setlocale(LC_ALL, "");

    struct notcurses* nc_context = nullptr;
    struct ncplane* stdplane = nullptr;
    unsigned term_rows = 80, term_cols = 120;

    if (!headless) {
    g_startup_phase = "notcurses-init";
    struct notcurses_options ncopts = {}; 
    ncopts.loglevel = NCLOGLEVEL_SILENT;
    ncopts.flags = NCOPTION_SUPPRESS_BANNERS;
    
    nc_context = notcurses_init(&ncopts, nullptr);
    if (nc_context == nullptr) {
        return 1;
    }
    g_nc_context = nc_context; // Let crash handler know about notcurses

    // --- Set charcoal background for consistent terminal experience ---
    stdplane = notcurses_stdplane(nc_context);
    {
        uint64_t bg_channels = NCCHANNELS_INITIALIZER(200, 200, 200, 30, 30, 30);
        ncplane_set_base(stdplane, " ", 0, bg_channels);
    }

    // --- Loading Screen ---
    ncplane_dim_yx(stdplane, &term_rows, &term_cols);
    } // end if (!headless)

    int loading_step = 0;
    constexpr int LOADING_TOTAL = 8;
    constexpr int BAR_WIDTH = 30;

    auto show_loading = [&](const char* phase) {
        loading_step++;
        if (headless) return;
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
    g_startup_phase = "system-init";

    entt::registry macro_registry;
    entt::dispatcher event_dispatcher;

    event_dispatcher.sink<NeonOubliette::ShutdownEvent>().connect<&shutdown_handler>();

    { // Scope block: scheduler/systems destroyed before notcurses_stop
    NeonOubliette::SystemScheduler scheduler(macro_registry, event_dispatcher);
    NeonOubliette::register_all_systems(scheduler, nc_context, macro_registry, event_dispatcher);
    
    NeonOubliette::SimulationCoordinator simulation_coordinator(macro_registry, event_dispatcher, scheduler);
    NeonOubliette::register_simulation_systems(simulation_coordinator, macro_registry, event_dispatcher);
    
    g_startup_phase = "scheduler-initialize";
    scheduler.initialize_all_systems();
    simulation_coordinator.initialize_all_systems();

    double delta_time = 1.0 / 60.0;

    // --- World Dimensions (single source of truth) ---
    constexpr int MACRO_COLS       = 20;
    constexpr int MACRO_ROWS       = 20;
    constexpr int MACRO_CELL_SIZE  = 40; // 40m city block
    constexpr int WORLD_WIDTH      = MACRO_COLS * MACRO_CELL_SIZE;  // 800
    constexpr int WORLD_HEIGHT     = MACRO_ROWS * MACRO_CELL_SIZE;  // 800

    // --- World Config ---
    g_startup_phase = "world-config";
    auto config_entity = macro_registry.create();
    macro_registry.emplace<NeonOubliette::WorldConfigComponent>(config_entity, WORLD_WIDTH, WORLD_HEIGHT, MACRO_CELL_SIZE, MACRO_CELL_SIZE * 2, 99999u);
    macro_registry.emplace<NeonOubliette::SimulationStateComponent>(config_entity);
    macro_registry.emplace<NeonOubliette::GodCursorComponent>(config_entity);
    macro_registry.emplace<NeonOubliette::StandardCursorComponent>(config_entity);
    macro_registry.emplace<NeonOubliette::DebugOverlayComponent>(config_entity);
    macro_registry.emplace<NeonOubliette::MilestoneComponent>(config_entity);
    probe_registry(macro_registry, "world-config");

    // --- Phase 2: Global Infrastructure Skeleton ---
    show_loading("Carving infrastructure...");
    g_startup_phase = "infra-skeleton";
    NeonOubliette::InfrastructureNetworkSystem infra_gen(macro_registry, event_dispatcher);
    infra_gen.generate_skeleton(WORLD_WIDTH, WORLD_HEIGHT);
    probe_registry(macro_registry, "infra-skeleton");

    // --- Phase 2: Zoning Solver ---
    show_loading("Solving zoning (WFC)...");
    g_startup_phase = "zoning-solver";
    NeonOubliette::ZoningSolverSystem zoning_solver(macro_registry, event_dispatcher);
    zoning_solver.solve_zoning(MACRO_COLS, MACRO_ROWS);
    probe_registry(macro_registry, "zoning-solver");

    // --- Phase A.1: City Planner (Block & Lot Subdivision) ---
    show_loading("Planning city layout...");
    g_startup_phase = "city-planner";
    NeonOubliette::CityPlannerSystem city_planner(macro_registry, event_dispatcher);
    city_planner.plan_city_layout();
    probe_registry(macro_registry, "city-planner");

    // --- Phase 2: Capillaries & Junctions ---
    show_loading("Resolving junctions...");
    g_startup_phase = "infra-junctions";
    infra_gen.initialize(); // Populate zone cache
    probe_registry(macro_registry, "infra-initialize");
    // Phase A.1: CityPlanner now handles block subdivision and secondary roads
    // infra_gen.generate_capillaries(); 
    infra_gen.resolve_junctions();
    probe_registry(macro_registry, "resolve-junctions");

    // --- Phase 3.3: Hierarchical Pathfinding Graph ---
    if (NeonOubliette::g_feature_flags.is_system_enabled("MacroNav")) {
        show_loading("Building navigation graph...");
        g_startup_phase = "nav-graph";
        NeonOubliette::MacroNavigationSystem macro_nav(macro_registry, event_dispatcher);
        macro_nav.rebuild_graph();
        probe_registry(macro_registry, "nav-graph");
    }

    // --- Entity Creation (Player) ---
    g_startup_phase = "player-entity";
    auto player_entity = macro_registry.create();

    // Find a walkable road/sidewalk position near world center using ArterialGrid
    // (Preliminary — will be refined after chunk materialization using actual terrain)
    int spawn_x = WORLD_WIDTH / 2, spawn_y = WORLD_HEIGHT / 2;
    {
        auto* grid = macro_registry.ctx().find<NeonOubliette::ArterialGrid>();
        if (grid) {
            NeonOubliette::ArterialType at;
            bool found = false;
            // Spiral outward from center looking for a road tile on layer 0
            for (int radius = 0; radius <= 120 && !found; ++radius) {
                for (int dx = -radius; dx <= radius && !found; ++dx) {
                    for (int dy = -radius; dy <= radius && !found; ++dy) {
                        if (std::abs(dx) != radius && std::abs(dy) != radius) continue; // perimeter only
                        int cx = WORLD_WIDTH / 2 + dx, cy = WORLD_HEIGHT / 2 + dy;
                        if (grid->type_at(cx, cy, 0, at)) {
                            if (at == NeonOubliette::ArterialType::ROAD_PRIMARY ||
                                at == NeonOubliette::ArterialType::ROAD_SECONDARY ||
                                at == NeonOubliette::ArterialType::SIDEWALK) {
                                spawn_x = cx; spawn_y = cy; found = true;
                            }
                        }
                    }
                }
            }
        }
    }

    macro_registry.emplace<NeonOubliette::PositionComponent>(player_entity, spawn_x, spawn_y, 0);
    macro_registry.emplace<NeonOubliette::PlayerCurrentLayerComponent>(player_entity, 0);
    macro_registry.emplace<NeonOubliette::PlayerComponent>(player_entity); 
    macro_registry.emplace<NeonOubliette::NameComponent>(player_entity, "Neon Operator");
    macro_registry.emplace<NeonOubliette::InventoryComponent>(player_entity);
    macro_registry.emplace<NeonOubliette::RenderableComponent>(player_entity, '@', "#FFA500", 0);
    macro_registry.emplace<NeonOubliette::HUDComponent>(player_entity); 
    macro_registry.emplace<NeonOubliette::PersistentEntityComponent>(player_entity, true);
    macro_registry.emplace<NeonOubliette::VisibilityComponent>(player_entity, 2000);
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
    g_startup_phase = "chunk-streaming";
    probe_registry(macro_registry, "pre-chunk-init");
    NeonOubliette::ChunkStreamingSystem streaming(macro_registry, event_dispatcher);
    streaming.initialize();
    probe_registry(macro_registry, "post-chunk-init");

    // --- Seed macro population into chunks BEFORE materialization ---
    // This ensures agents stored in hot chunks get instantiated during materialize_chunk().
    if (NeonOubliette::g_feature_flags.is_system_enabled("AgentSpawn")) {
        show_loading("Spawning population...");
        g_startup_phase = "agent-spawn";
        NeonOubliette::AgentSpawnSystem agent_spawn(macro_registry, event_dispatcher);
        agent_spawn.spawnAgentsIntoChunks(100000); // World-wide dormant population
        probe_registry(macro_registry, "post-spawn-chunks");
    }

    // Manual first update to populate initial area around player
    // (also instantiates macro agents stored in hot chunks)
    streaming.update(0.0);
    probe_registry(macro_registry, "post-chunk-update");

    // --- Relocate player to walkable terrain after chunks are materialized ---
    // ArterialGrid marks roads but buildings are placed on top during materialization.
    // Scan actual terrain entities to find the nearest walkable, non-obstacle tile.
    {
        // Build obstacle set for layer 0 — expand volumetric obstacles by their full bounding box
        std::unordered_set<uint64_t> obstacle_set;
        auto obs_view = macro_registry.view<NeonOubliette::PositionComponent, NeonOubliette::ObstacleComponent>();
        for (auto e : obs_view) {
            const auto& p = obs_view.get<NeonOubliette::PositionComponent>(e);
            if (p.layer_id != 0) continue;
            if (auto* sz = macro_registry.try_get<NeonOubliette::SizeComponent>(e)) {
                for (int ix = p.x; ix < p.x + sz->width; ++ix)
                    for (int iy = p.y; iy < p.y + sz->height; ++iy)
                        obstacle_set.insert((static_cast<uint64_t>(ix) << 32) | static_cast<uint32_t>(iy));
            } else {
                obstacle_set.insert((static_cast<uint64_t>(p.x) << 32) | static_cast<uint32_t>(p.y));
            }
        }

        // Collect walkable terrain positions on layer 0
        struct WalkPos { int x, y; };
        std::vector<WalkPos> candidates;
        auto terrain_view = macro_registry.view<NeonOubliette::PositionComponent, NeonOubliette::TerrainComponent>();
        for (auto e : terrain_view) {
            const auto& pos = terrain_view.get<NeonOubliette::PositionComponent>(e);
            if (pos.layer_id != 0) continue;
            const auto& terr = terrain_view.get<NeonOubliette::TerrainComponent>(e);
            if (terr.type != NeonOubliette::TerrainType::STREET &&
                terr.type != NeonOubliette::TerrainType::SIDEWALK) continue;
            uint64_t key = (static_cast<uint64_t>(pos.x) << 32) | static_cast<uint32_t>(pos.y);
            if (obstacle_set.count(key) == 0) candidates.push_back({pos.x, pos.y});
        }

        if (!candidates.empty()) {
            // Find the candidate closest to player's current position
            auto& player_pos = macro_registry.get<NeonOubliette::PositionComponent>(player_entity);
            int best_dist = INT_MAX;
            int best_x = player_pos.x, best_y = player_pos.y;
            for (const auto& c : candidates) {
                int dist = std::abs(c.x - player_pos.x) + std::abs(c.y - player_pos.y);
                if (dist < best_dist) { best_dist = dist; best_x = c.x; best_y = c.y; }
            }
            player_pos.x = best_x;
            player_pos.y = best_y;
        }
    }

    // All non-player population is now spawned via chunk records and materialized by chunk streaming.
    probe_registry(macro_registry, "post-population-streamed");

    // Welcome message
    if (!headless)
        event_dispatcher.trigger(NeonOubliette::HUDNotificationEvent{"Neon Oubliette: Infrastructure Active (Metric 1:1 Scale)", 5.0f, "#00FFFF"});

    // Move loading plane back to bottom so game planes are visible
    if (!headless)
        ncplane_move_bottom(stdplane);

    // Seed initial FOV/memory before first render so terrain appears immediately.
    g_startup_phase = "initial-visibility";
    {
        NeonOubliette::VisibilitySystem initial_visibility(macro_registry, event_dispatcher);
        initial_visibility.initialize();
        initial_visibility.update(0.0);
    }
    probe_registry(macro_registry, "post-visibility");

    // --- Headless mode: print entity counts and exit ---
    if (headless) {
        FILE* f = fopen("/tmp/neon_probe.log", "a");
        if (f) {
            fprintf(f, "[HEADLESS] Full startup complete. Final entity count: %zu\n",
                    macro_registry.storage<entt::entity>().size());
            size_t seg_count = 0;
            auto seg_view = macro_registry.view<NeonOubliette::InfrastructureSegmentComponent>();
            for (auto e : seg_view) { (void)e; seg_count++; }
            fprintf(f, "[HEADLESS] InfrastructureSegmentComponent entities: %zu\n", seg_count);
            size_t node_count = 0;
            auto node_view = macro_registry.view<NeonOubliette::InfrastructureNodeComponent>();
            for (auto e : node_view) { (void)e; node_count++; }
            fprintf(f, "[HEADLESS] InfrastructureNodeComponent entities: %zu\n", node_count);
            size_t npc_count = 0;
            auto npc_view = macro_registry.view<NeonOubliette::NPCComponent>();
            for (auto e : npc_view) { (void)e; npc_count++; }
            size_t agent_count = 0;
            auto agent_view = macro_registry.view<NeonOubliette::AgentComponent>();
            for (auto e : agent_view) { (void)e; agent_count++; }
            fprintf(f, "[HEADLESS] Live NPC entities: %zu  Agent entities: %zu\n", npc_count, agent_count);
            fclose(f);
        }
        return 0;
    }

    // --- Initial Render ---
    g_startup_phase = "initial-render";
    scheduler.run_phase(NeonOubliette::SystemScheduler::Phase::Output, macro_registry, event_dispatcher, delta_time);

    // --- Main Simulation Loop ---
    g_startup_phase = "game-loop";
    using clock = std::chrono::steady_clock;
    auto last_frame_time = clock::now();
    constexpr auto kFrameBudget = std::chrono::milliseconds(16); // ~60 FPS cap

    while (running) {
        auto frame_start = clock::now();
        delta_time = std::chrono::duration<double>(frame_start - last_frame_time).count();
        if (delta_time <= 0.0 || delta_time > 0.25) {
            delta_time = 1.0 / 60.0;
        }
        last_frame_time = frame_start;

        simulation_coordinator.advance_turn(delta_time);

        auto frame_elapsed = clock::now() - frame_start;
        if (frame_elapsed < kFrameBudget) {
            std::this_thread::sleep_for(kFrameBudget - frame_elapsed);
        }
        FrameMark;
    }

    g_nc_context = nullptr; // Don't double-stop in crash handler
    } // scheduler, simulation_coordinator destroyed here — before notcurses_stop
    notcurses_stop(nc_context);
    return 0;
}
