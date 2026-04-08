#include "power_grid_system.h"
#include "../components/components.h"
#include "../components/lod_components.h"
#include <algorithm>
#include <random>

namespace NeonOubliette::Systems {

PowerGridSystem::PowerGridSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : m_registry(registry), m_dispatcher(dispatcher) {
    
    m_dispatcher.sink<CrisisStartedEvent>().connect<&PowerGridSystem::on_crisis_started>(this);
    m_dispatcher.sink<CrisisResolvedEvent>().connect<&PowerGridSystem::on_crisis_resolved>(this);
    m_dispatcher.sink<PowerOutageEvent>().connect<&PowerGridSystem::on_power_outage>(this);
}

void PowerGridSystem::initialize() {
    // Initial check: ensure all chunks have a PowerGridComponent
    auto chunk_view = m_registry.view<ChunkComponent>();
    for (auto entity : chunk_view) {
        if (!m_registry.all_of<PowerGridComponent>(entity)) {
            m_registry.emplace<PowerGridComponent>(entity);
        }
    }
}

void PowerGridSystem::update(double delta_time) {
    (void)delta_time; // Turn-based simulation
    update_power_flow();
}

void PowerGridSystem::update_power_flow() {
    auto grid_view = m_registry.view<PowerGridComponent, ChunkComponent>();
    
    // 1. Reset power for non-sources (power is propagated from sources each tick)
    for (auto entity : grid_view) {
        auto& grid = grid_view.get<PowerGridComponent>(entity);
        if (!grid.is_grid_source) {
            // Decay power slightly if not actively supplied
            grid.power_level = std::max(0.0f, grid.power_level - 0.05f);
        } else {
            // Sources are always at 1.0 (unless sabotaged/failed)
            grid.power_level = 1.0f;
        }
    }

    // 2. Propagate power across connected chunks
    // Simple propagation: chunks pass power to neighbors
    // Note: In a more complex sim, we'd use the InfrastructureArterial graph.
    for (auto entity : grid_view) {
        auto& grid = grid_view.get<PowerGridComponent>(entity);
        if (grid.power_level > 0.5f) {
            const auto& chunk = grid_view.get<ChunkComponent>(entity);
            
            // Find neighbors (cheating for now: iterate all and check proximity)
            for (auto target : grid_view) {
                if (entity == target) continue;
                const auto& target_chunk = grid_view.get<ChunkComponent>(target);
                
                int dx = std::abs(chunk.chunk_x - target_chunk.chunk_x);
                int dy = std::abs(chunk.chunk_y - target_chunk.chunk_y);
                
                if (dx <= 1 && dy <= 1) {
                    auto& target_grid = grid_view.get<PowerGridComponent>(target);
                    float supply = grid.power_level * 0.95f; // Transmission loss
                    if (supply > target_grid.power_level) {
                        target_grid.power_level = supply;
                    }
                }
            }
        }
    }

    // 3. React to failure
    for (auto entity : grid_view) {
        auto& grid = grid_view.get<PowerGridComponent>(entity);
        if (grid.power_level < 0.2f) {
            grid.ticks_since_failure++;
        } else {
            grid.ticks_since_failure = 0;
        }
    }
}

void PowerGridSystem::on_crisis_started(const CrisisStartedEvent& event) {
    if (event.type == CrisisType::INFRASTRUCTURE_FAILURE) {
        m_dispatcher.trigger(HUDNotificationEvent{"GRID INSTABILITY DETECTED", 3.0f, "#FF9900"});
        
        // Randomly pick 1-3 chunks to suffer a failure
        auto chunk_view = m_registry.view<PowerGridComponent, ChunkComponent>();
        std::vector<entt::entity> candidates;
        for (auto entity : chunk_view) candidates.push_back(entity);
        
        if (candidates.empty()) return;

        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(candidates.begin(), candidates.end(), gen);

        int failure_count = std::min((int)candidates.size(), 1 + (int)(event.severity * 3));
        for (int i = 0; i < failure_count; ++i) {
            apply_outage_effects(candidates[i], event.severity);
        }
    }
}

void PowerGridSystem::on_crisis_resolved(const CrisisResolvedEvent& event) {
    if (event.type == CrisisType::INFRASTRUCTURE_FAILURE) {
        auto chunk_view = m_registry.view<PowerGridComponent>();
        for (auto entity : chunk_view) {
            restore_power(entity);
        }
        m_dispatcher.trigger(HUDNotificationEvent{"POWER GRID STABILIZED", 3.0f, "#00FF00"});
    }
}

void PowerGridSystem::on_power_outage(const PowerOutageEvent& event) {
    // This event might target a specific building or location
    // Find the chunk containing the location and apply failure
    auto pos_view = m_registry.view<PositionComponent, ChunkComponent>();
    // For now, if location is an entity with position, find its chunk
    if (m_registry.valid(event.location) && m_registry.all_of<PositionComponent>(event.location)) {
        auto& pos = m_registry.get<PositionComponent>(event.location);
        for (auto entity : pos_view) {
            const auto& chunk = pos_view.get<ChunkComponent>(entity);
            // Rough check (assuming chunk size is known or we check bounds)
            // Just fail the chunk the location is in.
            // (In a more robust system, ChunkStreamingSystem would provide this mapping)
            apply_outage_effects(entity, 0.8f);
        }
    }
}

void PowerGridSystem::apply_outage_effects(entt::entity chunk_entity, float severity) {
    if (!m_registry.all_of<PowerGridComponent>(chunk_entity)) return;
    
    auto& grid = m_registry.get<PowerGridComponent>(chunk_entity);
    grid.power_level = 0.0f;
    grid.voltage_stability = 0.0f;
    grid.is_grid_source = false; // Disable source if it was one

    // Trigger local events for agents/buildings
    // In L2 Cognitive simulation, agents will react to darkness
}

void PowerGridSystem::restore_power(entt::entity chunk_entity) {
    if (!m_registry.all_of<PowerGridComponent>(chunk_entity)) return;
    
    auto& grid = m_registry.get<PowerGridComponent>(chunk_entity);
    grid.power_level = 1.0f;
    grid.voltage_stability = 1.0f;
}

} // namespace NeonOubliette::Systems
