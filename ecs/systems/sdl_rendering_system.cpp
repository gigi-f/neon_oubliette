#include "sdl_rendering_system.h"
#include "../components/components.h"
#include "../components/base_types.h"
#include "../components/lod_components.h"
#include <iostream>

namespace NeonOubliette::Systems {

// ─────────────────────────────────────────────────────────────────────────────
// Viewport constants — match SDLRenderer cell dimensions (10×18 px)
// Window = 120×40 cells = 1200×720 px
// ─────────────────────────────────────────────────────────────────────────────
static constexpr int VIEW_W = 120;
static constexpr int VIEW_H = 40;

SDLRenderingSystem::SDLRenderingSystem(entt::registry& registry,
                                       std::shared_ptr<IRenderer> renderer,
                                       entt::dispatcher& event_dispatcher)
    : registry_(registry), renderer_(renderer), event_dispatcher_(event_dispatcher) {
}

void SDLRenderingSystem::initialize() {
    renderer_->create_layer({"Terrain",  0, VIEW_W, VIEW_H});
    renderer_->create_layer({"Entities", 1, VIEW_W, VIEW_H});
    renderer_->create_layer({"HUD",      2, VIEW_W,  2});

    // Build initial spatial indices
    rebuild_terrain_index();
    rebuild_entity_index();
}

// ─────────────────────────────────────────────────────────────────────────────
// Spatial index helpers (chunk-bucketed)
// ─────────────────────────────────────────────────────────────────────────────

static uint64_t make_key(int cx, int cy) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32)
         | static_cast<uint32_t>(cy);
}

void SDLRenderingSystem::rebuild_terrain_index() {
    terrain_idx_.clear();
    int chunk_sz = 16; // default; could read from registry ctx
    auto view = registry_.view<PositionComponent, RenderableComponent, TerrainComponent>();
    for (auto e : view) {
        const auto& pos = view.get<PositionComponent>(e);
        if (pos.layer_id != 0) continue;
        int cx = pos.x / chunk_sz;
        int cy = pos.y / chunk_sz;
        terrain_idx_[make_key(cx, cy)].push_back(e);
    }
    terrain_dirty_ = false;
}

void SDLRenderingSystem::rebuild_entity_index() {
    entity_idx_.clear();
    int chunk_sz = 16;
    auto view = registry_.view<PositionComponent, RenderableComponent>(entt::exclude<TerrainComponent>);
    for (auto e : view) {
        const auto& pos = view.get<PositionComponent>(e);
        if (pos.layer_id != 0) continue;
        int cx = pos.x / chunk_sz;
        int cy = pos.y / chunk_sz;
        entity_idx_[make_key(cx, cy)].push_back(e);
    }
    entity_dirty_ = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Update
// ─────────────────────────────────────────────────────────────────────────────

void SDLRenderingSystem::update(double delta_time) {
    (void)delta_time;

    // ── Input pump ────────────────────────────────────────────────────────────
    std::vector<EngineInputEvent> inputs;
    renderer_->poll_input(inputs);
    for (const auto& ev : inputs) {
        event_dispatcher_.trigger(ev);
    }

    // ── Camera: center on player ──────────────────────────────────────────────
    int cam_x = 0, cam_y = 0;
    {
        auto pv = registry_.view<PositionComponent, PlayerComponent>();
        for (auto e : pv) {
            const auto& pos = pv.get<PositionComponent>(e);
            cam_x = pos.x;
            cam_y = pos.y;
            break;
        }
    }
    const int tl_x = cam_x - VIEW_W / 2;
    const int tl_y = cam_y - VIEW_H / 2;

    // Chunk range visible on screen (inclusive)
    constexpr int CHUNK = 16;
    const int chunk_x0 = tl_x / CHUNK;
    const int chunk_y0 = tl_y / CHUNK;
    const int chunk_x1 = (tl_x + VIEW_W - 1) / CHUNK;
    const int chunk_y1 = (tl_y + VIEW_H - 1) / CHUNK;

    // ── Clear layers ──────────────────────────────────────────────────────────
    renderer_->clear_layer("Terrain");
    renderer_->clear_layer("Entities");
    renderer_->clear_layer("HUD");

    // ── Terrain — only chunks in view ─────────────────────────────────────────
    // Rebuild if flagged dirty (chunk load/unload events could set this)
    if (terrain_dirty_) rebuild_terrain_index();

    auto terrain_rc = registry_.view<PositionComponent, RenderableComponent, TerrainComponent>();
    for (int cy = chunk_y0; cy <= chunk_y1; ++cy) {
        for (int cx = chunk_x0; cx <= chunk_x1; ++cx) {
            auto it = terrain_idx_.find(make_key(cx, cy));
            if (it == terrain_idx_.end()) continue;
            for (auto e : it->second) {
                if (!registry_.valid(e)) continue;
                const auto& pos = terrain_rc.get<PositionComponent>(e);
                int sx = pos.x - tl_x;
                int sy = pos.y - tl_y;
                if (sx < 0 || sx >= VIEW_W || sy < 0 || sy >= VIEW_H) continue;
                const auto& rnd = terrain_rc.get<RenderableComponent>(e);
                std::string glyph(1, rnd.glyph);
                renderer_->draw_tile("Terrain", sx, sy, glyph,
                                     ColorRGB::from_hex(rnd.parsed_color_cache));
            }
        }
    }

    // ── Entities — only chunks in view ────────────────────────────────────────
    if (entity_dirty_) rebuild_entity_index();

    auto entity_rc = registry_.view<PositionComponent, RenderableComponent>(
                         entt::exclude<TerrainComponent>);
    for (int cy = chunk_y0; cy <= chunk_y1; ++cy) {
        for (int cx = chunk_x0; cx <= chunk_x1; ++cx) {
            auto it = entity_idx_.find(make_key(cx, cy));
            if (it == entity_idx_.end()) continue;
            for (auto e : it->second) {
                if (!registry_.valid(e)) continue;
                const auto& pos = entity_rc.get<PositionComponent>(e);
                int sx = pos.x - tl_x;
                int sy = pos.y - tl_y;
                if (sx < 0 || sx >= VIEW_W || sy < 0 || sy >= VIEW_H) continue;
                const auto& rnd = entity_rc.get<RenderableComponent>(e);
                std::string glyph(1, rnd.glyph);
                renderer_->draw_tile("Entities", sx, sy, glyph,
                                     ColorRGB::from_hex(rnd.parsed_color_cache));
            }
        }
    }

    // ── HUD ───────────────────────────────────────────────────────────────────
    std::string hud = "NEON OUBLIETTE  pos:(" + std::to_string(cam_x)
                    + "," + std::to_string(cam_y) + ")";
    renderer_->draw_text("HUD", 1, 0, hud, {0, 255, 255, 255}, {0, 0, 0, 200});

    renderer_->render_frame();
}

} // namespace NeonOubliette::Systems
