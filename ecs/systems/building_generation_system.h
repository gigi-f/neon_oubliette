#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_BUILDING_GENERATION_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_BUILDING_GENERATION_SYSTEM_H

#include <entt/entt.hpp>
#include "../components/components.h"
#include "../components/simulation_layers.h"
#include "../event_declarations.h"
#include "../system_scheduler.h"
#include <random>
#include <vector>
#include <algorithm>
#include <map>
#include <queue>
#include <set>

namespace NeonOubliette {

/**
 * @brief [NEW CLASS] Helper for BSP-based room partitioning.
 */
struct BSPRoomNode {
    int x, y, w, h;
    BSPRoomNode *left = nullptr, *right = nullptr;
    int split_pos = -1;
    bool split_h = false;

    BSPRoomNode(int _x, int _y, int _w, int _h) : x(_x), y(_y), w(_w), h(_h) {}
    ~BSPRoomNode() { delete left; delete right; }

    bool split(int min_size, std::mt19937& gen) {
        if (left || right) return false;
        split_h = (std::uniform_int_distribution<int>(0, 1)(gen) == 0);
        if (w > h && (float)w / h >= 1.25) split_h = false;
        else if (h > w && (float)h / w >= 1.25) split_h = true;
        int max = (split_h ? h : w) - min_size;
        if (max <= min_size) return false;
        split_pos = std::uniform_int_distribution<int>(min_size, max)(gen);
        if (split_h) { left = new BSPRoomNode(x, y, w, split_pos); right = new BSPRoomNode(x, y + split_pos, w, h - split_pos); }
        else { left = new BSPRoomNode(x, y, split_pos, h); right = new BSPRoomNode(x + split_pos, y, w - split_pos, h); }
        return true;
    }

    void getLeaves(std::vector<RoomData>& leaves) {
        if (!left && !right) { leaves.push_back({x, y, w, h, RoomTag::VOID}); return; }
        if (left) left->getLeaves(leaves); if (right) right->getLeaves(leaves);
    }

    void placeDoors(std::vector<PositionComponent>& doors, int layer_id, std::mt19937& gen) {
        if (!left || !right) return;
        if (split_h) { int dx = x + std::uniform_int_distribution<int>(0, w - 1)(gen); int dy = y + split_pos - 1; doors.push_back({dx, dy, layer_id}); }
        else { int dy = y + std::uniform_int_distribution<int>(0, h - 1)(gen); int dx = x + split_pos - 1; doors.push_back({dx, dy, layer_id}); }
        left->placeDoors(doors, layer_id, gen); right->placeDoors(doors, layer_id, gen);
    }
};

class BuildingGenerationSystem : public ISystem {
public:
    struct ExtDoor { int x, y, layer; StreetFacingSide wall; int offset; };

    BuildingGenerationSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher), m_gen(m_rd()) {}

    void initialize() override {
        m_dispatcher.sink<BuildingEntranceEvent>().connect<&BuildingGenerationSystem::handleEntrance>(this);
    }

    void update(double delta_time) override {}

