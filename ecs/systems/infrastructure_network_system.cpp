#include "infrastructure_network_system.h"
#include <random>
#include <cmath>
#include <map>
#include <algorithm>
#include "../components/zoning_components.h"

namespace NeonOubliette {

InfrastructureNetworkSystem::InfrastructureNetworkSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {}

void InfrastructureNetworkSystem::initialize() {
    auto zone_view = m_registry.view<MacroZoneComponent>();
    for (auto entity : zone_view) {
        const auto& zone = zone_view.get<MacroZoneComponent>(entity);
        m_zone_cache[{zone.macro_x, zone.macro_y}] = entity;
    }
}

void InfrastructureNetworkSystem::link_arterial_to_zone(entt::entity arterial, int x, int y) {
    auto config_view = m_registry.view<WorldConfigComponent>();
    if (config_view.empty()) return;
    int cell_size = config_view.get<WorldConfigComponent>(config_view.front()).macro_cell_size;

    int mx = x / cell_size;
    int my = y / cell_size;

    auto it = m_zone_cache.find({mx, my});
    if (it != m_zone_cache.end()) {
        m_registry.get<MacroZoneComponent>(it->second).arterial_entities.push_back(arterial);
    }
}
void InfrastructureNetworkSystem::update(double delta_time) { (void)delta_time; }

void InfrastructureNetworkSystem::generate_skeleton(int width, int height) {
    carve_river(width, height);
    carve_primary_roads(width, height);
    carve_rail_line(width, height);
}

void InfrastructureNetworkSystem::generate_capillaries() {
    carve_secondary_roads();
}

void InfrastructureNetworkSystem::carve_river(int width, int height) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> disX(width / 4, width * 3 / 4);
    
    int current_x = disX(gen);
    for (int y = 0; y < height; ++y) {
        if (y % 5 == 0) {
            std::uniform_int_distribution<> drift(-1, 1);
            current_x = std::clamp(current_x + drift(gen), 0, width - 1);
        }
        for (int dx = -1; dx <= 1; ++dx) {
            int rx = std::clamp(current_x + dx, 0, width - 1);
            create_arterial_segment(rx, y, ArterialType::WATERWAY_RIVER);
        }
    }
}

void InfrastructureNetworkSystem::carve_primary_roads(int width, int height) {
    for (int y = 20; y < height; y += 40) {
        for (int x = 0; x < width; ++x) {
            create_arterial_segment(x, y, ArterialType::ROAD_PRIMARY);
            
            // Create bus stop nodes
            if (x % 30 == 0) {
                auto junction = m_registry.create();
                m_registry.emplace<PositionComponent>(junction, x, y, 0);
                auto& node = m_registry.emplace<InfrastructureNodeComponent>(junction);
                node.node_name = "Bus Stop Node";
                m_registry.emplace<RenderableComponent>(junction, '+', "#FFFF00", 1);
                link_arterial_to_zone(junction, x, y);
            }
        }
    }
    for (int x = 20; x < width; x += 40) {
        for (int y = 0; y < height; ++y) {
            create_arterial_segment(x, y, ArterialType::ROAD_PRIMARY);
            
            if (y % 30 == 0) {
                auto junction = m_registry.create();
                m_registry.emplace<PositionComponent>(junction, x, y, 0);
                auto& node = m_registry.emplace<InfrastructureNodeComponent>(junction);
                node.node_name = "Bus Stop Node";
                m_registry.emplace<RenderableComponent>(junction, '+', "#FFFF00", 1);
                link_arterial_to_zone(junction, x, y);
            }
        }
    }
}

void InfrastructureNetworkSystem::carve_rail_line(int width, int height) {
    for (int i = 0; i < std::min(width, height); ++i) {
        create_arterial_segment(i, i, ArterialType::RAIL_ELEVATED);
        
        // Create nodes at intervals for stations
        if (i % 30 == 0) {
            auto junction = m_registry.create();
            m_registry.emplace<PositionComponent>(junction, i, i, 0);
            auto& node = m_registry.emplace<InfrastructureNodeComponent>(junction);
            node.node_name = "Rail Station Node";
            m_registry.emplace<RenderableComponent>(junction, '=', "#00FFFF", 1);
            link_arterial_to_zone(junction, i, i);
        }
    }
}

