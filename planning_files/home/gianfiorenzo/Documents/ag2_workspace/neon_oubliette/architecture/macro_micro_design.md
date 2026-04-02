# Macro/Micro Mode Architecture

## Mode Switching Strategy

| Mode | Time Step | Entity Scope | Rendering | Simulation Granularity |
|------|-----------|--------------|-----------|------------------------|
| **Macro** | ~333 ms | Global world (city, economy, power grid) | Abstract HUD, high‑level status | Coarse‑grain physics and AI (e.g., city‑wide population updates) |
| **Micro** | ~16 ms | Local area (room, field, encounter) | Full ASCII detail via Notcurses | Fine‑grain physics, AI, and animation per entity |

## Transition Logic

We transition automatically based on *focus distance* or *user intent* (e.g., a *zoom* command). The logic resides in a **SimulationModeManager** that owns a **MacroMode** and **MicroMode** subsystem and performs safe transitions.

```cpp
class SimulationModeManager {
public:
    enum class Mode { Macro, Micro };

    void update(float dt) {
        switch (current_mode_) {
            case Mode::Macro: macro_system_-\u003eupdate(dt); break;
            case Mode::Micro: micro_system_-\u003eupdate(dt); break;
        }
        handle_transition();
    }

private:
    void handle_transition() {
        // Example: zoom out triggers macro mode
        if (player_focus_radius_ \u003e threshold_) {
            if (current_mode_ != Mode::Macro) {
                transition_to(Mode::Macro);
            }
        } else {
            if (current_mode_ != Mode::Micro) {
                transition_to(Mode::Micro);
            }
        }
    }

    void transition_to(Mode new_mode) {
        // Clean up old mode if necessary
        if (current_mode_ == Mode::Macro) macro_system_-\u003eon_exit();
        else micro_system_-\u003eon_exit();

        current_mode_ = new_mode;
        if (current_mode_ == Mode::Macro) macro_system_-\u003eon_enter();
        else micro_system_-\u003eon_enter();
    }

    Mode current_mode_ = Mode::Micro;
    std::unique_ptr<IMacroSystem> macro_system_;
    std::unique_ptr<IMicroSystem> micro_system_;
    float player_focus_radius_ = 0.0f;
    const float threshold_ = 64.0f; // units (tiles)
};
```

### Interface Definitions

```cpp
// Macro‑level system interface
struct IMacroSystem {
    virtual void on_enter() = 0;   // called when macro mode becomes active
    virtual void on_exit()  = 0;   // called when leaving macro mode
    virtual void update(float dt) = 0;
    virtual ~IMacroSystem() = default;
};

// Micro‑level system interface
struct IMicroSystem {
    virtual void on_enter() = 0;
    virtual void on_exit()  = 0;
    virtual void update(float dt) = 0;
    virtual ~IMicroSystem() = default;
};
```

## Concrete Implementations

- **MacroMode** will own a *city* registry (`entt::registry`) that contains entities such as `Citizens`, `Buildings`, `PowerPlant`. It will expose aggregated statistics via a `WorldStats` resource in its context.
- **MicroMode** will create *Region* registries on demand (see the **Macro‑Micro scaling** section). Each region holds local entities (`Monsters`, `Items`, `Terrain`). A region is destroyed when it becomes inactive.

## Data Flow Diagram

```
┌─────────────────────┐          ┌─────────────────────┐
│   MacroRegistry      │  ⇅       │  RegionRegistry(s)  │
│ ─────┬─────┬─────────┤          │ ────┬───────┬───────┤
│     │     │         │          │     │       │       │
│  City│  Power │ Economy │          │  Room│  Dungeon│  Field│
│     │     │         │          │     │       │       │
└─────┴─────┴─────────┘          └─────┴───────┴───────┘
          ▲                              │
          │                              ▼
     MacroEventBus           MicroEventBus (regional)
          │                              │
          └──────────────────────────────┘
```

---

**Next Steps** – Draft a `ecs/entt_component_types.md` that enumerates all core components and their relationships. This will serve as the reference for both macro and micro mode subsystems.

## 9. Economy and Market Systems

### 9.1 Macro Market
* **Macro Economy Component**: Stores city-wide GDP, unemployment, tax rates, and wealth distribution.
* **TaxationSystem** (Macro Phase): Calculates taxes per building and NPC based on their `IncomeComponent` and `PropertyTaxComponent`.
* **BudgetComponent** (Macro): Each building has a budget that the city allocates via `BudgetAllocationSystem`.

### 9.2 Micro Market
* **ShopComponent**: Holds inventory, prices, and sales counters.
* **PurchaseSystem** (Micro Phase): When an NPC triggers a `PurchaseEvent`, the system checks inventory, adjusts NPC gold, and updates the shop’s sales record.

