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

#include "../command_buffer.h"
#include "zoning_components.h"
#include "transit_components.h"

namespace NeonOubliette {

// =====================================================================
// Enums & Constants
// =====================================================================

enum class AgentTaskType : uint32_t {
    IDLE, WANDER, SEEK_FOOD, SEEK_WATER, SEEK_SHELTER, GO_TO_WORK,
    PICK_UP_ITEM, CONSUME_ITEM, MOVE_TO_TARGET, MOVE_ALONG_PATH,
    SEEK_HARVESTABLE, OPEN_CONTAINER, TAKE_ITEM_FROM_CONTAINER,
    CRAFT_ITEM, USE_ITEM, AWAITING_PATH, SHOVE, PATROL,
    WAIT_FOR_TRANSIT, RIDE_TRANSIT
};

enum class ActivityType : uint32_t {
    NONE = 0, CRAFTING, HARVESTING, BUILDING, RESTING, MINING, RESEARCHING
};

enum class WorkstationType : uint32_t {
    NONE = 0, CRAFTING_BENCH, FORGE, LABORATORY
};

enum class TerrainType : uint8_t {
    VOID = 0,
    STREET,
    SIDEWALK,
    GRASS,
    DIRT,
    CONCRETE_FLOOR,
    WOOD_FLOOR,
    WALL,
    WINDOW,
    OFFICE_CARPET,
    FLOWER_BED,
    WATER_FEATURE,
    ARENA_FLOOR,
    ARENA_SEATING,
    RAIL
};

enum class Direction : uint8_t {
    NORTH, SOUTH, EAST, WEST
};

enum class WeatherState : uint8_t {
    CLEAR, OVERCAST, RAIN, HEAVY_RAIN, ACID_RAIN, SMOG, ELECTRICAL_STORM
};

enum class TimeOfDay : uint8_t {
    DAWN, DAY, DUSK, NIGHT
};

enum class RoutineState : uint8_t {
    SLEEPING,
    WORKING,
    LEISURE,
    COMMUTING
};

enum class SimulationMode : uint8_t {
    STANDARD,   // Player-centric gameplay
    GOD_MODE    // Observer/Director gameplay
};

enum class RoomTag : uint8_t {
    LOBBY,
    OFFICE,
    SERVER_ROOM,
    EXECUTIVE_SUITE,
    BEDROOM,
    KITCHEN,
    BATHROOM,
    LIVING_ROOM,
    FACTORY_FLOOR,
    STORAGE,
    SUPERVISOR_OFFICE,
    HALLWAY,
    VOID
};

enum class AudibilityLevel : uint8_t {
    CLEAR,      // 0 walls
    MUFFLED,    // 1-2 walls
    INAUDIBLE   // 3+ walls
};

enum class InteractionMode : uint8_t {
    SPEAK,
    OBSERVE,
    TRADE
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
    std::string text;
    uint32_t ticks_remaining = 0;
    entt::entity speaker = entt::null;
    AudibilityLevel audibility = AudibilityLevel::CLEAR;
    
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(text), CEREAL_NVP(ticks_remaining), CEREAL_NVP(speaker), CEREAL_NVP(audibility));
    }
};

struct WorldConfigComponent {
    int width = 0;
    int height = 0;
    int macro_cell_size = 20;
    uint32_t world_seed = 12345;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(width), CEREAL_NVP(height), CEREAL_NVP(macro_cell_size), CEREAL_NVP(world_seed)); }
};

struct NameComponent {
    std::string name;
    NameComponent() = default;
    NameComponent(std::string n) : name(n) {}
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(name)); }
};

struct PositionComponent {
    int x = 0; int y = 0; int layer_id = 0;
    PositionComponent() = default;
    PositionComponent(int x, int y, int layer_id = 0) : x(x), y(y), layer_id(layer_id) {}
    bool operator==(const PositionComponent& other) const {
        return x == other.x && y == other.y && layer_id == other.layer_id;
    }
    bool operator<(const PositionComponent& other) const {
        if (layer_id != other.layer_id) return layer_id < other.layer_id;
        if (x != other.x) return x < other.x;
        return y < other.y;
    }
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(x), CEREAL_NVP(y), CEREAL_NVP(layer_id)); }
};

} // namespace NeonOubliette

// std::hash specialization for PositionComponent (must be in namespace std)
template<>
struct std::hash<NeonOubliette::PositionComponent> {
    size_t operator()(const NeonOubliette::PositionComponent& p) const noexcept {
        // Combine x, y, layer_id into a single hash
        size_t h = std::hash<int>()(p.x);
        h ^= std::hash<int>()(p.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>()(p.layer_id) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

namespace NeonOubliette {

/**
 * @brief Defines the physical footprint of an entity in tiles.
 */
struct SizeComponent {
    int width = 1;
    int height = 1;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(width), CEREAL_NVP(height)); }
};

struct OrientationComponent {
    Direction facing = Direction::NORTH;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(facing)); }
};

struct RenderableComponent {
    char glyph = '?'; std::string color = "#FFFFFF"; int layer_id = 0;
    RenderableComponent() = default;
    RenderableComponent(char g, std::string c, int lid = 0) : glyph(g), color(c), layer_id(lid) {}
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(glyph), CEREAL_NVP(color), CEREAL_NVP(layer_id)); }
};

