#ifndef NEON_OUBLIETTE_ECS_COMPONENTS_H
#define NEON_OUBLIETTE_ECS_COMPONENTS_H

#include <cereal/archives/binary.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/set.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/unordered_set.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/vector.hpp>
#include <entt/entt.hpp>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <limits>



#include "../command_buffer.h"
#include "base_types.h"
#include "simulation_layers.h"
#include "zoning_components.h"
#include "transit_components.h"
#include "religion_components.h"
#include "crisis_components.h"
#include "underground_media_components.h"

namespace NeonOubliette {

// =====================================================================
// Enums & Constants
// =====================================================================

enum class AgentTaskType : uint32_t {
    IDLE, WANDER, SEEK_FOOD, SEEK_WATER, SEEK_SHELTER, GO_TO_WORK,
    PICK_UP_ITEM, CONSUME_ITEM, MOVE_TO_TARGET, MOVE_ALONG_PATH,
    SEEK_HARVESTABLE, OPEN_CONTAINER, TAKE_ITEM_FROM_CONTAINER,
    CRAFT_ITEM, USE_ITEM, AWAITING_PATH, SHOVE, PATROL,
    WAIT_FOR_TRANSIT, RIDE_TRANSIT, WORSHIP, VISIT_SHRINE,
    FOLLOW_LEADER, PROCESSION,
    // [I.1] Crime behaviors
    STEAL_FROM_AGENT, MULE_GOODS, SELL_CONTRABAND,
    // [I.2] Mugging behaviors
    MUG_AGENT, AWAIT_MUGGING_COMPLIANCE, REACT_TO_MUGGING, FLEE,
    // [I.3] Fencing behaviors
    SEEK_FENCE, FENCE_ITEM,
    // [I.6] Guard Response behaviors
    PURSUE, INVESTIGATE, ARREST,
    // [K.2] Maintenance & Squatting behaviors
    REPAIR, SQUAT,
    // [O.1] Raw Material Extraction
    EXTRACT_RESOURCE,
    // [O.2] Factory Production
    PRODUCE_GOODS
};

enum class ActivityType : uint32_t {
    NONE = 0, CRAFTING, HARVESTING, BUILDING, RESTING, MINING, RESEARCHING, WORKING
};

enum class GraffitiType : uint8_t {
    TAG,         // Quick, small mark ('*', '~')
    TERRITORY,   // Faction claim pattern ('#', '$', '^')
    MURAL,       // Large artistic piece ('&', '@')
    WARNING,     // Utility indicator ('!', '?')
    SLUR         // Hostile towards other factions ('%', 'X')
};

enum class WorkstationType : uint32_t {
    NONE = 0, CRAFTING_BENCH, FORGE, LABORATORY
};

struct InformationComponent {
    std::vector<InformationRecord> records;
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(records));
    }
};

// =====================================================================
// Core Components
// =====================================================================

struct SimulationStateComponent {
    SimulationMode mode = SimulationMode::STANDARD;
    bool is_paused = false;
    float god_mode_tps = 2.0f; // Ticks per second in God Mode
    float accumulator = 0.0f;
    
    // God Mode Building Focus [B.3]
    entt::entity focused_building = entt::null;
    bool is_inside_view = false;
    int focus_floor = 0;

    template <class Archive> void serialize(Archive& ar) { 
        ar(CEREAL_NVP(mode), CEREAL_NVP(is_paused), CEREAL_NVP(god_mode_tps), 
           CEREAL_NVP(accumulator), CEREAL_NVP(focused_building), 
           CEREAL_NVP(is_inside_view), CEREAL_NVP(focus_floor)); 
    }
};

struct GodCursorComponent {
    int x = 0;
    int y = 0;
    int layer_id = 0;
    bool active = false;
    template <class Archive> void serialize(Archive& ar) { 
        ar(CEREAL_NVP(x), CEREAL_NVP(y), CEREAL_NVP(layer_id), CEREAL_NVP(active)); 
    }
};

/**
 * @brief [D.3] Tracks which entity the God Mode camera is currently locked onto.
 */
struct GodModeFollowComponent {
    entt::entity target = entt::null;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(target)); }
};

/**
 * @brief [D.3] Allows God Mode to tag entities with metadata labels.
 */
struct TaggedComponent {
    std::string tag_label = "TAG";
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(tag_label)); }
};

/**
 * @brief [D.3] Context Menu state for God Mode interactions.
 */
struct ContextMenuComponent {
    bool open = false;
    int world_x = 0;
    int world_y = 0;
    int layer_id = 0;
    std::vector<std::string> options;
    int selected_index = 0;
    entt::entity target_entity = entt::null;
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(open), CEREAL_NVP(world_x), CEREAL_NVP(world_y), CEREAL_NVP(layer_id), 
           CEREAL_NVP(options), CEREAL_NVP(selected_index), CEREAL_NVP(target_entity));
    }
};

struct StandardCursorComponent {
    int x = 0;
    int y = 0;
    int layer_id = 0;
    bool active = false;
    bool mouse_driven = false;
    int screen_x = -1;  // raw terminal col of mouse pointer
    int screen_y = -1;  // raw terminal row of mouse pointer
    template <class Archive> void serialize(Archive& ar) { 
        ar(CEREAL_NVP(x), CEREAL_NVP(y), CEREAL_NVP(layer_id), CEREAL_NVP(active), CEREAL_NVP(mouse_driven)); 
    }
};

struct SpeechComponent {
    std::string full_text;
    std::vector<std::string> chunks;
    uint32_t current_chunk_index = 0;
    uint32_t ticks_remaining_in_chunk = 0;
    uint32_t ticks_per_chunk = 2; // Default 2 simulation steps per chunk as per F.5 "one phrase segment per simulation step" (adjusting to 2 for readability)
    
    entt::entity speaker = entt::null;
    AudibilityLevel audibility = AudibilityLevel::CLEAR;
    
    float alpha = 1.0f; // For alpha ramping [F.5]

    // [F.6] Overheard Intelligence
    bool has_record = false;
    InformationRecord record;

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(full_text), CEREAL_NVP(chunks), CEREAL_NVP(current_chunk_index), 
           CEREAL_NVP(ticks_remaining_in_chunk), CEREAL_NVP(ticks_per_chunk),
           CEREAL_NVP(speaker), CEREAL_NVP(audibility), CEREAL_NVP(alpha),
           CEREAL_NVP(has_record), CEREAL_NVP(record));
    }
};

/**
 * @brief [F.6] Stores the history of overheard conversations for the player.
 */
struct DialogueLogEntry {
    std::string speaker_name;
    std::string text;
    uint64_t tick = 0;
    int x = 0, y = 0, layer = 0;
    AudibilityLevel audibility = AudibilityLevel::CLEAR;

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(speaker_name), CEREAL_NVP(text), CEREAL_NVP(tick), CEREAL_NVP(x), CEREAL_NVP(y), CEREAL_NVP(layer), CEREAL_NVP(audibility));
    }
};

struct DialogueLogComponent {
    std::vector<DialogueLogEntry> entries;
    bool visible = false;
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(entries), CEREAL_NVP(visible));
    }
};

/**
 * @brief [F.3] Defines a specific speech style or dialect profile (e.g., CORPORATE, SYNDICATE).
 *        Modifies grammar template selection and applies post-processing filters.
 */
struct SpeechProfileComponent {
    std::string profile_id; // e.g., "CORPORATE", "SYNDICATE", "CACOGEN"
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(profile_id)); }
};

struct WorldConfigComponent {
    int width = 0;
    int height = 0;
    int macro_cell_size = 20;
    int chunk_size = 40;        // macro_cell_size * 2 — set during init
    uint32_t world_seed = 12345;
    uint64_t next_macro_id = 1000; // Start high to avoid collision with low-level stubs
    template <class Archive> void serialize(Archive& ar) { ar(cereal::make_nvp("width", width), cereal::make_nvp("height", height), cereal::make_nvp("macro_cell_size", macro_cell_size), cereal::make_nvp("chunk_size", chunk_size), cereal::make_nvp("world_seed", world_seed), cereal::make_nvp("next_macro_id", next_macro_id)); }
};

// Helper: get chunk_size from registry (returns chunk_size from WorldConfigComponent, defaults to 240)
inline int get_chunk_size(const entt::registry& reg) {
    auto v = reg.view<WorldConfigComponent>();
    if (v.begin() != v.end()) return v.get<WorldConfigComponent>(*v.begin()).chunk_size;
    return 240;
}