    void handleEntrance(const BuildingEntranceEvent& event) {
        if (!m_registry.valid(event.building)) return;

        // [B.2] Record Interior State on Visitor
        auto& state = m_registry.get_or_emplace<InteriorStateComponent>(event.visitor);
        state.building_entity = event.building;
        state.entry_x = event.entry_x;
        state.entry_y = event.entry_y;
        state.entry_layer = event.entry_layer;

        if (m_registry.valid(event.door_entity) && m_registry.all_of<DoorMetadataComponent>(event.door_entity)) {
            const auto& meta = m_registry.get<DoorMetadataComponent>(event.door_entity);
            state.entry_wall = meta.wall_side;
            state.entry_offset = meta.wall_offset;
        }

        auto& interior = m_registry.get_or_emplace<BuildingInteriorComponent>(event.building);
        auto const& b_comp = m_registry.get<BuildingComponent>(event.building);
        
        // Stable layer ID based on building's assigned unique ID
        int base_layer_id = 1000 + b_comp.building_id * 10;

        if (!interior.is_generated || interior.floor_entities.empty()) {
            generateInterior(event.building, interior, base_layer_id);
        }

        if (m_registry.all_of<PositionComponent>(event.visitor) && !interior.floor_entities.empty()) {
            auto& pos = m_registry.get<PositionComponent>(event.visitor);
            auto& player_layer = m_registry.get_or_emplace<PlayerCurrentLayerComponent>(event.visitor);
            pos.layer_id = base_layer_id; 
            
            // [B.2] Find the interior exit door that corresponds to this entry door's world position
            bool found_spawn = false;
            auto portal_view = m_registry.view<PositionComponent, PortalComponent>();
            for (auto portal_ent : portal_view) {
                const auto& p_pos = portal_view.get<PositionComponent>(portal_ent);
                const auto& portal = portal_view.get<PortalComponent>(portal_ent);
                if (p_pos.layer_id == base_layer_id && portal.target_x == state.entry_x && portal.target_y == state.entry_y) {
                    pos.x = p_pos.x;
                    pos.y = p_pos.y;
                    found_spawn = true;
                    break;
                }
            }

            if (!found_spawn) {
                if (!interior.internal_doors.empty()) { pos.x = interior.internal_doors[0].x; pos.y = interior.internal_doors[0].y; }
                else { pos.x = 2; pos.y = 2; }
            }

            // [B.5] Initial Room Lookup
            state.current_room_index = -1;
            for (size_t i = 0; i < interior.rooms.size(); ++i) {
                const auto& r = interior.rooms[i];
                if (pos.x >= r.x && pos.x < r.x + r.width && pos.y >= r.y && pos.y < r.y + r.height) {
                    state.current_room_index = (int)i;
                    break;
                }
            }

            player_layer.current_z = pos.layer_id;
        }
    }

private:
    entt::entity createTile(int x, int y, int layer, TerrainType type, char glyph, std::string color, bool is_obstacle = false) {
        auto entity = m_registry.create();
        m_registry.emplace<PositionComponent>(entity, x, y, layer);
        m_registry.emplace<TerrainComponent>(entity, type);
        m_registry.emplace<RenderableComponent>(entity, glyph, color, layer);
        if (is_obstacle) m_registry.emplace<ObstacleComponent>(entity);
        return entity;
    }

    RoomTag pickTag(ZoneType zone, size_t room_idx, size_t total_rooms) {
        if (room_idx == 0) {
            if (zone == ZoneType::RESIDENTIAL || zone == ZoneType::SLUM) return RoomTag::LIVING_ROOM;
            if (zone == ZoneType::INDUSTRIAL) return RoomTag::FACTORY_FLOOR;
            if (zone == ZoneType::RELIGIOUS) return RoomTag::NAVE;
            return RoomTag::LOBBY;
        }
        std::vector<RoomTag> pool;
        switch(zone) {
            case ZoneType::RESIDENTIAL: case ZoneType::SLUM: pool = {RoomTag::BEDROOM, RoomTag::KITCHEN, RoomTag::BATHROOM, RoomTag::STORAGE}; break;
            case ZoneType::CORPORATE: case ZoneType::URBAN_CORE: pool = {RoomTag::OFFICE, RoomTag::SERVER_ROOM, RoomTag::EXECUTIVE_SUITE, RoomTag::STORAGE, RoomTag::SECURITY_HUB}; break;
            case ZoneType::INDUSTRIAL: pool = {RoomTag::FACTORY_FLOOR, RoomTag::STORAGE, RoomTag::SUPERVISOR_OFFICE}; break;
            case ZoneType::COMMERCIAL: case ZoneType::MIXED_COMMERCIAL: pool = {RoomTag::OFFICE, RoomTag::STORAGE, RoomTag::HALLWAY}; break;
            case ZoneType::CIVIC: pool = {RoomTag::OFFICE, RoomTag::HOLDING_CELL, RoomTag::STORAGE, RoomTag::HALLWAY}; break;
            case ZoneType::RELIGIOUS: pool = {RoomTag::ALTAR, RoomTag::SEATING_AREA, RoomTag::STORAGE, RoomTag::HALLWAY}; break;
            default: pool = {RoomTag::STORAGE, RoomTag::HALLWAY}; break;
        }

        // [I.6] Holding Cells in Corporate/Security zones
        if ((zone == ZoneType::CORPORATE || zone == ZoneType::URBAN_CORE) && room_idx > 0) {
            std::uniform_real_distribution<> dis(0.0, 1.0);
            if (dis(m_gen) < 0.05) return RoomTag::HOLDING_CELL;
        }

        // [I.3] Slum Back-alley Fences (10% chance in Slum building secondary rooms)
        if (zone == ZoneType::SLUM && room_idx > 0) {
            std::uniform_real_distribution<> dis(0.0, 1.0);
            if (dis(m_gen) < 0.1) return RoomTag::FENCE;
        }
        // [I.4] Clandestine Labs in Industrial
        if (zone == ZoneType::INDUSTRIAL && room_idx > 0) {
            std::uniform_real_distribution<> dis(0.0, 1.0);
            if (dis(m_gen) < 0.05) return RoomTag::CLANDESTINE_LAB;
        }

        std::uniform_int_distribution<size_t> dist(0, pool.size() - 1);
        return pool[dist(m_gen)];
    }

