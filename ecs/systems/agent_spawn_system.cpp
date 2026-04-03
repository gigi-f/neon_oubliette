#include "agent_spawn_system.h"
#include "../components/simulation_layers.h"
#include <random>

namespace NeonOubliette {

void AgentSpawnSystem::spawnAgents(int count, int layer) {
    auto config_view = m_registry.view<WorldConfigComponent>();
    if (config_view.empty()) return;
    
    auto config_entity = config_view.front();
    const auto& config = config_view.get<WorldConfigComponent>(config_entity);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> disX(0, config.width - 1);
    std::uniform_int_distribution<> disY(0, config.height - 1);
    
    // Weighted selection: Civilians are common, Guards are rarer
    std::uniform_int_distribution<> disType(0, 10); // 0-8 Civilian, 9-10 Guard

    int spawned = 0;
    int attempts = 0;
    const int max_attempts = count * 20;

    while (spawned < count && attempts < max_attempts) {
        attempts++;
        int x = disX(gen);
        int y = disY(gen);

        if (isWalkable(x, y, layer)) {
            int type = disType(gen);
            if (type < 9) {
                createAgent(x, y, layer, "Citizen #" + std::to_string(spawned), "Civilian", 'o', "#AAAAAA");
            } else {
                createAgent(x, y, layer, "Peacekeeper #" + std::to_string(spawned), "Guard", 'G', "#5555FF");
            }
            spawned++;
        }
    }
}

entt::entity AgentSpawnSystem::createAgent(int x, int y, int layer, const std::string& name, const std::string& archetype, char glyph, const std::string& color) {
    auto entity = m_registry.create();
    
    m_registry.emplace<NameComponent>(entity, name);
    m_registry.emplace<PositionComponent>(entity, x, y, layer);
    m_registry.emplace<RenderableComponent>(entity, glyph, color, layer);
    m_registry.emplace<AgentComponent>(entity);
    m_registry.emplace<NPCComponent>(entity, 100, 0); // Health 100, no macro id for now
    m_registry.emplace<AgentTaskComponent>(entity, AgentTaskType::IDLE);
    m_registry.emplace<NeedsComponent>(entity, 100.0f, 100.0f);
    m_registry.emplace<SizeComponent>(entity, 1, 1);

    // Give them all layers so they are inspectable (Phase 1.5 prep)
    m_registry.emplace<Layer0PhysicsComponent>(entity);
    m_registry.emplace<Layer1BiologyComponent>(entity);
    m_registry.emplace<Layer2CognitiveComponent>(entity);
    m_registry.emplace<Layer3EconomicComponent>(entity);
    m_registry.emplace<Layer4PoliticalComponent>(entity);

    if (archetype == "Guard") {
        m_registry.emplace<FactionComponent>(entity, "GOVERNMENT", 50, 1.0f);
        
        // Add Patrol waypoints for Guards (Phase 1.3)
        PatrolComponent patrol;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> disRel(-5, 5);
        
        // Waypoint 1: near spawn
        patrol.waypoints.push_back({x, y, layer});
        
        // Waypoint 2: random offset
        int wx2 = std::clamp(x + disRel(gen), 0, 59); // Assuming 60x30 for now, should ideally use config
        int wy2 = std::clamp(y + disRel(gen), 0, 29);
        patrol.waypoints.push_back({wx2, wy2, layer});
        
        m_registry.emplace<PatrolComponent>(entity, patrol);
    } else {
        m_registry.emplace<FactionComponent>(entity, "CITIZEN", 10, 0.1f);
    }

    return entity;
}

bool AgentSpawnSystem::isWalkable(int x, int y, int layer) {
    // Check for terrain type
    bool is_terrain_walkable = false;
    auto terrain_view = m_registry.view<PositionComponent, TerrainComponent>();
    for (auto e : terrain_view) {
        const auto& pos = terrain_view.get<PositionComponent>(e);
        if (pos.x == x && pos.y == y && pos.layer_id == layer) {
            const auto& terrain = terrain_view.get<TerrainComponent>(e);
            // Walkable terrain types:
            if (terrain.type == TerrainType::SIDEWALK || terrain.type == TerrainType::STREET || 
                terrain.type == TerrainType::CONCRETE_FLOOR || terrain.type == TerrainType::GRASS) {
                is_terrain_walkable = true;
                break;
            }
        }
    }

    if (!is_terrain_walkable) return false;

    // Check for single-tile obstacles
    auto obs_view = m_registry.view<PositionComponent, ObstacleComponent>();
    for (auto obstacle : obs_view) {
        const auto& o_pos = obs_view.get<PositionComponent>(obstacle);
        if (o_pos.x == x && o_pos.y == y && o_pos.layer_id == layer) {
            return false;
        }
    }

    // Check for entities with SizeComponent and ObstacleComponent (Buildings, Vehicles)
    auto volumetric_obs = m_registry.view<PositionComponent, ObstacleComponent, SizeComponent>();
    for (auto obstacle : volumetric_obs) {
        const auto& o_pos = volumetric_obs.get<PositionComponent>(obstacle);
        const auto& o_size = volumetric_obs.get<SizeComponent>(obstacle);
        if (layer == o_pos.layer_id &&
            x >= o_pos.x && x < o_pos.x + o_size.width &&
            y >= o_pos.y && y < o_pos.y + o_size.height) {
            return false;
        }
    }

    return true;
}

} // namespace NeonOubliette
