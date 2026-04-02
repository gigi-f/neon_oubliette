# Rendering Pipeline Specification – ASCII (Notcurses)

## 1. Overview

The **Rendering Pipeline** is responsible for turning the world state (macro or micro) into a visual representation on the terminal using the Notcurses library.  It must be:

1. **Deterministic** – rendering must follow the same order every frame so that screenshots and replay data can be reproduced.
2. **Efficient** – only dirty cells are updated to keep the frame‑rate high, especially when the world contains thousands of tiles.
3. **Composable** – both macro and micro worlds share the same renderer but may have different *viewports* and *layers*.
4. **Extensible** – new visual effects (e.g., weather, particle systems) can be plugged in without touching the core.

### Notcurses Basics

* **Planes** – logical surfaces that can be composited together.  The root plane is the screen.
* **Color** – 24‑bit RGB values encoded as `uint32_t` (0xRRGGBB).  Notcurses uses an opaque struct `nc_rgb`.
* **Glyphs** – a single Unicode code point + foreground / background colors.
* **Dirty‑rect** – a set of coordinates that changed since the last frame.

## 2. Core Concepts

| Concept | Description |
|---------|-------------|
| `RenderContext` | Singleton stored in the macro registry’s `ctx()`; holds the Notcurses context, root plane, a *dirty bitmap*, and a pool of reusable sub‑planes for micro regions. |
| `GlyphComponent` | Holds the character and color for an entity; it is the source of visual data. |
| `PositionComponent` | Tile coordinate in the current registry. |
| `VisibilityComponent` | Marks an entity as visible to the current viewport; also stores visibility radius if needed. |
| `Camera` | Holds viewport origin and size; can be moved by user input or programmatically. |
| `Layer` | Logical depth order – background, ground, entities, UI, etc.  Each layer has its own plane. |
| `DirtyRectTracker` | Bitset or sparse set mapping `(x,y)` to a dirty flag. |
| `RegionPlane` | A child plane that represents a micro‑region; it is composited onto the parent plane in a deterministic order. |

## 3. Render Flow (Per Tick)

1. **Input & Camera Update** – UI and movement input updates the `Camera` component.
2. **Mark Dirty** – Any system that mutates a `GlyphComponent` or `PositionComponent` notifies the `DirtyRectTracker`.  This is done by calling `RenderContext::mark_dirty(x, y)`.
3. **Parallel Render Batch** – The `RenderSystem` runs after the command buffer has been applied.  It gathers a *view* of all entities that have `Position`, `Glyph`, and `Visibility`.
4. **Batch Composition** – For each entity:
   * Calculate screen coordinates: `screen_x = entity.pos.x - camera.x`, same for y.
   * Skip if outside viewport.
   * If the cell is dirty, push a `RenderCommand` into a per‑thread vector.
5. **Plane Compositing** – Once all commands are collected, the main rendering thread iterates over the dirty cells in a deterministic order (e.g., row‑major) and writes the glyphs to the appropriate plane using `ncplane_putc`.  The plane hierarchy is:
   * **Root plane** – covers the entire terminal.
   * **Layer planes** – `ground_plane`, `entity_plane`, `ui_plane`.
   * **Region planes** – created on demand for each active micro region; attached to `entity_plane`.
6. **Flush** – After all planes are updated, call `ncplane_move` + `ncplane_show` + `ncplane_puts` as needed, and finally `notcurses_render()`.
7. **Reset Dirty** – Clear the `DirtyRectTracker`.

## 4. Dirty‑Rect Tracking

### Data Structure

```cpp
// Using a bitmap: each bit represents a tile.
class DirtyBitmap {
public:
    void mark(int x, int y) { bitmap_.set(index(x,y)); }
    bool is_dirty(int x, int y) const { return bitmap_.test(index(x,y)); }
    void clear() { bitmap_.reset(); }
private:
    size_t index(int x, int y) const { return y * width_ + x; }
    std::vector<std::bitset<MAX_TILES>> bitmap_;
    size_t width_, height_;
};
```

*Marking* is done in the observer callbacks on `PositionComponent` and `GlyphComponent`:

```cpp
auto pos_observer = registry.observe<Position>(entt::collector.update<Position>()
    .each([&ctx](auto e, const Position &p) {
        ctx.mark_dirty(p.x, p.y);
    }));
```

### Performance

Only the dirty cells are touched; Notcurses’ plane API is O(1) per glyph.  For large maps this keeps the frame‑rate above 60 FPS.

## 5. Viewport & Camera

The camera is a simple struct:

```cpp
struct Camera {
    int x, y;          // top‑left corner in world coordinates
    int width, height; // viewport size in tiles
};
```

User input (arrow keys, zoom, etc.) updates the camera.  The renderer subtracts `camera.x/y` from the entity coordinates to get screen coordinates.

### Zoom & Layering

* **Zoom** – The camera can change `width/height` to zoom in/out; scaling glyphs is handled by mapping more world tiles per screen cell.
* **Layer Ordering** – `Layer` enum (`Background`, `Ground`, `Entities`, `UI`).  Each layer has its own plane.  During compositing we iterate layers in order.

## 6. Micro‑Region Plane Hierarchy

When a micro region becomes active, the renderer creates a child plane of the `entity_plane`:

```cpp
auto *region_plane = ncplane_new(entity_plane, region.width, region.height, 0, 0);
region_plane->set_parent(entity_plane);
```

The region plane’s origin is set to the region’s position relative to the viewport.  The region plane is destroyed when the region becomes inactive.

## 7. Rendering Extensions

* **Particle System** – A `ParticleComponent` can be added to a region plane.  Each frame a small number of particles are emitted and drawn on the region plane.
* **Weather Effects** – A global `WeatherComponent` can overlay semi‑transparent glyphs or modify the `GlyphComponent` colors.
* **Animation** – The `AnimationState` component indexes a shared animation buffer; the renderer reads the current frame’s glyph and colors.

## 8. Integration with Event Bus

* **Visibility Events** – Systems that toggle `Visible` can emit an event.  The renderer subscribes to `VisibilityChanged` and updates the dirty set accordingly.
* **Micro‑to‑Macro Sync** – When a micro region is created/destroyed, the region bus broadcasts to the macro bus to update the region registry list, which the renderer uses to know which child planes to create.

## 9. Implementation Notes

* All rendering occurs on the **main thread** to avoid Notcurses’ thread‑safety issues.
* The `RenderContext` is a singleton stored in `registry.ctx()`; it is passed to all systems that need to mark dirty cells.
* The `RenderSystem` runs *after* all entity mutations have been applied (CommandBufferApply phase) to avoid stale data.

---

**Next** – Add the rendering system header (`ecs/render_system.hpp`) and a minimal implementation skeleton that wires up the dirty‑rect logic described above.