### 9.3 Exchange Mechanism
* **CurrencyComponent**: `uint64_t` gold value.
* **ExchangeRateComponent**: For multi-currency scenarios (e.g., city credits vs. foreign coins). Not used yet but reserved.

## 10. Simulation Loop & Scheduler Overview

```mermaid
flowchart TD
  A[Input] --> B[Macro Systems]
  B --> C[Micro Task Dispatch]
  C --> D[Micro Systems per Building]
  D --> E[CommandBuffer Apply]
  E --> F[Event Bus Dispatch]
  F --> G[Output (Render)]
```

1. **Input** – Player actions, external events, or scheduled world events.
2. **Macro Systems** – Run on the main thread, update global state.
3. **Micro Task Dispatch** – Scheduler spawns one task per building (or batches to keep thread count bounded).
4. **Micro Systems** – Execute within each building’s registry, push to the building’s `CommandBuffer`.
5. **CommandBuffer Apply** – Drain each buffer sequentially by building ID.
6. **Event Bus Dispatch** – Process cross-layer events; macro events can enqueue micro events and vice‑versa.
7. **Output** – Render to terminal via Notcurses, or write to log.

## 11. Extensibility Hooks

* **Modding API**: Expose `IPlugin` interface; mods can register new component types, systems, and event handlers.
* **Hot‑Reloading**: Components and systems can be recompiled and reloaded at runtime via a simple shared‑library loader.
* **Data‑Driven Scripting**: NPC behavior trees, resource node configurations, and building blueprints are defined in JSON/YAML; the core loader parses them into ECS components.

---

**Documentation Summary**
* `macro_micro_design.md` – Overall world architecture.
* `ecs/entt_component_types.md` – Component definitions.
* `ecs/system_scheduler.md` – Scheduler configuration.
* `architecture/simulation_loop.md` – Execution pipeline.

All files reside under `/home/gianfiorenzo/Documents/ag2_workspace/neon_oubliette` and are ready for peer review.

## 12. Housing and Demography

### 12.1 Population Distribution
* **Macro Citizens**: A `CitizenComponent` in the macro registry tracks global traits (age, skill, wealth, family group). Each citizen has a `HousingPreferenceComponent` that encodes desired floor level, building type, and proximity to certain services.
* **Micro Residents**: The actual occupants of apartments or rooms are `ResidentComponent`s living in the building’s micro registry. They are linked to their macro citizen via `MacroCitizenId`.

### 12.2 Apartment Allocation
1. **HousingGrid**: Each `BuildingComponent` contains a 2D array of `ApartmentComponent`s representing floors & rooms. The grid tracks occupancy status.
2. **AllocationSystem** (Macro Phase): At each tick, the system scans the `GlobalHousingQueue` of unassigned citizens. It selects a suitable apartment based on `HousingPreferenceComponent` and assigns a `ResidentComponent` to that apartment entity in the building’s micro registry via its command buffer.
3. **Eviction & Mobility**: If a building’s capacity changes (e.g., new floors added), the `FloorSpawnerSystem` will trigger an `EvictionEvent` to free occupants or relocate them to other buildings.

### 12.3 Resource Flow to Housing
* **Utilities**: Power, water, and waste are allocated to apartments by the `VerticalFlowSystem` as part of the per‑floor distribution. Each `ApartmentComponent` receives a `UtilityAllocationComponent` that stores the current allocation state.
* **Dynamic Consumption**: `ResidentComponent` may emit `ConsumptionEvent` (e.g., electricity usage spike) which is processed by the `MicroUtilitySystem` and aggregated back to the building’s resource budget.

---

**End of Architecture Document**
## 13. Traffic Infrastructure and Vehicle Dynamics

### 13.1 Road & Rail Graph
* Roads and rails are stored as **graph edges** in the macro registry. Each edge holds:
  - `geometry` (start/end grid cells)
  - `max_speed`, `capacity`, and `traffic_density`.
* A lightweight **Pathfinder** (A* on the road graph) is executed once per vehicle per tick to compute the next edge.

### 13.2 Vehicle Lifecycle
1. **Spawn**: Import nodes create `Truck` or `Train` entities with a `VehicleComponent`.
2. **Transit**: Every tick, `TrafficFlowSystem` updates the vehicle’s `PositionComponent` along the current edge. Speed is throttled by `traffic_density`.
3. **Arrival**: Upon reaching a building, the vehicle triggers a `RawMaterialDeliveryEvent` on the target building’s command buffer.

### 13.3 Congestion & Penalties
* If `traffic_density > threshold`, `max_speed` is reduced and a **delay penalty** is applied to the vehicle’s remaining travel time.
* The system logs congestion events to the `MacroEventBus` for UI feedback.
