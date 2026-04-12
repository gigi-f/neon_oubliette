#pragma once
#include <string>

namespace NeonOubliette {

// ============================================================
// Feature Flags — edit this file to toggle subsystems on/off.
//
// Top-level flags cascade automatically: setting npcs = false
// disables ALL NPC sub-systems regardless of their sub-flags.
// ============================================================
struct FeatureFlags {

    // ── Top-level toggles ────────────────────────────────────
    bool npcs            = false;  // All NPC / agent systems
    bool city_generation = true;  // City, building, infra generation
    bool economy         = false;  // All economic simulation
    bool politics        = false;  // Factions, law, media, crisis
    bool environment     = false;  // Physics, biology, hazards, ecology
    bool rendering       = true;  // SDL renderer (disable for headless)
    bool player_actions  = true;  // Player-driven interactions (container, item use)

    // ── NPC sub-flags (only active when npcs == true) ────────
    struct {
        bool spawning           = true;
        bool movement           = true;
        bool pathfinding        = true;
        bool dialogue           = true;
        bool social_interaction = true;
        bool faction_behavior   = true;  // agent-level faction decisions
        bool crime              = true;  // guard response, wanted system
        bool cognitive          = true;  // L2 cognitive simulation layer
    } npc;

    // ── City sub-flags (only active when city_generation == true) ──
    struct {
        bool buildings       = true;
        bool infrastructure  = true;
        bool transit         = true;
        bool zoning          = true;
        bool chunk_streaming = true;
        bool demolition      = true;
        bool power_grid      = true;
        bool urban_decay     = true;
    } city;

    // ── Economy sub-flags (only active when economy == true) ─
    struct {
        bool markets              = true;
        bool supply_chain         = true;
        bool drug_manufacturing   = true;
        bool stock_market         = true;
        bool barter               = true;
        bool production           = true;
        bool resource_distribution = true;
    } econ;

    // ── Politics sub-flags (only active when politics == true) ──
    struct {
        bool factions           = true;
        bool religion           = true;
        bool political_opinions = true;
        bool guard_response     = true;  // GuardResponse + WantedLevel
        bool crisis             = true;
        bool media              = true;  // BroadcastTower, UndergroundMedia
    } pol;

    // ── Environment sub-flags (only active when environment == true) ──
    struct {
        bool physics   = true;
        bool biology   = true;
        bool ecosystem = true;
        bool hazards   = true;
    } env;

    // Returns true if the named system should execute.
    // Called by SystemScheduler and SimulationCoordinator.
    bool is_system_enabled(const std::string& name) const;

    // Load/Save flags from/to JSON
    void load_from_file(const std::string& path);
    void save_to_file(const std::string& path);
};

// Global instance — set flags before scheduler construction in main.cpp.
extern FeatureFlags g_feature_flags;

} // namespace NeonOubliette
