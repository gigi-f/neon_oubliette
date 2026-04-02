# Entity Component System Architecture

## System Update Sequence

1. **Physics Systems** – Handle movement, collision, and spatial queries.
2. **Behavior Systems** – AI, path‑finding, and decision logic.
3. **Animation Systems** – Update glyphs, timers, and visual state.
4. **Rendering Systems** – Emit Notcurses drawing commands.

### System Registry Interface

```cpp
// A minimal, type‑safe system manager using EnTT concepts.
class SystemManager {
public:
    template <typename System>
    void register_system();

    template <typename System>
    System &get_system();

    void update_all_systems(Timestep ts);

private:
    std::unordered_map<std::type_index, std::unique_ptr<IBaseSystem>> systems_;
};
```

#### Update Ordering
The manager maintains a **dependency graph** (directed acyclic graph). Each system declares a compile‑time list of dependencies via a static `constexpr` array. Before the main loop starts, the manager topologically sorts the systems and stores the execution order. A failure to produce a topological ordering indicates a circular dependency and aborts the build.

#### Thread‑Safety
All systems are executed on the **simulation thread** unless they declare `parallel() == true`. Parallel systems are scheduled on a thread‑pool; the manager guarantees no data races by only allowing read‑only views in those systems.

## Entity Creation Pipeline

1. **Request Phase** – Any thread that wants to create an entity pushes a lambda into its local `CommandBuffer`.
2. **Command Phase** – The scheduler, on the **CommandBufferApply** tick, drains all buffers and applies the lambdas to the **MacroRegistry**. This guarantees deterministic ordering and respects the ECS's strict single‑thread mutation rule.
3. **Post‑Creation Hooks** – EnTT's `on_construct` callbacks automatically attach components such as `Position` or `Glyph` as needed.

## Component Layout

All components are **plain‑old data** (POD) and reside in contiguous memory thanks to EnTT's archetype layout. Example components:

```cpp
struct Position { int x, y; };
struct Velocity { int dx, dy; };
struct Glyph   { char ch; uint16_t fg, bg; };
struct Visible {};
struct Health  { int cur, max; };
```

We intentionally keep components small to maximize cache locality; heavier data (e.g., full animation timelines) are referenced via lightweight handles.

## Extensibility

Plugins can register new systems or components by calling the manager's registration functions. Because the manager uses `std::unordered_map<std::type_index, ...>`, the engine is agnostic to the concrete type names, facilitating hot‑loading and mod support.

---

> **Note:** All code snippets are illustrative. The actual implementation will use C++20 modules, concepts, and EnTT 3.9+.
