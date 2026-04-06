#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_BUILDING_GENERATION_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_BUILDING_GENERATION_SYSTEM_H

#include <entt/entt.hpp>
#include "../components/components.h"
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
    BuildingGenerationSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher), m_gen(m_rd()) {}

    void initialize() override {
        m_dispatcher.sink<BuildingEntranceEvent>().connect<&BuildingGenerationSystem::handleEntrance>(this);
    }

    void update(double delta_time) override {}

    void handleEntrance(const BuildingEntranceEvent& event) {
        if (!m_registry.valid(event.building)) return;
        auto& interior = m_registry.get_or_emplace<BuildingInteriorComponent>(event.building);
        auto const& b_comp = m_registry.get<BuildingComponent>(event.building);
        
        // Stable layer ID based on building's assigned unique ID
        int base_layer_id = 1000 + b_comp.building_id * 10;

        if (!interior.is_generated || interior.floor_entities.empty()) {
            generateInterior(event.building, interior, event.entry_x, event.entry_y, event.entry_layer, base_layer_id);
        }

        if (m_registry.all_of<PositionComponent>(event.visitor) && !interior.floor_entities.empty()) {
            auto& pos = m_registry.get<PositionComponent>(event.visitor);
            auto& player_layer = m_registry.get_or_emplace<PlayerCurrentLayerComponent>(event.visitor);
            pos.layer_id = base_layer_id; 
            if (!interior.internal_doors.empty()) { pos.x = interior.internal_doors[0].x; pos.y = interior.internal_doors[0].y; }
            else { pos.x = 2; pos.y = 2; }
            player_layer.current_z = pos.layer_id;
            m_dispatcher.enqueue<HUDNotificationEvent>("Entered Building", 2.0f, "#AAAAFF");
        }
    }

