#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_VISIBILITY_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_VISIBILITY_SYSTEM_H

#include <entt/entt.hpp>
#include <set>
#include <cmath>
#include <algorithm>
#include <unordered_set>
#include "../components/components.h"
#include "../event_declarations.h"
#include "../system_scheduler.h"

namespace NeonOubliette {

/**
 * @brief Manages line-of-sight (FOV) and vertical visibility.
 *        Updates VisibilityComponent and MemoryComponent for entities.
 */
class VisibilitySystem : public ISystem {
public:
    VisibilitySystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {
        // Start dirty so first frame builds caches
        m_blocking_dirty = true;
        m_terrain_render_dirty = true;
        m_dispatcher.sink<ChunkChangedEvent>().connect<&VisibilitySystem::on_chunk_changed>(this);
    }

    void invalidate_caches() {
        m_blocking_dirty = true;
        m_terrain_render_dirty = true;
    }

    void on_chunk_changed(const ChunkChangedEvent&) {
        invalidate_caches();
    }

    void update(double delta_time) override {
        (void)delta_time;

        // Spatial caches are only rebuilt when explicitly invalidated
        // (e.g., chunk load/unload calls invalidate_caches())

        // Only compute FOV for the player (agents don't need visual FOV)
        auto view = m_registry.view<PlayerComponent, PositionComponent, VisibilityComponent>();
        for (auto entity : view) {
            auto& pos = view.get<PositionComponent>(entity);
            auto& vis = view.get<VisibilityComponent>(entity);

            vis.visible_tiles.clear();
            calculate_fov(entity, pos, vis);

            // Update MemoryComponent
            if (m_registry.all_of<MemoryComponent>(entity)) {
                update_memory(entity, vis);
            } else {
                m_registry.emplace<MemoryComponent>(entity);
                update_memory(entity, vis);
            }
        }

        // 3. Process Vertical Visibility (Legacy)
        auto vertical_view = m_registry.view<PlayerComponent, PositionComponent, VerticalViewComponent>();
        for (auto player : vertical_view) {
            auto& pos = vertical_view.get<PositionComponent>(player);
            auto& v_view = vertical_view.get<VerticalViewComponent>(player);

            // Vertical shafts logic...
            auto shaft_view = m_registry.view<PositionComponent, ShaftComponent>();
            bool in_shaft = false;
            for (auto shaft : shaft_view) {
                auto& s_pos = shaft_view.get<PositionComponent>(shaft);
                if (s_pos.x == pos.x && s_pos.y == pos.y && s_pos.layer_id == pos.layer_id) {
                    in_shaft = true;
                    break;
                }
            }

            if (in_shaft && m_registry.all_of<VisibilityComponent>(player)) {
                auto& vis = m_registry.get<VisibilityComponent>(player);
                for (int i = 1; i <= v_view.view_distance; ++i) {
                    // This is a bit simplified, ideally vertical visibility allows seeing the whole layer
                    // or at least tiles at the same (x,y) across layers.
                    vis.visible_tiles.insert(PositionComponent(pos.x, pos.y, pos.layer_id + i));
                    vis.visible_tiles.insert(PositionComponent(pos.x, pos.y, pos.layer_id - i));
                }
            }
        }
    }

private:
    static constexpr int OVERWORLD_LONG_VIEW_RANGE_METERS = 2000;
    // Rendering cannot display kilometers of map at once; cap computed FOV radius
    // to keep visibility updates proportional to playable on-screen density.
    static constexpr int MAX_COMPUTED_VIEW_RANGE_TILES = 96;
    static constexpr float MIN_OCCLUDER_HEIGHT_METERS = 2.0f;

    bool is_tall_visibility_blocker(entt::entity entity) const {
        if (!m_registry.valid(entity)) return false;

        // Signs are decorative overlays and should not occlude long-distance LOS.
        if (m_registry.all_of<FacadeSignComponent>(entity)) return false;

        if (auto* terrain = m_registry.try_get<TerrainComponent>(entity)) {
            if (terrain->type == TerrainType::WINDOW) return false;
            return terrain->type == TerrainType::WALL;
        }

        if (auto* building = m_registry.try_get<BuildingComponent>(entity)) {
            float approximate_height = static_cast<float>(std::max(1, building->height)) * 3.0f;
            return approximate_height >= MIN_OCCLUDER_HEIGHT_METERS;
        }

        if (auto* size = m_registry.try_get<SizeComponent>(entity)) {
            float approximate_height = static_cast<float>(std::max(1, size->height));
            return approximate_height >= MIN_OCCLUDER_HEIGHT_METERS;
        }

        // Unknown obstacle types default to "not tall enough" so small clutter does
        // not collapse long-range visibility.
        return false;
    }