void InfrastructureNetworkSystem::carve_secondary_roads() {
    auto zone_view = m_registry.view<MacroZoneComponent>();
    for (auto entity : zone_view) {
        const auto& zone = zone_view.get<MacroZoneComponent>(entity);
        int sx = zone.macro_x * 20;
        int sy = zone.macro_y * 20;
        int ex = sx + 19;
        int ey = sy + 19;

        switch (zone.type) {
            case ZoneType::CORPORATE:
            case ZoneType::COMMERCIAL:
                subdivide_block_corporate(sx, sy, ex, ey);
                break;
            case ZoneType::RESIDENTIAL:
                subdivide_block_residential(sx, sy, ex, ey);
                break;
            case ZoneType::SLUM:
                subdivide_block_slum(sx, sy, ex, ey);
                break;
            case ZoneType::INDUSTRIAL:
                subdivide_block_industrial(sx, sy, ex, ey);
                break;
            case ZoneType::PARK:
                subdivide_block_park(sx, sy, ex, ey);
                break;
            case ZoneType::AIRPORT:
                subdivide_block_airport(sx, sy, ex, ey);
                break;
            default:
                break;
        }
    }
}

void InfrastructureNetworkSystem::subdivide_block_airport(int sx, int sy, int ex, int ey) {
    // Airport has a perimeter road and a central connection to the terminal
    for (int x = sx; x <= ex; ++x) {
        create_arterial_segment(x, sy, ArterialType::ROAD_SECONDARY);
        create_arterial_segment(x, ey, ArterialType::ROAD_SECONDARY);
    }
    for (int y = sy; y <= ey; ++y) {
        create_arterial_segment(sx, y, ArterialType::ROAD_SECONDARY);
        create_arterial_segment(ex, y, ArterialType::ROAD_SECONDARY);
    }
    int mid_x = sx + (ex - sx) / 2;
    for (int y = sy; y <= ey; ++y) {
        create_arterial_segment(mid_x, y, ArterialType::ROAD_SECONDARY);
    }
}

void InfrastructureNetworkSystem::subdivide_block_corporate(int sx, int sy, int ex, int ey) {
    int mid_x = sx + (ex - sx) / 2;
    int mid_y = sy + (ey - sy) / 2;
    for (int x = sx; x <= ex; ++x) create_arterial_segment(x, mid_y, ArterialType::ROAD_SECONDARY);
    for (int y = sy; y <= ey; ++y) create_arterial_segment(mid_x, y, ArterialType::ROAD_SECONDARY);
}

void InfrastructureNetworkSystem::subdivide_block_industrial(int sx, int sy, int ex, int ey) {
    int inset = 5;
    for (int x = sx + inset; x <= ex - inset; ++x) {
        create_arterial_segment(x, sy + inset, ArterialType::ROAD_SECONDARY);
        create_arterial_segment(x, ey - inset, ArterialType::ROAD_SECONDARY);
    }
    for (int y = sy + inset; y <= ey - inset; ++y) {
        create_arterial_segment(sx + inset, y, ArterialType::ROAD_SECONDARY);
        create_arterial_segment(ex - inset, y, ArterialType::ROAD_SECONDARY);
    }
}

void InfrastructureNetworkSystem::subdivide_block_residential(int sx, int sy, int ex, int ey) {
    int mid_x = sx + (ex - sx) / 2;
    int mid_y = sy + (ey - sy) / 2;
    for (int y = sy; y <= mid_y; ++y) create_arterial_segment(mid_x, y, ArterialType::ROAD_SECONDARY);
    create_arterial_segment(mid_x - 1, mid_y, ArterialType::ROAD_SECONDARY);
    create_arterial_segment(mid_x + 1, mid_y, ArterialType::ROAD_SECONDARY);
}

void InfrastructureNetworkSystem::subdivide_block_park(int sx, int sy, int ex, int ey) {
    for (int i = 0; i < (ex - sx); ++i) {
        create_arterial_segment(sx + i, sy + i, ArterialType::ROAD_SECONDARY);
    }
}