struct NameComponent {
    std::string name;
    NameComponent() = default;
    NameComponent(std::string n) : name(n) {}
    template <class Archive> void serialize(Archive& ar) { ar(cereal::make_nvp("name", name)); }
};

struct PlayerInteractionComponent {
    InteractionMode current_mode = InteractionMode::OBSERVE;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(current_mode)); }
};

struct PlayerComponent { 
    uint64_t macro_id = 1; 
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(macro_id)); } 
};

struct HUDComponent {
    float health = 100.0f; int credits = 0; int current_layer_display = 0;
    std::vector<std::string> notifications;
    bool show_controls_help = true;
    bool inventory_open = false;
    int selected_inventory_index = 0;
    entt::entity held_item = entt::null; // [C.1]
    float held_item_flash_timer = 0.0f; // [C.2]
    
    // [L.2] Economic Crisis Display
    float economic_stress_display = 0.0f;
    std::string economy_status_label = "STABLE";
    
    // [I.5] Player Wanted Display
    std::map<std::string, int> faction_wanted_levels; 
    float global_notoriety = 0.0f;

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(health), CEREAL_NVP(credits), CEREAL_NVP(current_layer_display), 
           CEREAL_NVP(notifications), CEREAL_NVP(show_controls_help), 
           CEREAL_NVP(inventory_open), CEREAL_NVP(selected_inventory_index),
           CEREAL_NVP(held_item), CEREAL_NVP(held_item_flash_timer),
           CEREAL_NVP(faction_wanted_levels), CEREAL_NVP(global_notoriety),
           CEREAL_NVP(economic_stress_display), CEREAL_NVP(economy_status_label));
    }
};

struct PlayerCurrentLayerComponent {
    int current_z = 0;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(current_z)); }
};

struct InventoryComponent {
    std::vector<entt::entity> contained_items;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(contained_items)); }
};

struct ItemComponent {
    uint32_t item_type_id = 0; std::string name;
    ItemComponent() = default;
    ItemComponent(uint32_t id, std::string n) : item_type_id(id), name(n) {}
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(item_type_id), CEREAL_NVP(name)); }
};

struct ItemValueComponent {
    int value = 1;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(value)); }
};

struct ItemMarketCategoryComponent {
    ItemMarketCategory category = ItemMarketCategory::NONE;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(category)); }
};

struct ItemMaterialComponent {
    RawMaterialType material = RawMaterialType::METAL;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(material)); }
};

struct TerrainComponent {
    TerrainType type = TerrainType::VOID;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(type)); }
};

// =====================================================================
// Structural & Building Components
// =====================================================================

struct BuildingComponent {
    int height = 1; ZoneType zone_type = ZoneType::VOID; int occupant_count = 0;
    uint32_t building_id = 0; // [NEW] Stable ID for layer generation and caching
    CommandBuffer command_buffer;
    template <class Archive> void serialize(Archive& ar) { 
        ar(CEREAL_NVP(height), CEREAL_NVP(zone_type), CEREAL_NVP(occupant_count), CEREAL_NVP(building_id)); 
    }
};

struct NavGrid {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> grid; // 0 = passable, 1 = obstacle

    bool is_passable(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return false;
        return grid[y * width + x] == 0;
    }

    void set_passable(int x, int y, bool passable) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            grid[y * width + x] = passable ? 0 : 1;
        }
    }

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(width), CEREAL_NVP(height), CEREAL_NVP(grid));
    }
};

struct FloorComponent {
    int level = 0;
    int layer_id = 0;
    std::vector<entt::entity> rooms;
    NavGrid nav_grid;
    FloorComponent() = default;
    FloorComponent(int l, int lid = 0) : level(l), layer_id(lid) {}
    template <class Archive> void serialize(Archive& ar) { 
        ar(CEREAL_NVP(level), CEREAL_NVP(layer_id), CEREAL_NVP(rooms), CEREAL_NVP(nav_grid)); 
    }
};

struct RoomComponent {
    uint32_t room_id = 0; int capacity = 0;
    RoomComponent() = default; RoomComponent(uint32_t id, int cap) : room_id(id), capacity(cap) {}
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(room_id), CEREAL_NVP(capacity)); }
};

struct ApartmentComponent {
    int floor = 0; int unit_number = 0; bool is_occupied = false;
    ApartmentComponent() = default; ApartmentComponent(int f, int u, bool occ) : floor(f), unit_number(u), is_occupied(occ) {}
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(floor), CEREAL_NVP(unit_number), CEREAL_NVP(is_occupied)); }
};

struct StairsComponent {
    int connects_to_layer = 0;
    StairsComponent() = default; StairsComponent(int target) : connects_to_layer(target) {}
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(connects_to_layer)); }
};

struct ElevatorComponent {
    int top_layer = 0; int bottom_layer = 0;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(top_layer), CEREAL_NVP(bottom_layer)); }
};

struct RoomData {
    int x = 0; int y = 0; int width = 0; int height = 0;
    RoomTag tag = RoomTag::VOID;
    template <class Archive> void serialize(Archive& ar) { 
        ar(CEREAL_NVP(x), CEREAL_NVP(y), CEREAL_NVP(width), CEREAL_NVP(height), CEREAL_NVP(tag)); 
    }
};

/**
 * @brief [NEW CLASS] Record for items/furniture in a "cold" interior.
 */
struct MacroObjectRecord {
    std::string name;
    char glyph = '?';
    std::string color = "#FFFFFF";
    int x = 0;
    int y = 0;
    int layer_id = 0;
    bool is_obstacle = false;
    uint32_t item_type_id = 0;
    int item_value = 0;
    int restores_hunger = 0;
    int restores_thirst = 0;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(name), CEREAL_NVP(glyph), CEREAL_NVP(color), CEREAL_NVP(x), CEREAL_NVP(y),
           CEREAL_NVP(layer_id), CEREAL_NVP(is_obstacle), CEREAL_NVP(item_type_id),
           CEREAL_NVP(item_value), CEREAL_NVP(restores_hunger), CEREAL_NVP(restores_thirst));
    }
};

struct BuildingInteriorComponent {
    std::vector<RoomData> rooms;
    std::vector<PositionComponent> internal_doors;
    std::vector<PositionComponent> stairs;
    bool is_generated = false; 
    std::vector<entt::entity> floor_entities;
    std::vector<MacroObjectRecord> stored_objects; // [NEW] Non-agent objects in interior
    int interior_width = 0; // [NEW] Cache dimensions
    int interior_height = 0;
    bool is_dirty = false; // [NEW] Stability contract flag

    template <class Archive> void serialize(Archive& ar) { 
        ar(CEREAL_NVP(rooms), CEREAL_NVP(internal_doors), CEREAL_NVP(stairs), 
           CEREAL_NVP(is_generated), CEREAL_NVP(floor_entities), CEREAL_NVP(stored_objects),
           CEREAL_NVP(interior_width), CEREAL_NVP(interior_height), CEREAL_NVP(is_dirty)); 
    }
};

/**
 * @brief [B.5] Adjacency graph for sound propagation.
 */
struct AcousticsEdge {
    size_t target_room_index;
    entt::entity door_entity = entt::null; // if null, it's a solid wall

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(target_room_index), CEREAL_NVP(door_entity));
    }
};

struct RoomAcoustics {
    std::vector<AcousticsEdge> neighbors;
    bool has_window = false;

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(neighbors), CEREAL_NVP(has_window));
    }
};

struct BuildingAcousticsComponent {
    std::vector<RoomAcoustics> nodes;

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(nodes));
    }
};

struct BuildingEntranceComponent {
    entt::entity macro_building_id = entt::null; int entry_layer_id = 0;
    BuildingEntranceComponent() = default; BuildingEntranceComponent(entt::entity b, int l) : macro_building_id(b), entry_layer_id(l) {}
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(macro_building_id), CEREAL_NVP(entry_layer_id)); }
};

/**
 * @brief [NEW CLASS] Metadata for exterior doors to support spatially consistent exits.
 */
struct DoorMetadataComponent {
    StreetFacingSide wall_side = StreetFacingSide::NORTH;
    int wall_offset = 0; // Tiles from the left/top edge of the building wall
    entt::entity building_entity = entt::null;
    
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(wall_side), CEREAL_NVP(wall_offset), CEREAL_NVP(building_entity));
    }
};

