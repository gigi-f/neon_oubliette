#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_CITY_GENERATION_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_CITY_GENERATION_SYSTEM_H

#include <entt/entt.hpp>
#include "../components/components.h"
#include "../components/zoning_components.h"
#include "../components/simulation_layers.h"
#include "../components/infrastructure_components.h"
#include "../system_scheduler.h"
#include <string>
#include <set>
#include <random>
#include <queue>
#include "../components/transit_components.h"

namespace NeonOubliette {

/**
 * @brief Handles placement of tiles and buildings based on MacroZones and Infrastructure.
 */
class CityGenerationSystem : public ISystem {
public:
    CityGenerationSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {}
    void update(double delta_time) override {}

    void generate_chunk_content(entt::entity zone_entity) {
        auto const& zone = m_registry.get<MacroZoneComponent>(zone_entity);
        auto config_view = m_registry.view<WorldConfigComponent>();
        if (config_view.empty()) return;
        auto& config = config_view.get<WorldConfigComponent>(config_view.front());

        // Deterministic Seed for this zone
        uint32_t zone_seed = config.world_seed ^ (static_cast<uint32_t>(zone.macro_x) * 73856093) ^ (static_cast<uint32_t>(zone.macro_y) * 19349663);
        std::mt19937 zone_gen(zone_seed);

        std::set<std::pair<int, int>> arterial_footprint;
        
        for (auto e : zone.arterial_entities) {
            const auto& pos = m_registry.get<PositionComponent>(e);
            arterial_footprint.insert({pos.x, pos.y});
        }

        generateZoneInterior(zone, config.macro_cell_size, arterial_footprint, zone_gen);
        
        for (auto e : zone.arterial_entities) {
            if (!m_registry.all_of<InfrastructureArterialComponent>(e)) continue;

            const auto& pos = m_registry.get<PositionComponent>(e);
            const auto& art = m_registry.get<InfrastructureArterialComponent>(e);
            
            TerrainType type = TerrainType::STREET;
            MaterialType material = MaterialType::CONCRETE;
            char glyph = ' ';
            std::string color = "#333333";
            bool is_obstacle = false;
            bool is_liquid = false;

            switch(art.type) {
                case ArterialType::WATERWAY_RIVER:
                    type = TerrainType::VOID; glyph = '~'; color = "#0000FF";
                    is_obstacle = true; material = MaterialType::WATER; is_liquid = true;
                    break;
                case ArterialType::ROAD_PRIMARY:
                    type = TerrainType::STREET; glyph = ' '; color = "#222222";
                    material = MaterialType::CONCRETE;
                    break;
                case ArterialType::RAIL_ELEVATED:
                    type = TerrainType::VOID; glyph = '='; color = "#FFFF00";
                    material = MaterialType::STEEL;
                    break;
                default: break;
            }

            for (auto ne : zone.arterial_entities) {
                if (!m_registry.all_of<InfrastructureNodeComponent>(ne)) continue;

                const auto& n_pos = m_registry.get<PositionComponent>(ne);
                const auto& node = m_registry.get<InfrastructureNodeComponent>(ne);
                if (n_pos.x == pos.x && n_pos.y == pos.y && node.is_bridge) {
                    glyph = '='; color = "#AAAAAA";
                    is_obstacle = false;
                    material = MaterialType::STEEL;
                    break;
                }
            }
            
            auto tile = m_registry.create();
            m_registry.emplace<PositionComponent>(tile, pos.x, pos.y, 0);
            m_registry.emplace<TerrainComponent>(tile, type);
            m_registry.emplace<RenderableComponent>(tile, glyph, color, 0);
            auto& phys = m_registry.emplace<Layer0PhysicsComponent>(tile);
            phys.material = material;
            phys.is_liquid = is_liquid;
            if (is_obstacle) m_registry.emplace<ObstacleComponent>(tile);

            // [NEW] Randomly spawn personal vehicles on roads (Primary roads)
            if (art.type == ArterialType::ROAD_PRIMARY) {
                std::uniform_real_distribution<> v_dis(0.0, 1.0);
                if (v_dis(zone_gen) < 0.05) { // 5% chance per road tile
                    PersonalVehicleType v_type = PersonalVehicleType::CAR;
                    double r = v_dis(zone_gen);
                    if (r < 0.3) v_type = PersonalVehicleType::SCOOTER;
                    else if (r < 0.6) v_type = PersonalVehicleType::BIKE;
                    else if (r < 0.9) v_type = PersonalVehicleType::CAR;
                    else v_type = PersonalVehicleType::SCI_FI;
                    
                    spawnPersonalVehicle(pos.x, pos.y, 0, v_type);
                }
            }
        }
    }

private:
    void generateParkInterior(const MacroZoneComponent& zone, int cell_size, const std::set<std::pair<int, int>>& arterials, std::mt19937& gen, std::set<std::pair<int, int>>& footprint) {
        int start_x = zone.macro_x * cell_size;
        int start_y = zone.macro_y * cell_size;

        // 1. Central Park Anchor
        auto park_anchor = m_registry.create();
        m_registry.emplace<NameComponent>(park_anchor, zone.district_name + " Park");
        m_registry.emplace<PositionComponent>(park_anchor, start_x + (cell_size / 2), start_y + (cell_size / 2), 0);
        m_registry.emplace<ParkComponent>(park_anchor, 80, entt::null);
        
        auto& field = m_registry.emplace<ConduitFieldComponent>(park_anchor);
        field.radius = (float)cell_size;
        field.temperature_offset = -6.0f;
        field.economic_multiplier = 1.3f;

        // 2. Paths
        for (int i = 0; i < cell_size; ++i) {
            int px = start_x + i;
            int py1 = start_y + i;
            int py2 = start_y + (cell_size - 1 - i);
            if (!arterials.count({px, py1})) { createTile(px, py1, 0, TerrainType::SIDEWALK, '.', "#333333", MaterialType::CONCRETE); footprint.insert({px, py1}); }
            if (!arterials.count({px, py2})) { createTile(px, py2, 0, TerrainType::SIDEWALK, '.', "#333333", MaterialType::CONCRETE); footprint.insert({px, py2}); }
        }

        // 3. Nature Features (Sparse distribution)
        std::uniform_real_distribution<> feat_dis(0.0, 1.0);
        for (int x = start_x; x < start_x + cell_size; ++x) {
            for (int y = start_y; y < start_y + cell_size; ++y) {
                if (arterials.count({x, y}) || footprint.count({x, y})) continue;

                double roll = feat_dis(gen);
                if (roll < 0.08) { // Trees
                    createNatureFeature(x, y, "Tree", 'Y', "#00AA00", true);
                    footprint.insert({x, y});
                } else if (roll < 0.12) { // Benches
                    createNatureFeature(x, y, "Bench", '=', "#884400", false);
                    footprint.insert({x, y});
                } else if (roll < 0.13) { // Fountain (very rare)
                    createNatureFeature(x, y, "Fountain", '~', "#00FFFF", true, TerrainType::WATER_FEATURE);
                    footprint.insert({x, y});
                }
            }
        }
    }