void InfrastructureNetworkSystem::subdivide_block_slum(int sx, int sy, int ex, int ey) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 19);
    
    int cur_x = sx + dis(gen);
    int cur_y = sy;
    for (int i = 0; i < 30; ++i) {
        create_arterial_segment(cur_x, cur_y, ArterialType::ROAD_SECONDARY);
        std::uniform_int_distribution<> move(0, 3);
        int m = move(gen);
        if (m == 0) cur_x = std::clamp(cur_x + 1, sx, ex);
        else if (m == 1) cur_x = std::clamp(cur_x - 1, sx, ex);
        else if (m == 2) cur_y = std::clamp(cur_y + 1, sy, ey);
        else cur_y = std::clamp(cur_y - 1, sy, ey);
    }
}

void InfrastructureNetworkSystem::resolve_junctions() {
    auto view = m_registry.view<PositionComponent, InfrastructureArterialComponent>();
    
    std::map<std::pair<int, int>, std::vector<entt::entity>> pos_map;
    for (auto entity : view) {
        const auto& pos = view.get<PositionComponent>(entity);
        pos_map[{pos.x, pos.y}].push_back(entity);
    }

    for (auto const& [pos, entities] : pos_map) {
        if (entities.size() < 2) continue;

        bool has_road = false;
        bool has_river = false;
        bool has_rail = false;

        for (auto e : entities) {
            auto type = view.get<InfrastructureArterialComponent>(e).type;
            if (type == ArterialType::ROAD_PRIMARY || type == ArterialType::ROAD_SECONDARY) has_road = true;
            if (type == ArterialType::WATERWAY_RIVER) has_river = true;
            if (type == ArterialType::RAIL_ELEVATED) has_rail = true;
        }

        auto junction = m_registry.create();
        m_registry.emplace<PositionComponent>(junction, pos.first, pos.second, 0);
        auto& node = m_registry.emplace<InfrastructureNodeComponent>(junction);
        char glyph = '+';
        std::string color = "#FFFF00";

        if (has_road && has_river) {
            node.node_name = "Bridge Crossing";
            node.is_bridge = true;
            m_registry.emplace<ConduitFieldComponent>(junction, 2.0f, 0.0f, 1.3f, 0.0f);
            glyph = 'H'; color = "#FFD700";
        } else if (has_road && has_rail) {
            node.node_name = "Rail Level Crossing";
            m_registry.emplace<ConduitFieldComponent>(junction, 1.0f, 0.0f, 1.1f, 0.1f);
            glyph = 'X'; color = "#FF00FF";
        } else if (has_road) {
            node.node_name = "Arterial Intersection";
            m_registry.emplace<ConduitFieldComponent>(junction, 2.0f, 0.0f, 1.2f, 0.0f);
            glyph = '+'; color = "#FFFF00";
        }
        m_registry.emplace<RenderableComponent>(junction, glyph, color, 1);
        link_arterial_to_zone(junction, pos.first, pos.second);
    }
}

void InfrastructureNetworkSystem::create_arterial_segment(int x, int y, ArterialType type) {
    auto entity = m_registry.create();
    m_registry.emplace<PositionComponent>(entity, x, y, 0);
    m_registry.emplace<InfrastructureArterialComponent>(entity, type, 2.0f, true);
    
    auto& field = m_registry.emplace<ConduitFieldComponent>(entity);
    char glyph = '?';
    std::string color = "#FFFFFF";
    
    switch(type) {
        case ArterialType::WATERWAY_RIVER:
            field.radius = 4.0f;
            field.temperature_offset = -5.0f; 
            glyph = '~'; color = "#0055FF";
            break;
        case ArterialType::ROAD_PRIMARY:
            field.radius = 2.0f;
            field.economic_multiplier = 1.2f; 
            glyph = '#'; color = "#AAAAAA";
            break;
        case ArterialType::ROAD_SECONDARY:
            field.radius = 1.0f;
            field.economic_multiplier = 1.05f;
            glyph = '.'; color = "#555555";
            break;
        case ArterialType::RAIL_ELEVATED:
            field.radius = 3.0f;
            field.economic_multiplier = 1.5f; 
            glyph = '='; color = "#00FFFF";
            break;
        default: break;
    }
    m_registry.emplace<RenderableComponent>(entity, glyph, color, 0);
    link_arterial_to_zone(entity, x, y);
}

} // namespace NeonOubliette