    bool validateConnectivity(int width, int height, const std::vector<RoomData>& rooms, const std::vector<PositionComponent>& doors, const std::vector<std::pair<int, int>>& interior_exits) {
        if (rooms.empty()) return false;
        std::vector<bool> reachable(width * height, false); std::queue<std::pair<int, int>> q;
        
        if (interior_exits.empty()) {
            if (doors.empty()) return false;
            q.push({doors[0].x, doors[0].y}); reachable[doors[0].y * width + doors[0].x] = true;
        } else {
            for (auto const& ie : interior_exits) {
                q.push({ie.first, ie.second}); reachable[ie.second * width + ie.first] = true;
            }
        }

        while (!q.empty()) {
            auto [cx, cy] = q.front(); q.pop();
            int dx[] = {0,0,1,-1}, dy[] = {1,-1,0,0};
            for (int i=0; i<4; ++i) {
                int nx = cx + dx[i], ny = cy + dy[i];
                if (nx < 0 || nx >= width || ny < 0 || ny >= height || reachable[ny * width + nx]) continue;
                bool passable = false;
                for (auto const& d : doors) if (d.x == nx && d.y == ny) { passable = true; break; }
                if (!passable) for (auto const& r : rooms) if (nx >= r.x && nx < r.x + r.width && ny >= r.y && ny < r.y + r.height) { passable = true; break; }
                
                if (!passable) {
                    for (auto const& ie : interior_exits) if (ie.first == nx && ie.second == ny) { passable = true; break; }
                }

                if (passable) { reachable[ny * width + nx] = true; q.push({nx, ny}); }
            }
        }
        for (auto const& r : rooms) {
            bool r_ok = false;
            for (int rx = r.x; rx < r.x + r.width && !r_ok; ++rx) for (int ry = r.y; ry < r.y + r.height && !r_ok; ++ry) if (reachable[ry * width + rx]) r_ok = true;
            if (!r_ok) return false;
        }
        return true;
    }

