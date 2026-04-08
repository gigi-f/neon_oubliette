#include "infrastructure_network_system.h"
#include <random>
#include <cmath>
#include <map>
#include <algorithm>
#include "../components/zoning_components.h"
#include "../components/lod_components.h"

namespace NeonOubliette {

InfrastructureNetworkSystem::InfrastructureNetworkSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {}

void InfrastructureNetworkSystem::initialize() {
    auto zone_view = m_registry.view<MacroZoneComponent>();
    for (auto entity : zone_view) {
        const auto& zone = zone_view.get<MacroZoneComponent>(entity);
        m_zone_cache[{zone.macro_x, zone.macro_y}] = entity;
    }

    // Re-link all existing arterial segments to their zones.
    // generate_skeleton() ran before zones existed, so arterials were never linked.
    auto arterial_view = m_registry.view<PositionComponent, InfrastructureArterialComponent>();
    for (auto entity : arterial_view) {
        const auto& pos = arterial_view.get<PositionComponent>(entity);
        link_arterial_to_zone(entity, pos.x, pos.y);
    }
    // Also link junction nodes (they have InfrastructureNodeComponent but may not have ArterialComponent)
    auto node_view = m_registry.view<PositionComponent, InfrastructureNodeComponent>();
    for (auto entity : node_view) {
        if (m_registry.all_of<InfrastructureArterialComponent>(entity)) continue; // already linked above
        const auto& pos = node_view.get<PositionComponent>(entity);
        link_arterial_to_zone(entity, pos.x, pos.y);
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
    carve_electric_grid(width, height);
    carve_sewers(width, height);
}

void InfrastructureNetworkSystem::carve_electric_grid(int width, int height) {
    // High-voltage lines follow the primary road grid but with fewer nodes
    for (int y = 20; y < height; y += 40) {
        for (int x = 0; x < width; ++x) {
            create_arterial_segment(x, y, ArterialType::ELECTRIC_GRID);
            
            // Substations at every other intersection
            if (x % 80 == 0) {
                auto junction = m_registry.create();
                m_registry.emplace<PositionComponent>(junction, x, y, 0);
                auto& node = m_registry.emplace<InfrastructureNodeComponent>(junction);
                node.node_name = "Power Substation";
                node.is_substation = true;
                
                // Mark chunk as a source if it's near the edge (simulating external supply)
                if (x == 0 || y == 20) {
                    // Find chunk entity for this position
                    auto chunk_view = m_registry.view<ChunkComponent>();
                    for (auto entity : chunk_view) {
                        const auto& chunk = chunk_view.get<ChunkComponent>(entity);
                        // Assuming 40x40 chunks for now based on roadmap
                        if (x / 40 == chunk.chunk_x && y / 40 == chunk.chunk_y) {
                            if (!m_registry.all_of<PowerGridComponent>(entity)) {
                                m_registry.emplace<PowerGridComponent>(entity, 1.0f, 1.0f, true);
                            } else {
                                m_registry.get<PowerGridComponent>(entity).is_grid_source = true;
                            }
                        }
                    }
                }
                
                link_arterial_to_zone(junction, x, y);
            }
        }
    }
}

void InfrastructureNetworkSystem::carve_sewers(int width, int height) {
    // Sewers follow the main road grid but on Layer -1.
    // They also branch out into industrial and slum zones more heavily.
    for (int y = 20; y < height; y += 40) {
        for (int x = 0; x < width; ++x) {
            create_arterial_segment(x, y, ArterialType::SEWER, -1);
            
            // Access points (manholes) at intersections
            if (x % 40 == 20) {
                auto junction = m_registry.create();
                m_registry.emplace<PositionComponent>(junction, x, y, -1);
                auto& node = m_registry.emplace<InfrastructureNodeComponent>(junction);
                node.node_name = "Sewer Access (Underground)";
                link_arterial_to_zone(junction, x, y);
            }
        }
    }
    for (int x = 20; x < width; x += 40) {
        for (int y = 0; y < height; ++y) {
            create_arterial_segment(x, y, ArterialType::SEWER, -1);
        }
    }
    
    // Some random cross-connections and maintenance tunnels
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> disX(0, width - 1);
    std::uniform_int_distribution<> disY(0, height - 1);
    
    for (int i = 0; i < 50; ++i) {
        int sx = disX(gen), sy = disY(gen);
        int length = 20 + (disX(gen) % 30);
        bool horizontal = (disX(gen) % 2 == 0);
        
        for (int l = 0; l < length; ++l) {
            int cx = horizontal ? std::clamp(sx + l, 0, width - 1) : sx;
            int cy = horizontal ? sy : std::clamp(sy + l, 0, height - 1);
            create_arterial_segment(cx, cy, ArterialType::UNDERGROUND_TUNNEL, -1);
        }
    }
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
                link_arterial_to_zone(junction, x, y);
            }
        }
    }
}

