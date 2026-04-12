#include "ambient_spawn_system.h"
#include "../components/components.h"
#include "../components/infrastructure_components.h"
#include "../entity_creation_functions.h"
#include <cmath>

namespace NeonOubliette::Systems {

static constexpr float MAX_SIM_RADIUS = 40.0f;
static constexpr size_t MAX_AMBIENT_VEHICLES = 50;
static constexpr size_t MAX_AMBIENT_PEDESTRIANS = 100;

AmbientSpawnSystem::AmbientSpawnSystem(entt::registry& registry, entt::dispatcher& event_dispatcher)
    : m_registry(registry), m_dispatcher(event_dispatcher) {
    std::random_device rd;
    m_gen = std::mt19937(rd());
}

void AmbientSpawnSystem::initialize() {}

void AmbientSpawnSystem::update(double delta_time) {
    m_accumulated_time += delta_time;
    if (m_accumulated_time < 1.0) return; // Update every second
    m_accumulated_time = 0;

    despawn_distant_entities();
    spawn_ambient_entities();
}

void AmbientSpawnSystem::spawn_ambient_entities() {
    auto player_view = m_registry.view<PlayerComponent, PositionComponent>();
    if (player_view.begin() == player_view.end()) return;

    auto* grid = m_registry.ctx().find<ArterialGrid>();
    if (!grid) return;

    auto vehicle_view = m_registry.view<AmbientTagComponent, VehicleComponent>();
    size_t current_vehicles = std::distance(vehicle_view.begin(), vehicle_view.end());
    
    auto ped_view = m_registry.view<AmbientTagComponent, AgentComponent>();
    size_t current_pedestrians = std::distance(ped_view.begin(), ped_view.end());

    std::uniform_real_distribution<float> dist(-MAX_SIM_RADIUS, MAX_SIM_RADIUS);
    std::uniform_real_distribution<float> chance(0.0f, 1.0f);

    for (auto player_ent : player_view) {
        const auto& p_pos = player_view.get<PositionComponent>(player_ent);

        // Attempt to spawn a vehicle
        if (current_vehicles < MAX_AMBIENT_VEHICLES && chance(m_gen) < 0.2f) {
            int sx = static_cast<int>(p_pos.x + dist(m_gen));
            int sy = static_cast<int>(p_pos.y + dist(m_gen));
            
            ArterialType type;
            if (grid->type_at(sx, sy, p_pos.layer_id, type)) {
                if (type == ArterialType::ROAD_PRIMARY || type == ArterialType::ROAD_SECONDARY) {
                    auto vehicle = m_registry.create();
                    m_registry.emplace<AmbientTagComponent>(vehicle);
                    m_registry.emplace<PositionComponent>(vehicle, (float)sx, (float)sy, p_pos.layer_id);
                    m_registry.emplace<RenderableComponent>(vehicle, 'v', "#AAAAAA", 2);
                    m_registry.emplace<VehicleComponent>(vehicle);
                    current_vehicles++;
                }
            }
        }

        // Attempt to spawn a pedestrian
        if (current_pedestrians < MAX_AMBIENT_PEDESTRIANS && chance(m_gen) < 0.4f) {
            int sx = static_cast<int>(p_pos.x + dist(m_gen));
            int sy = static_cast<int>(p_pos.y + dist(m_gen));
            
            ArterialType type;
            if (grid->type_at(sx, sy, p_pos.layer_id, type)) {
                if (type == ArterialType::SIDEWALK || type == ArterialType::PEDESTRIAN_PATH) {
                    auto ped = m_registry.create();
                    m_registry.emplace<AmbientTagComponent>(ped);
                    m_registry.emplace<PositionComponent>(ped, (float)sx, (float)sy, p_pos.layer_id);
                    m_registry.emplace<RenderableComponent>(ped, 'p', "#CCCCAA", 1);
                    m_registry.emplace<AgentComponent>(ped);
                    m_registry.emplace<AgentTaskComponent>(ped, AgentTaskType::WANDER);
                    current_pedestrians++;
                }
            }
        }
    }
}

void AmbientSpawnSystem::despawn_distant_entities() {
    auto player_view = m_registry.view<PlayerComponent, PositionComponent>();
    if (player_view.begin() == player_view.end()) return;

    auto ambient_view = m_registry.view<AmbientTagComponent, PositionComponent>();
    std::vector<entt::entity> to_destroy;

    for (auto entity : ambient_view) {
        const auto& pos = ambient_view.get<PositionComponent>(entity);
        bool near_any_player = false;

        for (auto player_ent : player_view) {
            const auto& p_pos = player_view.get<PositionComponent>(player_ent);
            float dx = pos.x - p_pos.x;
            float dy = pos.y - p_pos.y;
            if (std::sqrt(dx * dx + dy * dy) < MAX_SIM_RADIUS + 10.0f) {
                near_any_player = true;
                break;
            }
        }

        if (!near_any_player) {
            to_destroy.push_back(entity);
        }
    }

    m_registry.destroy(to_destroy.begin(), to_destroy.end());
}

} // namespace NeonOubliette::Systems