    void generateInterior(entt::entity building, BuildingInteriorComponent& interior, int base_layer_id) {
        if (interior.is_generated) { materializeInterior(building, interior, base_layer_id); return; }
        
        auto const& b_data = m_registry.get<BuildingComponent>(building);
        auto const* b_size = m_registry.try_get<SizeComponent>(building);
        int width = b_size ? b_size->width + 6 : 15;
        int height = b_size ? b_size->height + 6 : 15;
        width = std::max(width, 8); height = std::max(height, 8);
        interior.interior_width = width; interior.interior_height = height;

        std::vector<ExtDoor> ext_doors;
        auto door_view = m_registry.view<PositionComponent, DoorMetadataComponent>();
        for (auto door_ent : door_view) {
            const auto& meta = door_view.get<DoorMetadataComponent>(door_ent);
            if (meta.building_entity == building) {
                const auto& d_pos = door_view.get<PositionComponent>(door_ent);
                ext_doors.push_back({d_pos.x, d_pos.y, d_pos.layer_id, meta.wall_side, meta.wall_offset});
            }
        }

        int target_rooms = 2;
        if (width >= 11 || height >= 11) target_rooms = 6 + (std::max(width, height) / 5);
        else if (width >= 5 || height >= 5) target_rooms = 3 + (std::max(width, height) / 4);

        for (int i = 0; i < b_data.height; ++i) {
            int layer_id = base_layer_id + i;
            auto floor_ent = m_registry.create();
            auto& floor_comp = m_registry.emplace<FloorComponent>(floor_ent, i, layer_id);
            m_registry.emplace<NameComponent>(floor_ent, "Floor " + std::to_string(i));
            floor_comp.nav_grid.width = width; floor_comp.nav_grid.height = height; floor_comp.nav_grid.grid.assign(width * height, 0);

            std::vector<RoomData> floor_rooms; std::vector<PositionComponent> floor_doors;
            int gen_attempts = 0; bool success = false;
            
            std::vector<std::pair<int, int>> interior_exits;
            if (i == 0) {
                for (auto const& ed : ext_doors) {
                    int ex = 1, ey = 1;
                    if (ed.wall == StreetFacingSide::NORTH) { ex = 1 + ed.offset; ey = 0; }
                    else if (ed.wall == StreetFacingSide::SOUTH) { ex = 1 + ed.offset; ey = height - 1; }
                    else if (ed.wall == StreetFacingSide::WEST) { ex = 0; ey = 1 + ed.offset; }
                    else if (ed.wall == StreetFacingSide::EAST) { ex = width - 1; ey = 1 + ed.offset; }
                    ex = std::clamp(ex, 0, width - 1); ey = std::clamp(ey, 0, height - 1);
                    interior_exits.push_back({ex, ey});
                }
            }

            while (!success && gen_attempts < 10) {
                floor_rooms.clear(); floor_doors.clear();
                BSPRoomNode* root = new BSPRoomNode(1, 1, width - 2, height - 2);
                std::vector<BSPRoomNode*> nodes = { root };
                int split_attempts = 0;
                while (nodes.size() < (size_t)target_rooms && split_attempts < 100) {
                    std::uniform_int_distribution<size_t> dist(0, nodes.size() - 1);
                    size_t idx = dist(m_gen);
                    if (nodes[idx]->split(3, m_gen)) { nodes.push_back(nodes[idx]->left); nodes.push_back(nodes[idx]->right); }
                    split_attempts++;
                }
                root->getLeaves(floor_rooms); root->placeDoors(floor_doors, layer_id, m_gen);
                
                if (i == 0 && !interior_exits.empty()) {
                    for (auto const& ie : interior_exits) {
                        int min_dist = 9999; size_t best_room = 0;
                        for (size_t r = 0; r < floor_rooms.size(); ++r) {
                            int dx = std::max(0, std::max(floor_rooms[r].x - ie.first, ie.first - (floor_rooms[r].x + floor_rooms[r].width - 1)));
                            int dy = std::max(0, std::max(floor_rooms[r].y - ie.second, ie.second - (floor_rooms[r].y + floor_rooms[r].height - 1)));
                            if (dx + dy < min_dist) { min_dist = dx + dy; best_room = r; }
                        }
                        int dx = ie.first, dy = ie.second;
                        if (dx == 0) dx++; else if (dx == width - 1) dx--;
                        if (dy == 0) dy++; else if (dy == height - 1) dy--;
                        floor_doors.push_back({dx, dy, layer_id});
                    }
                } else if (i == 0) {
                    floor_doors.insert(floor_doors.begin(), {floor_rooms[0].x, floor_rooms[0].y, layer_id});
                }

                if (validateConnectivity(width, height, floor_rooms, floor_doors, interior_exits)) success = true;
                delete root; gen_attempts++;
            }

            for (size_t r = 0; r < floor_rooms.size(); ++r) {
                floor_rooms[r].tag = pickTag(b_data.zone_type, r, floor_rooms.size());
                if (i == 0) interior.rooms.push_back(floor_rooms[r]);
            }

            buildFloorStructure(width, height, layer_id, floor_rooms, floor_doors, i, ext_doors, interior, floor_comp);
            
            // [B.5] Build acoustics graph for the first floor (or all floors?)
            if (i == 0) {
                buildAcousticsGraph(building, floor_rooms, floor_doors, layer_id);
            }

            for (auto const& room : floor_rooms) {
                spawnRoomFurniture(room, layer_id, floor_comp.nav_grid);
                spawnRoomItems(room, layer_id);
                spawnRoomAgents(room, layer_id);
            }
            int sx = width - 2, sy = height - 2;
            if (i < b_data.height - 1) { spawnStairs(sx, sy, layer_id, layer_id + 1, true); if (i == 0) interior.stairs.push_back({sx, sy, layer_id}); }
            if (i > 0) spawnStairs(sx, sy, layer_id, layer_id - 1, false);
            interior.floor_entities.push_back(floor_ent);
        }
        interior.is_generated = true; m_dispatcher.enqueue<LogEvent>("Generated Interior", LogSeverity::INFO, "BuildingGen");
    }