/**
 * @brief [NEW CLASS] Tracks the visitor's state relative to the building they are currently in.
 *        Enables spatially consistent exits.
 */
struct InteriorStateComponent {
    entt::entity building_entity = entt::null;
    StreetFacingSide entry_wall = StreetFacingSide::NORTH;
    int entry_offset = 0;
    int entry_x = 0; // World-space coordinates of the door
    int entry_y = 0;
    int entry_layer = 0;
    int current_room_index = -1; // [B.5]

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(building_entity), CEREAL_NVP(entry_wall), CEREAL_NVP(entry_offset), 
           CEREAL_NVP(entry_x), CEREAL_NVP(entry_y), CEREAL_NVP(entry_layer), CEREAL_NVP(current_room_index));
    }
};

/**
 * @brief Decorative facade sign projected onto the ground layer to simulate elevated signage.
 */
struct FacadeSignComponent {
    int simulated_height_meters = 6;
    bool is_advertisement = true;

    template <class Archive>
    void serialize(Archive& ar) {
        ar(CEREAL_NVP(simulated_height_meters), CEREAL_NVP(is_advertisement));
    }
};

/**
 * @brief Portals connect two specific positions across layers.
 *        Enables coherent door-to-door mechanics.
 */
struct PortalComponent {
    int target_x = 0;
    int target_y = 0;
    int target_layer = 0;
    bool is_two_way = true;
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(target_x), CEREAL_NVP(target_y), CEREAL_NVP(target_layer), CEREAL_NVP(is_two_way));
    }
};

// =====================================================================
// AI & Agent Components
// =====================================================================

/**
 * @brief [J.1] Defines the life stage of an agent.
 */
enum class LifeStage : uint8_t {
    INFANT,       // Metaphor: Small, fragile ('.')
    CHILD,        // Metaphor: Maturing, lowercase
    YOUNG_ADULT,  // Metaphor: Peak vibrancy
    ADULT,        // Metaphor: Standard city dweller
    ELDER,        // Metaphor: Fading, desaturated/dimmed
    ANCIENT,      // Metaphor: Mythic, specialized glyphs
    AGELESS       // Metaphor: Unchanging (Cacogens/Hierodules)
};

/**
 * @brief [J.2] Tracks the biological reproduction process for an agent.
 */
struct ReproductionComponent {
    bool is_pregnant = false;
    uint32_t gestation_ticks_remaining = 0;
    entt::entity partner = entt::null;
    float genetic_health = 1.0f;

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(is_pregnant), CEREAL_NVP(gestation_ticks_remaining), 
           CEREAL_NVP(partner), CEREAL_NVP(genetic_health));
    }
};

/**
 * @brief [J.1] Tracks chronological and biological age.
 */
struct AgeComponent {
    uint32_t current_age_ticks = 0;
    uint32_t years = 0;
    LifeStage stage = LifeStage::ADULT;
    float biological_wear = 0.0f; // 0.0 (fresh) to 1.0 (critical decay)
    uint32_t expected_lifespan_years = 120; // Default for enhanced humans

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(current_age_ticks), CEREAL_NVP(years), CEREAL_NVP(stage), 
           CEREAL_NVP(biological_wear), CEREAL_NVP(expected_lifespan_years));
    }
};

struct AgentComponent { template <class Archive> void serialize(Archive&) {} };
struct NPCComponent { 
    int health = 100; uint64_t macro_id = 0;
    NPCComponent() = default; NPCComponent(int h, uint64_t m) : health(h), macro_id(m) {}
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(health), CEREAL_NVP(macro_id)); }
};
struct CitizenComponent { template <class Archive> void serialize(Archive&) {} };
struct AgentTaskComponent {
    AgentTaskType task_type = AgentTaskType::IDLE; entt::entity target_entity = entt::null; entt::entity secondary_entity = entt::null;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(task_type), CEREAL_NVP(target_entity), CEREAL_NVP(secondary_entity)); }
};
struct GoalComponent {
    int target_x = 0; int target_y = 0; int target_layer = 0;
    GoalComponent() = default; GoalComponent(int x, int y, int l) : target_x(x), target_y(y), target_layer(l) {}
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(target_x), CEREAL_NVP(target_y), CEREAL_NVP(target_layer)); }
};
struct NeedsComponent {
    float hunger = 100.0f; float thirst = 100.0f; float frustration = 0.0f;
    float socialization = 100.0f; // [G.5]
    NeedsComponent() = default; NeedsComponent(float h, float t) : hunger(h), thirst(t) {}
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(hunger), CEREAL_NVP(thirst), CEREAL_NVP(frustration), CEREAL_NVP(socialization)); }
};

/**
 * @brief [I.1] Tracks an agent's propensity for criminal activity.
 */
struct CrimeRiskComponent {
    float boldness = 50.0f; // 0-100
    uint64_t last_crime_tick = 0;
    bool is_active_criminal = false;

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(boldness), CEREAL_NVP(last_crime_tick), CEREAL_NVP(is_active_criminal));
    }
};

/**
 * @brief [I.5] Tracks the player's wanted level per faction and global notoriety.
 */
struct WantedComponent {
    std::unordered_map<std::string, int> faction_wanted_levels; // faction_id -> 0-5 stars
    float global_notoriety = 0.0f; // 0-100 base notoriety

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(faction_wanted_levels), CEREAL_NVP(global_notoriety));
    }
};

/**
 * @brief [P.1] Tracks the player's social and political standing with city factions.
 *        Distinct from 'Wanted' (legal/guards), this affects trade, dialogue, and access.
 */
struct ReputationComponent {
    // Faction ID -> Standing (-100.0 to 100.0). 
    // -100: Kill on Sight (Social), 0: Neutral, 100: Hero/Key Ally.
    std::unordered_map<std::string, float> faction_standing;

    // Fame (0.0 to 1.0): How widely recognized the player's actions are.
    // Higher fame makes standing shifts propagate faster and affects NPC reactions.
    float fame = 0.0f;

    ReputationTier get_tier(const std::string& faction_id) const {
        float standing = 0.0f;
        if (faction_standing.contains(faction_id)) {
            standing = faction_standing.at(faction_id);
        }

        if (standing <= -70.0f) return ReputationTier::EXCOMMUNICATED;
        if (standing <= -30.0f) return ReputationTier::HOSTILE;
        if (standing <= -10.0f) return ReputationTier::SUSPICIOUS;
        if (standing >= 70.0f) return ReputationTier::ALLY;
        if (standing >= 30.0f) return ReputationTier::FRIENDLY;
        if (standing >= 10.0f) return ReputationTier::FAVORED;
        return ReputationTier::NEUTRAL;
    }

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(faction_standing), CEREAL_NVP(fame));
    }
};

/**
 * @brief [I.4] Tracks production in a clandestine lab.
 */
struct ClandestineLabComponent {
    float production_progress = 0.0f;
    float production_rate = 0.05f; // progress per simulation tick
    int raw_chemicals_stored = 0;
    int max_raw_chemicals = 20;
    int drugs_produced_stored = 0;
    int max_drugs_produced = 20;
    entt::entity controlling_faction = entt::null;
    bool is_raided = false;

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(production_progress), CEREAL_NVP(production_rate),
           CEREAL_NVP(raw_chemicals_stored), CEREAL_NVP(max_raw_chemicals),
           CEREAL_NVP(drugs_produced_stored), CEREAL_NVP(max_drugs_produced),
           CEREAL_NVP(controlling_faction), CEREAL_NVP(is_raided));
    }
};

/**
 * @brief [I.4] Tag for raw chemical inputs.
 */
struct RawChemicalComponent {
    template <class Archive> void serialize(Archive& ar) {}
};

/**
 * @brief [I.3] Flag for stolen items.
 */
struct StolenComponent {
    entt::entity original_owner = entt::null;
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(original_owner));
    }
};

/**
 * @brief [P.2] Marks an item dropped by the player to enable reputation from charity.
 */
struct DroppedByPlayerComponent {
    uint64_t tick_dropped = 0;
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(tick_dropped));
    }
};

/**
 * @brief [I.3] Flag for contraband items.
 */
struct ContrabandComponent {
    template <class Archive> void serialize(Archive& ar) {}
};