    void calculate_fov(entt::entity entity, const PositionComponent& pos, VisibilityComponent& vis) {
        (void)entity;
        int range = vis.view_range;
        if (pos.layer_id == 0) range = std::max(range, OVERWORLD_LONG_VIEW_RANGE_METERS);
        range = std::min(range, MAX_COMPUTED_VIEW_RANGE_TILES);
        if (range <= 0) return;
        int layer = pos.layer_id;

        // Build blocking set ONCE per FOV calculation (cached across rays)
        if (m_blocking_dirty) {
            m_blocking_set.clear();
            auto obstacle_view = m_registry.view<PositionComponent, ObstacleComponent>();
            for (auto obs : obstacle_view) {
                if (!is_tall_visibility_blocker(obs)) continue;

                const auto& o_pos = obstacle_view.get<PositionComponent>(obs);
                if (auto* size = m_registry.try_get<SizeComponent>(obs)) {
                    for (int dx = 0; dx < std::max(1, size->width); ++dx) {
                        for (int dy = 0; dy < std::max(1, size->height); ++dy) {
                            m_blocking_set.insert(PositionComponent(o_pos.x + dx, o_pos.y + dy, o_pos.layer_id));
                        }
                    }
                } else {
                    m_blocking_set.insert(o_pos);
                }
            }

            auto terrain_view = m_registry.view<PositionComponent, TerrainComponent>();
            for (auto ter : terrain_view) {
                if (terrain_view.get<TerrainComponent>(ter).type == TerrainType::WALL) {
                    m_blocking_set.insert(terrain_view.get<PositionComponent>(ter));
                }
            }
            m_blocking_dirty = false;
        }

        vis.visible_tiles.insert(pos);

        static constexpr int multipliers[4][8] = {
            { 1, 0, 0, -1, -1, 0, 0, 1 },
            { 0, 1, -1, 0, 0, -1, 1, 0 },
            { 0, 1, 1, 0, 0, -1, -1, 0 },
            { 1, 0, 0, 1, -1, 0, 0, -1 }
        };

        for (int oct = 0; oct < 8; ++oct) {
            cast_light(pos.x,
                       pos.y,
                       1,
                       1.0f,
                       0.0f,
                       range,
                       multipliers[0][oct],
                       multipliers[1][oct],
                       multipliers[2][oct],
                       multipliers[3][oct],
                       layer,
                       vis);
        }
    }

    void cast_light(int cx,
                    int cy,
                    int row,
                    float start,
                    float end,
                    int radius,
                    int xx,
                    int xy,
                    int yx,
                    int yy,
                    int layer,
                    VisibilityComponent& vis) {
        if (start < end) return;

        float next_start = start;
        for (int dist = row; dist <= radius; ++dist) {
            bool blocked = false;
            int dy = -dist;

            for (int dx = -dist; dx <= 0; ++dx) {
                float l_slope = (dx - 0.5f) / (dy + 0.5f);
                float r_slope = (dx + 0.5f) / (dy - 0.5f);

                if (start < r_slope) continue;
                if (end > l_slope) break;

                int sax = dx * xx + dy * xy;
                int say = dx * yx + dy * yy;
                int ax = cx + sax;
                int ay = cy + say;

                PositionComponent tile_pos(ax, ay, layer);
                if ((dx * dx + dy * dy) <= (radius * radius)) {
                    vis.visible_tiles.insert(tile_pos);
                }

                const bool tile_blocks = (m_blocking_set.count(tile_pos) > 0);
                if (blocked) {
                    if (tile_blocks) {
                        next_start = r_slope;
                    } else {
                        blocked = false;
                        start = next_start;
                    }
                } else if (tile_blocks && dist < radius) {
                    blocked = true;
                    cast_light(cx, cy, dist + 1, start, l_slope, radius, xx, xy, yx, yy, layer, vis);
                    next_start = r_slope;
                }
            }

            if (blocked) break;
        }
    }

    void update_memory(entt::entity player, const VisibilityComponent& vis) {
        auto& memory = m_registry.get<MemoryComponent>(player);
        
        // Build position->renderable lookup ONCE per update
        if (m_terrain_render_dirty) {
            m_terrain_render_map.clear();
            auto terrain_view = m_registry.view<PositionComponent, TerrainComponent, RenderableComponent>();
            for (auto entity : terrain_view) {
                auto& p = terrain_view.get<PositionComponent>(entity);
                auto& r = terrain_view.get<RenderableComponent>(entity);
                m_terrain_render_map[p] = { r.glyph, r.color };
            }
            m_terrain_render_dirty = false;
        }

        for (const auto& pos : vis.visible_tiles) {
            auto it = m_terrain_render_map.find(pos);
            if (it != m_terrain_render_map.end()) {
                memory.remembered_tiles[pos] = it->second;
            } else {
                memory.remembered_tiles[pos] = { ' ', "#222222" };
            }
        }
    }

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;

    // Spatial caches rebuilt per frame to avoid O(n) scans in inner loops
    bool m_blocking_dirty = true;
    std::unordered_set<PositionComponent> m_blocking_set;
    bool m_terrain_render_dirty = true;
    std::unordered_map<PositionComponent, MemoryComponent::RememberedTile> m_terrain_render_map;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_VISIBILITY_SYSTEM_H