    void createNatureFeature(int x, int y, std::string name, char glyph, std::string color, bool is_obstacle, TerrainType terrain = TerrainType::GRASS) {
        auto e = m_registry.create();
        m_registry.emplace<PositionComponent>(e, x, y, 0);
        m_registry.emplace<NameComponent>(e, name);
        m_registry.emplace<RenderableComponent>(e, glyph, color, 0);
        m_registry.emplace<NatureEffectComponent>(e, 2.0f, -1.0f); // 2 frustration reduction per tick
        if (is_obstacle) m_registry.emplace<ObstacleComponent>(e);
        createTile(x, y, 0, terrain, terrain == TerrainType::GRASS ? '"' : '~', terrain == TerrainType::GRASS ? "#004400" : "#0055FF", MaterialType::WATER);
    }

    void generateAirportInterior(const MacroZoneComponent& zone, int cell_size, const std::set<std::pair<int, int>>& arterials, std::mt19937& gen) {
        int start_x = zone.macro_x * cell_size;
        int start_y = zone.macro_y * cell_size;

        // 1. Runways (top and bottom)
        for (int x = start_x + 5; x < start_x + cell_size - 5; ++x) {
            createTile(x, start_y + 5, 0, TerrainType::CONCRETE_FLOOR, '=', "#FFFF00", MaterialType::CONCRETE);
            createTile(x, start_y + 6, 0, TerrainType::CONCRETE_FLOOR, '=', "#FFFF00", MaterialType::CONCRETE);
            createTile(x, start_y + cell_size - 6, 0, TerrainType::CONCRETE_FLOOR, '=', "#FFFF00", MaterialType::CONCRETE);
            createTile(x, start_y + cell_size - 7, 0, TerrainType::CONCRETE_FLOOR, '=', "#FFFF00", MaterialType::CONCRETE);
        }

        // 2. Terminal (central)
        int tw = 12; int th = 10;
        int tx = start_x + (cell_size - tw) / 2;
        int ty = start_y + (cell_size - th) / 2;
        createBuildingShell("Terminal", tx, ty, tw, th, 3, "#FFFF55", ZoneType::AIRPORT);
        generateAccessPath(tx + (tw/2), ty + th - 1, arterials);

        // 3. Control Tower (corner)
        createBuildingShell("Control Tower", start_x + 2, start_y + 2, 4, 4, 10, "#AAAAFF", ZoneType::AIRPORT);

        // 4. Cargo Crates
        std::uniform_real_distribution<> item_dis(0.0, 1.0);
        for (int i = 0; i < 5; ++i) {
            int cx = start_x + 10 + (int)(item_dis(gen) * (cell_size - 20));
            int cy = start_y + 10 + (int)(item_dis(gen) * (cell_size - 20));
            if (!arterials.count({cx, cy})) {
                spawnItem(cx, cy, 0, "Cargo Crate", '[', "#AA8800", 500);
            }
        }
    }