/**
 * @brief [I.3] Marks an entity as a Fence.
 */
struct FenceComponent {
    float fee_multiplier = 0.5f; // Value paid to agent for stolen goods (40-60%)
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(fee_multiplier));
    }
};

struct PatrolComponent {
    std::vector<PositionComponent> waypoints;
    size_t current_waypoint_index = 0;
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(waypoints), CEREAL_NVP(current_waypoint_index));
    }
};

/**
 * @brief [H.4] Used by agents to follow a leader in a group/procession.
 */
struct FollowComponent {
    entt::entity target = entt::null;
    int target_distance = 1;
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(target), CEREAL_NVP(target_distance));
    }
};

/**
 * @brief [NEW CLASS] Defines an agent's daily schedule logic.
 */
struct ScheduleComponent {
    bool active = true;
    RoutineState get_current_state(TimeOfDay time) const {
        switch(time) {
            case TimeOfDay::NIGHT: return RoutineState::SLEEPING;
            case TimeOfDay::DAWN: return RoutineState::LEISURE;
            case TimeOfDay::DAY: return RoutineState::WORKING;
            case TimeOfDay::DUSK: return RoutineState::COMMUTING;
            default: return RoutineState::LEISURE;
        }
    }
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(active)); }
};

struct HomeComponent { 
    int x = -1; int y = -1; int layer = 0;
    entt::entity building_entity = entt::null; 
    bool is_squatting = false; // [K.2]
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(x), CEREAL_NVP(y), CEREAL_NVP(layer), CEREAL_NVP(is_squatting)); } 
};
struct WorkplaceComponent { 
    int x = -1; int y = -1; int layer = 0;
    entt::entity building_entity = entt::null; 
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(x), CEREAL_NVP(y), CEREAL_NVP(layer)); } 
};

// =====================================================================
// Environment & Infrastructure
// =====================================================================

struct WasteComponent { float waste_level = 0.0f; template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(waste_level)); } };

/**
 * @brief [K.4] Graffiti and environmental textures applied to buildings.
 */
struct GraffitiComponent {
    entt::entity creator_faction = entt::null;
    GraffitiType type = GraffitiType::TAG;
    float density = 0.5f;       // 0.0 to 1.0 (affects glyph selection/brightness)
    uint32_t color = 0xFFFFFF;   // Faction-specific color
    char glyph = '*';            // Visual metaphor
    
    template <class Archive> void serialize(Archive& ar) {
        ar(cereal::make_nvp("creator_faction", creator_faction), 
           cereal::make_nvp("type", type), 
           cereal::make_nvp("density", density), 
           cereal::make_nvp("color", color), 
           cereal::make_nvp("glyph", glyph));
    }
};

/**
 * @brief [NEW CLASS] Nature effects reduce agent frustration.
 */
struct NatureEffectComponent {
    float frustration_reduction_per_tick = 1.0f;
    float cooling_offset = -2.0f;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(frustration_reduction_per_tick), CEREAL_NVP(cooling_offset)); }
};

/**
 * @brief [NEW CLASS] Park metadata for macro-simulation.
 */
struct ParkComponent {
    int quality = 50; // 0-100
    entt::entity zone_entity = entt::null;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(quality), CEREAL_NVP(zone_entity)); }
};

struct RoadComponent { float traffic_density = 0.0f; template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(traffic_density)); } };
struct PollutionLevelComponent { float air_pollution = 0.0f; template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(air_pollution)); } };
struct ObstacleComponent { template <class Archive> void serialize(Archive&) {} };

struct WeatherComponent {
    WeatherState state = WeatherState::CLEAR;
    float intensity = 0.0f;
    float ambient_temperature = 20.0f;
    uint32_t ticks_remaining = 100;
    TimeOfDay time_of_day = TimeOfDay::DAY;
    uint32_t time_of_day_ticks = 0;
    
    // Day cycle durations (in turns)
    static constexpr uint32_t DAWN_DURATION  = 1000;
    static constexpr uint32_t DAY_DURATION   = 4000;
    static constexpr uint32_t DUSK_DURATION  = 1000;
    static constexpr uint32_t NIGHT_DURATION = 4000;

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(state), CEREAL_NVP(intensity), CEREAL_NVP(ambient_temperature), CEREAL_NVP(ticks_remaining), CEREAL_NVP(time_of_day), CEREAL_NVP(time_of_day_ticks));
    }
};

// =====================================================================
// Macro & Market Components
// =====================================================================

struct PublicOpinionComponent {
    std::map<std::string, float> faction_approval;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(faction_approval)); }
};
struct FactionComponent {
    std::string faction_id; std::string speech_profile; int standing = 0; float influence = 0.0f;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(faction_id), CEREAL_NVP(speech_profile), CEREAL_NVP(standing), CEREAL_NVP(influence)); }
};
struct MacroMarketComponent {
    double GDP = 0.0; float unemployment_rate = 0.0f; uint64_t tax_revenue = 0;
    float wage_index = 1.0f; // Multiplier for wages (EconomicSystem uses this)
    float average_wealth = 100.0f; // [J.4] Average credits per agent in this chunk
    float crime_rate = 0.0f;      // [J.4] Frequency of CrimeReportEvent in this chunk
    
    // [O.1] Resource Scarcity (1.0 = normal, >1.0 = scarce/expensive)
    std::map<RawMaterialType, float> material_scarcity;

    template <class Archive> void serialize(Archive& ar) { 
        ar(CEREAL_NVP(GDP), CEREAL_NVP(unemployment_rate), CEREAL_NVP(tax_revenue), \
           CEREAL_NVP(wage_index), CEREAL_NVP(average_wealth), CEREAL_NVP(crime_rate), \
           CEREAL_NVP(material_scarcity)); 
    }
};
struct TaxationComponent { float income_tax_rate = 0.1f; template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(income_tax_rate)); } };

// =====================================================================
// Verticality & Visibility Components
// =====================================================================

struct ShaftComponent { template <class Archive> void serialize(Archive&) {} };
struct CeilingComponent { template <class Archive> void serialize(Archive&) {} };
struct VerticalFloorComponent { template <class Archive> void serialize(Archive&) {} };
struct VerticalViewComponent { int view_distance = 1; template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(view_distance)); } };

struct VisibilityComponent {
    int view_range = 15;
    std::unordered_set<PositionComponent> visible_tiles;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(view_range), CEREAL_NVP(visible_tiles)); }
};

struct MemoryComponent {
    struct RememberedTile {
        char glyph;
        std::string color;
        template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(glyph), CEREAL_NVP(color)); }
    };
    std::unordered_map<PositionComponent, RememberedTile> remembered_tiles;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(remembered_tiles)); }
};

// =====================================================================
// Interaction & Usability Components
// =====================================================================

struct UsableComponent {
    std::string effect_id = "default";
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(effect_id)); }
};
struct DoorComponent {
    bool is_locked = false; bool is_open = false;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(is_locked), CEREAL_NVP(is_open)); }
};
struct ConsumableComponent {
    int restores_hunger = 0; int restores_thirst = 0;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(restores_hunger), CEREAL_NVP(restores_thirst)); }
};
struct WeaponComponent {
    int damage = 10;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(damage)); }
};
struct KeyComponent {
    std::string key_id;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(key_id)); }
};
struct DurabilityComponent {
    float current = 100.0f; float max = 100.0f;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(current), CEREAL_NVP(max)); }
};
struct ContainerComponent {
    std::vector<entt::entity> contained_items; bool is_open = false; bool is_locked = false;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(contained_items), CEREAL_NVP(is_open), CEREAL_NVP(is_locked)); }
};

// =====================================================================
// Stubs for remaining Serialization components
// =====================================================================

struct CityComponent { uint64_t time_tick = 0; template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(time_tick)); } };
struct RailNetworkComponent { template <class Archive> void serialize(Archive&) {} };

/**
 * @brief [O.1] Chunk-level resource availability.
 */
struct RawMaterialFieldComponent {
    std::map<RawMaterialType, float> concentrations; // 0.0 to 1.0
    std::map<RawMaterialType, float> regen_rates;   // progress per tick
    
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(concentrations), CEREAL_NVP(regen_rates));
    }
};

/**
 * @brief [O.2] Defines a production process.
 */
