#ifndef NEON_OUBLIETTE_SDL_RENDERING_SYSTEM_H
#define NEON_OUBLIETTE_SDL_RENDERING_SYSTEM_H

#include "system_scheduler.h"
#include "../../src/renderer/IRenderer.h"
#include <entt/entt.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

namespace NeonOubliette::Systems {

class SDLRenderingSystem : public ISystem {
public:
    SDLRenderingSystem(entt::registry& registry,
                       std::shared_ptr<IRenderer> renderer,
                       entt::dispatcher& event_dispatcher);
    ~SDLRenderingSystem() = default;

    void initialize() override;
    void update(double delta_time) override;

    // Mark spatial indices for rebuild (call when chunks are loaded/unloaded)
    void mark_terrain_dirty() { terrain_dirty_ = true; }
    void mark_entity_dirty()  { entity_dirty_  = true; }

private:
    entt::registry&          registry_;
    std::shared_ptr<IRenderer> renderer_;
    entt::dispatcher&        event_dispatcher_;

    // Chunk-bucketed spatial indices keyed by (chunk_x << 32 | chunk_y)
    using EntityList = std::vector<entt::entity>;
    std::unordered_map<uint64_t, EntityList> terrain_idx_;
    std::unordered_map<uint64_t, EntityList> entity_idx_;

    bool terrain_dirty_ = true;
    bool entity_dirty_  = true;

    void rebuild_terrain_index();
    void rebuild_entity_index();
};

} // namespace NeonOubliette::Systems

#endif // NEON_OUBLIETTE_SDL_RENDERING_SYSTEM_H