private:
    void createTile(int x, int y, int layer, TerrainType type, char glyph, std::string color, bool is_obstacle = false) {
        auto entity = m_registry.create();
        m_registry.emplace<PositionComponent>(entity, x, y, layer);
        m_registry.emplace<TerrainComponent>(entity, type);
        m_registry.emplace<RenderableComponent>(entity, glyph, color, layer);
        if (is_obstacle) m_registry.emplace<ObstacleComponent>(entity);
    }

    RoomTag pickTag(ZoneType zone, size_t room_idx, size_t total_rooms) {
        if (room_idx == 0) {
            if (zone == ZoneType::RESIDENTIAL || zone == ZoneType::SLUM) return RoomTag::LIVING_ROOM;
            if (zone == ZoneType::INDUSTRIAL) return RoomTag::FACTORY_FLOOR;
            return RoomTag::LOBBY;
        }
        std::vector<RoomTag> pool;
        switch(zone) {
            case ZoneType::RESIDENTIAL: case ZoneType::SLUM: pool = {RoomTag::BEDROOM, RoomTag::KITCHEN, RoomTag::BATHROOM, RoomTag::STORAGE}; break;
            case ZoneType::CORPORATE: case ZoneType::URBAN_CORE: pool = {RoomTag::OFFICE, RoomTag::SERVER_ROOM, RoomTag::EXECUTIVE_SUITE, RoomTag::STORAGE}; break;
            case ZoneType::INDUSTRIAL: pool = {RoomTag::FACTORY_FLOOR, RoomTag::STORAGE, RoomTag::SUPERVISOR_OFFICE}; break;
            case ZoneType::COMMERCIAL: case ZoneType::MIXED_COMMERCIAL: pool = {RoomTag::OFFICE, RoomTag::STORAGE, RoomTag::HALLWAY}; break;
            default: pool = {RoomTag::STORAGE, RoomTag::HALLWAY}; break;
        }
        std::uniform_int_distribution<size_t> dist(0, pool.size() - 1);
        return pool[dist(m_gen)];
    }

    bool validateConnectivity(int width, int height, const std::vector<RoomData>& rooms, const std::vector<PositionComponent>& doors) {
        if (rooms.empty() || doors.empty()) return false;
        std::vector<bool> reachable(width * height, false); std::queue<std::pair<int, int>> q;
        q.push({doors[0].x, doors[0].y}); reachable[doors[0].y * width + doors[0].x] = true;
        while (!q.empty()) {
            auto [cx, cy] = q.front(); q.pop();
            int dx[] = {0,0,1,-1}, dy[] = {1,-1,0,0};
            for (int i=0; i<4; ++i) {
                int nx = cx + dx[i], ny = cy + dy[i];
                if (nx < 0 || nx >= width || ny < 0 || ny >= height || reachable[ny * width + nx]) continue;
                bool passable = false;
                for (auto const& d : doors) if (d.x == nx && d.y == ny) { passable = true; break; }
                if (!passable) for (auto const& r : rooms) if (nx >= r.x && nx < r.x + r.width && ny >= r.y && ny < r.y + r.height) { passable = true; break; }
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

    void generateInterior(entt::entity building, BuildingInteriorComponent& interior, int ex, int ey, int el, int base_layer_id) {
        if (interior.is_generated) { materializeInterior(building, interior, ex, ey, el, base_layer_id); return; }
        
        auto const& b_data = m_registry.get<BuildingComponent>(building);
        auto const* b_size = m_registry.try_get<SizeComponent>(building);
        int width = b_size ? b_size->width + 6 : 15;
        int height = b_size ? b_size->height + 6 : 15;
        width = std::max(width, 8); height = std::max(height, 8);
        interior.interior_width = width; interior.interior_height = height;

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
            while (!success && gen_attempts < 10) {
                floor_rooms.clear(); floor_doors.clear();
                BSPRoomNode* root = new BSPRoomNode(1, 1, width - 2, height - 2);
                std::vector<BSPRoomNode*> nodes = { root };
                int split_attempts = 0;
                while (nodes.size() < (size_t)target_rooms && split_attempts < 100) {
                    std::uniform_int_distribution<size_t> dist(0, nodes.size() - 1);
                    if (nodes[dist(m_gen)]->split(3, m_gen)) { nodes.push_back(nodes.back()->left); nodes.push_back(nodes.back()->right); }
                    split_attempts++;
                }
                root->getLeaves(floor_rooms); root->placeDoors(floor_doors, layer_id, m_gen);
                if (i == 0) floor_doors.insert(floor_doors.begin(), {floor_rooms[0].x, floor_rooms[0].y, layer_id});
                if (validateConnectivity(width, height, floor_rooms, floor_doors)) success = true;
                delete root; gen_attempts++;
            }

            for (size_t r = 0; r < floor_rooms.size(); ++r) {
                floor_rooms[r].tag = pickTag(b_data.zone_type, r, floor_rooms.size());
                if (i == 0) interior.rooms.push_back(floor_rooms[r]);
            }

            buildFloorStructure(width, height, layer_id, floor_rooms, floor_doors, i, ex, ey, el, interior, floor_comp);
            for (auto const& room : floor_rooms) {
                spawnRoomFurniture(room, layer_id, floor_comp.nav_grid);
                spawnRoomItems(room, layer_id);
            }
            int sx = width - 2, sy = height - 2;
            if (i < b_data.height - 1) { spawnStairs(sx, sy, layer_id, layer_id + 1, true); if (i == 0) interior.stairs.push_back({sx, sy, layer_id}); }
            if (i > 0) spawnStairs(sx, sy, layer_id, layer_id - 1, false);
            interior.floor_entities.push_back(floor_ent);
        }
        interior.is_generated = true; m_dispatcher.enqueue<LogEvent>("Generated Interior", LogSeverity::INFO, "BuildingGen");
    }

    void materializeInterior(entt::entity building, BuildingInteriorComponent& interior, int ex, int ey, int el, int base_layer_id) {
        auto const& b_data = m_registry.get<BuildingComponent>(building);
        int width = interior.interior_width, height = interior.interior_height;

        for (int i = 0; i < b_data.height; ++i) {
            int layer_id = base_layer_id + i;
            auto floor_ent = m_registry.create();
            auto& floor_comp = m_registry.emplace<FloorComponent>(floor_ent, i, layer_id);
            m_registry.emplace<NameComponent>(floor_ent, "Floor " + std::to_string(i));
            floor_comp.nav_grid.width = width; floor_comp.nav_grid.height = height; floor_comp.nav_grid.grid.assign(width * height, 0);

            // Rebuild walls and floors from cached room data
            buildFloorStructure(width, height, layer_id, interior.rooms, interior.internal_doors, i, ex, ey, el, interior, floor_comp);
            
            // Re-materialize specific objects (furniture, items)
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
        interior.stored_objects.clear(); // Flush cache after materialization
        m_dispatcher.enqueue<LogEvent>("Materialized Interior from Cache", LogSeverity::INFO, "BuildingGen");
    }

    void buildFloorStructure(int width, int height, int layer_id, const std::vector<RoomData>& rooms, const std::vector<PositionComponent>& doors, int floor_idx, int ex, int ey, int el, BuildingInteriorComponent& interior, FloorComponent& floor_comp) {
        for (int x = 0; x < width; ++x) for (int y = 0; y < height; ++y) {
            if (x == 0 || x == width - 1 || y == 0 || y == height - 1) { floor_comp.nav_grid.set_passable(x, y, false); createTile(x, y, layer_id, TerrainType::WALL, '#', "#444444", true); }
            else createTile(x, y, layer_id, TerrainType::CONCRETE_FLOOR, '.', "#222222", false);
        }
        for (auto const& room : rooms) {
            for (int x = room.x - 1; x <= room.x + room.width; ++x) for (int y = room.y - 1; y <= room.y + room.height; ++y) {
                if (x == room.x - 1 || x == room.x + room.width || y == room.y - 1 || y == room.y + room.height) {
                    if (x > 0 && x < width - 1 && y > 0 && y < height - 1) { floor_comp.nav_grid.set_passable(x, y, false); createTile(x, y, layer_id, TerrainType::WALL, '#', "#555555", true); }
                }
            }
        }
        for (size_t d_idx = 0; d_idx < doors.size(); ++d_idx) {
            auto const& d = doors[d_idx]; if (d.layer_id != layer_id) continue;
            floor_comp.nav_grid.set_passable(d.x, d.y, true);
            if (floor_idx == 0 && d_idx == 0) {
                auto door_ent = m_registry.create(); m_registry.emplace<PositionComponent>(door_ent, d.x, d.y, layer_id);
                m_registry.emplace<RenderableComponent>(door_ent, 'E', "#FF00FF", layer_id); m_registry.emplace<NameComponent>(door_ent, "Exit to City");
                m_registry.emplace<PortalComponent>(door_ent, ex, ey, el, true);
            } else createTile(d.x, d.y, layer_id, TerrainType::CONCRETE_FLOOR, 'D', "#FFFFCC", false);
        }
    }

    void spawnStairs(int x, int y, int layer, int target, bool up) {
        auto s = m_registry.create(); m_registry.emplace<PositionComponent>(s, x, y, layer);
        m_registry.emplace<RenderableComponent>(s, up ? '>' : '<', "#FFFFFF", layer);
        m_registry.emplace<StairsComponent>(s, target);
    }

    void spawnRoomFurniture(const RoomData& room, int layer_id, NavGrid& grid) {
        int fx = room.x + room.width / 2, fy = room.y + room.height / 2;
        char glyph = '?'; std::string color = "#FFFFFF", name = "Furniture";
        switch(room.tag) {
            case RoomTag::KITCHEN: glyph = 'K'; color = "#FFAA55"; name = "Kitchen Unit"; break;
            case RoomTag::BEDROOM: glyph = 'B'; color = "#55AAFF"; name = "Bed"; break;
            case RoomTag::SERVER_ROOM: glyph = 'S'; color = "#55FF55"; name = "Server Rack"; break;
            case RoomTag::OFFICE: glyph = 'O'; color = "#AAAAAA"; name = "Desk"; break;
            case RoomTag::FACTORY_FLOOR: glyph = 'M'; color = "#FF5555"; name = "Machine"; break;
            case RoomTag::STORAGE: glyph = 'C'; color = "#AA8844"; name = "Crate"; break;
            case RoomTag::EXECUTIVE_SUITE: glyph = 'X'; color = "#FFFF55"; name = "Luxury Chair"; break;
            case RoomTag::LOBBY: glyph = 'R'; color = "#8888FF"; name = "Reception Desk"; break;
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
        } else if (room.tag == RoomTag::SERVER_ROOM) {
            auto tech = m_registry.create(); m_registry.emplace<PositionComponent>(tech, ix, iy, layer_id);
            m_registry.emplace<RenderableComponent>(tech, '*', "#55FFFF", layer_id); m_registry.emplace<ItemComponent>(tech, 2, "Data Drive"); m_registry.emplace<ItemValueComponent>(tech, 500);
        }
    }

    entt::registry& m_registry; entt::dispatcher& m_dispatcher; std::random_device m_rd; std::mt19937 m_gen;
};

} // namespace NeonOubliette

#endif