struct ProductionRecipe {
    std::string recipe_name = "Generic Widget";
    std::map<RawMaterialType, float> inputs;
    uint32_t output_item_type_id = 0;
    std::string output_name = "Widget";
    char output_glyph = '?';
    std::string output_color = "#FFFFFF";
    float work_required = 100.0f; // total progress points
    float efficiency_base = 1.0f;
    ItemMarketCategory output_category = ItemMarketCategory::NONE;
    RawMaterialType output_material = RawMaterialType::METAL;

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(recipe_name), CEREAL_NVP(inputs), CEREAL_NVP(output_item_type_id), 
           CEREAL_NVP(output_name), CEREAL_NVP(output_glyph), CEREAL_NVP(output_color), 
           CEREAL_NVP(work_required), CEREAL_NVP(efficiency_base),
           CEREAL_NVP(output_category), CEREAL_NVP(output_material));
    }
};

/**
 * @brief [O.2] Building-scale industrial production.
 */
struct FactoryComponent {
    std::vector<ProductionRecipe> available_recipes;
    std::map<RawMaterialType, float> input_stockpile;
    uint32_t active_recipe_index = 0;
    float current_progress = 0.0f;
    bool is_active = false;
    entt::entity storage_container = entt::null; // Link to a STORAGE room container
    std::string owning_faction = "CORPORATE"; // [O.3] Faction that owns/runs the factory
    float base_efficiency = 1.0f;           // [O.3] Native efficiency (pre-disruption)

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(available_recipes), CEREAL_NVP(input_stockpile), CEREAL_NVP(active_recipe_index), CEREAL_NVP(current_progress), CEREAL_NVP(is_active), CEREAL_NVP(storage_container), CEREAL_NVP(owning_faction), CEREAL_NVP(base_efficiency));
    }
};

/**
 * @brief [O.3] Tracks supply chain disruption factors for a factory or chunk.
 */
struct SupplyChainDisruptionComponent {
    float transport_bottleneck = 0.0f; // 0.0 to 1.0 (clogged logistics/infrastructure)
    float labor_strike = 0.0f;         // 0.0 to 1.0 (unhappy workers)
    float sabotage_risk = 0.0f;        // 0.0 to 1.0 (criminal/rival faction interference)
    
    // Recovery rate: how fast disruption clears naturally
    float recovery_rate = 0.01f;

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(transport_bottleneck), CEREAL_NVP(labor_strike), CEREAL_NVP(sabotage_risk), CEREAL_NVP(recovery_rate));
    }
};

/**
 * @brief [O.1] Specific point of extraction.
 */
struct ResourceNodeComponent {
    RawMaterialType material_type = RawMaterialType::METAL;
    float current_yield = 1.0f; // Yield per work unit
    float depletion_rate = 0.01f; // How much field concentration drops per harvest
    bool is_exhausted = false;
    
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(material_type), CEREAL_NVP(current_yield), CEREAL_NVP(depletion_rate), CEREAL_NVP(is_exhausted));
    }
};

/**
 * @brief [O.1] Tracks progress of an agent extracting resources.
 */
struct ExtractionProgressComponent {
    float progress = 0.0f;
    float work_rate = 0.05f; // progress per simulation tick
    entt::entity target_node = entt::null;
    
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(progress), CEREAL_NVP(work_rate), CEREAL_NVP(target_node));
    }
};

/**
 * @brief [O.4] Tracks an agent's (or player's) employment at a factory.
 */
struct FactoryJobComponent {
    entt::entity factory_entity = entt::null;
    int shift_duration_ticks = 100; // Total duration of a work shift
    int ticks_worked_current_shift = 0;
    int wage_per_shift = 50;
    bool is_currently_working = false;

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(factory_entity), CEREAL_NVP(shift_duration_ticks), 
           CEREAL_NVP(ticks_worked_current_shift), CEREAL_NVP(wage_per_shift), 
           CEREAL_NVP(is_currently_working));
    }
};

/**
 * @brief [O.4] Tracks a logistic agent's current transport task.
 */
struct MuleComponent {
    entt::entity source_entity = entt::null;
    entt::entity destination_entity = entt::null;
    RawMaterialType material_type = RawMaterialType::METAL;
    bool is_carrying = false;
    entt::entity carried_item = entt::null;

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(source_entity), CEREAL_NVP(destination_entity), 
           CEREAL_NVP(material_type), CEREAL_NVP(is_carrying), CEREAL_NVP(carried_item));
    }
};

struct HousingPreferenceComponent { template <class Archive> void serialize(Archive&) {} };
struct GlobalResourceStockpile { template <class Archive> void serialize(Archive&) {} };
struct BudgetComponent { template <class Archive> void serialize(Archive&) {} };
struct WorkstationComponent {
    WorkstationType type = WorkstationType::NONE; entt::entity user_entity = entt::null;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(type), CEREAL_NVP(user_entity)); }
};
struct ContainedByComponent { entt::entity container = entt::null; template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(container)); } };
struct ShopComponent { template <class Archive> void serialize(Archive&) {} };
struct ResidentComponent { template <class Archive> void serialize(Archive&) {} };
struct UtilityAllocationComponent { template <class Archive> void serialize(Archive&) {} };

enum class PersonalVehicleType : uint8_t {
    SCOOTER,
    BIKE,
    CAR,
    SCI_FI
};

struct PersonalVehicleComponent {
    PersonalVehicleType type = PersonalVehicleType::CAR;
    entt::entity driver = entt::null;
    int capacity = 1;
    float speed_multiplier = 1.0f;
    template <class Archive> void serialize(Archive& ar) {
        ar(cereal::make_nvp("type", type), cereal::make_nvp("driver", driver), cereal::make_nvp("capacity", capacity), cereal::make_nvp("speed_multiplier", speed_multiplier));
    }
};

struct VehicleComponent {
    entt::entity destination_building_id = entt::null;
    struct ResourceTransport { uint32_t type; uint64_t amount; } resource_transport;
    entt::entity source_node_id = entt::null;
    template <class Archive> void serialize(Archive&) {}
};
struct ElevatorControlComponent { template <class Archive> void serialize(Archive&) {} };
struct InteractionQueue { template <class Archive> void serialize(Archive&) {} };
enum class RelationshipTier : uint8_t {
    STRANGER,
    ACQUAINTANCE,
    COWORKER,
    FRIEND,
    FAMILY
};

struct RelationshipRecord {
    RelationshipTier tier = RelationshipTier::STRANGER;
    float affinity = 0.0f; // -100 to 100
    uint64_t last_interaction_tick = 0;
    bool shared_home = false;
    uint64_t target_macro_id = 0; // The stable ID of the other agent
    std::vector<std::string> interaction_history; // [F.7] Memory of last 3 interactions

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(tier), CEREAL_NVP(affinity), CEREAL_NVP(last_interaction_tick), CEREAL_NVP(shared_home), CEREAL_NVP(target_macro_id), CEREAL_NVP(interaction_history));
    }
};

struct RelationshipComponent {
    std::unordered_map<uint64_t, RelationshipRecord> records; // Key is target_macro_id
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(records));
    }
};

/**
 * @brief [NEW] Singleton to map stable macro_ids to live entities.
 */
struct MacroIdMappingTag {
    std::unordered_map<uint64_t, entt::entity> mapping;
};

/**
 * @brief [F.1] Tracks the current conversation state for an agent.
 */
struct ConversationComponent {
    entt::entity partner = entt::null;
    std::string topic_atom_tag;
    uint32_t step_counter = 0;
    uint32_t duration_ticks = 0;
    bool is_initiator = false;

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(partner), CEREAL_NVP(topic_atom_tag), CEREAL_NVP(step_counter), CEREAL_NVP(duration_ticks), CEREAL_NVP(is_initiator));
    }
};

/**
 * @brief [F.1] Handles authored branching dialogue via InkCPP.
 *        Used for "Gold Paths" like AGI leaders and major NPCs.
 */
/**
 * @brief [F.1] Handles authored branching dialogue via InkCPP.
 *        Used for "Gold Paths" like AGI leaders and major NPCs.
 */
struct InkStoryComponent {
    std::string story_path;
    std::shared_ptr<void> story_obj;
    std::shared_ptr<void> runner_obj;
    bool is_initialized = false;
    
    // Non-serializable runtime data is reloaded on deserialize
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(story_path));
    }
};

struct DialogueStateComponent {
    bool is_open = false;
    entt::entity target_agent = entt::null;
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(is_open), CEREAL_NVP(target_agent));
    }
};