    void generateColosseumInterior(const MacroZoneComponent& zone, int cell_size, const std::set<std::pair<int, int>>& arterials, std::mt19937& gen) {
        int start_x = zone.macro_x * cell_size;
        int start_y = zone.macro_y * cell_size;

        // 1. Central Arena (Sand/Dirt)
        int arena_radius = cell_size / 3;
        int center_x = start_x + cell_size / 2;
        int center_y = start_y + cell_size / 2;

        for (int x = start_x; x < start_x + cell_size; ++x) {
            for (int y = start_y; y < start_y + cell_size; ++y) {
                if (arterials.count({x, y})) continue;

                double dx = x - center_x;
                double dy = y - center_y;
                double dist = std::sqrt(dx*dx + dy*dy);

                if (dist < arena_radius) {
                    // Arena floor
                    createTile(x, y, 0, TerrainType::ARENA_FLOOR, '.', "#886644", MaterialType::CONCRETE);
                } else if (dist < arena_radius + 2) {
                    // Walls separating arena from seating
                    createTile(x, y, 0, TerrainType::WALL, '#', "#AAAAAA", MaterialType::STEEL);
                } else if (dist < arena_radius + 6) {
                    // Seating
                    createTile(x, y, 0, TerrainType::ARENA_SEATING, '=', "#555555", MaterialType::CONCRETE);
                    // Add some seats (ASCII)
                    if (((int)dist % 2 == 0) && ((x + y) % 2 == 0)) {
                        auto seat = m_registry.create();
                        m_registry.emplace<PositionComponent>(seat, x, y, 0);
                        m_registry.emplace<RenderableComponent>(seat, 'h', "#888888", 0);
                        m_registry.emplace<NameComponent>(seat, "Arena Seat");
                    }
                } else {
                    // Concourse
                    createTile(x, y, 0, TerrainType::CONCRETE_FLOOR, '.', "#222222", MaterialType::CONCRETE);
                }
            }
        }

        // 2. Main Entrance
        createBuildingShell("Colosseum Grand Entrance", center_x - 4, start_y + 2, 8, 6, 2, "#FFCC00", ZoneType::COLOSSEUM);
        generateAccessPath(center_x, start_y + 7, arterials);
    }

