#include "social_interaction_system.h"
#include <iostream>
#include <cmath>
#include <unordered_map>

namespace NeonOubliette {

namespace {
    struct GridKey {
        int cx, cy, layer;
        bool operator==(const GridKey& o) const { return cx == o.cx && cy == o.cy && layer == o.layer; }
    };
    struct GridKeyHash {
        size_t operator()(const GridKey& k) const {
            size_t h = std::hash<int>()(k.cx);
            h ^= std::hash<int>()(k.cy) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>()(k.layer) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };
}

void SocialInteractionSystem::update(double delta_time) {
    (void)delta_time;
    
    // 1. Tick down yielding states
    auto yield_view = m_registry.view<SocialHierarchyComponent>();
    for (auto entity : yield_view) {
        auto& hierarchy = yield_view.get<SocialHierarchyComponent>(entity);
        if (hierarchy.yield_ticks_remaining > 0) {
            hierarchy.yield_ticks_remaining--;
            if (hierarchy.yield_ticks_remaining == 0) {
                hierarchy.currently_yielding_to = entt::null;
            }
        }
    }

    // 2. Build spatial grid for O(1) proximity lookup (cell size = 3 to cover interaction range of 2)
    constexpr int CELL_SIZE = 3;
    std::unordered_map<GridKey, std::vector<entt::entity>, GridKeyHash> grid;

    auto agent_view = m_registry.view<PositionComponent, SocialHierarchyComponent, AgentComponent>();
    for (auto agent : agent_view) {
        const auto& pos = agent_view.get<PositionComponent>(agent);
        GridKey key{pos.x / CELL_SIZE, pos.y / CELL_SIZE, pos.layer_id};
        grid[key].push_back(agent);
    }

    // 3. Check only agents in same or adjacent cells
    for (auto& [key, agents] : grid) {
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                GridKey neighbor_key{key.cx + dx, key.cy + dy, key.layer};
                auto it = grid.find(neighbor_key);
                if (it == grid.end()) continue;

                for (auto agent_a : agents) {
                    const auto& pos_a = agent_view.get<PositionComponent>(agent_a);
                    const auto& hierarchy_a = agent_view.get<SocialHierarchyComponent>(agent_a);

                    for (auto agent_b : it->second) {
                        if (agent_a >= agent_b) continue; // avoid duplicate & self pairs

                        const auto& pos_b = agent_view.get<PositionComponent>(agent_b);
                        const auto& hierarchy_b = agent_view.get<SocialHierarchyComponent>(agent_b);

                        int dist = std::abs(pos_a.x - pos_b.x) + std::abs(pos_a.y - pos_b.y);
                        if (dist <= 2) {
                            float delta = hierarchy_a.status - hierarchy_b.status;
                            if (delta > STATUS_DELTA_THRESHOLD) {
                                handle_social_yielding(agent_b, agent_a);
                            } else if (delta < -STATUS_DELTA_THRESHOLD) {
                                handle_social_yielding(agent_a, agent_b);
                            }
                        }
                    }
                }
            }
        }
    }
}

void SocialInteractionSystem::handle_social_yielding(entt::entity yielder, entt::entity master) {
    auto& hierarchy = m_registry.get<SocialHierarchyComponent>(yielder);
    
    // Only update if not already yielding or yielding to someone lower status
    if (hierarchy.currently_yielding_to == master) {
        hierarchy.yield_ticks_remaining = YIELD_DURATION_TICKS;
        return;
    }
    
    hierarchy.currently_yielding_to = master;
    hierarchy.yield_ticks_remaining = YIELD_DURATION_TICKS;

    // Apply mechanical effect: Frustration increase for autonomous agents
    if (hierarchy.is_autonomous) {
        if (auto* needs = m_registry.try_get<NeedsComponent>(yielder)) {
            needs->frustration += 0.5f;
        }
    }
    
    // Log social interaction (optional/debug)
    // std::cout << "[Social] " << (int)entt::to_integral(yielder) << " yielding to " << (int)entt::to_integral(master) << std::endl;
}

} // namespace NeonOubliette