enum class PersonalityTag : uint8_t {
    NEUTRAL,
    LACONIC,
    VERBOSE,
    PARANOID,
    AGGRESSIVE,
    SUBSERVIENT
};

/**
 * @brief [F.1] Traits that modify an agent's dialogue style and grammar selection.
 */
struct PersonalityComponent {
    std::vector<PersonalityTag> tags;
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(tags));
    }
};

/**
 * @brief [T.1] Tracks the current barter UI state.
 */
struct BarterUIComponent {
    bool is_open = false;
    entt::entity target_agent = entt::null;
    std::vector<entt::entity> player_offer;
    std::vector<entt::entity> npc_offer;
    std::vector<InformationRecord> player_info_offer;
    std::vector<InformationRecord> npc_info_offer;
    int selected_inventory_index = 0;
    bool focusing_npc_inventory = false;
    
    // [T.3] Negotiation Mechanics
    float npc_patience = 1.0f;     // 1.0 (calm) to 0.0 (walk away)
    float npc_greed_margin = 1.1f; // Desired profit multiplier (e.g., 1.1 = 10% markup)
    float current_leverage = 0.0f; // Reduction to greed margin from pressure
    std::string npc_feedback = "Interested in a trade?";
    
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(is_open), CEREAL_NVP(target_agent), 
           CEREAL_NVP(player_offer), CEREAL_NVP(npc_offer), 
           CEREAL_NVP(player_info_offer), CEREAL_NVP(npc_info_offer),
           CEREAL_NVP(selected_inventory_index), CEREAL_NVP(focusing_npc_inventory),
           CEREAL_NVP(npc_patience), CEREAL_NVP(npc_greed_margin), 
           CEREAL_NVP(current_leverage), CEREAL_NVP(npc_feedback));
    }
};

struct InfluenceComponent { template <class Archive> void serialize(Archive&) {} };
struct BeliefComponent { template <class Archive> void serialize(Archive&) {} };
struct VoterComponent { template <class Archive> void serialize(Archive&) {} };
struct FactionAffiliationComponent {
    entt::entity faction_id = entt::null; float loyalty = 1.0f;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(faction_id), CEREAL_NVP(loyalty)); }
};
struct CandidateComponent { template <class Archive> void serialize(Archive&) {} };
struct PromiseComponent { template <class Archive> void serialize(Archive&) {} };
struct LieComponent { template <class Archive> void serialize(Archive&) {} };
struct ElectedOfficeComponent { template <class Archive> void serialize(Archive&) {} };
struct BillComponent { template <class Archive> void serialize(Archive&) {} };
struct BriberyComponent { template <class Archive> void serialize(Archive&) {} };
struct FavorComponent { template <class Archive> void serialize(Archive&) {} };
struct BlackMarketComponent { template <class Archive> void serialize(Archive&) {} };
struct BackroomDealComponent { template <class Archive> void serialize(Archive&) {} };
struct CreatureComponent { std::string species; int health = 100; template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(species), CEREAL_NVP(health)); } };
struct ForageNodeComponent { int current_amount = 10; template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(current_amount)); } };
struct PipeSegmentComponent { template <class Archive> void serialize(Archive&) {} };
struct WaterQualityComponent { template <class Archive> void serialize(Archive&) {} };
struct WorkOrderComponent {
    entt::entity factory_entity = entt::null;
    float contribution_per_tick = 1.0f;
    bool is_fulfilled = false;
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(factory_entity), CEREAL_NVP(contribution_per_tick), CEREAL_NVP(is_fulfilled));
    }
};
struct RepairProgressComponent { template <class Archive> void serialize(Archive&) {} };
struct SkillComponent { template <class Archive> void serialize(Archive&) {} };
struct JobRequirementComponent { template <class Archive> void serialize(Archive&) {} };
struct EmploymentContractComponent {
    int wage = 10;
    int shift_length = 12;
    entt::entity boss_entity = entt::null;
    std::string job_title = "Assembler";
    
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(wage), CEREAL_NVP(shift_length), CEREAL_NVP(boss_entity), CEREAL_NVP(job_title));
    }
};
struct MicroPresenceComponent { bool is_active = false; entt::entity micro_entity = entt::null; template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(is_active), CEREAL_NVP(micro_entity)); } };
struct ActivityComponent {
    ActivityType type = ActivityType::NONE; int turns_remaining = 0; int total_turns_required = 0;
    entt::entity target_entity = entt::null; entt::entity secondary_entity = entt::null; std::string description;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(type), CEREAL_NVP(turns_remaining), CEREAL_NVP(total_turns_required), CEREAL_NVP(target_entity), CEREAL_NVP(secondary_entity), CEREAL_NVP(description)); }
};
struct CurrentPathComponent {
    std::vector<PositionComponent> path;
    std::vector<PositionComponent> macro_path; // Sequence of high-level nodes
    size_t current_step_index = 0;
    entt::entity target_entity = entt::null;
    uint32_t request_id = 0;
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(path), CEREAL_NVP(macro_path), CEREAL_NVP(current_step_index), CEREAL_NVP(target_entity), CEREAL_NVP(request_id));
    }
};
struct CraftingRecipeComponent {
    std::string recipe_name = "Unnamed Recipe"; WorkstationType workstation_type = WorkstationType::NONE;
    std::map<uint32_t, int> ingredients; uint32_t result_item_type_id = 0; int turns_to_craft = 1;
    CraftingRecipeComponent() = default;
    CraftingRecipeComponent(std::string n, WorkstationType wt, std::map<uint32_t, int> ing, uint32_t rid, int t) : recipe_name(n), workstation_type(wt), ingredients(ing), result_item_type_id(rid), turns_to_craft(t) {}
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(recipe_name), CEREAL_NVP(workstation_type), CEREAL_NVP(ingredients), CEREAL_NVP(result_item_type_id), CEREAL_NVP(turns_to_craft)); }
};
struct HarvestableComponent {
    uint32_t yields_item_type_id = 0; int quantity_per_harvest = 1; int turns_to_harvest = 1; int charges_remaining = 1;
    WorkstationType required_tool_type = WorkstationType::NONE;
    HarvestableComponent() = default;
    HarvestableComponent(uint32_t yid, int q, int t, int c, WorkstationType rt) : yields_item_type_id(yid), quantity_per_harvest(q), turns_to_harvest(t), charges_remaining(c), required_tool_type(rt) {}
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(yields_item_type_id), CEREAL_NVP(quantity_per_harvest), CEREAL_NVP(turns_to_harvest), CEREAL_NVP(charges_remaining), CEREAL_NVP(required_tool_type)); }
};

// =====================================================================
// Xeno & Extraterrestrial Components
// =====================================================================

enum class XenoType : uint8_t {
    HIERODULE,   // Post-human/Ancient servant, reality-warping
    CACOGEN,     // Scavenger/Monster from off-world, biological threat
    MEGATHERIAN, // Massive, slow-moving or static, localized world impact
    POST_HUMAN   // Highly evolved human variant, extremely high status
};

/**
 * @brief [NEW CLASS] Defines an entity as an extraterrestrial or post-human.
 *        Loosely influenced by "Book of the New Sun".
 */
struct XenoComponent {
    XenoType type = XenoType::CACOGEN;
    float stability = 1.0f; // Behavior predictability
    std::string origin = "Orbital";
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(type), CEREAL_NVP(stability), CEREAL_NVP(origin)); }
};

/**
 * @brief [NEW CLASS] Unique systemic effects emitted by Xeno entities.
 *        Influences the 5 simulation layers (Physics, Bio, Cog, Econ, Pol).
 */
struct XenoInfluenceComponent {
    int radius = 5;
    float temperature_offset = 0.0f; // Layer 0 Physics
    float frustration_delta = 0.0f;   // Layer 2 Cognitive
    float scarcity_modifier = 0.0f;  // Layer 3 Economic
    float gravity_local = 1.0f;      // Layer 0 Physics (new concept)
    template <class Archive> void serialize(Archive& ar) { 
        ar(CEREAL_NVP(radius), CEREAL_NVP(temperature_offset), CEREAL_NVP(frustration_delta), CEREAL_NVP(scarcity_modifier), CEREAL_NVP(gravity_local)); 
    }
};