void InfrastructureNetworkSystem::carve_rail_line(int width, int height) {
    for (int y = 0; y < height; y += 80) {
        for (int x = 0; x < width; ++x) {
            create_arterial_segment(x, y, ArterialType::RAIL_ELEVATED, 5);
            if (x % 40 == 0) {
                auto junction = m_registry.create();
                m_registry.emplace<PositionComponent>(junction, x, y, 5);
                auto& node = m_registry.emplace<InfrastructureNodeComponent>(junction);
                node.node_name = "Rail Station Node";
                m_registry.emplace<CommerceHubComponent>(junction, 15.0f, 1.8f);
                link_arterial_to_zone(junction, x, y);
            }
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
    // Rectilinear paths for parks
    int mid_x = sx + (ex - sx) / 2;
    int mid_y = sy + (ey - sy) / 2;
    for (int x = sx; x <= ex; ++x) create_arterial_segment(x, mid_y, ArterialType::ROAD_SECONDARY);
    for (int y = sy; y <= ey; ++y) create_arterial_segment(mid_x, y, ArterialType::ROAD_SECONDARY);
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
        }
        
        link_arterial_to_zone(junction, pos.first, pos.second);
    }
}

void InfrastructureNetworkSystem::create_arterial_segment(int x, int y, ArterialType type, int layer_id) {
    auto entity = m_registry.create();
    m_registry.emplace<PositionComponent>(entity, x, y, layer_id);
    m_registry.emplace<InfrastructureArterialComponent>(entity, type, 2.0f, true);
    
    auto& field = m_registry.emplace<ConduitFieldComponent>(entity);
    
    switch(type) {
        case ArterialType::WATERWAY_RIVER:
            field.radius = 4.0f;
            field.temperature_offset = -5.0f; 
            break;
        case ArterialType::ROAD_PRIMARY:
            field.radius = 2.0f;
            field.economic_multiplier = 1.2f; 
            break;
        case ArterialType::ROAD_SECONDARY:
            field.radius = 1.0f;
            field.economic_multiplier = 1.05f;
            break;
        case ArterialType::ROAD_ALLEY:
            field.radius = 0.5f;
            field.crime_modifier = 0.2f;
            break;
        case ArterialType::SIDEWALK:
            field.radius = 0.5f;
            break;
        case ArterialType::RAIL_ELEVATED:
            field.radius = 3.0f;
            field.economic_multiplier = 1.5f; 
            break;
        case ArterialType::SEWER:
            field.radius = 2.0f;
            field.temperature_offset = -2.0f; // Damp/Cool
            field.crime_modifier = 0.3f;      // Underground is shady
            break;
        case ArterialType::UNDERGROUND_TUNNEL:
            field.radius = 1.0f;
            field.crime_modifier = 0.5f;
            break;
        default: break;
    }
    // No RenderableComponent — conduit segments are infrastructure metadata.
    // Visual representation is handled by terrain tiles created in city_generation_system.
    link_arterial_to_zone(entity, x, y);
}

} // namespace NeonOubliette