    void materializeInterior(entt::entity building, BuildingInteriorComponent& interior, int base_layer_id) {
        auto const& b_data = m_registry.get<BuildingComponent>(building);
        int width = interior.interior_width, height = interior.interior_height;

        std::vector<ExtDoor> ext_doors;
        auto door_view = m_registry.view<PositionComponent, DoorMetadataComponent>();
        for (auto door_ent : door_view) {
            const auto& meta = door_view.get<DoorMetadataComponent>(door_ent);
            if (meta.building_entity == building) {
                const auto& d_pos = door_view.get<PositionComponent>(door_ent);
                ext_doors.push_back({d_pos.x, d_pos.y, d_pos.layer_id, meta.wall_side, meta.wall_offset});
            }
        }

        for (int i = 0; i < b_data.height; ++i) {
            int layer_id = base_layer_id + i;
            auto floor_ent = m_registry.create();
            auto& floor_comp = m_registry.emplace<FloorComponent>(floor_ent, i, layer_id);
            m_registry.emplace<NameComponent>(floor_ent, "Floor " + std::to_string(i));
            floor_comp.nav_grid.width = width; floor_comp.nav_grid.height = height; floor_comp.nav_grid.grid.assign(width * height, 0);

            buildFloorStructure(width, height, layer_id, interior.rooms, interior.internal_doors, i, ext_doors, interior, floor_comp);
            
            for (auto const& obj : interior.stored_objects) {
                if (obj.layer_id == layer_id) {
                    auto ent = m_registry.create();
                    m_registry.emplace<PositionComponent>(ent, obj.x, obj.y, obj.layer_id);
                    m_registry.emplace<RenderableComponent>(ent, obj.glyph, obj.color, obj.layer_id);
                    m_registry.emplace<NameComponent>(ent, obj.name);
                    if (obj.is_obstacle) { m_registry.emplace<ObstacleComponent>(ent); floor_comp.nav_grid.set_passable(obj.x, obj.y, false); }
                    if (obj.item_type_id > 0) m_registry.emplace<ItemComponent>(ent, obj.item_type_id, obj.name);
                    if (obj.item_value > 0) m_registry.emplace<ItemValueComponent>(ent, obj.item_value);
                    if (obj.restores_hunger > 0 || obj.restores_thirst > 0) m_registry.emplace<ConsumableComponent>(ent, obj.restores_hunger, obj.restores_thirst);
                }
            }
            int sx = width - 2, sy = height - 2;
            if (i < b_data.height - 1) spawnStairs(sx, sy, layer_id, layer_id + 1, true);
            if (i > 0) spawnStairs(sx, sy, layer_id, layer_id - 1, false);
            interior.floor_entities.push_back(floor_ent);
        }
        interior.stored_objects.clear();
        m_dispatcher.enqueue<LogEvent>("Materialized Interior from Cache", LogSeverity::INFO, "BuildingGen");
        
        // [FIX] Invalidate visibility caches after creating new terrain
        m_dispatcher.enqueue<ChunkChangedEvent>();
    }

    void buildFloorStructure(int width, int height, int layer_id, const std::vector<RoomData>& rooms, const std::vector<PositionComponent>& doors, int floor_idx, const std::vector<ExtDoor>& ext_doors, BuildingInteriorComponent& interior, FloorComponent& floor_comp) {
        // [B.5] Map door coordinates to entity for consistency
        std::map<std::pair<int, int>, entt::entity> door_entity_map;
        
        // 1. Pre-calculate Wall Mask to avoid duplicate terrain entities
        std::vector<bool> wall_mask(width * height, false);
        for (int x = 0; x < width; ++x) for (int y = 0; y < height; ++y) {
            if (x == 0 || x == width - 1 || y == 0 || y == height - 1) wall_mask[y * width + x] = true;
        }
        for (auto const& room : rooms) {
            for (int x = room.x - 1; x <= room.x + room.width; ++x) for (int y = room.y - 1; y <= room.y + room.height; ++y) {
                if (x == room.x - 1 || x == room.x + room.width || y == room.y - 1 || y == room.y + room.height) {
                    if (x > 0 && x < width - 1 && y > 0 && y < height - 1) wall_mask[y * width + x] = true;
                }
            }
        }

        // 2. Clear walls at interior door positions
        for (auto const& d : doors) {
            if (d.layer_id == layer_id && d.x >= 0 && d.x < width && d.y >= 0 && d.y < height) {
                wall_mask[d.y * width + d.x] = false;
            }
        }

        // 3. Clear walls at exterior door positions (first floor only)
        if (floor_idx == 0) {
            for (auto const& ed : ext_doors) {
                int ex = 1, ey = 1;
                if (ed.wall == StreetFacingSide::NORTH) { ex = 1 + ed.offset; ey = 0; }
                else if (ed.wall == StreetFacingSide::SOUTH) { ex = 1 + ed.offset; ey = height - 1; }
                else if (ed.wall == StreetFacingSide::WEST) { ex = 0; ey = 1 + ed.offset; }
                else if (ed.wall == StreetFacingSide::EAST) { ex = width - 1; ey = 1 + ed.offset; }
                ex = std::clamp(ex, 0, width - 1); ey = std::clamp(ey, 0, height - 1);
                wall_mask[ey * width + ex] = false;
            }
        }

        // 4. Materialize Based on Mask
        for (int x = 0; x < width; ++x) for (int y = 0; y < height; ++y) {
            if (wall_mask[y * width + x]) {
                floor_comp.nav_grid.set_passable(x, y, false);
                std::string color = (x == 0 || x == width - 1 || y == 0 || y == height - 1) ? "#444444" : "#555555";
                createTile(x, y, layer_id, TerrainType::WALL, '#', color, true);
            } else {
                createTile(x, y, layer_id, TerrainType::CONCRETE_FLOOR, '.', "#222222", false);
            }
        }
        
        // 5. Special Entities (Doors)
        if (floor_idx == 0) {
            for (auto const& ed : ext_doors) {
                int ex = 1, ey = 1;
                if (ed.wall == StreetFacingSide::NORTH) { ex = 1 + ed.offset; ey = 0; }
                else if (ed.wall == StreetFacingSide::SOUTH) { ex = 1 + ed.offset; ey = height - 1; }
                else if (ed.wall == StreetFacingSide::WEST) { ex = 0; ey = 1 + ed.offset; }
                else if (ed.wall == StreetFacingSide::EAST) { ex = width - 1; ey = 1 + ed.offset; }
                ex = std::clamp(ex, 0, width - 1); ey = std::clamp(ey, 0, height - 1);

                auto door_ent = m_registry.create(); 
                m_registry.emplace<PositionComponent>(door_ent, ex, ey, layer_id);
                m_registry.emplace<RenderableComponent>(door_ent, 'E', "#FF00FF", layer_id); 
                m_registry.emplace<NameComponent>(door_ent, "Exit to City");
                m_registry.emplace<PortalComponent>(door_ent, ed.x, ed.y, ed.layer, true);
                floor_comp.nav_grid.set_passable(ex, ey, true);
                m_registry.emplace<DoorComponent>(door_ent, false, true); 
            }
        }

        for (size_t d_idx = 0; d_idx < doors.size(); ++d_idx) {
            auto const& d = doors[d_idx]; if (d.layer_id != layer_id) continue;
            floor_comp.nav_grid.set_passable(d.x, d.y, true);
            auto door_ent = createTile(d.x, d.y, layer_id, TerrainType::CONCRETE_FLOOR, 'D', "#FFFFCC", false);
            m_registry.emplace<DoorComponent>(door_ent, false, false); // closed by default
        }
    }

