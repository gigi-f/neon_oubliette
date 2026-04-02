# System Scheduler Specification

## 1. Overview

The System Scheduler is responsible for orchestrating the execution of all **simulation systems** during the tick loop. It enforces a deterministic order, handles dependencies, and schedules parallel‑eligible systems across a thread‑pool.

## 2. Phases

The scheduler operates on the following *phases*, each represented by an enum value. The phases are executed in a strict order every tick.

| Phase | Description |
|-------|-------------|
| `Input` | Handle raw input, update UI state. |
| `Macro` | Global world updates (city, economy, power grid). |
| `Micro` | Local area updates (rooms, encounters). |
| `Parallel` | Read‑only systems that can run concurrently. |
| `CommandBufferApply` | Drain all queued entity mutations. |
| `Render` | Generate and flush Notcurses draw calls. |

> **Rationale** – The split into `Macro` and `Micro` phases mirrors the *mode switching* logic. A tick may execute only one of these depending on the active mode, but both are part of the full simulation cycle.

## 3. System Registration

Each system implements the `ISimulationSystem` interface (defined in `ecs/interfaces.hpp`). Additionally, a system declares its *phase*, *dependencies*, and whether it is *parallel‑eligible*.

```cpp
struct MyPhysicsSystem : public ISimulationSystem {
    constexpr static Phase phase = Phase::Macro;
    constexpr static std::array<Phase, 0> dependencies{};
    constexpr static bool parallel = false;

    void update(float dt) override;
};
```

The scheduler collects these static metadata at compile time using a **registration helper**.

### Registration Helper

```cpp
template <typename System>
void register_system(SystemManager& mgr) {
    mgr.register_system<System>();
}
```

`SystemManager` maintains an internal DAG for each phase.

## 4. Dependency Graph & Topological Sort

For each phase, the scheduler builds a graph where nodes are systems and edges represent *depends_on*. It performs a topological sort during initialization. If a cycle is detected, the engine aborts with a diagnostic message.

```cpp
void SystemManager::build_phase_order() {
    for (auto& [phase, systems] : phase_map_) {
        auto order = topological_sort(systems);
        phase_execution_order_[phase] = std::move(order);
    }
}
```

## 5. Parallel Execution

Systems flagged `parallel == true` are grouped into *read‑only* batches. The scheduler ensures that no two systems in the same batch write to the same component type. A simple **write‑set** analysis is performed:

```cpp
bool can_run_in_parallel(const ISystem& a, const ISystem& b) {
    return !std::ranges::any_of(a.write_set, [&](auto comp){ return b.write_set.contains(comp); })
        && !std::ranges::any_of(b.write_set, [&](auto comp){ return a.write_set.contains(comp); });
}
```

The resulting batches are then dispatched to a `ThreadPool`.

## 6. Command Buffer Application

All entity mutations are enqueued into thread‑local `CommandBuffer`s. During the `CommandBufferApply` phase, the main thread drains each buffer in a deterministic order (e.g., by thread ID). The operations are executed directly on the **MacroRegistry** or the relevant **RegionRegistry**.

## 7. Tick Loop Skeleton

```cpp
for (;;) {
    // 1. Input
    input_system.update(dt);

    // 2. Macro or Micro
    if (mode == Mode::Macro) {
        run_phase(Phase::Macro);
    } else {
        run_phase(Phase::Micro);
    }

    // 3. Parallel (read‑only)
    run_parallel();

    // 4. Command Buffer
    apply_command_buffers();

    // 5. Render
    render_system.update(dt);
}
```

## 8. Extensibility

New systems are added by including their header and calling `register_system<NewSystem>(mgr);` during engine initialization. The registration helper automatically extracts the static metadata.

---

**Next** – Draft the lightweight *Event Bus* spec to facilitate cross‑registry communication.
