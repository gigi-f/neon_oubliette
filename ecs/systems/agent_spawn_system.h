#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_AGENT_SPAWN_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_AGENT_SPAWN_SYSTEM_H

#include <entt/entt.hpp>
#include "../components/components.h"
#include "../system_scheduler.h"
#include <string>
#include <vector>

namespace NeonOubliette {

/**
 * [NEW SYSTEM]
 * @brief Handles spawning of agents and population of the world with NPCs.
 */
class AgentSpawnSystem : public ISystem {
public:
    AgentSpawnSystem(entt::registry& registry, entt::dispatcher& dispatcher)
        : m_registry(registry), m_dispatcher(dispatcher) {}

    void initialize() override {}
    void update(double delta_time) override {}

    /**
     * @brief Spawns N agents on walkable tiles.
     * @param count Number of agents to spawn.
     * @param layer The layer ID to spawn them on.
     */
    void spawnAgents(int count, int layer = 0);

    /**
     * @brief Spawns a single agent of a specific type.
     */
    entt::entity createAgent(int x, int y, int layer, const std::string& name, const std::string& archetype, char glyph, const std::string& color);

private:
    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;

    bool isWalkable(int x, int y, int layer);
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_AGENT_SPAWN_SYSTEM_H