struct ActiveCrisis {
    CrisisType type = CrisisType::NONE;
    float severity = 0.0f; // 0.0 to 1.0
    uint32_t ticks_remaining = 0;
    std::string description;
    entt::entity epicenter = entt::null; // Could be a chunk, building, or faction

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(type), CEREAL_NVP(severity), CEREAL_NVP(ticks_remaining), CEREAL_NVP(description), CEREAL_NVP(epicenter));
    }
};

/**
 * @brief [L.6] Singleton to track the state of the God Mode Crisis Dashboard.
 */
struct CrisisDashboardComponent {
    bool visible = false;
    int selected_crisis_index = 0;
    
    // Stress history for visualization (0.0 to 1.0)
    std::vector<float> economic_stress_history;
    std::vector<float> political_stress_history;
    std::vector<float> biological_stress_history;
    std::vector<float> environmental_stress_history;
    
    std::vector<std::string> propagation_vectors;
    
    size_t history_limit = 40; 

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(visible), CEREAL_NVP(selected_crisis_index),
           CEREAL_NVP(economic_stress_history), CEREAL_NVP(political_stress_history),
           CEREAL_NVP(biological_stress_history), CEREAL_NVP(environmental_stress_history),
           CEREAL_NVP(propagation_vectors));
    }
};

/**
 * @brief [L.1] Singleton component to track active macro-scale crises in the city.
 */
struct CrisisComponent {
    std::vector<ActiveCrisis> active_crises;
    float stress_level = 0.0f; // Global "stress" that increases crisis probability
    uint64_t last_crisis_tick = 0;

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(active_crises), CEREAL_NVP(stress_level), CEREAL_NVP(last_crisis_tick));
    }
};

// =====================================================================
// Debug Overlay Component (singleton — one per registry)
// =====================================================================
struct DebugOverlayComponent {
    bool visible = true;
    uint64_t turn = 0;

    // Per-phase timing (milliseconds) from the LAST completed simulation tick
    float ms_input      = 0.0f;
    float ms_sim_layers = 0.0f;
    float ms_macro      = 0.0f;
    float ms_micro      = 0.0f;
    float ms_post_micro = 0.0f;
    float ms_output     = 0.0f;
    float ms_dispatch   = 0.0f;
    float ms_total_tick = 0.0f;

    // Hottest (slowest) system per phase from the latest scheduler run
    float ms_hottest_input      = 0.0f;
    float ms_hottest_macro      = 0.0f;
    float ms_hottest_micro      = 0.0f;
    float ms_hottest_post_micro = 0.0f;
    float ms_hottest_output     = 0.0f;
    std::string hottest_input_system;
    std::string hottest_macro_system;
    std::string hottest_micro_system;
    std::string hottest_post_micro_system;
    std::string hottest_output_system;

    // "Currently executing" label — set BEFORE each phase, cleared after
    std::string current_phase = "idle";

    // Most-recently-started system within the current phase
    std::string current_system;

    // Entity counts
    size_t num_agents      = 0;
    size_t num_pathfinding = 0;

    // Extra diagnostic lines
    std::string diag_line1;
    std::string diag_line2;
};

// =====================================================================
// Shared Spatial Query Cache
// =====================================================================
struct SpatialQueryCache {
    bool dirty = true;
    uint64_t turn_built = std::numeric_limits<uint64_t>::max();
    uint64_t build_serial = 0;

    // Position -> all entities at this tile
    std::unordered_map<PositionComponent, std::vector<entt::entity>> entities_at;

    // Position -> one blocking entity for O(1) obstacle checks
    std::unordered_map<PositionComponent, entt::entity> obstacle_at;

    // Terrain lookup for O(1) movement cost / tile-type checks
    std::unordered_map<PositionComponent, TerrainType> terrain_at;

    // Positions that should be treated as passable portal/door entrance points
    std::unordered_set<PositionComponent> entrance_tiles;

    // Position -> entrance entity at that tile (first encountered)
    std::unordered_map<PositionComponent, entt::entity> entrance_entity_at;

    // Broken windows remain passable despite obstacle tags
    std::unordered_set<PositionComponent> passable_window_tiles;
};

inline uint64_t get_current_sim_turn(const entt::registry& registry) {
    auto dbg_view = registry.view<DebugOverlayComponent>();
    if (dbg_view.begin() == dbg_view.end()) return 0;
    return dbg_view.get<DebugOverlayComponent>(*dbg_view.begin()).turn;
}

inline void invalidate_spatial_query_cache(entt::registry& registry) {
    if (auto* cache = registry.ctx().find<SpatialQueryCache>()) {
        cache->dirty = true;
    }
}

inline SpatialQueryCache& get_spatial_query_cache(entt::registry& registry) {
    auto* cache = registry.ctx().find<SpatialQueryCache>();
    if (!cache) {
        registry.ctx().emplace<SpatialQueryCache>();
        cache = registry.ctx().find<SpatialQueryCache>();
    }

    const uint64_t turn = get_current_sim_turn(registry);
    if (!cache->dirty && cache->turn_built == turn) {
        return *cache;
    }

    cache->entities_at.clear();
    cache->obstacle_at.clear();
    cache->terrain_at.clear();
    cache->entrance_tiles.clear();
    cache->entrance_entity_at.clear();
    cache->passable_window_tiles.clear();

    auto pos_view = registry.view<PositionComponent>();
    for (auto entity : pos_view) {
        const auto& pos = pos_view.get<PositionComponent>(entity);
        cache->entities_at[pos].push_back(entity);
    }

    auto terrain_view = registry.view<PositionComponent, TerrainComponent>();
    for (auto entity : terrain_view) {
        const auto& pos = terrain_view.get<PositionComponent>(entity);
        const auto type = terrain_view.get<TerrainComponent>(entity).type;
        cache->terrain_at[pos] = type;
    }

    auto entrance_view = registry.view<PositionComponent, BuildingEntranceComponent>();
    for (auto entity : entrance_view) {
        const auto& pos = entrance_view.get<PositionComponent>(entity);
        cache->entrance_tiles.insert(pos);
        cache->entrance_entity_at.emplace(pos, entity);
    }

    auto sized_obs_view = registry.view<PositionComponent, ObstacleComponent, SizeComponent>();
    for (auto entity : sized_obs_view) {
        const auto& base = sized_obs_view.get<PositionComponent>(entity);
        const auto& size = sized_obs_view.get<SizeComponent>(entity);
        const int w = std::max(1, size.width);
        const int h = std::max(1, size.height);
        for (int dx = 0; dx < w; ++dx) {
            for (int dy = 0; dy < h; ++dy) {
                PositionComponent p(base.x + dx, base.y + dy, base.layer_id);
                cache->obstacle_at.emplace(p, entity);
            }
        }
    }

    auto single_obs_view = registry.view<PositionComponent, ObstacleComponent>(entt::exclude<SizeComponent>);
    for (auto entity : single_obs_view) {
        const auto& pos = single_obs_view.get<PositionComponent>(entity);

        if (registry.all_of<TerrainComponent>(entity)) {
            if (registry.get<TerrainComponent>(entity).type == TerrainType::WINDOW) {
                if (auto* phys = registry.try_get<Layer0PhysicsComponent>(entity)) {
                    if (phys->structural_integrity < 0.5f) {
                        cache->passable_window_tiles.insert(pos);
                        continue;
                    }
                }
            }
        }

        cache->obstacle_at.emplace(pos, entity);
    }

    cache->dirty = false;
    cache->turn_built = turn;
    cache->build_serial++;
    return *cache;
}

