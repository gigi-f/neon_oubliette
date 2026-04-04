#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_INFRASTRUCTURE_INFLUENCE_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_INFRASTRUCTURE_INFLUENCE_SYSTEM_H

#include "simulation_coordinator.h"
#include "../components/components.h"
#include "../components/infrastructure_components.h"
#include "../components/simulation_layers.h"
#include <unordered_map>
#include <vector>

namespace NeonOubliette {

namespace detail_infra {
    struct CellKey {
        int cx, cy, layer;
        bool operator==(const CellKey& o) const { return cx == o.cx && cy == o.cy && layer == o.layer; }
    };
    struct CellKeyHash {
        size_t operator()(const CellKey& k) const {
            size_t h = std::hash<int>()(k.cx);
            h ^= std::hash<int>()(k.cy) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>()(k.layer) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };
}

/**
 * @brief Ticks turns to apply ConduitField effects from infrastructure to 
 *        Physics (L0) and Economic (L3) layers.
 */
class InfrastructureInfluenceSystem : public ISimulationSystem {
public:
    InfrastructureInfluenceSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {}

    void update(double delta_time) override {
        (void)delta_time;

        // Build spatial grid of conduit fields (cell size = max_radius rounded up)
        constexpr int CELL_SIZE = 4; // covers radius up to 4
        using CellKey = detail_infra::CellKey;
        using CellKeyHash = detail_infra::CellKeyHash;

        struct FieldEntry {
            PositionComponent pos;
            ConduitFieldComponent field;
        };
        std::unordered_map<CellKey, std::vector<FieldEntry>, CellKeyHash> field_grid;

        auto field_view = m_registry.view<PositionComponent, ConduitFieldComponent>();
        for (auto field_ent : field_view) {
            const auto& f_pos = field_view.get<PositionComponent>(field_ent);
            const auto& field = field_view.get<ConduitFieldComponent>(field_ent);
            CellKey key{f_pos.x / CELL_SIZE, f_pos.y / CELL_SIZE, f_pos.layer_id};
            field_grid[key].push_back({f_pos, field});
        }

        auto check_nearby_fields = [&](const PositionComponent& p_pos, auto callback) {
            int cx = p_pos.x / CELL_SIZE;
            int cy = p_pos.y / CELL_SIZE;
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    CellKey key{cx + dx, cy + dy, p_pos.layer_id};
                    auto it = field_grid.find(key);
                    if (it == field_grid.end()) continue;
                    for (const auto& entry : it->second) {
                        float fdx = (float)(p_pos.x - entry.pos.x);
                        float fdy = (float)(p_pos.y - entry.pos.y);
                        float dist_sq = fdx*fdx + fdy*fdy;
                        if (dist_sq <= entry.field.radius * entry.field.radius) {
                            callback(entry.field);
                        }
                    }
                }
            }
        };
        
        // 1. Influence Physics (L0)
        auto phys_view = m_registry.view<PositionComponent, Layer0PhysicsComponent>();
        for (auto phys_ent : phys_view) {
            auto& p_pos = phys_view.get<PositionComponent>(phys_ent);
            auto& phys = phys_view.get<Layer0PhysicsComponent>(phys_ent);
            check_nearby_fields(p_pos, [&](const ConduitFieldComponent& field) {
                phys.temperature_celsius += field.temperature_offset * 0.05f; 
            });
        }

        // 2. Influence Economy (L3)
        auto prop_view = m_registry.view<PositionComponent, PropertyComponent>();
        for (auto prop_ent : prop_view) {
            auto& p_pos = prop_view.get<PositionComponent>(prop_ent);
            auto& prop = prop_view.get<PropertyComponent>(prop_ent);
            check_nearby_fields(p_pos, [&](const ConduitFieldComponent& field) {
                if (field.economic_multiplier > 1.0f) {
                    prop.current_market_value = (int)(prop.current_market_value * 1.01f); 
                }
                if (prop.current_market_value > prop.base_value * 3) prop.current_market_value = prop.base_value * 3;
            });
        }
    }

    SimulationLayer simulation_layer() const override {
        return SimulationLayer::L0_Physics; 
    }

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
};

} // namespace NeonOubliette

#endif