    void spawnStairs(int x, int y, int layer, int target, bool up) {
        auto s = m_registry.create(); m_registry.emplace<PositionComponent>(s, x, y, layer);
        m_registry.emplace<RenderableComponent>(s, up ? '>' : '<', "#AAFFAA", layer);
        m_registry.emplace<StairsComponent>(s, target);
    }

    void buildAcousticsGraph(entt::entity building, const std::vector<RoomData>& rooms, const std::vector<PositionComponent>& doors, int layer_id) {
        auto& acoustics = m_registry.emplace<BuildingAcousticsComponent>(building);
        acoustics.nodes.resize(rooms.size());

        auto const* b_pos = m_registry.try_get<PositionComponent>(building);
        auto const* b_size = m_registry.try_get<SizeComponent>(building);

        for (size_t i = 0; i < rooms.size(); ++i) {
            const auto& ri = rooms[i];
            
            // [B.5] Check for windows in this room
            if (layer_id == 1000 + m_registry.get<BuildingComponent>(building).building_id * 10) { // Ground floor
                // Simplification: room has window if it touches the floor boundary
                // In generateInterior, interior_width = b_size->width + 6
                int width = b_size ? b_size->width + 6 : 15;
                int height = b_size ? b_size->height + 6 : 15;
                if (ri.x <= 1 || ri.x + ri.width >= width - 1 || ri.y <= 1 || ri.y + ri.height >= height - 1) {
                    acoustics.nodes[i].has_window = true;
                }
            }

            for (size_t j = i + 1; j < rooms.size(); ++j) {
                const auto& ri = rooms[i];
                const auto& rj = rooms[j];
                
                bool adjacent = false;
                int ax = -1, ay = -1;

                // Vertical shared wall
                if (ri.x + ri.width == rj.x - 1 || rj.x + rj.width == ri.x - 1) {
                    if (std::max(ri.y, rj.y) < std::min(ri.y + ri.height, rj.y + rj.height)) {
                        adjacent = true;
                        ax = (ri.x + ri.width == rj.x - 1) ? ri.x + ri.width : rj.x + rj.width;
                        ay = std::max(ri.y, rj.y);
                    }
                }
                // Horizontal shared wall
                else if (ri.y + ri.height == rj.y - 1 || rj.y + rj.height == ri.y - 1) {
                    if (std::max(ri.x, rj.x) < std::min(ri.x + ri.width, rj.x + rj.width)) {
                        adjacent = true;
                        ax = std::max(ri.x, rj.x);
                        ay = (ri.y + ri.height == rj.y - 1) ? ri.y + ri.height : rj.y + rj.height;
                    }
                }

                if (adjacent) {
                    // Check if there is a door on this boundary
                    entt::entity door_ent = entt::null;
                    
                    // Look for door in m_registry at this boundary? 
                    // Actually, let's check the door coordinates from 'doors' vector.
                    for (const auto& d : doors) {
                        if (d.layer_id == layer_id) {
                            // Boundary wall can have multiple tiles, check if door is anywhere on it
                            bool on_wall = false;
                            if (ri.x + ri.width == rj.x - 1 || rj.x + rj.width == ri.x - 1) {
                                int wall_x = (ri.x + ri.width == rj.x - 1) ? ri.x + ri.width : rj.x + rj.width;
                                if (d.x == wall_x && d.y >= std::max(ri.y, rj.y) && d.y < std::min(ri.y + ri.height, rj.y + rj.height)) on_wall = true;
                            } else {
                                int wall_y = (ri.y + ri.height == rj.y - 1) ? ri.y + ri.height : rj.y + rj.height;
                                if (d.y == wall_y && d.x >= std::max(ri.x, rj.x) && d.x < std::min(ri.x + ri.width, rj.x + rj.width)) on_wall = true;
                            }

                            if (on_wall) {
                                // Find the door entity at this position
                                auto d_view = m_registry.view<PositionComponent, DoorComponent>();
                                for (auto ent : d_view) {
                                    const auto& dp = d_view.get<PositionComponent>(ent);
                                    if (dp.x == d.x && dp.y == d.y && dp.layer_id == d.layer_id) {
                                        door_ent = ent;
                                        break;
                                    }
                                }
                                break;
                            }
                        }
                    }

                    acoustics.nodes[i].neighbors.push_back({j, door_ent});
                    acoustics.nodes[j].neighbors.push_back({i, door_ent});
                }
            }
        }
    }

