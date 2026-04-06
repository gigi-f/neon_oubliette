#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_CITY_GENERATION_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_CITY_GENERATION_SYSTEM_H

#include <entt/entt.hpp>
#include "../components/components.h"
#include "../components/zoning_components.h"
#include "../components/simulation_layers.h"
#include "../components/infrastructure_components.h"
#include "../components/lod_components.h"
#include "../system_scheduler.h"
#include <string>
#include <set>
#include <map>
#include <random>
#include <queue>
#include <cmath>
#include "../components/transit_components.h"

namespace NeonOubliette {

/**
 * @brief Handles placement of tiles and buildings based on MacroZones and Infrastructure.
 */
class CityGenerationSystem : public ISystem {
public:
    struct DoorInfo {
        int x, y;
        bool primary;
    };

    CityGenerationSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {}
    void update(double delta_time) override { (void)delta_time; }

    void generate_chunk_content(entt::entity zone_entity) {
        auto const& zone = m_registry.get<MacroZoneComponent>(zone_entity);
        auto config_view = m_registry.view<WorldConfigComponent>();
        if (config_view.empty()) return;
        auto& config = config_view.get<WorldConfigComponent>(config_view.front());

        // Find parent chunk for interior caching
        entt::entity chunk_ent = entt::null;
        auto chunk_view = m_registry.view<ChunkComponent>();
        for (auto ce : chunk_view) {
            const auto& c = chunk_view.get<ChunkComponent>(ce);
            if (c.chunk_x == zone.macro_x / 2 && c.chunk_y == zone.macro_y / 2) {
                chunk_ent = ce;
                break;
            }
        }

        // Deterministic Seed for this zone
        uint32_t zone_seed = config.world_seed ^ (static_cast<uint32_t>(zone.macro_x) * 73856093) ^ (static_cast<uint32_t>(zone.macro_y) * 19349663);
        std::mt19937 zone_gen(zone_seed);

        std::map<std::pair<int, int>, ArterialType> arterial_map;
        for (auto e : zone.arterial_entities) {
            if (!m_registry.all_of<InfrastructureArterialComponent>(e)) continue;
            const auto& pos = m_registry.get<PositionComponent>(e);
            const auto& art = m_registry.get<InfrastructureArterialComponent>(e);
            arterial_map[{pos.x, pos.y}] = art.type;
        }

        generateZoneInterior(zone, config.macro_cell_size, arterial_map, zone_gen, zone_entity, chunk_ent);

        spawnCommerceHubExtras(zone, config.macro_cell_size, arterial_map, zone_gen);
        
        for (auto const& [pos_pair, type] : arterial_map) {
            TerrainType t_type = TerrainType::STREET;
            MaterialType material = MaterialType::CONCRETE;
            char glyph = ' ';
            std::string color = "#333333";
            bool is_obstacle = false;
            bool is_liquid = false;

            switch(type) {
                case ArterialType::WATERWAY_RIVER:
                    t_type = TerrainType::VOID; glyph = '~'; color = "#0000FF";
                    is_obstacle = true; material = MaterialType::WATER; is_liquid = true;
                    break;
                case ArterialType::ROAD_PRIMARY:
                    t_type = TerrainType::STREET; glyph = ' '; color = "#222222";
                    material = MaterialType::CONCRETE;
                    break;
                case ArterialType::ROAD_SECONDARY:
                    t_type = TerrainType::STREET; glyph = '.'; color = "#444444";
                    material = MaterialType::CONCRETE;
                    break;
                case ArterialType::ROAD_ALLEY:
                    t_type = TerrainType::STREET; glyph = '.'; color = "#221100";
                    material = MaterialType::CONCRETE;
                    break;
                case ArterialType::SIDEWALK:
                    t_type = TerrainType::SIDEWALK; glyph = '.'; color = "#555555";
                    material = MaterialType::CONCRETE;
                    break;
                case ArterialType::RAIL_ELEVATED:
                    t_type = TerrainType::VOID; glyph = '='; color = "#FFFF00";
                    material = MaterialType::STEEL;
                    break;
                default: break;
            }

            // Node check for bridges
            for (auto ne : zone.arterial_entities) {
                if (!m_registry.all_of<InfrastructureNodeComponent>(ne)) continue;
                const auto& n_pos = m_registry.get<PositionComponent>(ne);
                const auto& node = m_registry.get<InfrastructureNodeComponent>(ne);
                if (n_pos.x == pos_pair.first && n_pos.y == pos_pair.second && node.is_bridge) {
                    glyph = '='; color = "#AAAAAA";
                    is_obstacle = false;
                    material = MaterialType::STEEL;
                    break;
                }
            }
            
            auto tile = m_registry.create();
            m_registry.emplace<PositionComponent>(tile, pos_pair.first, pos_pair.second, 0);
            m_registry.emplace<TerrainComponent>(tile, t_type);
            m_registry.emplace<RenderableComponent>(tile, glyph, color, 0);
            auto& phys = m_registry.emplace<Layer0PhysicsComponent>(tile);
            phys.material = material;
            phys.is_liquid = is_liquid;
            if (is_obstacle) m_registry.emplace<ObstacleComponent>(tile);

            if (type == ArterialType::ROAD_PRIMARY) {
                std::uniform_real_distribution<> v_dis(0.0, 1.0);
                if (v_dis(zone_gen) < 0.05) {
                    PersonalVehicleType v_type = PersonalVehicleType::CAR;
                    double r = v_dis(zone_gen);
                    if (r < 0.3) v_type = PersonalVehicleType::SCOOTER;
                    else if (r < 0.6) v_type = PersonalVehicleType::BIKE;
                    else if (r < 0.9) v_type = PersonalVehicleType::CAR;
                    else v_type = PersonalVehicleType::SCI_FI;
                    spawnPersonalVehicle(pos_pair.first, pos_pair.second, 0, v_type);
                }
            }
        }
    }
private:
    void generateZoneInterior(const MacroZoneComponent& zone, int cell_size, std::map<std::pair<int, int>, ArterialType>& arterials, std::mt19937& gen, entt::entity zone_entity, entt::entity chunk_ent) {
        int start_x = zone.macro_x * cell_size;
        int start_y = zone.macro_y * cell_size;
        std::set<std::pair<int, int>> structure_footprint;
        float dist_to_core = calculateDistanceToUrbanCore(zone);

        if (zone.type == ZoneType::URBAN_CORE) {
            generateUrbanCoreInterior(zone, cell_size, arterials, gen, structure_footprint, chunk_ent);
        } else if (zone.type == ZoneType::AIRPORT) {
            generateAirportInterior(zone, cell_size, arterials, gen, chunk_ent);
        } else if (zone.type == ZoneType::PARK) {
            generateParkInterior(zone, cell_size, arterials, gen, structure_footprint);
        } else if (zone.type == ZoneType::COLOSSEUM) {
            generateColosseumInterior(zone, cell_size, arterials, gen, chunk_ent);
        } else if (zone.type == ZoneType::MIXED_COMMERCIAL) {
            generateMixedCommercialInterior(zone, cell_size, arterials, gen, structure_footprint, chunk_ent);
        } else if (zone.type != ZoneType::TRANSIT) {
            for (auto block_ent : zone.block_entities) {
                const auto& block = m_registry.get<BlockComponent>(block_ent);
                for (auto l_entity : block.lots) {
                    const auto& lot = m_registry.get<LotComponent>(l_entity);
                    std::uniform_real_distribution<> dis(0.0, 1.0);
                    if (dis(gen) < zone.density) {
                        int bx = lot.x; int by = lot.y; int bw = lot.width; int bh = lot.height;
                        bool n_road = isRoad(bx, by - 1, bw, 1, arterials);
                        bool s_road = isRoad(bx, by + bh, bw, 1, arterials);
                        bool w_road = isRoad(bx - 1, by, 1, bh, arterials);
                        bool e_road = isRoad(bx + bw, by, 1, bh, arterials);
                        if (n_road) { for (int x = bx; x < bx + bw; ++x) arterials[{x, by}] = ArterialType::SIDEWALK; by++; bh--; }
                        if (s_road) { for (int x = bx; x < bx + bw; ++x) arterials[{x, by + bh - 1}] = ArterialType::SIDEWALK; bh--; }
                        if (w_road) { for (int y = by; y < by + bh; ++y) arterials[{bx, y}] = ArterialType::SIDEWALK; bx++; bw--; }
                        if (e_road) { for (int y = by; y < by + bh; ++y) arterials[{bx + bw - 1, y}] = ArterialType::SIDEWALK; bw--; }
                        if (bw > 1 && bh > 1) {
                            std::string b_name = "Building"; std::string b_color = "#555555"; int floors = 1;
                            switch(zone.type) {
                                case ZoneType::CORPORATE: b_name = "Highrise"; b_color = "#0055FF"; floors = 10; break;
                                case ZoneType::SLUM: b_name = "Shanty"; b_color = "#884400"; floors = 1; break;
                                case ZoneType::INDUSTRIAL: b_name = "Plant"; b_color = "#AA2200"; floors = 2; break;
                                case ZoneType::RESIDENTIAL: 
                                    b_name = "Apartments"; b_color = "#00AA44"; floors = std::max(1, 8 - (int)dist_to_core);
                                    if (floors <= 2) b_name = "Row-house";
                                    break;
                                default: break;
                            }
                            uint8_t shared_sides = calculateSharedSides(lot, block);
                            auto doors = calculateDoorPositions(lot, bx, by, bw, bh, shared_sides, arterials);
                            uint32_t stable_id = static_cast<uint32_t>(bx * 10000 + by);
                            createBuildingShell(b_name, bx, by, bw, bh, floors, b_color, zone.type, (uint8_t)lot.facing, (uint8_t)lot.alley_facing, shared_sides, doors, stable_id, chunk_ent);
                            for (int fx = bx; fx < bx + bw; ++fx) for (int fy = by; fy < by + bh; ++fy) structure_footprint.insert({fx, fy});
                            for (const auto& d : doors) if (d.primary) generateAccessPath(d.x, d.y, arterials);
                        }
                    }
                }
            }
        }
        for (int x = start_x; x < start_x + cell_size; ++x) {
            for (int y = start_y; y < start_y + cell_size; ++y) {
                if (structure_footprint.count({x, y}) || arterials.count({x, y})) continue;
                TerrainType t_type = TerrainType::GRASS; char glyph = '"'; std::string color = "#004400";
                switch(zone.type) {
                    case ZoneType::URBAN_CORE: case ZoneType::CORPORATE: t_type = TerrainType::SIDEWALK; glyph = '.'; color = "#111111"; break;
                    case ZoneType::SLUM: t_type = TerrainType::DIRT; glyph = '\''; color = "#443322"; break;
                    case ZoneType::INDUSTRIAL: t_type = TerrainType::CONCRETE_FLOOR; glyph = '.'; color = "#222222"; break;
                    case ZoneType::RESIDENTIAL: t_type = TerrainType::GRASS; glyph = '"'; color = "#004400"; break;
                    case ZoneType::TRANSIT: t_type = TerrainType::STREET; glyph = ' '; color = "#333333"; break;
                    case ZoneType::AIRPORT: t_type = TerrainType::CONCRETE_FLOOR; glyph = '.'; color = "#222222"; break;
                    case ZoneType::PARK: t_type = TerrainType::GRASS; glyph = '"'; color = "#004400"; break;
                    case ZoneType::COLOSSEUM: t_type = TerrainType::ARENA_FLOOR; glyph = '.'; color = "#222222"; break;
                    case ZoneType::MIXED_COMMERCIAL: t_type = TerrainType::SIDEWALK; glyph = '.'; color = "#221122"; break;
                    default: break;
                }
                createTile(x, y, 0, t_type, glyph, color, MaterialType::CONCRETE);
            }
        }
    }