    void generateZoneInterior(const MacroZoneComponent& zone, int cell_size, const std::set<std::pair<int, int>>& arterials, std::mt19937& gen) {
        int start_x = zone.macro_x * cell_size;
        int start_y = zone.macro_y * cell_size;

        std::set<std::pair<int, int>> structure_footprint;

        std::uniform_real_distribution<> dis(0.0, 1.0);
        if (dis(gen) < zone.density) {
            int bw = 4 + (int)(dis(gen) * 6);
            int bh = 4 + (int)(dis(gen) * 6);
            int bx = start_x + (cell_size - bw) / 2;
            int by = start_y + (cell_size - bh) / 2;
            
            bool obstructed = false;
            for (int fx = bx; fx < bx + bw; ++fx) {
                for (int fy = by; fy < by + bh; ++fy) {
                    if (arterials.count({fx, fy})) obstructed = true;
                }
            }

            if (!obstructed && zone.type != ZoneType::TRANSIT && zone.type != ZoneType::PARK) {
                std::string building_name = "Tower";
                std::string building_color = "#0055FF";
                int floors = 1;

                switch(zone.type) {
                    case ZoneType::CORPORATE: building_name = "Highrise"; building_color = "#0055FF"; floors = 10; break;
                    case ZoneType::SLUM: building_name = "Shanty"; building_color = "#884400"; floors = 1; break;
                    case ZoneType::INDUSTRIAL: building_name = "Plant"; building_color = "#AA2200"; floors = 2; break;
                    case ZoneType::RESIDENTIAL: building_name = "Apartments"; building_color = "#00AA44"; floors = 4; break;
                    case ZoneType::AIRPORT: building_name = "Terminal"; building_color = "#FFFF55"; floors = 3; break;
                    case ZoneType::COLOSSEUM: building_name = "Grand Arena"; building_color = "#FFCC00"; floors = 5; break;
                    default: break;
                }

                if (zone.type == ZoneType::AIRPORT) {
                    generateAirportInterior(zone, cell_size, arterials, gen);
                } else if (zone.type == ZoneType::PARK) {
                    generateParkInterior(zone, cell_size, arterials, gen, structure_footprint);
                } else if (zone.type == ZoneType::COLOSSEUM) {
                    generateColosseumInterior(zone, cell_size, arterials, gen);
                } else {
                    createBuildingShell(building_name, bx, by, bw, bh, floors, building_color, zone.type);
                    for (int fx = bx; fx < bx + bw; ++fx) {
                        for (int fy = by; fy < by + bh; ++fy) {
                            structure_footprint.insert({fx, fy});
                        }
                    }
                    generateAccessPath(bx + (bw/2), by + bh - 1, arterials);
                }
                
                // [NEW] Spawn some tools/items near buildings
                std::uniform_real_distribution<> item_dis(0.0, 1.0);
                if (item_dis(gen) < 0.3) {
                    spawnItem(bx + (bw/2), by + bh, 0, "Bio Scanner", '!', "#00FF00", 101, false, 0, 0, "bio_scanner");
                }
                if (item_dis(gen) < 0.2) {
                    spawnItem(bx + (bw/2) + 1, by + bh, 0, "Diagnostic Tool", '?', "#00FFFF", 102, false, 0, 0, "diagnostic_scanner");
                }
                if (item_dis(gen) < 0.5) {
                    spawnItem(bx + (bw/2) - 1, by + bh, 0, "Synth-Bar", '(', "#884400", 201, true, 20, 5);
                }
            }
        }

        for (int x = start_x; x < start_x + cell_size; ++x) {
            for (int y = start_y; y < start_y + cell_size; ++y) {
                if (structure_footprint.count({x, y}) || arterials.count({x, y})) continue;

                TerrainType type = TerrainType::GRASS;
                char glyph = '"';
                std::string color = "#004400";
                MaterialType material = MaterialType::CONCRETE; // Defaults

                switch(zone.type) {
                    case ZoneType::CORPORATE: type = TerrainType::SIDEWALK; glyph = '.'; color = "#111111"; break;
                    case ZoneType::SLUM: type = TerrainType::DIRT; glyph = '\''; color = "#443322"; break;
                    case ZoneType::INDUSTRIAL: type = TerrainType::CONCRETE_FLOOR; glyph = '.'; color = "#222222"; break;
                    case ZoneType::RESIDENTIAL: type = TerrainType::GRASS; glyph = '"'; color = "#004400"; break;
                    case ZoneType::TRANSIT: type = TerrainType::STREET; glyph = ' '; color = "#333333"; break;
                    case ZoneType::AIRPORT: type = TerrainType::CONCRETE_FLOOR; glyph = '.'; color = "#222222"; break;
                    case ZoneType::PARK: type = TerrainType::GRASS; glyph = '"'; color = "#004400"; break;
                    case ZoneType::COLOSSEUM: type = TerrainType::ARENA_FLOOR; glyph = '.'; color = "#222222"; break;
                    default: break;
                }
                createTile(x, y, 0, type, glyph, color, material);
            }
        }
    }