    void spawnRoomFurniture(const RoomData& room, int layer_id, NavGrid& grid) {
        int fx = room.x + room.width / 2, fy = room.y + room.height / 2;
        char glyph = '?'; std::string color = "#FFFFFF", name = "Furniture";
        switch(room.tag) {
            case RoomTag::KITCHEN: glyph = 'K'; color = "#FFAA55"; name = "Kitchen Unit"; break;
            case RoomTag::BEDROOM: glyph = 'B'; color = "#55AAFF"; name = "Bed"; break;
            case RoomTag::SERVER_ROOM: glyph = 'S'; color = "#55FF55"; name = "Server Rack"; break;
            case RoomTag::OFFICE: glyph = 'O', color = "#AAAAAA"; name = "Desk"; break;
            case RoomTag::FACTORY_FLOOR: glyph = 'M'; color = "#FF5555"; name = "Machine"; break;
            case RoomTag::STORAGE: glyph = 'C'; color = "#AA8844"; name = "Crate"; break;
            case RoomTag::EXECUTIVE_SUITE: glyph = 'X'; color = "#FFFF55"; name = "Luxury Chair"; break;
            case RoomTag::LOBBY: glyph = 'R'; color = "#8888FF"; name = "Reception Desk"; break;
            case RoomTag::NAVE: glyph = '='; color = "#AA88FF"; name = "Nave Bench"; break;
            case RoomTag::ALTAR: glyph = '*'; color = "#FFD700"; name = "Holy Altar"; break;
            case RoomTag::SEATING_AREA: glyph = '='; color = "#AA88FF"; name = "Pew"; break;
            case RoomTag::FENCE: glyph = 'F'; color = "#AA00AA"; name = "Fence Counter"; break;
            case RoomTag::CLANDESTINE_LAB: glyph = 'L'; color = "#00FF55"; name = "Chemical Processor"; break;
            case RoomTag::SECURITY_HUB: glyph = 'H'; color = "#5555FF"; name = "Security Terminal"; break;
            case RoomTag::HOLDING_CELL: glyph = 'C'; color = "#FF5555"; name = "Holding Cell Gate"; break;
            default: return;
        }
        auto ent = m_registry.create(); m_registry.emplace<PositionComponent>(ent, fx, fy, layer_id);
        m_registry.emplace<RenderableComponent>(ent, glyph, color, layer_id); m_registry.emplace<NameComponent>(ent, name);
        m_registry.emplace<ObstacleComponent>(ent); grid.set_passable(fx, fy, false);
    }

