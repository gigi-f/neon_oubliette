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

namespace detail {
    struct PosHash {
        size_t operator()(const PositionComponent& p) const {
            // Combine x, y, layer_id into a single hash
            size_t h = std::hash<int>()(p.x);
            h ^= std::hash<int>()(p.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>()(p.layer_id) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };
}

/**
 * @brief Manages line-of-sight (FOV) and vertical visibility.
 *        Updates VisibilityComponent and MemoryComponent for entities.
 */
class VisibilitySystem : public ISystem {
public:
    VisibilitySystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {}

    void update(double delta_time) override {
        (void)delta_time;

        // Mark spatial caches as needing rebuild this frame
        m_blocking_dirty = true;
        m_terrain_render_dirty = true;

        // 1. Process Horizontal FOV for all entities with VisibilityComponent
        auto view = m_registry.view<PositionComponent, VisibilityComponent>();
        for (auto entity : view) {
            auto& pos = view.get<PositionComponent>(entity);
            auto& vis = view.get<VisibilityComponent>(entity);

            vis.visible_tiles.clear();
            calculate_fov(entity, pos, vis);

            // 2. If it's a player, update MemoryComponent
            if (m_registry.all_of<PlayerComponent>(entity)) {
                if (m_registry.all_of<MemoryComponent>(entity)) {
                    update_memory(entity, vis);
                } else {
                    m_registry.emplace<MemoryComponent>(entity);
                    update_memory(entity, vis);
                }
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
    void calculate_fov(entt::entity entity, const PositionComponent& pos, VisibilityComponent& vis) {
        int range = vis.view_range;
        int layer = pos.layer_id;

        // Build blocking set ONCE per FOV calculation (cached across rays)
        if (m_blocking_dirty) {
            m_blocking_set.clear();
            auto obstacle_view = m_registry.view<PositionComponent, ObstacleComponent>();
            for (auto obs : obstacle_view) {
                const auto& o_pos = obstacle_view.get<PositionComponent>(obs);
                m_blocking_set.insert(o_pos);
            }
            auto terrain_view = m_registry.view<PositionComponent, TerrainComponent>();
            for (auto ter : terrain_view) {
                if (terrain_view.get<TerrainComponent>(ter).type == TerrainType::WALL) {
                    m_blocking_set.insert(terrain_view.get<PositionComponent>(ter));
                }
            }
            m_blocking_dirty = false;
        }

        // Use 2-degree steps instead of 1-degree (180 rays vs 360 — same visual quality)
        for (int angle = 0; angle < 360; angle += 2) {
            float rad = angle * (float)M_PI / 180.0f;
            float dx = std::cos(rad);
            float dy = std::sin(rad);

            float cx = pos.x + 0.5f;
            float cy = pos.y + 0.5f;

            for (int r = 0; r <= range; ++r) {
                int tx = (int)cx;
                int ty = (int)cy;

                PositionComponent tile_pos(tx, ty, layer);
                vis.visible_tiles.insert(tile_pos);

                if (m_blocking_set.count(tile_pos)) break;

                cx += dx;
                cy += dy;
            }
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
    std::unordered_set<PositionComponent, detail::PosHash> m_blocking_set;
    bool m_terrain_render_dirty = true;
    std::map<PositionComponent, MemoryComponent::RememberedTile> m_terrain_render_map;
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_VISIBILITY_SYSTEM_H