struct PlayerInteractionComponent {
    InteractionMode current_mode = InteractionMode::OBSERVE;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(current_mode)); }
};

struct PlayerComponent { template <class Archive> void serialize(Archive&) {} };

struct HUDComponent {
    float health = 100.0f; int credits = 0; int current_layer_display = 0;
    std::vector<std::string> notifications;
    bool show_controls_help = true;
    bool inventory_open = false;
    int selected_inventory_index = 0;
    entt::entity held_item = entt::null; // [C.1]
    float held_item_flash_timer = 0.0f; // [C.2]

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(health), CEREAL_NVP(credits), CEREAL_NVP(current_layer_display), 
           CEREAL_NVP(notifications), CEREAL_NVP(show_controls_help), 
           CEREAL_NVP(inventory_open), CEREAL_NVP(selected_inventory_index),
           CEREAL_NVP(held_item), CEREAL_NVP(held_item_flash_timer));
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
    NeedsComponent() = default; NeedsComponent(float h, float t) : hunger(h), thirst(t) {}
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(hunger), CEREAL_NVP(thirst), CEREAL_NVP(frustration)); }
};
struct PatrolComponent {
    std::vector<PositionComponent> waypoints;
    size_t current_waypoint_index = 0;
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(waypoints), CEREAL_NVP(current_waypoint_index));
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
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(x), CEREAL_NVP(y), CEREAL_NVP(layer)); } 
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
    std::map<entt::entity, float> faction_approval;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(faction_approval)); }
};
struct FactionComponent {
    std::string faction_id; int standing = 0; float influence = 0.0f;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(faction_id), CEREAL_NVP(standing), CEREAL_NVP(influence)); }
};
struct MacroMarketComponent {
    double GDP = 0.0; float unemployment_rate = 0.0f; uint64_t tax_revenue = 0;
    template <class Archive> void serialize(Archive& ar) { ar(CEREAL_NVP(GDP), CEREAL_NVP(unemployment_rate), CEREAL_NVP(tax_revenue)); }
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
struct ResourceNodeComponent { template <class Archive> void serialize(Archive&) {} };
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

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(tier), CEREAL_NVP(affinity), CEREAL_NVP(last_interaction_tick), CEREAL_NVP(shared_home));
    }
};

struct RelationshipComponent {
    std::unordered_map<entt::entity, RelationshipRecord> records;
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(records));
    }
};

/**
 * @brief [F.1] Tracks the current conversation state for an agent.
 */
struct ConversationComponent {
    entt::entity partner = entt::null;
    std::vector<std::string> topic_stack;
    uint32_t step_counter = 0;
    uint32_t duration_ticks = 0;

    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(partner), CEREAL_NVP(topic_stack), CEREAL_NVP(step_counter), CEREAL_NVP(duration_ticks));
    }
};

struct DialogueStateComponent {
    bool is_open = false;
    entt::entity target_agent = entt::null;
    template <class Archive> void serialize(Archive& ar) {
        ar(CEREAL_NVP(is_open), CEREAL_NVP(target_agent));
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
struct WorkOrderComponent { template <class Archive> void serialize(Archive&) {} };
struct RepairProgressComponent { template <class Archive> void serialize(Archive&) {} };
struct SkillComponent { template <class Archive> void serialize(Archive&) {} };
struct JobRequirementComponent { template <class Archive> void serialize(Archive&) {} };
struct EmploymentContractComponent { template <class Archive> void serialize(Archive&) {} };
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

// =====================================================================
// Debug Overlay Component (singleton — one per registry)
// =====================================================================
struct DebugOverlayComponent {
    bool visible = true;
    uint64_t turn = 0;

    // Per-phase timing (milliseconds) from the LAST completed simulation tick
    float ms_sim_layers = 0.0f;
    float ms_macro      = 0.0f;
    float ms_micro      = 0.0f;
    float ms_post_micro = 0.0f;
    float ms_output     = 0.0f;
    float ms_dispatch   = 0.0f;
    float ms_total_tick = 0.0f;

    // "Currently executing" label — set BEFORE each phase, cleared after
    std::string current_phase = "idle";

    // Entity counts
    size_t num_agents      = 0;
    size_t num_pathfinding = 0;

    // Extra diagnostic lines
    std::string diag_line1;
    std::string diag_line2;
};

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
    using ResourceNodeComponent = NeonOubliette::ResourceNodeComponent;
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
    using SimulationStateComponent = NeonOubliette::SimulationStateComponent;
    using GodCursorComponent = NeonOubliette::GodCursorComponent;
    using StandardCursorComponent = NeonOubliette::StandardCursorComponent;
    using SpeechComponent = NeonOubliette::SpeechComponent;
    using AudibilityLevel = NeonOubliette::AudibilityLevel;
    using BuildingAcousticsComponent = NeonOubliette::BuildingAcousticsComponent;
    using DebugOverlayComponent = NeonOubliette::DebugOverlayComponent;
}

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_COMPONENTS_H