    void spawnRoomItems(const RoomData& room, int layer_id) {
        int ix = room.x + 1, iy = room.y + 1;
        if (room.tag == RoomTag::KITCHEN) {
            auto food = m_registry.create(); m_registry.emplace<PositionComponent>(food, ix, iy, layer_id);
            m_registry.emplace<RenderableComponent>(food, '%', "#00FF00", layer_id); m_registry.emplace<ItemComponent>(food, 1, "Synthe-Food"); m_registry.emplace<ConsumableComponent>(food, 20, 0);
            m_registry.emplace<ItemMarketCategoryComponent>(food, ItemMarketCategory::FOOD);
            m_registry.emplace<ItemMaterialComponent>(food, RawMaterialType::BIOMASS);
        } else if (room.tag == RoomTag::SERVER_ROOM) {
            auto tech = m_registry.create(); m_registry.emplace<PositionComponent>(tech, ix, iy, layer_id);
            m_registry.emplace<RenderableComponent>(tech, '*', "#55FFFF", layer_id); m_registry.emplace<ItemComponent>(tech, 2, "Data Drive"); m_registry.emplace<ItemValueComponent>(tech, 500);
            m_registry.emplace<ItemMarketCategoryComponent>(tech, ItemMarketCategory::TECHNOLOGY);
            m_registry.emplace<ItemMaterialComponent>(tech, RawMaterialType::ELECTRONIC);
        } else if (room.tag == RoomTag::ALTAR) {
            auto relic = m_registry.create(); m_registry.emplace<PositionComponent>(relic, ix, iy, layer_id);
            m_registry.emplace<RenderableComponent>(relic, '!', "#FFD700", layer_id); m_registry.emplace<ItemComponent>(relic, 100, "Sacred Relic"); m_registry.emplace<ItemValueComponent>(relic, 1000);
            m_registry.emplace<ItemMarketCategoryComponent>(relic, ItemMarketCategory::LUXURY);
            m_registry.emplace<ItemMaterialComponent>(relic, RawMaterialType::METAL);
        }
    }

    void spawnRoomAgents(const RoomData& room, int layer_id) {
        if (room.tag == RoomTag::FENCE) {
            auto agent = m_registry.create();
            m_registry.emplace<PositionComponent>(agent, room.x + room.width / 2 + 1, room.y + room.height / 2, layer_id);
            m_registry.emplace<RenderableComponent>(agent, 'F', "#AA00AA", layer_id);
            m_registry.emplace<NameComponent>(agent, "Fence");
            m_registry.emplace<AgentComponent>(agent);
            m_registry.emplace<InventoryComponent>(agent);
            auto& econ = m_registry.emplace<Layer3EconomicComponent>(agent);
            econ.cash_on_hand = 500;
            m_registry.emplace<FenceComponent>(agent, 0.5f); // pays 50%
            auto& pol = m_registry.emplace<Layer4PoliticalComponent>(agent);
            pol.primary_faction = "SYNDICATE";
            pol.faction_loyalty = 0.5f;
        } else if (room.tag == RoomTag::CLANDESTINE_LAB) {
            auto dealer = m_registry.create();
            m_registry.emplace<PositionComponent>(dealer, room.x + room.width / 2, room.y + room.height / 2, layer_id);
            m_registry.emplace<RenderableComponent>(dealer, 'D', "#00FF55", layer_id);
            m_registry.emplace<NameComponent>(dealer, "Contraband Dealer");
            m_registry.emplace<AgentComponent>(dealer);
            m_registry.emplace<InventoryComponent>(dealer);
            auto& econ = m_registry.emplace<Layer3EconomicComponent>(dealer);
            econ.cash_on_hand = 200;
            
            // Spawn some contraband in their inventory
            auto& inv = m_registry.get<InventoryComponent>(dealer);
            auto medkit = m_registry.create();
            m_registry.emplace<NameComponent>(medkit, "Military Medkit");
            m_registry.emplace<ItemComponent>(medkit, 10, "MEDKIT");
            m_registry.emplace<ContrabandComponent>(medkit);
            m_registry.emplace<ItemValueComponent>(medkit, 300);
            m_registry.emplace<ItemMarketCategoryComponent>(medkit, ItemMarketCategory::MEDICAL);
            m_registry.emplace<ItemMaterialComponent>(medkit, RawMaterialType::CHEMICAL);
            inv.contained_items.push_back(medkit);
        }
    }

    entt::registry& m_registry; entt::dispatcher& m_dispatcher; std::random_device m_rd; std::mt19937 m_gen;
};

} // namespace NeonOubliette

#endif
