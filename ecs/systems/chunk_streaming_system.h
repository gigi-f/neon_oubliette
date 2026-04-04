#ifndef NEON_OUBLIETTE_ECS_SYSTEMS_CHUNK_STREAMING_SYSTEM_H
#define NEON_OUBLIETTE_ECS_SYSTEMS_CHUNK_STREAMING_SYSTEM_H

#include <entt/entt.hpp>
#include "../system_scheduler.h"
#include "../components/components.h"
#include "../components/lod_components.h"
#include "../components/zoning_components.h"
#include "../components/simulation_layers.h"
#include <map>
#include <set>

namespace NeonOubliette {

/**
 * @brief Manages the spatial loading and unloading of world chunks (20x20 tiles).
 *        Implements Agent LOD (Level of Detail) by materializing/dematerializing
 *        NPCs based on proximity to the player.
 */
class ChunkStreamingSystem : public ISystem {
public:
    ChunkStreamingSystem(entt::registry& registry, entt::dispatcher& dispatcher);

    void initialize() override;
    void update(double delta_time) override;

private:
    /**
     * @brief Updates the status (Hot/Warm/Cold) of all chunks based on player position.
     */
    void update_chunk_states(int player_x, int player_y);

    /**
     * @brief Transitions a chunk to HOT state, instantiating all entities.
     */
    void materialize_chunk(entt::entity chunk_entity, ChunkComponent& chunk);

    /**
     * @brief Transitions a chunk out of HOT state, serializing agents into records.
     */
    void dematerialize_chunk(entt::entity chunk_entity, ChunkComponent& chunk);

    /**
     * @brief Performs statistical "catch-up" simulation for agents in non-Hot chunks.
     */
    void simulate_macro_agents(ChunkComponent& chunk, TimeOfDay current_time);

    entt::registry& m_registry;
    entt::dispatcher& m_dispatcher;
    
    int m_last_player_chunk_x = -999;
    int m_last_player_chunk_y = -999;
    
    std::map<std::pair<int, int>, entt::entity> m_chunk_map;
    std::set<entt::entity> m_hot_chunks;

    // Chunk radius settings
    const int HOT_RADIUS = 1;  // 3x3
    const int WARM_RADIUS = 2; // 5x5
};

} // namespace NeonOubliette

#endif // NEON_OUBLIETTE_ECS_SYSTEMS_CHUNK_STREAMING_SYSTEM_H