    bool isRoad(int x, int y, int w, int h, const std::map<std::pair<int, int>, ArterialType>& arterials) {
        for (int ix = x; ix < x + w; ++ix) for (int iy = y; iy < y + h; ++iy) {
            auto it = arterials.find({ix, iy}); if (it != arterials.end() && (it->second == ArterialType::ROAD_PRIMARY || it->second == ArterialType::ROAD_SECONDARY)) return true;
        }
        return false;
    }
    void spawnCommerceHubExtras(const MacroZoneComponent& zone, int cell_size, const std::map<std::pair<int, int>, ArterialType>& arterials, std::mt19937& gen) {
        auto hub_view = m_registry.view<PositionComponent, CommerceHubComponent>();
        int start_x = zone.macro_x * cell_size, start_y = zone.macro_y * cell_size;
        int end_x = start_x + cell_size, end_y = start_y + cell_size;
        for (auto hub_entity : hub_view) {
            const auto& hub_pos = hub_view.get<PositionComponent>(hub_entity); const auto& hub_comp = hub_view.get<CommerceHubComponent>(hub_entity);
            if (hub_pos.x >= start_x - hub_comp.influence_radius && hub_pos.x < end_x + hub_comp.influence_radius && hub_pos.y >= start_y - hub_comp.influence_radius && hub_pos.y < end_y + hub_comp.influence_radius) {
                std::uniform_real_distribution<> dis(0.0, 1.0);
                for (int i = 0; i < 5; ++i) {
                    float r = dis(gen) * hub_comp.influence_radius, theta = dis(gen) * 2.0f * 3.14159f;
                    int kx = (int)(hub_pos.x + r * std::cos(theta)), ky = (int)(hub_pos.y + r * std::sin(theta));
                    if (kx >= start_x && kx < end_x && ky >= start_y && ky < end_y) {
                        bool occupied = false; auto obstacle_view = m_registry.view<PositionComponent, ObstacleComponent>();
                        for(auto oe : obstacle_view) { const auto& o_pos = obstacle_view.get<PositionComponent>(oe); if (o_pos.x == kx && o_pos.y == ky) { occupied = true; break; } }
                        if (!occupied && !arterials.count({kx, ky})) createKiosk(kx, ky, "Hub Kiosk", "#00FF00");
                    }
                }
            }
        }
    }
    float calculateDistanceToUrbanCore(const MacroZoneComponent& zone) {
        auto view = m_registry.view<MacroZoneComponent>(); float min_dist = 100.0f; bool found = false;
        for (auto entity : view) {
            const auto& z = view.get<MacroZoneComponent>(entity);
            if (z.type == ZoneType::URBAN_CORE) { float dx = (float)(z.macro_x - zone.macro_x), dy = (float)(z.macro_y - zone.macro_y); float dist = std::sqrt(dx*dx + dy*dy); if (dist < min_dist) min_dist = dist; found = true; }
        }
        return found ? min_dist : 10.0f;
    }
    bool facesAnyArterial(int x, int y, int w, int h, const std::map<std::pair<int, int>, ArterialType>& arterials) {
        for (int ix = x; ix < x + w; ++ix) for (int iy = y; iy < y + h; ++iy) if (arterials.count({ix, iy})) return true;
        return false;
    }
    void generateUrbanCoreInterior(const MacroZoneComponent& zone, int cell_size, std::map<std::pair<int, int>, ArterialType>& arterials, std::mt19937& gen, std::set<std::pair<int, int>>& structure_footprint, entt::entity chunk_ent) {
        placeTrainTerminals(zone, arterials, gen, structure_footprint, chunk_ent);
        for (auto b_entity : zone.block_entities) {
            const auto& block = m_registry.get<BlockComponent>(b_entity);
            for (auto l_entity : block.lots) {
                const auto& lot = m_registry.get<LotComponent>(l_entity); bool faces_arterial = facesAnyArterial(lot.x - 1, lot.y - 1, lot.width + 2, lot.height + 2, arterials);
                std::uniform_real_distribution<> dis(0.0, 1.0);
                if (dis(gen) < zone.density) {
                    int bw = std::max(4, lot.width - 1), bh = std::max(4, lot.height - 1);
                    int bx = lot.x + (lot.width - bw) / 2, by = lot.y + (lot.height - bh) / 2;
                    std::string name = faces_arterial ? "Plaza Tower" : "Core Spires";
                    int floors = 20 + (int)(dis(gen) * 80); std::string color = (floors > 60) ? "#FFFFFF" : (floors > 40) ? "#CCCCFF" : "#8888FF";
                    uint8_t shared_sides = calculateSharedSides(lot, block); auto doors = calculateDoorPositions(lot, bx, by, bw, bh, shared_sides, arterials);
                    uint32_t stable_id = bx * 10000 + by;
                    createSkyscraperShell(name, bx, by, bw, bh, floors, color, ZoneType::URBAN_CORE, (uint8_t)lot.facing, (uint8_t)lot.alley_facing, shared_sides, doors, stable_id, chunk_ent);
                    for (int fx = bx; fx < bx + bw; ++fx) for (int fy = by; fy < by + bh; ++fy) structure_footprint.insert({fx, fy});
                    for (const auto& d : doors) if (d.primary) generateAccessPath(d.x, d.y, arterials);
                }
            }
        }
    }
    void generateAirportInterior(const MacroZoneComponent& zone, int cell_size, std::map<std::pair<int, int>, ArterialType>& arterials, std::mt19937& gen, entt::entity chunk_ent) {
        int start_x = zone.macro_x * cell_size, start_y = zone.macro_y * cell_size;
        for (int x = start_x + 5; x < start_x + cell_size - 5; ++x) {
            createTile(x, start_y + 5, 0, TerrainType::CONCRETE_FLOOR, '=', "#FFFF00", MaterialType::CONCRETE);
            createTile(x, start_y + 6, 0, TerrainType::CONCRETE_FLOOR, '=', "#FFFF00", MaterialType::CONCRETE);
            createTile(x, start_y + cell_size - 6, 0, TerrainType::CONCRETE_FLOOR, '=', "#FFFF00", MaterialType::CONCRETE);
            createTile(x, start_y + cell_size - 7, 0, TerrainType::CONCRETE_FLOOR, '=', "#FFFF00", MaterialType::CONCRETE);
        }
        int tw = 12, th = 10, tx = start_x + (cell_size - tw) / 2, ty = start_y + (cell_size - th) / 2;
        std::vector<DoorInfo> t_doors = {{tx + tw/2, ty + th - 1, true}};
        createBuildingShell("Terminal", tx, ty, tw, th, 3, "#FFFF55", ZoneType::AIRPORT, 0, t_doors, tx * 10000 + ty, chunk_ent);
        generateAccessPath(tx + (tw/2), ty + th - 1, arterials);
        createBuildingShell("Control Tower", start_x + 2, start_y + 2, 4, 4, 10, "#AAAAFF", ZoneType::AIRPORT, 0, {{start_x + 4, start_y + 5, true}}, (start_x+2)*10000+(start_y+2), chunk_ent);
    }
    void generateParkInterior(const MacroZoneComponent& zone, int cell_size, std::map<std::pair<int, int>, ArterialType>& arterials, std::mt19937& gen, std::set<std::pair<int, int>>& footprint) {
        int start_x = zone.macro_x * cell_size, start_y = zone.macro_y * cell_size;
        auto park_anchor = m_registry.create(); m_registry.emplace<NameComponent>(park_anchor, zone.district_name + " Park");
        m_registry.emplace<PositionComponent>(park_anchor, start_x + (cell_size / 2), start_y + (cell_size / 2), 0);
        m_registry.emplace<ParkComponent>(park_anchor, 80, entt::null); auto& field = m_registry.emplace<ConduitFieldComponent>(park_anchor);
        field.radius = (float)cell_size; field.temperature_offset = -6.0f; field.economic_multiplier = 1.3f;
        for (int i = 0; i < cell_size; ++i) {
            int px = start_x + i, py1 = start_y + i, py2 = start_y + (cell_size - 1 - i);
            if (!arterials.count({px, py1})) { arterials[{px, py1}] = ArterialType::SIDEWALK; footprint.insert({px, py1}); }
            if (!arterials.count({px, py2})) { arterials[{px, py2}] = ArterialType::SIDEWALK; footprint.insert({px, py2}); }
        }
        std::uniform_real_distribution<> feat_dis(0.0, 1.0);
        for (int x = start_x; x < start_x + cell_size; ++x) {
            for (int y = start_y; y < start_y + cell_size; ++y) {
                if (arterials.count({x, y}) || footprint.count({x, y})) continue;
                double roll = feat_dis(gen);
                if (roll < 0.08) { createNatureFeature(x, y, "Tree", 'Y', "#00AA00", true); footprint.insert({x, y}); }
                else if (roll < 0.12) { createNatureFeature(x, y, "Bench", '=', "#884400", false); footprint.insert({x, y}); }
                else if (roll < 0.13) { createNatureFeature(x, y, "Fountain", '~', "#00FFFF", true, TerrainType::WATER_FEATURE); footprint.insert({x, y}); }
            }
        }
    }

