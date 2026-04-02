# Verticality Design – Skyscrapers, Towers, and Multi‑Story Buildings

## 1. Motivation

- **Vertical growth** is a core thematic pillar of *Neon Oubliette*: the city rises, not only expands horizontally.
- Skyscrapers introduce **layered micro‑environments** inside a single macro‑entity, adding depth to the simulation.
- The design must preserve **macro‑micro scaling**: the city‑wide economy (macro) interacts with individual floors (micro) without breaking determinism or performance.

## 2. Core Concepts

| Concept | Description |
|---------|-------------|
| **Building** | The macro‑level entity that owns a stack of floors. It stores height, power usage, zoning, and a reference to the *floor registry*.
| **Floor** | A micro‑entity inside a Building. Each Floor has a local registry (`entt::registry`) for occupants, shops, elevators, and interior systems.
| **Elevator** | A transport system that connects floors. It is a component of the Building that tracks the current floor, capacity, and request queue.
| **Zoning** | A tag (Residential / Commercial / Industrial / Mixed) that defines the floor‑level constraints and resource demands.
| **Vertical Resource Flow** | The vertical transfer of electricity, water, waste, and information between Building and floors, managed by the `VerticalFlowSystem`.

## 3. Component Overview

```text
// Macro components (registered in main registry)
struct BuildingComponent {           // Marker + metadata
    std::string name;                // Human readable name
    int base_floor;                  // Floor number of ground level (usually 0)
    int height;                      // Total number of floors
    float power_consumption;         // Base power draw per floor
    float water_consumption;         // Base water draw per floor
    entt::entity floor_registry;     // Handle to the micro‑registry of floors
    ElevatorComponent elevator;      // Current elevator state
    Zoning zoning;                   // Current zoning type
};

// Floor components (registered in floor registry)
struct FloorComponent {             // Marker + metadata
    int level;                       // Floor index relative to building base
    std::unordered_set<entt::entity> occupants; // Entities inside the floor
    float power_requirement;          // Per‑floor consumption
    float water_requirement;          // Per‑floor consumption
};

struct ElevatorComponent {
    int current_floor;
    std::vector<int> destination_queue;   // Floor requests
    int capacity;
    bool moving;
};

struct Zoning { enum class Type{ Residential, Commercial, Industrial, Mixed } type; };
```

## 4. System Design

| System | Phase | Responsibility |
|--------|-------|----------------|
| `BuildingSystem` | `Macro` | Spawns / destroys Buildings, handles height changes, allocates floor registries.
| `FloorSpawnerSystem` | `Micro` | During `CommandBufferApply`, creates Floor entities in the building’s registry.
| `ElevatorSystem` | `Micro` | Processes elevator queues, moves occupants between floors, generates `ElevatorEvent`.
| `VerticalFlowSystem` | `Macro` | Aggregates resource demands from all Floors, pulls from global utilities, and pushes back.
| `ZoningSystem` | `Macro` | Enforces zoning rules: limits on floor counts per type, checks for illegal mixes.
| `SkyscraperVisualSystem` | `Render` | Draws multi‑floor buildings: a stack of glyphs per floor, with height‑based shading.

### Dependency Graph (simplified)

```
BuildingSystem -> FloorSpawnerSystem -> ElevatorSystem -> VerticalFlowSystem
```

## 5. Macro‑Micro Interaction

- **Command Buffers**: All modifications to a Floor are queued in its local registry. The `CommandBufferApply` phase drains the buffers *per building* before the global macro systems run.
- **Resource Aggregation**: `VerticalFlowSystem` queries each building’s floor registry to compute total consumption.
- **Determinism**: The order of floors is fixed (base to top). When the elevator moves, the movement events are serialized via the global event bus to ensure consistent state.

## 6. Performance Notes

- **Registry Partitioning**: Each building owns its own floor registry, so micro‑systems can run in parallel across buildings.
- **Cache‑Friendly Data**: The `FloorComponent` stores `std::unordered_set` of occupants; for tight loops we can switch to `std::vector<entt::entity>` when occupancy is low.
- **Lazy Rendering**: The renderer only draws floors that are within the current viewport, using the `DirtyRectTracker` to skip unchanged tiles.

## 7. Extensibility Hooks

- **Modding**: A mod can register a new `FloorType` component and a system that reacts to it (e.g., `ZeroGravityFloorSystem`).
- **Plug‑ins**: Expose a `IPlugin` that provides custom elevator logic (e.g., AI‑controlled elevators).
- **Procedural Hooks**: During building generation, a plugin can influence the number of floors per zoning type.

---

**Next Steps**: Add the `BuildingComponent`, `FloorComponent`, and `ElevatorComponent` definitions to `ecs/entt_component_types.md` and register them in `component_registry.h`.
