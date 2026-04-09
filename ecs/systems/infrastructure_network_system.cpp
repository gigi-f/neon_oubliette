#include "infrastructure_network_system.h"
#include <random>
#include <cmath>
#include <map>
#include <set>
#include <tuple>
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

    // Re-link all existing segments to their zones.
    // generate_skeleton() ran before zones existed, so segments were never linked.
    auto seg_view = m_registry.view<InfrastructureSegmentComponent>();
    for (auto entity : seg_view) {
        const auto& seg = seg_view.get<InfrastructureSegmentComponent>(entity);
        link_segment_to_zones(entity, seg.x1, seg.y1, seg.x2, seg.y2);
    }
    // Also link junction nodes (they have InfrastructureNodeComponent)
    auto node_view = m_registry.view<PositionComponent, InfrastructureNodeComponent>();
    for (auto entity : node_view) {
        const auto& pos = node_view.get<PositionComponent>(entity);
        link_node_to_zone(entity, pos.x, pos.y);
    }

    // Build ArterialGrid spatial index from all segments
    build_arterial_grid();
}

void InfrastructureNetworkSystem::link_node_to_zone(entt::entity node, int x, int y) {
    auto config_view = m_registry.view<WorldConfigComponent>();
    if (config_view.empty()) return;
    int cell_size = config_view.get<WorldConfigComponent>(config_view.front()).macro_cell_size;

    int mx = x / cell_size;
    int my = y / cell_size;

    auto it = m_zone_cache.find({mx, my});
    if (it != m_zone_cache.end()) {
        m_registry.get<MacroZoneComponent>(it->second).arterial_entities.push_back(node);
    }
}

void InfrastructureNetworkSystem::link_segment_to_zones(entt::entity seg_entity, int x1, int y1, int x2, int y2) {
    auto config_view = m_registry.view<WorldConfigComponent>();
    if (config_view.empty()) return;
    int cell_size = config_view.get<WorldConfigComponent>(config_view.front()).macro_cell_size;
    if (cell_size <= 0) return;

    std::set<std::pair<int,int>> visited_zones;

    if (y1 == y2) {
        // Horizontal segment
        int y = y1;
        int my = y / cell_size;
        int mx_min = std::min(x1, x2) / cell_size;
        int mx_max = std::max(x1, x2) / cell_size;
        for (int mx = mx_min; mx <= mx_max; ++mx)
            visited_zones.insert({mx, my});
    } else if (x1 == x2) {
        // Vertical segment
        int x = x1;
        int mx = x / cell_size;
        int my_min = std::min(y1, y2) / cell_size;
        int my_max = std::max(y1, y2) / cell_size;
        for (int my = my_min; my <= my_max; ++my)
            visited_zones.insert({mx, my});
    } else {
        // Point or diagonal — link to single zone
        visited_zones.insert({x1 / cell_size, y1 / cell_size});
    }

    for (const auto& mz : visited_zones) {
        auto it = m_zone_cache.find(mz);
        if (it != m_zone_cache.end()) {
            m_registry.get<MacroZoneComponent>(it->second).arterial_entities.push_back(seg_entity);
        }
    }
}