    void generateColosseumInterior(const MacroZoneComponent& zone, int cell_size, std::map<std::pair<int, int>, ArterialType>& arterials, std::mt19937& gen, entt::entity chunk_ent) {
        int start_x = zone.macro_x * cell_size, start_y = zone.macro_y * cell_size;
        int arena_radius = cell_size / 3, center_x = start_x + cell_size / 2, center_y = start_y + cell_size / 2;
        for (int x = start_x; x < start_x + cell_size; ++x) for (int y = start_y; y < start_y + cell_size; ++y) {
            if (arterials.count({x, y})) continue;
            double dx = x - center_x, dy = y - center_y, dist = std::sqrt(dx*dx + dy*dy);
            if (dist < arena_radius) createTile(x, y, 0, TerrainType::ARENA_FLOOR, '.', "#886644", MaterialType::CONCRETE);
            else if (dist < arena_radius + 2) createTile(x, y, 0, TerrainType::WALL, '#', "#AAAAAA", MaterialType::STEEL);
            else if (dist < arena_radius + 6) createTile(x, y, 0, TerrainType::ARENA_SEATING, '=', "#555555", MaterialType::CONCRETE);
            else createTile(x, y, 0, TerrainType::CONCRETE_FLOOR, '.', "#222222", MaterialType::CONCRETE);
        }
        createBuildingShell("Colosseum Grand Entrance", center_x - 4, start_y + 2, 8, 6, 2, "#FFCC00", ZoneType::COLOSSEUM, 0, {{center_x, start_y + 7, true}}, (center_x-4)*10000+(start_y+2), chunk_ent);
        generateAccessPath(center_x, start_y + 7, arterials);
    }
    void generateMixedCommercialInterior(const MacroZoneComponent& zone, int cell_size, std::map<std::pair<int, int>, ArterialType>& arterials, std::mt19937& gen, std::set<std::pair<int, int>>& footprint, entt::entity chunk_ent) {
        for (auto block_ent : zone.block_entities) {
            const auto& block = m_registry.get<BlockComponent>(block_ent);
            for (auto l_entity : block.lots) {
                const auto& lot = m_registry.get<LotComponent>(l_entity); std::uniform_real_distribution<> dis(0.0, 1.0);
                if (dis(gen) < 0.9f) {
                    int bw = lot.width, bh = lot.height, bx = lot.x, by = lot.y;
                    if (bw >= 3 && bh >= 3) {
                        uint8_t shared_sides = calculateSharedSides(lot, block); auto doors = calculateDoorPositions(lot, bx, by, bw, bh, shared_sides, arterials);
                        uint32_t stable_id = bx * 10000 + by;
                        createBuildingShell("Shop", bx, by, bw, bh, 1 + (int)(dis(gen) * 2), "#FF00FF", ZoneType::MIXED_COMMERCIAL, shared_sides, doors, stable_id, chunk_ent);
                        for (int fx = bx; fx < bx + bw; ++fx) for (int fy = by; fy < by + bh; ++fy) footprint.insert({fx, fy});
                        spawnVendor(bx + (bw/2), by + (bh/2), 0, "Vendor " + std::to_string(bx));
                        for (const auto& d : doors) if (d.primary) generateAccessPath(d.x, d.y, arterials);
                    } else if (bw >= 1 && bh >= 1) { createKiosk(bx, by, "Kiosk", "#FFFF00"); footprint.insert({bx, by}); }
                }
            }
        }
    }
    void generateAccessPath(int door_x, int door_y, std::map<std::pair<int, int>, ArterialType>& arterials) {
        std::queue<std::pair<int, int>> q; q.push({door_x, door_y}); std::set<std::pair<int, int>> visited; std::map<std::pair<int, int>, std::pair<int, int>> parent;
        visited.insert({door_x, door_y}); std::pair<int, int> target = {-1, -1};
        while(!q.empty()) {
            auto curr = q.front(); q.pop(); if (arterials.count(curr)) { target = curr; break; }
            int dx[] = {0, 0, 1, -1}, dy[] = {1, -1, 0, 0};
            for(int i=0; i<4; ++i) {
                std::pair<int, int> next = {curr.first + dx[i], curr.second + dy[i]};
                if (next.first >= 0 && next.first < 8000 && next.second >= 0 && next.second < 8000 && !visited.count(next)) { visited.insert(next); parent[next] = curr; q.push(next); }
            }
            if (visited.size() > 200) break;
        }
        if (target.first != -1) { std::pair<int, int> p = target; while (p != std::make_pair(door_x, door_y)) { arterials[{p.first, p.second}] = ArterialType::SIDEWALK; p = parent[p]; } }
    }
    void createKiosk(int x, int y, std::string name, std::string color) {
        auto e = m_registry.create(); m_registry.emplace<PositionComponent>(e, x, y, 0); m_registry.emplace<NameComponent>(e, name);
        m_registry.emplace<RenderableComponent>(e, 'K', color, 0); m_registry.emplace<ObstacleComponent>(e);
        m_registry.emplace<BuildingComponent>(e, 1, ZoneType::MIXED_COMMERCIAL, 0, static_cast<uint32_t>(x * 10000 + y));
        spawnVendor(x, y, 0, name + " Vendor");
    }
    void spawnVendor(int x, int y, int layer, std::string name) {
        auto e = m_registry.create(); m_registry.emplace<PositionComponent>(e, x, y, layer); m_registry.emplace<NameComponent>(e, name); m_registry.emplace<RenderableComponent>(e, 'V', "#FFFFFF", layer); m_registry.emplace<AgentComponent>(e);
    }
    void createSkyscraperShell(std::string name, int x, int y, int w, int h, int floors, std::string color, ZoneType ztype, uint8_t facing_sides, uint8_t alley_sides, uint8_t shared_sides, const std::vector<DoorInfo>& doors, uint32_t stable_id, entt::entity chunk_ent) {
        auto building = m_registry.create(); m_registry.emplace<NameComponent>(building, name); m_registry.emplace<PositionComponent>(building, x, y, 0);
        m_registry.emplace<BuildingComponent>(building, floors, ztype, 0, stable_id); m_registry.emplace<SizeComponent>(building, w, h); m_registry.emplace<PropertyComponent>(building);
        if (chunk_ent != entt::null) {
            auto& chunk = m_registry.get<ChunkComponent>(chunk_ent); auto it = chunk.building_interiors.find({x, y});
            if (it != chunk.building_interiors.end()) m_registry.emplace<BuildingInteriorComponent>(building, it->second);
        }
        for (int cur_x = x; cur_x < x + w; ++cur_x) for (int cur_y = y; cur_y < y + h; ++cur_y) {
            bool is_edge = (cur_x == x || cur_x == x + w - 1 || cur_y == y || cur_y == y + h - 1); bool is_door = false;
            for (const auto& d : doors) if (cur_x == d.x && cur_y == d.y) { is_door = true; break; }
            if (is_edge) {
                if (is_door) {
                    auto door = m_registry.create(); m_registry.emplace<PositionComponent>(door, cur_x, cur_y, 0); m_registry.emplace<RenderableComponent>(door, '+', "#FFFF00", 0);
                    m_registry.emplace<BuildingEntranceComponent>(door, building, 0); createTile(cur_x, cur_y, 0, TerrainType::CONCRETE_FLOOR, '.', "#111111", MaterialType::CONCRETE);
                } else {
                    char glyph = (floors > 80) ? '^' : (floors > 50) ? 'A' : '#';
                    bool is_shared = false; 
                    if (cur_x == x && (shared_sides & (uint8_t)StreetFacingSide::WEST)) is_shared = true;
                    if (cur_x == x + w - 1 && (shared_sides & (uint8_t)StreetFacingSide::EAST)) is_shared = true;
                    if (cur_y == y && (shared_sides & (uint8_t)StreetFacingSide::NORTH)) is_shared = true;
                    if (cur_y == y + h - 1 && (shared_sides & (uint8_t)StreetFacingSide::SOUTH)) is_shared = true;

                    bool is_street = false;
                    if (cur_x == x && (facing_sides & (uint8_t)StreetFacingSide::WEST)) is_street = true;
                    if (cur_x == x + w - 1 && (facing_sides & (uint8_t)StreetFacingSide::EAST)) is_street = true;
                    if (cur_y == y && (facing_sides & (uint8_t)StreetFacingSide::NORTH)) is_street = true;
                    if (cur_y == y + h - 1 && (facing_sides & (uint8_t)StreetFacingSide::SOUTH)) is_street = true;

                    bool place_window = false;
                    if (!is_shared && is_street) {
                        // Skyscrapers (Urban Core / Corporate) have floor-to-ceiling windows
                        if (cur_x % 3 != 0 || cur_y % 3 != 0) place_window = true;
                    }

                    if (place_window) createTile(cur_x, cur_y, 0, TerrainType::WINDOW, '0', "#00FFFF", MaterialType::GLASS);
                    else createTile(cur_x, cur_y, 0, TerrainType::WALL, glyph, color, MaterialType::STEEL);
                }
            } else createTile(cur_x, cur_y, 0, TerrainType::CONCRETE_FLOOR, '.', "#050505", MaterialType::CONCRETE);
        }
    }
    void placeTrainTerminals(const MacroZoneComponent& zone, std::map<std::pair<int, int>, ArterialType>& arterials, std::mt19937& gen, std::set<std::pair<int, int>>& footprint, entt::entity chunk_ent) {
        std::map<std::pair<int, int>, int> intersection_check; for (auto const& [pos, type] : arterials) intersection_check[pos]++;
        for (auto const& [pos, count] : intersection_check) if (count >= 2) {
            std::uniform_real_distribution<> dis(0.0, 1.0);
            if (dis(gen) < 0.2) {
                int tw = 10, th = 10, tx = pos.first - 5, ty = pos.second - 5; std::vector<DoorInfo> h_doors = {{pos.first, ty + th - 1, true}, {pos.first, ty, true}, {tx, pos.second, true}, {tx + tw - 1, pos.second, true}};
                createSkyscraperShell("Urban Transit Hub", tx, ty, tw, th, 15, "#00FFFF", ZoneType::TRANSIT, true, h_doors, tx * 10000 + ty, chunk_ent);
                auto hub = m_registry.create(); m_registry.emplace<PositionComponent>(hub, pos.first, pos.second, 0); m_registry.emplace<CommerceHubComponent>(hub, 20.0f, 1.6f);
                for (int fx = tx; fx < tx + tw; ++fx) for (int fy = ty; fy < ty + th; ++fy) footprint.insert({fx, fy});
            }
        }
    }
    void createNatureFeature(int x, int y, std::string name, char glyph, std::string color, bool is_obstacle, TerrainType terrain = TerrainType::GRASS) {
        auto e = m_registry.create(); m_registry.emplace<PositionComponent>(e, x, y, 0); m_registry.emplace<NameComponent>(e, name); m_registry.emplace<RenderableComponent>(e, glyph, color, 0);
        m_registry.emplace<NatureEffectComponent>(e, 2.0f, -1.0f); if (is_obstacle) m_registry.emplace<ObstacleComponent>(e);
        createTile(x, y, 0, terrain, terrain == TerrainType::GRASS ? '"' : '~', terrain == TerrainType::GRASS ? "#004400" : "#0055FF", MaterialType::WATER);
    }
    void createTile(int x, int y, int layer, TerrainType type, char glyph, std::string color, MaterialType material = MaterialType::CONCRETE) {
        auto e = m_registry.create(); m_registry.emplace<PositionComponent>(e, x, y, layer); m_registry.emplace<TerrainComponent>(e, type); m_registry.emplace<RenderableComponent>(e, glyph, color, layer);
        auto& phys = m_registry.emplace<Layer0PhysicsComponent>(e); phys.material = material; 
        if (type == TerrainType::WALL || type == TerrainType::WINDOW) m_registry.emplace<ObstacleComponent>(e);
    }
    void spawnPersonalVehicle(int x, int y, int layer, PersonalVehicleType type) {
        auto v = m_registry.create(); m_registry.emplace<PositionComponent>(v, x, y, layer); m_registry.emplace<NameComponent>(v, "Vehicle"); m_registry.emplace<SizeComponent>(v, 1, 1); m_registry.emplace<TransitOccupantsComponent>(v);
        char glyph = 'V'; std::string color = "#FFFF00"; int capacity = 1; float speed = 1.0f;
        switch(type) {
            case PersonalVehicleType::SCOOTER: glyph = ','; color = "#AAAAAA"; capacity = 1; speed = 1.5f; break;
            case PersonalVehicleType::BIKE: glyph = 'i'; color = "#55AAFF"; capacity = 1; speed = 1.2f; break;
            case PersonalVehicleType::CAR: glyph = 'A'; color = "#FF5555"; capacity = 4; speed = 2.0f; break;
            case PersonalVehicleType::SCI_FI: glyph = 'X'; color = "#FF55FF"; capacity = 2; speed = 3.0f; break;
        }
        m_registry.emplace<RenderableComponent>(v, glyph, color, layer); m_registry.emplace<PersonalVehicleComponent>(v, type, entt::null, capacity, speed); m_registry.emplace<ObstacleComponent>(v);
    }
    std::vector<DoorInfo> calculateDoorPositions(const LotComponent& lot, int bx, int by, int bw, int bh, uint8_t shared_sides, const std::map<std::pair<int, int>, ArterialType>& arterials) {
        std::vector<DoorInfo> doors; auto add_doors_to_wall = [&](StreetFacingSide side, bool is_primary) {
            uint8_t s = static_cast<uint8_t>(side); if (shared_sides & s) return;
            int door_count = 1; if (is_primary && (lot.zone_class == ZoneType::URBAN_CORE || lot.zone_class == ZoneType::MIXED_COMMERCIAL || lot.zone_class == ZoneType::TRANSIT)) {
                if (side == StreetFacingSide::NORTH || side == StreetFacingSide::SOUTH) { if (bw >= 10) door_count = 2; if (bw >= 18) door_count = 3; } else { if (bh >= 10) door_count = 2; if (bh >= 18) door_count = 3; }
            }
            for (int i = 0; i < door_count; ++i) {
                int dx, dy; float fraction = (float)(i + 1) / (float)(door_count + 1); if (side == StreetFacingSide::NORTH) { dx = bx + (int)(bw * fraction); dy = by; }
                else if (side == StreetFacingSide::SOUTH) { dx = bx + (int)(bw * fraction); dy = by + bh - 1; } else if (side == StreetFacingSide::WEST) { dx = bx; dy = by + (int)(bh * fraction); }
                else if (side == StreetFacingSide::EAST) { dx = bx + bw - 1; dy = by + (int)(bh * fraction); } else continue;
                if (validateDoor(dx, dy, arterials)) doors.push_back({dx, dy, is_primary});
                else {
                    bool found = false; for (int offset = 1; offset < std::max(bw, bh) && !found; ++offset) for (int dir = -1; dir <= 1; dir += 2) {
                        int nx = dx, ny = dy; if (side == StreetFacingSide::NORTH || side == StreetFacingSide::SOUTH) nx += offset * dir; else ny += offset * dir;
                        if (nx >= bx && nx < bx + bw && ny >= by && ny < by + bh) if (validateDoor(nx, ny, arterials)) { doors.push_back({nx, ny, is_primary}); found = true; break; }
                    }
                }
            }
        };
        uint8_t f = static_cast<uint8_t>(lot.facing); if (f & (uint8_t)StreetFacingSide::NORTH) add_doors_to_wall(StreetFacingSide::NORTH, true); if (f & (uint8_t)StreetFacingSide::SOUTH) add_doors_to_wall(StreetFacingSide::SOUTH, true);
        if (f & (uint8_t)StreetFacingSide::EAST) add_doors_to_wall(StreetFacingSide::EAST, true); if (f & (uint8_t)StreetFacingSide::WEST) add_doors_to_wall(StreetFacingSide::WEST, true);
        uint8_t af = static_cast<uint8_t>(lot.alley_facing); if (af & (uint8_t)StreetFacingSide::NORTH) add_doors_to_wall(StreetFacingSide::NORTH, false); if (af & (uint8_t)StreetFacingSide::SOUTH) add_doors_to_wall(StreetFacingSide::SOUTH, false);
        if (af & (uint8_t)StreetFacingSide::EAST) add_doors_to_wall(StreetFacingSide::EAST, false); if (af & (uint8_t)StreetFacingSide::WEST) add_doors_to_wall(StreetFacingSide::WEST, false);
        return doors;
    }
    bool validateDoor(int x, int y, const std::map<std::pair<int, int>, ArterialType>& arterials) {
        int dx[] = {0, 0, 1, -1}, dy[] = {1, -1, 0, 0}; for (int i = 0; i < 4; ++i) if (arterials.count({x + dx[i], y + dy[i]})) return true;
        return false;
    }
    uint8_t calculateSharedSides(const LotComponent& lot, const BlockComponent& block) {
        uint8_t shared_sides = 0; for (auto other_l_entity : block.lots) {
            auto* other_ptr = m_registry.try_get<LotComponent>(other_l_entity); if (!other_ptr || &lot == other_ptr) continue;
            const auto& other = *other_ptr; if (lot.x == other.x + other.width - 1) shared_sides |= (uint8_t)StreetFacingSide::WEST;
            if (lot.x + lot.width - 1 == other.x) shared_sides |= (uint8_t)StreetFacingSide::EAST; if (lot.y == other.y + other.height - 1) shared_sides |= (uint8_t)StreetFacingSide::NORTH;
            if (lot.y + lot.height - 1 == other.y) shared_sides |= (uint8_t)StreetFacingSide::SOUTH;
        }
        return shared_sides;
    }
    void createBuildingShell(std::string name, int x, int y, int w, int h, int floors, std::string color, ZoneType ztype, uint8_t facing_sides, uint8_t alley_sides, uint8_t shared_sides, const std::vector<DoorInfo>& doors, uint32_t stable_id, entt::entity chunk_ent) {
        auto building = m_registry.create(); m_registry.emplace<NameComponent>(building, name); m_registry.emplace<PositionComponent>(building, x, y, 0);
        m_registry.emplace<BuildingComponent>(building, floors, ztype, 0, stable_id); m_registry.emplace<SizeComponent>(building, w, h);
        auto& b_phys = m_registry.emplace<Layer0PhysicsComponent>(building); b_phys.material = MaterialType::STEEL; m_registry.emplace<PropertyComponent>(building);
        if (chunk_ent != entt::null) {
            auto& chunk = m_registry.get<ChunkComponent>(chunk_ent); auto it = chunk.building_interiors.find({x, y});
            if (it != chunk.building_interiors.end()) m_registry.emplace<BuildingInteriorComponent>(building, it->second);
        }
        char glyph = (floors > 8) ? 'A' : (floors > 4) ? 'H' : '#'; if (ztype == ZoneType::SLUM) glyph = 'n'; else if (ztype == ZoneType::RESIDENTIAL && floors <= 2) glyph = '='; else if (ztype == ZoneType::MIXED_COMMERCIAL) glyph = 'S';
        for (int cur_x = x; cur_x < x + w; ++cur_x) for (int cur_y = y; cur_y < y + h; ++cur_y) {
            bool is_edge = (cur_x == x || cur_x == x + w - 1 || cur_y == y || cur_y == y + h - 1); bool is_door = false;
            for (const auto& d : doors) if (cur_x == d.x && cur_y == d.y) { is_door = true; break; }
            if (is_edge) {
                if (is_door) {
                    auto door = m_registry.create(); m_registry.emplace<PositionComponent>(door, cur_x, cur_y, 0); m_registry.emplace<RenderableComponent>(door, '+', "#FFFF00", 0);
                    m_registry.emplace<BuildingEntranceComponent>(door, building, 0); createTile(cur_x, cur_y, 0, TerrainType::CONCRETE_FLOOR, '.', "#111111", MaterialType::CONCRETE);
                } else {
                    bool is_shared = false; if (cur_x == x && (shared_sides & (uint8_t)StreetFacingSide::WEST)) is_shared = true; if (cur_x == x + w - 1 && (shared_sides & (uint8_t)StreetFacingSide::EAST)) is_shared = true;
                    if (cur_y == y && (shared_sides & (uint8_t)StreetFacingSide::NORTH)) is_shared = true; if (cur_y == y + h - 1 && (shared_sides & (uint8_t)StreetFacingSide::SOUTH)) is_shared = true;
                    
                    bool is_street = false;
                    if (cur_x == x && (facing_sides & (uint8_t)StreetFacingSide::WEST)) is_street = true;
                    if (cur_x == x + w - 1 && (facing_sides & (uint8_t)StreetFacingSide::EAST)) is_street = true;
                    if (cur_y == y && (facing_sides & (uint8_t)StreetFacingSide::NORTH)) is_street = true;
                    if (cur_y == y + h - 1 && (facing_sides & (uint8_t)StreetFacingSide::SOUTH)) is_street = true;

                    if (is_shared) createTile(cur_x, cur_y, 0, TerrainType::WALL, glyph, color, MaterialType::CONCRETE);
                    else {
                        bool place_window = false;
                        if (is_street) {
                            if (ztype == ZoneType::RESIDENTIAL) {
                                if (cur_x % 2 == 0 || cur_y % 2 == 0) place_window = true;
                            } else if (ztype == ZoneType::CORPORATE) {
                                // "floor-to-ceiling"
                                if (cur_x % 3 != 0 || cur_y % 3 != 0) place_window = true;
                            } else if (ztype == ZoneType::INDUSTRIAL) {
                                // "server room" -> none
                                place_window = false;
                            } else if (ztype == ZoneType::MIXED_COMMERCIAL) {
                                if (cur_x % 3 == 0 || cur_y % 3 == 0) place_window = true;
                            } else if (ztype == ZoneType::SLUM) {
                                if (cur_x % 4 == 0 || cur_y % 4 == 0) place_window = true;
                            }
                        }

                        if (place_window) createTile(cur_x, cur_y, 0, TerrainType::WINDOW, 'o', "#00FFFF", MaterialType::GLASS);
                        else createTile(cur_x, cur_y, 0, TerrainType::WALL, glyph, color, MaterialType::CONCRETE);
                    }
                }
            } else createTile(cur_x, cur_y, 0, TerrainType::CONCRETE_FLOOR, '.', "#111111", MaterialType::CONCRETE);
        }
    }
    entt::registry& m_registry; entt::dispatcher& m_dispatcher;
};
} // namespace NeonOubliette
#endif
