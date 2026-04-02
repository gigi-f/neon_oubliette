# Core Simulation Loop Design

## Overview
The simulation loop orchestrates the **macro** world and **micro** worlds (buildings, floors, shops, NPCs) in a deterministic, scalable, and extensible manner. It is split into discrete *phases* that mirror the logical progression of a game tick:

```
Input  →  Macro  →  Micro  →  CommandBufferApply  →  Render  →  Sync
```

### Phase Definition
| Phase | Purpose | Typical Systems | Dependencies |
|-------|---------|-----------------|--------------|
| **Input** | Gather external events (keyboard, network) | `InputSystem`, `NetworkSyncSystem` | None |
| **Macro** | Update city‑wide state (economy, weather, utilities) | `WeatherSystem`, `TaxationSystem`, `ResourceAllocationSystem` | Requires finished Input |
| **Micro** | Update building‑level and floor‑level simulation | `ElevatorSystem`, `ShopTransactionSystem`, `FloorSpawnerSystem` | Requires finished Macro |
| **CommandBufferApply** | Apply all queued state changes in a deterministic order | `CommandBufferSystem` | Executes after Micro |
| **Render** | Render ASCII view with Notcurses | `SkyscraperVisualSystem`, `SpriteRenderSystem` | Requires final state |
| **Sync** | Wait for frame budget, optionally sleep | `FrameLimiter` | None |

### Data Flow & Threading
1. **Macro Registry** (`entt::registry`) holds city‑wide entities. All *macro* systems operate on this registry.
2. **Micro Registries**: Each `BuildingComponent` owns a child `entt::registry` for its floors. Micro systems run *in parallel* across buildings using a thread pool. A per‑building *command buffer* is used to serialize changes back to the child registry.
3. **Command Buffers**: Every micro system writes to a per‑building `CommandBuffer` (a simple vector of lambda). At the `CommandBufferApply` phase, each buffer is drained sequentially, guaranteeing deterministic ordering.
4. **Event Bus**: The global event bus lives in the macro registry’s `ctx()`. Micro registries also have local event buses for intra‑building communication. Events are processed after the `CommandBufferApply` phase.

### Determinism Guarantees
- All RNG usage is *seeded* from the building/entity ID (`hash(seed, entity)`).
- No global mutable RNG state is modified during system execution.
- Command buffers enforce strict order; no system writes to the registry directly.

### Extensibility
- Adding a new phase is a matter of inserting a new `enum class Phase` entry and extending the scheduler.
- Systems declare their phase via a static member `constexpr Phase phase = Phase::X` and optional dependencies.
- Plugin systems register themselves with the scheduler via the `IPlugin` interface.

---

### Implementation Outline
```cpp
class Scheduler {
    std::vector<std::unique_ptr<ISystem>> input_systems;
    std::vector<std::unique_ptr<IMacroSystem>> macro_systems;
    std::vector<std::unique_ptr<IMicroSystem>> micro_systems;
    // ...
    void run(Timestep ts) {
        for (auto& s : input_systems)    s->update(ts);
        for (auto& s : macro_systems)    s->update(ts);
        for (auto& s : micro_systems)    s->update(ts);
        apply_command_buffers();
        for (auto& s : render_systems)    s->update(ts);
    }
};
```