void InfrastructureNetworkSystem::build_arterial_grid() {
    auto* grid_ptr = m_registry.ctx().find<ArterialGrid>();
    if (!grid_ptr) {
        m_registry.ctx().emplace<ArterialGrid>();
        grid_ptr = m_registry.ctx().find<ArterialGrid>();
    }
    grid_ptr->clear();

    auto seg_view = m_registry.view<InfrastructureSegmentComponent>();
    for (auto entity : seg_view) {
        const auto& seg = seg_view.get<InfrastructureSegmentComponent>(entity);
        grid_ptr->add_segment(seg);
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

// ---------- Field defaults per ArterialType ----------
static void apply_field_defaults(InfrastructureSegmentComponent& seg) {
    switch (seg.type) {
        case ArterialType::WATERWAY_RIVER:
            seg.field_radius = 4.0f; seg.temperature_offset = -5.0f; break;
        case ArterialType::ROAD_PRIMARY:
            seg.field_radius = 2.0f; seg.economic_multiplier = 1.2f; break;
        case ArterialType::ROAD_SECONDARY:
            seg.field_radius = 1.0f; seg.economic_multiplier = 1.05f; break;
        case ArterialType::ROAD_ALLEY:
            seg.field_radius = 0.5f; seg.crime_modifier = 0.2f; break;
        case ArterialType::SIDEWALK:
            seg.field_radius = 0.5f; break;
        case ArterialType::RAIL_ELEVATED:
            seg.field_radius = 3.0f; seg.economic_multiplier = 1.5f; break;
        case ArterialType::SEWER:
            seg.field_radius = 2.0f; seg.temperature_offset = -2.0f; seg.crime_modifier = 0.3f; break;
        case ArterialType::UNDERGROUND_TUNNEL:
            seg.field_radius = 1.0f; seg.crime_modifier = 0.5f; break;
        default: break;
    }
}

entt::entity InfrastructureNetworkSystem::create_line_segment(int x1, int y1, int x2, int y2,
                                                               ArterialType type, int layer_id) {
    auto entity = m_registry.create();
    auto& seg = m_registry.emplace<InfrastructureSegmentComponent>(entity);
    seg.type = type;
    seg.x1 = x1; seg.y1 = y1;
    seg.x2 = x2; seg.y2 = y2;
    seg.layer_id = layer_id;
    apply_field_defaults(seg);
    return entity;
}

// ---------- Skeleton carving functions ----------

void InfrastructureNetworkSystem::carve_electric_grid(int width, int height) {
    int cell_size = 20;
    auto config_view = m_registry.view<WorldConfigComponent>();
    if (!config_view.empty()) cell_size = config_view.get<WorldConfigComponent>(config_view.front()).macro_cell_size;

    for (int y = cell_size; y < height; y += cell_size * 2) {
        // One horizontal segment spanning the full width
        create_line_segment(0, y, width - 1, y, ArterialType::ELECTRIC_GRID);

        // Substations at every other intersection
        for (int x = 0; x < width; x += cell_size * 4) {
            auto junction = m_registry.create();
            m_registry.emplace<PositionComponent>(junction, x, y, 0);
            auto& node = m_registry.emplace<InfrastructureNodeComponent>(junction);
            node.node_name = "Power Substation";
            node.is_substation = true;

            // Mark chunk as a source if it's near the edge (simulating external supply)
            if (x == 0 || y == cell_size) {
                auto chunk_view = m_registry.view<ChunkComponent>();
                for (auto chunk_ent : chunk_view) {
                    const auto& chunk = chunk_view.get<ChunkComponent>(chunk_ent);
                    if (x / (cell_size / 2) == chunk.chunk_x && y / (cell_size / 2) == chunk.chunk_y) {
                        if (!m_registry.all_of<PowerGridComponent>(chunk_ent)) {
                            m_registry.emplace<PowerGridComponent>(chunk_ent, 1.0f, 1.0f, true);
                        } else {
                            m_registry.get<PowerGridComponent>(chunk_ent).is_grid_source = true;
                        }
                    }
                }
            }

            link_node_to_zone(junction, x, y);
        }
    }
}

void InfrastructureNetworkSystem::carve_sewers(int width, int height) {
    int cell_size = 20;
    auto config_view = m_registry.view<WorldConfigComponent>();
    if (!config_view.empty()) cell_size = config_view.get<WorldConfigComponent>(config_view.front()).macro_cell_size;

    // Horizontal sewer lines
    for (int y = cell_size; y < height; y += cell_size * 2) {
        create_line_segment(0, y, width - 1, y, ArterialType::SEWER, -1);

        // Manholes at intersections
        for (int x = cell_size; x < width; x += cell_size * 2) {
            auto junction = m_registry.create();
            m_registry.emplace<PositionComponent>(junction, x, y, -1);
            auto& node = m_registry.emplace<InfrastructureNodeComponent>(junction);
            node.node_name = "Sewer Access (Underground)";
            link_node_to_zone(junction, x, y);
        }
    }

    // Vertical sewer lines
    for (int x = cell_size; x < width; x += cell_size * 2) {
        create_line_segment(x, 0, x, height - 1, ArterialType::SEWER, -1);
    }

    // Some random cross-connections and maintenance tunnels
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> disX(0, width - 1);
    std::uniform_int_distribution<> disY(0, height - 1);

    for (int i = 0; i < 50; ++i) {
        int sx = disX(gen), sy = disY(gen);
        int length = cell_size + (disX(gen) % cell_size);
        bool horizontal = (disX(gen) % 2 == 0);

        if (horizontal) {
            int ex = std::clamp(sx + length, 0, width - 1);
            create_line_segment(sx, sy, ex, sy, ArterialType::UNDERGROUND_TUNNEL, -1);
        } else {
            int ey = std::clamp(sy + length, 0, height - 1);
            create_line_segment(sx, sy, sx, ey, ArterialType::UNDERGROUND_TUNNEL, -1);
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
    int half_r = RIVER_WIDTH / 2;
    int seg_start_y = 0;

    for (int y = 0; y < height; ++y) {
        if (y % 10 == 0 && y > 0) {
            // End current segment at y-1, start new after drift
            create_line_segment(current_x, seg_start_y, current_x, y - 1, ArterialType::WATERWAY_RIVER);

            std::uniform_int_distribution<> drift(-2, 2);
            current_x = std::clamp(current_x + drift(gen), half_r, width - half_r);
            seg_start_y = y;
        }
    }
    // Final segment
    if (seg_start_y < height) {
        create_line_segment(current_x, seg_start_y, current_x, height - 1, ArterialType::WATERWAY_RIVER);
    }
}

void InfrastructureNetworkSystem::carve_primary_roads(int width, int height) {
    int cell_size = 20;
    auto config_view = m_registry.view<WorldConfigComponent>();
    if (!config_view.empty()) cell_size = config_view.get<WorldConfigComponent>(config_view.front()).macro_cell_size;

    // Horizontal primary roads — one segment per road line
    for (int y = cell_size; y < height; y += cell_size) {
        create_line_segment(0, y, width - 1, y, ArterialType::ROAD_PRIMARY);

        // Intersection nodes at grid crossings
        for (int x = 0; x < width; x += cell_size) {
            auto junction = m_registry.create();
            m_registry.emplace<PositionComponent>(junction, x, y, 0);
            auto& node = m_registry.emplace<InfrastructureNodeComponent>(junction);
            node.node_name = "Primary Intersection";
            link_node_to_zone(junction, x, y);
        }
        // Bus stops
        for (int x = cell_size / 2; x < width; x += cell_size * 2) {
            auto bus = m_registry.create();
            m_registry.emplace<PositionComponent>(bus, x, y, 0);
            auto& node = m_registry.emplace<InfrastructureNodeComponent>(bus);
            node.node_name = "Bus Stop Node";
            link_node_to_zone(bus, x, y);
        }
    }

    // Vertical primary roads — one segment per road line
    for (int x = cell_size; x < width; x += cell_size) {
        create_line_segment(x, 0, x, height - 1, ArterialType::ROAD_PRIMARY);

        // Bus stops on vertical roads
        for (int y = 0; y < height; y += cell_size * 2) {
            auto bus = m_registry.create();
            m_registry.emplace<PositionComponent>(bus, x, y, 0);
            auto& node = m_registry.emplace<InfrastructureNodeComponent>(bus);
            node.node_name = "Bus Stop Node";
            link_node_to_zone(bus, x, y);
        }
    }
}

void InfrastructureNetworkSystem::carve_rail_line(int width, int height) {
    int cell_size = 20;
    auto config_view = m_registry.view<WorldConfigComponent>();
    if (!config_view.empty()) cell_size = config_view.get<WorldConfigComponent>(config_view.front()).macro_cell_size;

    for (int y = 0; y < height; y += cell_size * 4) {
        // One horizontal segment per rail line
        create_line_segment(0, y, width - 1, y, ArterialType::RAIL_ELEVATED, 5);

        // Station nodes
        for (int x = 0; x < width; x += cell_size * 2) {
            auto junction = m_registry.create();
            m_registry.emplace<PositionComponent>(junction, x, y, 5);
            auto& node = m_registry.emplace<InfrastructureNodeComponent>(junction);
            node.node_name = "Rail Station Node";
            m_registry.emplace<CommerceHubComponent>(junction, 15.0f, 1.8f);
            link_node_to_zone(junction, x, y);
        }
    }
}

// ---------- Secondary roads (one segment per line) ----------

void InfrastructureNetworkSystem::carve_secondary_roads() {
    auto zone_view = m_registry.view<MacroZoneComponent>();
    int cell_size = 20;
    auto config_view = m_registry.view<WorldConfigComponent>();
    if (!config_view.empty()) cell_size = config_view.get<WorldConfigComponent>(config_view.front()).macro_cell_size;

    for (auto entity : zone_view) {
        const auto& zone = zone_view.get<MacroZoneComponent>(entity);
        int sx = zone.macro_x * cell_size;
        int sy = zone.macro_y * cell_size;
        int ex = sx + cell_size - 1;
        int ey = sy + cell_size - 1;

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
    // Airport: perimeter road + central vertical
    create_line_segment(sx, sy, ex, sy, ArterialType::ROAD_SECONDARY); // top
    create_line_segment(sx, ey, ex, ey, ArterialType::ROAD_SECONDARY); // bottom
    create_line_segment(sx, sy, sx, ey, ArterialType::ROAD_SECONDARY); // left
    create_line_segment(ex, sy, ex, ey, ArterialType::ROAD_SECONDARY); // right
    int mid_x = sx + (ex - sx) / 2;
    create_line_segment(mid_x, sy, mid_x, ey, ArterialType::ROAD_SECONDARY); // center
}

void InfrastructureNetworkSystem::subdivide_block_corporate(int sx, int sy, int ex, int ey) {
    int mid_x = sx + (ex - sx) / 2;
    int mid_y = sy + (ey - sy) / 2;
    create_line_segment(sx, mid_y, ex, mid_y, ArterialType::ROAD_SECONDARY);
    create_line_segment(mid_x, sy, mid_x, ey, ArterialType::ROAD_SECONDARY);
}

void InfrastructureNetworkSystem::subdivide_block_industrial(int sx, int sy, int ex, int ey) {
    int inset = 5;
    create_line_segment(sx + inset, sy + inset, ex - inset, sy + inset, ArterialType::ROAD_SECONDARY);
    create_line_segment(sx + inset, ey - inset, ex - inset, ey - inset, ArterialType::ROAD_SECONDARY);
    create_line_segment(sx + inset, sy + inset, sx + inset, ey - inset, ArterialType::ROAD_SECONDARY);
    create_line_segment(ex - inset, sy + inset, ex - inset, ey - inset, ArterialType::ROAD_SECONDARY);
}

void InfrastructureNetworkSystem::subdivide_block_residential(int sx, int sy, int ex, int ey) {
    int mid_x = sx + (ex - sx) / 2;
    int mid_y = sy + (ey - sy) / 2;
    create_line_segment(mid_x, sy, mid_x, mid_y, ArterialType::ROAD_SECONDARY);
    // T-junction extra tiles
    create_line_segment(mid_x - 1, mid_y, mid_x + 1, mid_y, ArterialType::ROAD_SECONDARY);
}

void InfrastructureNetworkSystem::subdivide_block_park(int sx, int sy, int ex, int ey) {
    int mid_x = sx + (ex - sx) / 2;
    int mid_y = sy + (ey - sy) / 2;
    create_line_segment(sx, mid_y, ex, mid_y, ArterialType::ROAD_SECONDARY);
    create_line_segment(mid_x, sy, mid_x, ey, ArterialType::ROAD_SECONDARY);
}

void InfrastructureNetworkSystem::subdivide_block_slum(int sx, int sy, int ex, int ey) {
    // Slums: random walk path — create point segments for each step
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 19);

    int cur_x = sx + dis(gen);
    int cur_y = sy;
    for (int i = 0; i < 30; ++i) {
        create_line_segment(cur_x, cur_y, cur_x, cur_y, ArterialType::ROAD_SECONDARY);
        std::uniform_int_distribution<> move(0, 3);
        int m = move(gen);
        if (m == 0) cur_x = std::clamp(cur_x + 1, sx, ex);
        else if (m == 1) cur_x = std::clamp(cur_x - 1, sx, ex);
        else if (m == 2) cur_y = std::clamp(cur_y + 1, sy, ey);
        else cur_y = std::clamp(cur_y - 1, sy, ey);
    }
}

// ---------- Junction resolution (geometric segment crossings) ----------

void InfrastructureNetworkSystem::resolve_junctions() {
    struct HSeg { int y, x_min, x_max; ArterialType type; int layer; };
    struct VSeg { int x, y_min, y_max; ArterialType type; int layer; };
    std::vector<HSeg> h_segs;
    std::vector<VSeg> v_segs;

    auto seg_view = m_registry.view<InfrastructureSegmentComponent>();
    for (auto entity : seg_view) {
        const auto& seg = seg_view.get<InfrastructureSegmentComponent>(entity);
        if (seg.is_horizontal() && seg.x1 != seg.x2) {
            h_segs.push_back({seg.y1, std::min(seg.x1, seg.x2), std::max(seg.x1, seg.x2), seg.type, seg.layer_id});
        }
        if (seg.is_vertical() && seg.y1 != seg.y2) {
            v_segs.push_back({seg.x1, std::min(seg.y1, seg.y2), std::max(seg.y1, seg.y2), seg.type, seg.layer_id});
        }
    }

    // Detect H-V crossings on the same layer where types differ
    struct CrossingInfo {
        std::set<ArterialType> types;
    };
    std::map<std::tuple<int,int,int>, CrossingInfo> crossings; // (x, y, layer) → types

    for (const auto& h : h_segs) {
        for (const auto& v : v_segs) {
            if (h.layer != v.layer) continue;
            if (v.x >= h.x_min && v.x <= h.x_max && h.y >= v.y_min && h.y <= v.y_max) {
                auto& info = crossings[std::make_tuple(v.x, h.y, h.layer)];
                info.types.insert(h.type);
                info.types.insert(v.type);
            }
        }
    }

    // Create junction entities for multi-type crossings
    for (const auto& [pos_key, info] : crossings) {
        if (info.types.size() < 2) continue; // Same-type crossings handled by explicit intersection nodes

        auto [x, y, layer] = pos_key;

        bool has_road = false, has_river = false, has_rail = false;
        for (auto t : info.types) {
            if (t == ArterialType::ROAD_PRIMARY || t == ArterialType::ROAD_SECONDARY) has_road = true;
            if (t == ArterialType::WATERWAY_RIVER) has_river = true;
            if (t == ArterialType::RAIL_ELEVATED) has_rail = true;
        }

        auto junction = m_registry.create();
        m_registry.emplace<PositionComponent>(junction, x, y, layer);
        auto& node = m_registry.emplace<InfrastructureNodeComponent>(junction);

        if (has_road && has_river) {
            node.node_name = "Bridge Crossing";
            node.is_bridge = true;
            m_registry.emplace<ConduitFieldComponent>(junction, 2.0f, 0.0f, 1.3f, 0.0f);
        } else if (has_road && has_rail) {
            node.node_name = "Rail Level Crossing";
            m_registry.emplace<ConduitFieldComponent>(junction, 1.0f, 0.0f, 1.1f, 0.1f);
        } else if (has_road) {
            node.node_name = "Arterial Intersection";
            m_registry.emplace<ConduitFieldComponent>(junction, 2.0f, 0.0f, 1.2f, 0.0f);
        } else {
            node.node_name = "Infrastructure Junction";
        }

        link_node_to_zone(junction, x, y);
    }

    // Rebuild ArterialGrid to include all segments (skeleton + city planner)
    build_arterial_grid();
}

} // namespace NeonOubliette
