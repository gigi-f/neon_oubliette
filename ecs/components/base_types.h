#ifndef NEON_OUBLIETTE_ECS_COMPONENTS_BASE_TYPES_H
#define NEON_OUBLIETTE_ECS_COMPONENTS_BASE_TYPES_H

#include <string>
#include <vector>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/map.hpp>
#include <entt/entt.hpp>

namespace NeonOubliette {

// --- Size/Metric System ---
constexpr float METERS_PER_TILE = 1.0f;

// --- Real-World Scaling Constants (Meters/Tiles) ---
constexpr int ROAD_WIDTH_PRIMARY    = 16;
constexpr int ROAD_WIDTH_SECONDARY  = 8;
constexpr int ROAD_WIDTH_ALLEY      = 4;
constexpr int SIDEWALK_WIDTH        = 4;
constexpr int RIVER_WIDTH           = 20;

enum class Direction : uint8_t {
    NORTH, SOUTH, EAST, WEST
};

enum class RawMaterialType : uint8_t {
    METAL,      // Salvaged scrap, industrial ore
    CHEMICAL,   // Lab reagents, fuel, acid
    BIOMASS,    // Food base, biological tissue
    ELECTRONIC, // Components, chips
    ENERGY      // Fuel cells, isotopes
};

enum class ZoneType : uint8_t {
    VOID = 0,
    CORPORATE,
    COMMERCIAL,
    RESIDENTIAL,
    SLUM,
    INDUSTRIAL,
    PARK,
    TRANSIT,
    AIRPORT,
    COLOSSEUM,
    URBAN_CORE,
    MIXED_COMMERCIAL,
    CIVIC,
    RELIGIOUS,
    Count
};

enum class StreetFacingSide : uint8_t {
    NONE  = 0,
    NORTH = 1 << 0,
    SOUTH = 1 << 1,
    EAST  = 1 << 2,
    WEST  = 1 << 3,
    ALL   = 0x0F
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
    RAIL,
    SEWER_FLOOR,
    SEWER_WATER
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

enum class ReputationTier : int8_t {
    EXCOMMUNICATED = -3, // <-70 (Kill on sight/Complete block)
    HOSTILE = -2,        // -70 to -30 (Aggressive dialogue/High markup)
    SUSPICIOUS = -1,     // -30 to -10 (Cool dialogue/Minor markup)
    NEUTRAL = 0,         // -10 to +10 (Standard)
    FAVORED = 1,         // +10 to +30 (Warm dialogue/Minor discount)
    FRIENDLY = 2,        // +30 to +70 (Supportive dialogue/Major discount)
    ALLY = 3             // >+70 (Exclusive access/Highest discount)
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
    NAVE,         // [H.3]
    ALTAR,        // [H.3]
    SEATING_AREA, // [H.3]
    FENCE,        // [I.3]
    CLANDESTINE_LAB, // [I.4]
    SECURITY_HUB,    // [I.6]
    HOLDING_CELL,    // [I.6]
    VOID
};

enum class AudibilityLevel : uint8_t {
    CLEAR,      // 0 walls, direct address
    OVERHEARD,  // 0 walls, not direct address (italic/dim)
    MUFFLED,    // 1-2 walls (dim, asterisks)
    INAUDIBLE   // 3+ walls
};

enum class InteractionMode : uint8_t {
    SPEAK,
    OBSERVE,
    TRADE,
    ACTION
};

enum class ItemMarketCategory : uint8_t {
    NONE = 0,
    FOOD,
    WATER,
    MEDICAL,
    TECHNOLOGY,
    LUXURY,
    CLOTHING,
    WEAPONRY,
    TOOLS,
    CONTRABAND
};

enum class CrisisType : uint8_t {
    NONE = 0,
    ECONOMIC_COLLAPSE,    // Inflation/unemployment spikes
    BIOLOGICAL_OUTBREAK,  // Sickness spreading through agents
    INFRASTRUCTURE_FAILURE, // Power/water outages
    POLITICAL_UNREST,      // High crime/protests
    XENO_INCURSION,       // Cacogen activity increase
    ENVIRONMENTAL_HAZARD, // Acid rain/Toxicity spikes
    FACTION_WAR           // Active combat between factions
};

enum class InformationType : uint8_t {
    RUMOR,
    PROPAGANDA,
    INTELLIGENCE,
    PRICE_TIP,
    GOSSIP
};

struct InformationRecord {
    InformationType type = InformationType::RUMOR;
    std::string content_tag; // e.g., "BLACK_MARKET_MEDKITS", "SYNDICATE_PUSH"
    entt::entity source_faction = entt::null;
    uint64_t origin_tick = 0;
    float veracity = 1.0f; // 0.0 to 1.0
    int hops = 0; // Number of times retold

    template <class Archive> void serialize(Archive& ar) {
        ar(cereal::make_nvp("type", type), cereal::make_nvp("content_tag", content_tag), cereal::make_nvp("source_faction", source_faction), 
           cereal::make_nvp("origin_tick", origin_tick), cereal::make_nvp("veracity", veracity), cereal::make_nvp("hops", hops));
    }
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
    template <class Archive> void serialize(Archive& ar) { ar(cereal::make_nvp("x", x), cereal::make_nvp("y", y), cereal::make_nvp("layer_id", layer_id)); }
};

struct SizeComponent {
    int width = 1;
    int height = 1;
    template <class Archive> void serialize(Archive& ar) { ar(cereal::make_nvp("width", width), cereal::make_nvp("height", height)); }
};

struct RenderableComponent {
    char glyph = '?'; std::string color = "#FFFFFF"; int layer_id = 0;
    RenderableComponent() = default;
    RenderableComponent(char g, std::string c, int lid = 0) : glyph(g), color(c), layer_id(lid) {}
    template <class Archive> void serialize(Archive& ar) { ar(cereal::make_nvp("glyph", glyph), cereal::make_nvp("color", color), cereal::make_nvp("layer_id", layer_id)); }
};

struct OrientationComponent {
    Direction facing = Direction::NORTH;
    template <class Archive> void serialize(Archive& ar) { ar(cereal::make_nvp("facing", facing)); }
};

} // namespace NeonOubliette

template<>
struct std::hash<NeonOubliette::PositionComponent> {
    size_t operator()(const NeonOubliette::PositionComponent& p) const noexcept {
        size_t h = std::hash<int>()(p.x);
        h ^= std::hash<int>()(p.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>()(p.layer_id) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

#endif
