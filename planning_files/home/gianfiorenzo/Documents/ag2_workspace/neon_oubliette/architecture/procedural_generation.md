# Procedural Generation Design – City & Micro‑Worlds

## 1. Goals

- **Deterministic** generation that can be reproduced with a single seed.
- **Scalable**: support millions of tiles for macro‑worlds while keeping micro‑worlds lightweight.
- **Layered**: generate macro layers (city layout, zoning, infrastructure) and micro layers (buildings, rooms, shops) separately.
- **Extensible**: allow mods to plug in new generation algorithms (e.g., procedural biomes, custom building styles).
- **Data‑Driven**: expose configuration tables (JSON/YAML) for designers to tweak parameters without recompiling.

## 2. Architectural Overview

```
[Seed] ──► RNG ──► CityGenerator ──► CityRegistry
                          │          │
                          │          └─► BuildingGenerator
                          │                     │
                          └─► MicroWorldGenerator ──► RegionRegistry
```

- **RNG**: A single `std::mt19937_64` seeded with the world seed.  For deterministic *per‑building* generation we derive a sub‑seed from the building's unique ID.
- **CityGenerator**: Builds the macro‑world grid (city map), zones, roads, parks, and places macro entities (government, power plant, etc.).
- **BuildingGenerator**: For every `BuildingComponent` in the macro registry, a micro‑registry is instantiated and populated.
- **MicroWorldGenerator**: Handles generation of rooms, interior layouts, shops, and NPCs inside a building or a room.

## 3. Macro‑World Generation

| Step | Description | Data Structures |
|------|-------------|-----------------|
| 1. Terrain | Heightmap or flat grid. | `std::vector<int>` 2‑D or `boost::multi_array` |
| 2. Zoning | Random assignment based on city plan. | `std::unordered_map<entt::entity, Zoning>` |
| 3. Roads | Procedural maze or grid overlay. | `RoadComponent` with path polyline |
| 4. Macro Entities | Government, power plants, parks. | `std::vector<entt::entity>` |
| 5. Building Placement | Poisson disk sampling to avoid clustering. | `SpatialHash` |

The output is a `macro_registry` containing all macro components.  A `CityRegistry` struct holds the macro registry and a list of `BuildingComponent` references.

## 4. Micro‑World Generation

- Each `BuildingComponent` holds a `floor_registry` (`entt::registry`) that is initialized by `BuildingGenerator`.
- Floors are generated in a deterministic order (base → top).  For each floor:
  1. Create a `FloorComponent` and optional `InteriorLayoutComponent`.
  2. Randomly decide if the floor is a shop, office, residential apartment, or maintenance.
  3. If a shop, generate a `ShopComponent` with inventory.
- Rooms are generated with a simple BSP (Binary Space Partitioning) algorithm.  Each node becomes a room; walls are added between nodes.
- NPCs and items are spawned via `NPCGenerator` and `ItemGenerator`, which use sub‑seeds derived from the floor ID.

## 5. Randomness Model

- **Global Seed**: Provided by the user or random.
- **Entity‑Level Seeds**: For each entity `e`, compute `seed = hash(global_seed, e)` using a 64‑bit hash (e.g., `std::hash`).  Use this seed to instantiate a `std::mt19937_64`.
- **Determinism**: All random decisions must be recorded or derived from the entity's seed; no global RNG state is mutated during systems.

## 6. Procedural Hooks

- **Plugin API**: `IGenerationPlugin` with methods:
  - `void on_city_generated(CityRegistry&, rng_type&);`
  - `void on_building_generated(BuildingComponent&, FloorRegistry&, rng_type&);`
  - `void on_floor_generated(FloorComponent&, rng_type&);`
- **Configuration**: Each plugin can read its own config file (JSON) at load time.

## 7. Integration with Event Bus

- After a building is fully generated, a `BuildingGeneratedEvent` is posted to the macro bus.
- Micro‑world systems can subscribe to this event to perform post‑generation logic (e.g., place elevator shafts).
- When a new floor is spawned, a `FloorGeneratedEvent` is posted to the micro bus.

## 8. Performance Considerations

- **Lazy Generation**: Only generate micro‑worlds when the player enters the building or when a system requires it.
- **Caching**: Store generated floor data in a file (serialization) so subsequent loads are instant.
- **Parallel Generation**: Use the thread‑pool to generate independent buildings in parallel during the `CommandBufferApply` phase.

## 9. Future Work

- Add procedural city biomes (industrial, residential, commercial) based on altitude or district.
- Implement a *flood* system where water can overflow from lower districts to higher ones, affecting vertical flow.
- Provide a GUI for designers to tweak generation parameters.

---

**Next** – Create the `economy_and_shops.md` spec detailing money flow, resource trading, shop mechanics, and macro‑micro economic interactions.