    void generateAccessPath(int door_x, int door_y, const std::set<std::pair<int, int>>& arterials) {
        std::queue<std::pair<int, int>> q;
        q.push({door_x, door_y});
        
        std::set<std::pair<int, int>> visited;
        std::map<std::pair<int, int>, std::pair<int, int>> parent;
        visited.insert({door_x, door_y});

        std::pair<int, int> target = {-1, -1};
        while(!q.empty()) {
            auto curr = q.front(); q.pop();
            if (arterials.count(curr)) { target = curr; break; }

            int dx[] = {0, 0, 1, -1};
            int dy[] = {1, -1, 0, 0};
            for(int i=0; i<4; ++i) {
                std::pair<int, int> next = {curr.first + dx[i], curr.second + dy[i]};
                if (next.first >= 0 && next.first < 200 && next.second >= 0 && next.second < 200 && !visited.count(next)) {
                    visited.insert(next);
                    parent[next] = curr;
                    q.push(next);
                }
            }
            if (visited.size() > 200) break; 
        }

        if (target.first != -1) {
            std::pair<int, int> p = target;
            while (p != std::make_pair(door_x, door_y)) {
                createTile(p.first, p.second, 0, TerrainType::SIDEWALK, '.', "#444444", MaterialType::CONCRETE);
                p = parent[p];
            }
        }
    }

    void createTile(int x, int y, int layer, TerrainType type, char glyph, std::string color, MaterialType material = MaterialType::CONCRETE) {
        auto e = m_registry.create();
        m_registry.emplace<PositionComponent>(e, x, y, layer);
        m_registry.emplace<TerrainComponent>(e, type);
        m_registry.emplace<RenderableComponent>(e, glyph, color, layer);
        auto& phys = m_registry.emplace<Layer0PhysicsComponent>(e);
        phys.material = material;
        if (type == TerrainType::WALL) m_registry.emplace<ObstacleComponent>(e);
    }