// =====================================================================
// Legacy Redirection Namespace
// =====================================================================
namespace ECS {
    using NameComponent = NeonOubliette::NameComponent;
    using PositionComponent = NeonOubliette::PositionComponent;
    using SizeComponent = NeonOubliette::SizeComponent;
    using RenderableComponent = NeonOubliette::RenderableComponent;
    using PlayerInteractionComponent = NeonOubliette::PlayerInteractionComponent;
    using InteractionMode = NeonOubliette::InteractionMode;
    using PlayerComponent = NeonOubliette::PlayerComponent;
    using HUDComponent = NeonOubliette::HUDComponent;
    using PlayerCurrentLayerComponent = NeonOubliette::PlayerCurrentLayerComponent;
    using InventoryComponent = NeonOubliette::InventoryComponent;
    using ItemComponent = NeonOubliette::ItemComponent;
    using ItemValueComponent = NeonOubliette::ItemValueComponent;
    using BuildingComponent = NeonOubliette::BuildingComponent;
    using FloorComponent = NeonOubliette::FloorComponent;
    using RoomComponent = NeonOubliette::RoomComponent;
    using ApartmentComponent = NeonOubliette::ApartmentComponent;
    using StairsComponent = NeonOubliette::StairsComponent;
    using ElevatorComponent = NeonOubliette::ElevatorComponent;
    using BuildingInteriorComponent = NeonOubliette::BuildingInteriorComponent;
    using RoomTag = NeonOubliette::RoomTag;
    using RoomData = NeonOubliette::RoomData;
    using BuildingEntranceComponent = NeonOubliette::BuildingEntranceComponent;
    using DoorMetadataComponent = NeonOubliette::DoorMetadataComponent;
    using AgentComponent = NeonOubliette::AgentComponent;
    using NPCComponent = NeonOubliette::NPCComponent;
    using CitizenComponent = NeonOubliette::CitizenComponent;
    using AgentTaskComponent = NeonOubliette::AgentTaskComponent;
    using GoalComponent = NeonOubliette::GoalComponent;
    using NeedsComponent = NeonOubliette::NeedsComponent;
    using WasteComponent = NeonOubliette::WasteComponent;
    using RoadComponent = NeonOubliette::RoadComponent;
    using PollutionLevelComponent = NeonOubliette::PollutionLevelComponent;
    using ObstacleComponent = NeonOubliette::ObstacleComponent;
    using PublicOpinionComponent = NeonOubliette::PublicOpinionComponent;
    using FactionComponent = NeonOubliette::FactionComponent;
    using MacroMarketComponent = NeonOubliette::MacroMarketComponent;
    using TaxationComponent = NeonOubliette::TaxationComponent;
    using ShaftComponent = NeonOubliette::ShaftComponent;
    using CeilingComponent = NeonOubliette::CeilingComponent;
    using VerticalFloorComponent = NeonOubliette::VerticalFloorComponent;
    using VerticalViewComponent = NeonOubliette::VerticalViewComponent;
    using UsableComponent = NeonOubliette::UsableComponent;
    using DoorComponent = NeonOubliette::DoorComponent;
    using ConsumableComponent = NeonOubliette::ConsumableComponent;
    using ContainerComponent = NeonOubliette::ContainerComponent;
    using CityComponent = NeonOubliette::CityComponent;
    using RailNetworkComponent = NeonOubliette::RailNetworkComponent;
    using RawMaterialFieldComponent = NeonOubliette::RawMaterialFieldComponent;
    using ResourceNodeComponent = NeonOubliette::ResourceNodeComponent;
    using ExtractionProgressComponent = NeonOubliette::ExtractionProgressComponent;
    using RawMaterialType = NeonOubliette::RawMaterialType;
    using HousingPreferenceComponent = NeonOubliette::HousingPreferenceComponent;
    using GlobalResourceStockpile = NeonOubliette::GlobalResourceStockpile;
    using BudgetComponent = NeonOubliette::BudgetComponent;
    using WorkstationComponent = NeonOubliette::WorkstationComponent;
    using ContainedByComponent = NeonOubliette::ContainedByComponent;
    using ShopComponent = NeonOubliette::ShopComponent;
    using ResidentComponent = NeonOubliette::ResidentComponent;
    using UtilityAllocationComponent = NeonOubliette::UtilityAllocationComponent;
    using VehicleComponent = NeonOubliette::VehicleComponent;
    using ElevatorControlComponent = NeonOubliette::ElevatorControlComponent;
    using InteractionQueue = NeonOubliette::InteractionQueue;
    using RelationshipComponent = NeonOubliette::RelationshipComponent;
    using InfluenceComponent = NeonOubliette::InfluenceComponent;
    using BeliefComponent = NeonOubliette::BeliefComponent;
    using VoterComponent = NeonOubliette::VoterComponent;
    using FactionAffiliationComponent = NeonOubliette::FactionAffiliationComponent;
    using CandidateComponent = NeonOubliette::CandidateComponent;
    using PromiseComponent = NeonOubliette::PromiseComponent;
    using LieComponent = NeonOubliette::LieComponent;
    using ElectedOfficeComponent = NeonOubliette::ElectedOfficeComponent;
    using BillComponent = NeonOubliette::BillComponent;
    using BriberyComponent = NeonOubliette::BriberyComponent;
    using FavorComponent = NeonOubliette::FavorComponent;
    using BlackMarketComponent = NeonOubliette::BlackMarketComponent;
    using BackroomDealComponent = NeonOubliette::BackroomDealComponent;
    using CreatureComponent = NeonOubliette::CreatureComponent;
    using ForageNodeComponent = NeonOubliette::ForageNodeComponent;
    using PipeSegmentComponent = NeonOubliette::PipeSegmentComponent;
    using WaterQualityComponent = NeonOubliette::WaterQualityComponent;
    using FactoryComponent = NeonOubliette::FactoryComponent;
    using ProductionRecipe = NeonOubliette::ProductionRecipe;
    using WorkOrderComponent = NeonOubliette::WorkOrderComponent;
    using RepairProgressComponent = NeonOubliette::RepairProgressComponent;
    using SkillComponent = NeonOubliette::SkillComponent;
    using JobRequirementComponent = NeonOubliette::JobRequirementComponent;
    using EmploymentContractComponent = NeonOubliette::EmploymentContractComponent;
    using MicroPresenceComponent = NeonOubliette::MicroPresenceComponent;
    using PersonalVehicleComponent = NeonOubliette::PersonalVehicleComponent;
    using PersonalVehicleType = NeonOubliette::PersonalVehicleType;
    using HomeComponent = NeonOubliette::HomeComponent;
    using WorkplaceComponent = NeonOubliette::WorkplaceComponent;
    using ActivityComponent = NeonOubliette::ActivityComponent;
    using CurrentPathComponent = NeonOubliette::CurrentPathComponent;
    using CraftingRecipeComponent = NeonOubliette::CraftingRecipeComponent;
    using HarvestableComponent = NeonOubliette::HarvestableComponent;
    using TerrainComponent = NeonOubliette::TerrainComponent;
    using PortalComponent = NeonOubliette::PortalComponent;
    using InteriorStateComponent = NeonOubliette::InteriorStateComponent;
    using TransitVehicleComponent = NeonOubliette::TransitVehicleComponent;
    using TransitRouteComponent = NeonOubliette::TransitRouteComponent;
    using TransitStationComponent = NeonOubliette::TransitStationComponent;
    using TransitOccupantsComponent = NeonOubliette::TransitOccupantsComponent;
    using RidingComponent = NeonOubliette::RidingComponent;
    using ScheduleComponent = NeonOubliette::ScheduleComponent;
    using XenoComponent = NeonOubliette::XenoComponent;
    using XenoInfluenceComponent = NeonOubliette::XenoInfluenceComponent;
    using CrimeRiskComponent = NeonOubliette::CrimeRiskComponent;
    using WantedComponent = NeonOubliette::WantedComponent;
    using ReputationComponent = NeonOubliette::ReputationComponent;
    using StolenComponent = NeonOubliette::StolenComponent;
    using ContrabandComponent = NeonOubliette::ContrabandComponent;
    using FenceComponent = NeonOubliette::FenceComponent;
    using SimulationStateComponent = NeonOubliette::SimulationStateComponent;
    using GodCursorComponent = NeonOubliette::GodCursorComponent;
    using StandardCursorComponent = NeonOubliette::StandardCursorComponent;
    using SpeechComponent = NeonOubliette::SpeechComponent;
    using AudibilityLevel = NeonOubliette::AudibilityLevel;
    using BuildingAcousticsComponent = NeonOubliette::BuildingAcousticsComponent;
    using DebugOverlayComponent = NeonOubliette::DebugOverlayComponent;
    using CrisisComponent = NeonOubliette::CrisisComponent;
    using CrisisDashboardComponent = NeonOubliette::CrisisDashboardComponent;
    using CrisisType = NeonOubliette::CrisisType;
    using ActiveCrisis = NeonOubliette::ActiveCrisis;
    using InfectionComponent = NeonOubliette::InfectionComponent;
    using EnvironmentalHazardComponent = NeonOubliette::EnvironmentalHazardComponent;
}

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_COMPONENTS_H
