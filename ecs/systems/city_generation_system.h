#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_CITY_GENERATION_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_CITY_GENERATION_SYSTEM_H

#include <entt/entt.hpp>
#include "../components/components.h"
#include "../components/zoning_components.h"
#include "../components/simulation_layers.h"
#include "../components/infrastructure_components.h"
#include "../components/lod_components.h"
#include "../components/broadcast_components.h"
#include "../system_scheduler.h"
#include <string>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
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
        : m_registry(registry), m_dispatcher(dispatcher) {
        // Pre-populate tile index from any existing terrain entities for O(1) duplicate checks
        auto view = m_registry.view<PositionComponent, TerrainComponent>();
        for (auto ent : view) {
            const auto& p = view.get<PositionComponent>(ent);
            m_tile_index[tile_key(p.x, p.y, p.layer_id)] = ent;
        }
    }

    void initialize() override {}
    void update(double delta_time) override { (void)delta_time; }

    void generate_chunk_content(entt::entity zone_entity);

private:
    void spawnFactoryExtras(const MacroZoneComponent& zone, int cell_size, std::mt19937& gen);
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
                        bool n_road = isRoad(bx, by - ROAD_WIDTH_SECONDARY / 2 - 1, bw, 1, arterials);
                        bool s_road = isRoad(bx, by + bh + ROAD_WIDTH_SECONDARY / 2, bw, 1, arterials);
                        bool w_road = isRoad(bx - ROAD_WIDTH_SECONDARY / 2 - 1, by, 1, bh, arterials);
                        bool e_road = isRoad(bx + bw + ROAD_WIDTH_SECONDARY / 2, by, 1, bh, arterials);
                        if (n_road) { for (int x = bx; x < bx + bw; ++x) for (int sw = 0; sw < SIDEWALK_WIDTH; ++sw) arterials[{x, by + sw}] = ArterialType::SIDEWALK; by += SIDEWALK_WIDTH; bh -= SIDEWALK_WIDTH; }
                        if (s_road) { for (int x = bx; x < bx + bw; ++x) for (int sw = 0; sw < SIDEWALK_WIDTH; ++sw) arterials[{x, by + bh - 1 - sw}] = ArterialType::SIDEWALK; bh -= SIDEWALK_WIDTH; }
                        if (w_road) { for (int y = by; y < by + bh; ++y) for (int sw = 0; sw < SIDEWALK_WIDTH; ++sw) arterials[{bx + sw, y}] = ArterialType::SIDEWALK; bx += SIDEWALK_WIDTH; bw -= SIDEWALK_WIDTH; }
                        if (e_road) { for (int y = by; y < by + bh; ++y) for (int sw = 0; sw < SIDEWALK_WIDTH; ++sw) arterials[{bx + bw - 1 - sw, y}] = ArterialType::SIDEWALK; bw -= SIDEWALK_WIDTH; }
                        if (bw > 1 && bh > 1) {
                            std::string b_name = "Building"; std::string b_color = "#778899"; int floors = 1;
                            
                            std::uniform_real_distribution<> rel_dis(0.0, 1.0);
                            bool placed_religious = false;
                            if (rel_dis(gen) < 0.05) { // 5% chance
                                ReligionRecord* rel = pickReligionForZone(zone.type, gen);
                                if (rel) {
                                    uint8_t shared_sides = calculateSharedSides(lot, block);
                                    auto doors = calculateDoorPositions(lot, bx, by, bw, bh, shared_sides, arterials);
                                    uint32_t stable_id = static_cast<uint32_t>(bx * 10000 + by);
                                    const_cast<LotComponent&>(lot).building_entity = createReligiousBuildingShell(rel, bx, by, bw, bh, doors, stable_id, chunk_ent);
                                    placed_religious = true;
                                    for (int fx = bx; fx < bx + bw; ++fx) {
                                        for (int fy = by; fy < by + bh; ++fy) {
                                            if (arterials.count({fx, fy})) continue;
                                            structure_footprint.insert({fx, fy});
                                        }
                                    }
                                    for (const auto& d : doors) if (d.primary) generateAccessPath(d.x, d.y, arterials);
                                }
                            }

                            if (!placed_religious) {
                                switch(zone.type) {
                                    case ZoneType::CORPORATE: b_name = "Highrise"; b_color = "#4488FF"; floors = 10; break;
                                    case ZoneType::SLUM: b_name = "Shanty"; b_color = "#CC7733"; floors = 1; break;
                                    case ZoneType::INDUSTRIAL: b_name = "Plant"; b_color = "#DD4422"; floors = 2; break;
                                    case ZoneType::RESIDENTIAL: 
                                        b_name = "Apartments"; b_color = "#33AA55"; floors = std::max(1, 8 - (int)dist_to_core);
                                        if (floors <= 2) b_name = "Row-house";
                                        break;
                                    default: break;
                                }
                                uint8_t shared_sides = calculateSharedSides(lot, block);
                                auto doors = calculateDoorPositions(lot, bx, by, bw, bh, shared_sides, arterials);
                                uint32_t stable_id = static_cast<uint32_t>(bx * 10000 + by);
                                const_cast<LotComponent&>(lot).building_entity = createBuildingShell(b_name, bx, by, bw, bh, floors, b_color, zone.type, (uint8_t)lot.facing, (uint8_t)lot.alley_facing, shared_sides, doors, stable_id, chunk_ent, gen);
                                for (int fx = bx; fx < bx + bw; ++fx) {
                                    for (int fy = by; fy < by + bh; ++fy) {
                                        if (arterials.count({fx, fy})) continue; // Skip if arterial present
                                        structure_footprint.insert({fx, fy});
                                    }
                                }
                                for (const auto& d : doors) if (d.primary) generateAccessPath(d.x, d.y, arterials);
                            }
                        }
                    }
                }
            }
        }
        for (int x = start_x; x < start_x + cell_size; ++x) {
            for (int y = start_y; y < start_y + cell_size; ++y) {
                if (structure_footprint.count({x, y}) || arterials.count({x, y})) continue;
                TerrainType t_type = TerrainType::GRASS; char glyph = '"'; std::string color = "#2D7044";
                switch(zone.type) {
                    case ZoneType::URBAN_CORE: case ZoneType::CORPORATE: t_type = TerrainType::SIDEWALK; glyph = '.'; color = "#1A3A5C"; break;
                    case ZoneType::SLUM: t_type = TerrainType::DIRT; glyph = '\''; color = "#7A4422"; break;
                    case ZoneType::INDUSTRIAL: t_type = TerrainType::CONCRETE_FLOOR; glyph = '.'; color = "#5A3520"; break;
                    case ZoneType::RESIDENTIAL: t_type = TerrainType::GRASS; glyph = '"'; color = "#2D7044"; break;
                    case ZoneType::TRANSIT: t_type = TerrainType::STREET; glyph = '.'; color = "#2A4060"; break;
                    case ZoneType::AIRPORT: t_type = TerrainType::CONCRETE_FLOOR; glyph = '.'; color = "#3A5070"; break;
                    case ZoneType::PARK: t_type = TerrainType::GRASS; glyph = '"'; color = "#3A8840"; break;
                    case ZoneType::COLOSSEUM: t_type = TerrainType::ARENA_FLOOR; glyph = '.'; color = "#6A1510"; break;
                    case ZoneType::MIXED_COMMERCIAL: t_type = TerrainType::SIDEWALK; glyph = '.'; color = "#502060"; break;
                    default: break;
                }
                createTile(x, y, 0, t_type, glyph, color, MaterialType::CONCRETE);

                // Layer -1 (Underground) filling: default to Rock/Wall if not an arterial
                bool is_underground_arterial = false;
                auto it = arterials.find({x, y});
                if (it != arterials.end() && (it->second == ArterialType::SEWER || it->second == ArterialType::UNDERGROUND_TUNNEL)) {
                    is_underground_arterial = true;
                }
                
                if (!is_underground_arterial) {
                    createTile(x, y, -1, TerrainType::WALL, '#', "#2A2A2A", MaterialType::CONCRETE);
                }
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
        placeTrainTerminals(zone, gen, structure_footprint, chunk_ent);
        
        // [N.2] Random Broadcast Tower Placement in Urban Core
        std::uniform_real_distribution<> tower_dis(0.0, 1.0);
        if (tower_dis(gen) < 0.15) { // 15% chance per Urban Core macro cell
            int tx = zone.macro_x * cell_size + 5 + (int)(tower_dis(gen) * (cell_size - 10));
            int ty = zone.macro_y * cell_size + 5 + (int)(tower_dis(gen) * (cell_size - 10));
            
            // Ensure not on an arterial or existing structure
            if (!arterials.count({tx, ty}) && !structure_footprint.count({tx, ty})) {
                // Find a faction to assign
                entt::entity faction = entt::null;
                auto faction_view = m_registry.view<FactionComponent>();
                if (!faction_view.empty()) {
                    std::uniform_int_distribution<size_t> f_dist(0, faction_view.size() - 1);
                    auto it = faction_view.begin();
                    std::advance(it, f_dist(gen));
                    faction = *it;
                }
                
                spawnBroadcastTower(tx, ty, 0, faction);
                structure_footprint.insert({tx, ty});
            }
        }

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
                    const_cast<LotComponent&>(lot).building_entity = createSkyscraperShell(name, bx, by, bw, bh, floors, color, ZoneType::URBAN_CORE, (uint8_t)lot.facing, (uint8_t)lot.alley_facing, shared_sides, doors, stable_id, chunk_ent, gen);
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
        createBuildingShell("Terminal", tx, ty, tw, th, 3, "#FFFF55", ZoneType::AIRPORT, 0, 0, 0, t_doors, tx * 10000 + ty, chunk_ent, gen);
        generateAccessPath(tx + (tw/2), ty + th - 1, arterials);
        createBuildingShell("Control Tower", start_x + 2, start_y + 2, 4, 4, 10, "#AAAAFF", ZoneType::AIRPORT, 0, 0, 0, {{start_x + 4, start_y + 5, true}}, (start_x+2)*10000+(start_y+2), chunk_ent, gen);
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
                else if (roll < 0.14) { // 1% chance for a small shrine in the park
                    ReligionRecord* rel = pickReligionForZone(ZoneType::PARK, gen);
                    if (rel) {
                        createShrine(x, y, rel);
                        footprint.insert({x, y});
                    }
                }
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
        createBuildingShell("Colosseum Grand Entrance", center_x - 4, start_y + 2, 8, 6, 2, "#FFCC00", ZoneType::COLOSSEUM, 0, 0, 0, {{center_x, start_y + 7, true}}, (center_x-4)*10000+(start_y+2), chunk_ent, gen);
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
                        const_cast<LotComponent&>(lot).building_entity = createBuildingShell("Shop", bx, by, bw, bh, 1 + (int)(dis(gen) * 2), "#FF00FF", ZoneType::MIXED_COMMERCIAL, (uint8_t)lot.facing, (uint8_t)lot.alley_facing, shared_sides, doors, stable_id, chunk_ent, gen);
                        for (int fx = bx; fx < bx + bw; ++fx) for (int fy = by; fy < by + bh; ++fy) footprint.insert({fx, fy});
                        spawnVendor(bx + (bw/2), by + (bh/2), 0, "Vendor " + std::to_string(bx));
                        for (const auto& d : doors) if (d.primary) generateAccessPath(d.x, d.y, arterials);
                    } else if (bw >= 1 && bh >= 1) { createKiosk(bx, by, "Kiosk", "#FFFF00"); footprint.insert({bx, by}); }
                }
            }
        }
    }
    public: void generateAccessPath(int door_x, int door_y, std::map<std::pair<int, int>, ArterialType>& arterials) {
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
        auto e = m_registry.create(); m_registry.emplace<PositionComponent>(e, x, y, layer); m_registry.emplace<NameComponent>(e, name); m_registry.emplace<RenderableComponent>(e, 'V', "#FFAA33", layer); m_registry.emplace<AgentComponent>(e);
    }
    public: entt::entity createSkyscraperShell(std::string name, int x, int y, int w, int h, int floors, std::string color, ZoneType ztype, uint8_t facing_sides, uint8_t alley_sides, uint8_t shared_sides, const std::vector<DoorInfo>& doors, uint32_t stable_id, entt::entity chunk_ent, std::mt19937& gen) {
        auto building = m_registry.create(); m_registry.emplace<NameComponent>(building, name); m_registry.emplace<PositionComponent>(building, x, y, 0);
        m_registry.emplace<BuildingComponent>(building, floors, ztype, 0, stable_id); m_registry.emplace<SizeComponent>(building, w, h); m_registry.emplace<PropertyComponent>(building);
        m_registry.emplace<ObstacleComponent>(building); // [B.1] Add ObstacleComponent to the entire footprint
        m_registry.emplace<BuildingHealthComponent>(building); // [K.1] Physical integrity tracking
        auto& health = m_registry.get<BuildingHealthComponent>(building);
        health.maintenance_budget = 1000.0f; // [K.2] Highrise initial budget
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
                    m_registry.emplace<BuildingEntranceComponent>(door, building, 0);
                    
                    // [B.1] Store Door Metadata
                    auto& meta = m_registry.emplace<DoorMetadataComponent>(door);
                    meta.building_entity = building;
                    if (cur_y == y) { meta.wall_side = StreetFacingSide::NORTH; meta.wall_offset = cur_x - x; }
                    else if (cur_y == y + h - 1) { meta.wall_side = StreetFacingSide::SOUTH; meta.wall_offset = cur_x - x; }
                    else if (cur_x == x) { meta.wall_side = StreetFacingSide::WEST; meta.wall_offset = cur_y - y; }
                    else if (cur_x == x + w - 1) { meta.wall_side = StreetFacingSide::EAST; meta.wall_offset = cur_y - y; }

                    createTile(cur_x, cur_y, 0, TerrainType::CONCRETE_FLOOR, '.', "#111111", MaterialType::CONCRETE);
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
        return building;
    }

    void placeTrainTerminals(const MacroZoneComponent& zone, std::mt19937& gen, std::set<std::pair<int, int>>& footprint, entt::entity chunk_ent) {
        auto config_view = m_registry.view<WorldConfigComponent>();
        if (config_view.empty()) return;
        auto& config = config_view.get<WorldConfigComponent>(config_view.front());
        int cell_size = config.macro_cell_size;

        int start_x = zone.macro_x * cell_size, start_y = zone.macro_y * cell_size;
        int end_x = start_x + cell_size, end_y = start_y + cell_size;

        auto node_view = m_registry.view<PositionComponent, InfrastructureNodeComponent>();
        for (auto node_ent : node_view) {
            const auto& pos = node_view.get<PositionComponent>(node_ent);
            if (pos.x >= start_x && pos.x < end_x && pos.y >= start_y && pos.y < end_y && pos.layer_id == 0) {
                const auto& node = node_view.get<InfrastructureNodeComponent>(node_ent);
                if (node.node_name == "Primary Intersection") {
                    std::uniform_real_distribution<> dis(0.0, 1.0);
                    if (dis(gen) < 0.25) { // 25% chance
                        int tw = 20, th = 20, tx = pos.x - 10, ty = pos.y - 10;
                        std::vector<DoorInfo> h_doors = {{pos.x, ty + th - 1, true}, {pos.x, ty, true}, {tx, pos.y, true}, {tx + tw - 1, pos.y, true}};
                        createSkyscraperShell("Urban Transit Hub", tx, ty, tw, th, 15, "#00FFFF", ZoneType::TRANSIT, (uint8_t)StreetFacingSide::ALL, 0, 0, h_doors, tx * 10000 + ty, chunk_ent, gen);
                        auto hub = m_registry.create(); m_registry.emplace<PositionComponent>(hub, pos.x, pos.y, 0); m_registry.emplace<CommerceHubComponent>(hub, 40.0f, 1.8f);
                        for (int fx = tx; fx < tx + tw; ++fx) for (int fy = ty; fy < ty + th; ++fy) footprint.insert({fx, fy});
                    }
                }
            }
        }
    }
    void createNatureFeature(int x, int y, std::string name, char glyph, std::string color, bool is_obstacle, TerrainType terrain = TerrainType::GRASS) {
        auto e = m_registry.create(); m_registry.emplace<PositionComponent>(e, x, y, 0); m_registry.emplace<NameComponent>(e, name); m_registry.emplace<RenderableComponent>(e, glyph, color, 0);
        m_registry.emplace<NatureEffectComponent>(e, 2.0f, -1.0f); if (is_obstacle) m_registry.emplace<ObstacleComponent>(e);
        createTile(x, y, 0, terrain, terrain == TerrainType::GRASS ? '"' : '~', terrain == TerrainType::GRASS ? "#004400" : "#0055FF", terrain == TerrainType::GRASS ? MaterialType::CONCRETE : MaterialType::WATER);
    }

    /**
     * @brief [NEW] Spawns a Broadcast Tower entity at the specified location.
     */
    void spawnBroadcastTower(int x, int y, int layer, entt::entity faction) {
        auto tower = m_registry.create();
        m_registry.emplace<PositionComponent>(tower, x, y, layer);
        m_registry.emplace<NameComponent>(tower, "Broadcast Tower");
        
        // Visual representation (as per vision_artist spec)
        m_registry.emplace<RenderableComponent>(tower, '^', "#AAAAAA", layer); 
        m_registry.emplace<ObstacleComponent>(tower);
        
        auto& broadcast = m_registry.emplace<BroadcastTowerComponent>(tower);
        broadcast.controlling_faction = faction;
        broadcast.radius = 40;
        broadcast.broadcast_interval = 25;

        // Create an Information entity for this tower's active message
        auto info_ent = m_registry.create();
        auto& info = m_registry.emplace<InformationComponent>(info_ent);
        
        InformationRecord record;
        record.type = InformationType::RUMOR;
        record.content_tag = "URBAN_CORE_STABILITY";
        record.source_faction = faction;
        record.veracity = 1.0f;
        info.records.push_back(record);
        
        broadcast.active_information_entity = info_ent;

        // Add a base tile
        createTile(x, y, layer, TerrainType::WALL, 'X', "#888888", MaterialType::STEEL);
    }

    public: void createTile(int x, int y, int layer, TerrainType type, char glyph, std::string color, MaterialType material = MaterialType::CONCRETE) {
        // [MOD] O(1) duplicate check via spatial index (was O(N) entity scan causing O(N²) freeze)
        uint64_t key = tile_key(x, y, layer);
        auto it = m_tile_index.find(key);
        if (it != m_tile_index.end()) {
            auto ent = it->second;
            if (m_registry.valid(ent) && m_registry.all_of<TerrainComponent>(ent)) {
                const auto& terr = m_registry.get<TerrainComponent>(ent);
                // Keep rails on L5 and Sewer tiles on L-1
                if (layer == 5 && terr.type == TerrainType::RAIL) return;
                if (layer == -1 && (terr.type == TerrainType::SEWER_FLOOR || terr.type == TerrainType::SEWER_WATER)) return;
            }
        }

        auto e = m_registry.create(); m_registry.emplace<PositionComponent>(e, x, y, layer); m_registry.emplace<TerrainComponent>(e, type); m_registry.emplace<RenderableComponent>(e, glyph, color, layer);
        auto& phys = m_registry.emplace<Layer0PhysicsComponent>(e); phys.material = material; 
        if (type == TerrainType::WALL || type == TerrainType::WINDOW) m_registry.emplace<ObstacleComponent>(e);
        m_tile_index[key] = e;
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

    void spawnUndergroundMedia(const MacroZoneComponent& zone, std::mt19937& gen) {
        // [N.3] Clandestine spawn logic
        std::uniform_real_distribution<> dis(0.0, 1.0);
        int cell_size = 20; // Default or read from config
        
        // 1. Pirate Nodes in Sewers or Slums
        if (zone.type == ZoneType::SLUM) {
             if (dis(gen) < 0.15) { // 15% chance per slum cell
                int tx = zone.macro_x * cell_size + (int)(dis(gen) * cell_size);
                int ty = zone.macro_y * cell_size + (int)(dis(gen) * cell_size);
                
                auto node = m_registry.create();
                m_registry.emplace<PositionComponent>(node, tx, ty, 0);
                m_registry.emplace<NameComponent>(node, "Pirate Node");
                m_registry.emplace<RenderableComponent>(node, '!', "#FF00FF", 0);
                
                auto& pirate = m_registry.emplace<PirateNodeComponent>(node);
                pirate.radius = 12;
                pirate.is_hidden = true;
                
                // Assign a rumor
                auto info_ent = m_registry.create();
                auto& info = m_registry.emplace<InformationComponent>(info_ent);
                InformationRecord record;
                record.content_tag = "SLUM_REBELLION_RUMOR";
                record.veracity = 0.6f;
                info.records.push_back(record);
                pirate.active_information_entity = info_ent;
             }
        }
        
        // 2. Data Slabs as random loot
        if (dis(gen) < 0.2) { // 20% chance per macro cell
             int tx = zone.macro_x * cell_size + (int)(dis(gen) * cell_size);
             int ty = zone.macro_y * cell_size + (int)(dis(gen) * cell_size);
             
             auto slab = m_registry.create();
             m_registry.emplace<PositionComponent>(slab, tx, ty, 0);
             m_registry.emplace<NameComponent>(slab, "Encrypted Data Slab");
             m_registry.emplace<RenderableComponent>(slab, '[', "#00FFFF", 0);
             m_registry.emplace<ItemComponent>(slab, 500, "Data Slab");
             m_registry.emplace<ItemMarketCategoryComponent>(slab, ItemMarketCategory::TECHNOLOGY);
             m_registry.emplace<ItemMaterialComponent>(slab, RawMaterialType::ELECTRONIC);
             
             auto& slab_comp = m_registry.emplace<DataSlabComponent>(slab);
             slab_comp.stored_record.content_tag = "SECRET_SYNDICATE_FREQUENCY";
             slab_comp.stored_record.veracity = 1.0f;
             slab_comp.is_encrypted = (dis(gen) < 0.3);
        }
    }
    public: std::vector<DoorInfo> calculateDoorPositions(const LotComponent& lot, int bx, int by, int bw, int bh, uint8_t shared_sides, const std::map<std::pair<int, int>, ArterialType>& arterials) {
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
    public: uint8_t calculateSharedSides(const LotComponent& lot, const BlockComponent& block) {
        uint8_t shared_sides = 0; for (auto other_l_entity : block.lots) {
            auto* other_ptr = m_registry.try_get<LotComponent>(other_l_entity); if (!other_ptr || &lot == other_ptr) continue;
            const auto& other = *other_ptr; if (lot.x == other.x + other.width - 1) shared_sides |= (uint8_t)StreetFacingSide::WEST;
            if (lot.x + lot.width - 1 == other.x) shared_sides |= (uint8_t)StreetFacingSide::EAST; if (lot.y == other.y + other.height - 1) shared_sides |= (uint8_t)StreetFacingSide::NORTH;
            if (lot.y + lot.height - 1 == other.y) shared_sides |= (uint8_t)StreetFacingSide::SOUTH;
        }
        return shared_sides;
    }
    public: entt::entity createBuildingShell(std::string name, int x, int y, int w, int h, int floors, std::string color, ZoneType ztype, uint8_t facing_sides, uint8_t alley_sides, uint8_t shared_sides, const std::vector<DoorInfo>& doors, uint32_t stable_id, entt::entity chunk_ent, std::mt19937& gen) {
        auto building = m_registry.create(); m_registry.emplace<NameComponent>(building, name); m_registry.emplace<PositionComponent>(building, x, y, 0);
        m_registry.emplace<BuildingComponent>(building, floors, ztype, 0, stable_id); m_registry.emplace<SizeComponent>(building, w, h);
        m_registry.emplace<ObstacleComponent>(building); // [B.1] Add ObstacleComponent to the entire footprint
        m_registry.emplace<BuildingHealthComponent>(building); // [K.1] Physical integrity tracking
        auto& health = m_registry.get<BuildingHealthComponent>(building);
        switch(ztype) {
            case ZoneType::CORPORATE: health.maintenance_budget = 500.0f; break;
            case ZoneType::INDUSTRIAL: health.maintenance_budget = 300.0f; break;
            case ZoneType::RESIDENTIAL: health.maintenance_budget = 150.0f; break;
            case ZoneType::SLUM: health.maintenance_budget = 20.0f; break;
            default: health.maintenance_budget = 100.0f; break;
        }
        
        // [I.4] Clandestine Lab designation
        if (ztype == ZoneType::INDUSTRIAL || ztype == ZoneType::SLUM) {
            std::uniform_real_distribution<> lab_dis(0.0, 1.0);
            if (lab_dis(gen) < 0.15) { // 15% chance
                auto& lab = m_registry.emplace<ClandestineLabComponent>(building);
                lab.raw_chemicals_stored = 5; // Starting stock
                lab.max_raw_chemicals = 20;
                lab.drugs_produced_stored = 0;
                lab.max_drugs_produced = 20;
                lab.production_rate = 0.05f;
            }
        }

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
                    m_registry.emplace<BuildingEntranceComponent>(door, building, 0);
                    
                    // [B.1] Store Door Metadata
                    auto& meta = m_registry.emplace<DoorMetadataComponent>(door);
                    meta.building_entity = building;
                    if (cur_y == y) { meta.wall_side = StreetFacingSide::NORTH; meta.wall_offset = cur_x - x; }
                    else if (cur_y == y + h - 1) { meta.wall_side = StreetFacingSide::SOUTH; meta.wall_offset = cur_x - x; }
                    else if (cur_x == x) { meta.wall_side = StreetFacingSide::WEST; meta.wall_offset = cur_y - y; }
                    else if (cur_x == x + w - 1) { meta.wall_side = StreetFacingSide::EAST; meta.wall_offset = cur_y - y; }

                    createTile(cur_x, cur_y, 0, TerrainType::CONCRETE_FLOOR, '.', "#1A1A1A", MaterialType::CONCRETE);
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
            } else createTile(cur_x, cur_y, 0, TerrainType::CONCRETE_FLOOR, '.', "#1A1A1A", MaterialType::CONCRETE);
        }
        return building;
    }

    ReligionRecord* pickReligionForZone(ZoneType zone, std::mt19937& gen) {
        auto* registry_ptr = m_registry.ctx().find<ReligionRegistryComponent>();
        if (!registry_ptr || registry_ptr->religions.empty()) return nullptr;
        
        std::vector<ReligionRecord*> pool;
        for (auto& [id, rel] : registry_ptr->religions) {
            if (zone == ZoneType::URBAN_CORE || zone == ZoneType::CORPORATE) {
                if (rel.home_building_type == "TEMPLE" || rel.home_building_type == "BROADCAST_TOWER") pool.push_back(&rel);
            } else if (zone == ZoneType::SLUM || zone == ZoneType::INDUSTRIAL) {
                if (rel.home_building_type == "UNDERGROUND_CHAPEL") pool.push_back(&rel);
            } else if (zone == ZoneType::PARK) {
                if (rel.home_building_type == "SHRINE") pool.push_back(&rel);
            }
        }
        
        if (pool.empty()) return nullptr;
        std::uniform_int_distribution<size_t> dist(0, pool.size() - 1);
        return pool[dist(gen)];
    }

    entt::entity createReligiousBuildingShell(const ReligionRecord* rel, int x, int y, int w, int h, const std::vector<DoorInfo>& doors, uint32_t stable_id, entt::entity chunk_ent) {
        auto building = m_registry.create();
        m_registry.emplace<NameComponent>(building, rel->name + " " + rel->home_building_type);
        m_registry.emplace<PositionComponent>(building, x, y, 0);
        int floors = (rel->home_building_type == "TEMPLE") ? 4 : 2;
        m_registry.emplace<BuildingComponent>(building, floors, ZoneType::RELIGIOUS, 0, stable_id);
        m_registry.emplace<SizeComponent>(building, w, h);
        m_registry.emplace<ObstacleComponent>(building);
        m_registry.emplace<PropertyComponent>(building);
        m_registry.emplace<WorshipPlaceComponent>(building, rel->religion_id, 20); // [H.3]
        auto& health = m_registry.emplace<BuildingHealthComponent>(building); // [K.1]
        health.maintenance_budget = 400.0f; // Religious endowment
        
        if (chunk_ent != entt::null) {
            auto& chunk = m_registry.get<ChunkComponent>(chunk_ent);
            auto it = chunk.building_interiors.find({x, y});
            if (it != chunk.building_interiors.end()) m_registry.emplace<BuildingInteriorComponent>(building, it->second);
        }

        std::string b_color = rel->color_hex;
        char b_glyph = rel->glyph;

        for (int cur_x = x; cur_x < x + w; ++cur_x) {
            for (int cur_y = y; cur_y < y + h; ++cur_y) {
                bool is_edge = (cur_x == x || cur_x == x + w - 1 || cur_y == y || cur_y == y + h - 1);
                bool is_door = false;
                for (const auto& d : doors) if (cur_x == d.x && cur_y == d.y) { is_door = true; break; }
                
                if (is_edge) {
                    if (is_door) {
                        auto door = m_registry.create();
                        m_registry.emplace<PositionComponent>(door, cur_x, cur_y, 0);
                        m_registry.emplace<RenderableComponent>(door, '+', "#FFFF00", 0);
                        m_registry.emplace<BuildingEntranceComponent>(door, building, 0);
                        auto& meta = m_registry.emplace<DoorMetadataComponent>(door);
                        meta.building_entity = building;
                        if (cur_y == y) { meta.wall_side = StreetFacingSide::NORTH; meta.wall_offset = cur_x - x; }
                        else if (cur_y == y + h - 1) { meta.wall_side = StreetFacingSide::SOUTH; meta.wall_offset = cur_x - x; }
                        else if (cur_x == x) { meta.wall_side = StreetFacingSide::WEST; meta.wall_offset = cur_y - y; }
                        else if (cur_x == x + w - 1) { meta.wall_side = StreetFacingSide::EAST; meta.wall_offset = cur_y - y; }
                        createTile(cur_x, cur_y, 0, TerrainType::CONCRETE_FLOOR, '.', "#1A1A1A", MaterialType::CONCRETE);
                    } else {
                        createTile(cur_x, cur_y, 0, TerrainType::WALL, b_glyph, b_color, MaterialType::CONCRETE);
                    }
                } else {
                    createTile(cur_x, cur_y, 0, TerrainType::CONCRETE_FLOOR, '.', "#1A1A1A", MaterialType::CONCRETE);
                }
            }
        }
        return building;
    }


    void createShrine(int x, int y, const ReligionRecord* rel) {
        auto e = m_registry.create();
        m_registry.emplace<PositionComponent>(e, x, y, 0);
        m_registry.emplace<NameComponent>(e, rel->name + " Shrine");
        m_registry.emplace<RenderableComponent>(e, rel->glyph, rel->color_hex, 0);
        m_registry.emplace<ObstacleComponent>(e);
        m_registry.emplace<BuildingComponent>(e, 1, ZoneType::RELIGIOUS, 0, static_cast<uint32_t>(x * 10000 + y));
        m_registry.emplace<WorshipPlaceComponent>(e, rel->religion_id, 5);
        createTile(x, y, 0, TerrainType::CONCRETE_FLOOR, '.', "#1A1A1A", MaterialType::CONCRETE);
    }

    void generateResourceFields(const MacroZoneComponent& zone, int cell_size, std::mt19937& gen, entt::entity chunk_ent) {
        if (chunk_ent == entt::null) return;
        auto& field = m_registry.emplace_or_replace<RawMaterialFieldComponent>(chunk_ent);
        
        switch(zone.type) {
            case ZoneType::INDUSTRIAL:
                field.concentrations[RawMaterialType::METAL] = 0.8f;
                field.concentrations[RawMaterialType::CHEMICAL] = 0.6f;
                field.regen_rates[RawMaterialType::METAL] = 0.01f;
                break;
            case ZoneType::SLUM:
                field.concentrations[RawMaterialType::METAL] = 0.4f;
                field.concentrations[RawMaterialType::BIOMASS] = 0.3f;
                break;
            case ZoneType::PARK:
                field.concentrations[RawMaterialType::BIOMASS] = 0.9f;
                field.regen_rates[RawMaterialType::BIOMASS] = 0.05f;
                break;
            case ZoneType::CORPORATE:
                field.concentrations[RawMaterialType::ELECTRONIC] = 0.7f;
                field.concentrations[RawMaterialType::ENERGY] = 0.8f;
                break;
            default:
                field.concentrations[RawMaterialType::METAL] = 0.1f;
                break;
        }

        std::uniform_real_distribution<> noise(-0.1, 0.1);
        for (auto& [type, conc] : field.concentrations) {
             conc = std::clamp(conc + (float)noise(gen), 0.0f, 1.0f);
        }

        int start_x = zone.macro_x * cell_size, start_y = zone.macro_y * cell_size;
        for (auto const& [type, conc] : field.concentrations) {
            if (conc > 0.4f) {
                int num_nodes = 1 + (int)(conc * 4);
                for (int i = 0; i < num_nodes; ++i) {
                    int nx = start_x + (gen() % cell_size), ny = start_y + (gen() % cell_size);
                    spawnResourceNode(nx, ny, 0, type, gen);
                }
            }
        }
    }

    void spawnResourceNode(int x, int y, int layer, RawMaterialType type, std::mt19937& gen) {
        auto view = m_registry.view<PositionComponent, ObstacleComponent>();
        for (auto ent : view) { 
            const auto& p = view.get<PositionComponent>(ent); 
            if (p.x == x && p.y == y && p.layer_id == layer) return; 
        }

        auto node = m_registry.create();
        m_registry.emplace<PositionComponent>(node, x, y, layer);
        char glyph = '%'; std::string color = "#FFFFFF"; std::string name = "Resource";
        switch(type) {
            case RawMaterialType::METAL: glyph = '%'; color = "#AAAAAA"; name = "Scrap Heap"; break;
            case RawMaterialType::CHEMICAL: glyph = '!'; color = "#00FF00"; name = "Chemical Barrels"; break;
            case RawMaterialType::BIOMASS: glyph = '"'; color = "#2D7044"; name = "Vat Growth"; break;
            case RawMaterialType::ELECTRONIC: glyph = '&'; color = "#00FFFF"; name = "Electronic Salvage"; break;
            case RawMaterialType::ENERGY: glyph = '*'; color = "#FFFF00"; name = "Energy Cell"; break;
        }
        m_registry.emplace<RenderableComponent>(node, glyph, color, layer);
        m_registry.emplace<NameComponent>(node, name);
        m_registry.emplace<ResourceNodeComponent>(node, type, 1.0f, 0.01f, false);
        m_registry.emplace<ObstacleComponent>(node);
        uint32_t item_id = 1001 + (uint32_t)type; 
        m_registry.emplace<HarvestableComponent>(node, item_id, 1, 5, 50, WorkstationType::NONE);
    }

    entt::registry& m_registry; entt::dispatcher& m_dispatcher;

    // Spatial index for O(1) tile-position duplicate checking (replaces O(N) entity scan)
    std::unordered_map<uint64_t, entt::entity> m_tile_index;

    static uint64_t tile_key(int x, int y, int layer) {
        // Pack (x, y, layer) into a single uint64. Supports coords up to ~16M and layers -32768..32767.
        uint64_t ux = static_cast<uint64_t>(static_cast<unsigned int>(x));
        uint64_t uy = static_cast<uint64_t>(static_cast<unsigned int>(y));
        uint64_t ul = static_cast<uint64_t>(static_cast<unsigned short>(static_cast<short>(layer)));
        return (ux << 40) | (uy << 16) | ul;
    }
};
} // namespace NeonOubliette
#endif