    void spawnItem(int x, int y, int layer, std::string name, char glyph, std::string color, uint32_t type_id, bool consumable = false, int hunger = 0, int thirst = 0, std::string effect_id = "") {
        auto e = m_registry.create();
        m_registry.emplace<PositionComponent>(e, x, y, layer);
        m_registry.emplace<NameComponent>(e, name);
        m_registry.emplace<RenderableComponent>(e, glyph, color, layer);
        m_registry.emplace<ItemComponent>(e, type_id, name);
        if (consumable) {
            m_registry.emplace<ConsumableComponent>(e, hunger, thirst);
        }
        if (!effect_id.empty()) {
            m_registry.emplace<UsableComponent>(e, effect_id);
        }
    }

    void spawnPersonalVehicle(int x, int y, int layer, PersonalVehicleType type) {
        auto v = m_registry.create();
        m_registry.emplace<PositionComponent>(v, x, y, layer);
        m_registry.emplace<NameComponent>(v, "Vehicle");
        m_registry.emplace<SizeComponent>(v, 1, 1);
        m_registry.emplace<TransitOccupantsComponent>(v);
        
        char glyph = 'V';
        std::string color = "#FFFF00";
        int capacity = 1;
        float speed = 1.0f;

        switch(type) {
            case PersonalVehicleType::SCOOTER: glyph = ','; color = "#AAAAAA"; capacity = 1; speed = 1.5f; break;
            case PersonalVehicleType::BIKE: glyph = 'i'; color = "#55AAFF"; capacity = 1; speed = 1.2f; break;
            case PersonalVehicleType::CAR: glyph = 'A'; color = "#FF5555"; capacity = 4; speed = 2.0f; break;
            case PersonalVehicleType::SCI_FI: glyph = 'X'; color = "#FF55FF"; capacity = 2; speed = 3.0f; break;
        }

        m_registry.emplace<RenderableComponent>(v, glyph, color, layer);
        m_registry.emplace<PersonalVehicleComponent>(v, type, entt::null, capacity, speed);
        m_registry.emplace<ObstacleComponent>(v); // Vehicles are solid objects
    }

    void createBuildingShell(std::string name, int x, int y, int w, int h, int floors, std::string color, ZoneType ztype) {
        auto building = m_registry.create();
        m_registry.emplace<NameComponent>(building, name);
        m_registry.emplace<PositionComponent>(building, x, y, 0);
        m_registry.emplace<BuildingComponent>(building, floors, ztype, 0);
        m_registry.emplace<SizeComponent>(building, w, h);
        auto& b_phys = m_registry.emplace<Layer0PhysicsComponent>(building);
        b_phys.material = MaterialType::STEEL; // Buildings usually have steel frames
        m_registry.emplace<PropertyComponent>(building);

        for (int cur_x = x; cur_x < x + w; ++cur_x) {
            for (int cur_y = y; cur_y < y + h; ++cur_y) {
                bool is_edge = (cur_x == x || cur_x == x + w - 1 || cur_y == y || cur_y == y + h - 1);
                if (is_edge) {
                    if (cur_y == y + h - 1 && cur_x == x + (w/2)) {
                        auto door = m_registry.create();
                        m_registry.emplace<PositionComponent>(door, cur_x, cur_y, 0);
                        m_registry.emplace<RenderableComponent>(door, '+', "#FFFF00", 0);
                        m_registry.emplace<NameComponent>(door, name + " Entrance");
                        m_registry.emplace<BuildingEntranceComponent>(door, building, 0);
                        createTile(cur_x, cur_y, 0, TerrainType::CONCRETE_FLOOR, '.', "#111111", MaterialType::CONCRETE);
                    } else {
                        createTile(cur_x, cur_y, 0, TerrainType::WALL, '#', color, MaterialType::CONCRETE);
                    }
                } else {
                    createTile(cur_x, cur_y, 0, TerrainType::CONCRETE_FLOOR, '.', "#111111", MaterialType::CONCRETE);
                }
            }
        }
    }

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif
