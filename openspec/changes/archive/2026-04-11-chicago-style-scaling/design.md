## Context

The current world generation uses a `macro_cell_size` of 20 tiles (or 120 in some initializations), leading to sparse building placement (e.g., 4 skyscrapers per 20x20 zone). Building heights are calculated using a simple linear distance-to-core formula, and the `stable_id` used for interior lookups is a 32-bit hash that is prone to collisions as density increases.

## Goals / Non-Goals

**Goals:**
- Increase building density by doubling `macro_cell_size` and significantly increasing subdivision counts.
- Implement a steepened "Chicago" height gradient based on zone type and distance to the urban core.
- Standardize mid-block alleys for all dense zones (CORPORATE, URBAN_CORE, RESIDENTIAL, SLUM).
- Ensure transit infrastructure remains at 1:1 scale while "enterable" structures shrink to fit smaller lots.
- Support multi-tile vehicle dimensions (6-tile cars, 12-tile trains).

**Non-Goals:**
- Redesigning the ECS or chunk streaming architecture.
- Implementing complex traffic simulation; only the static/spawn-time scaling of vehicles.
- Changing the building interior coordinate space (they remain 1:1 via portals).

## Decisions

- **Macro Cell Expansion**: Increase `macro_cell_size` to 40. This maintains the macro-coordinate system while providing 4x the area (1600 tiles vs 400) for subdivision.
- **Subdivision Strategy**: 
    - URBAN_CORE: 4x4 blocks, 2x2 lots per block. Results in ~4x4 tile buildings (packed tight).
    - RESIDENTIAL: 4x2 blocks, 6x3 lots per block. Results in ~2x2 tile row houses.
    - SLUM: 5x3 blocks, 8x3 lots per block. Results in ~1x2 tile shanty rows.
- **Height Formula**: Replace `std::max(1, 8 - (int)dist_to_core)` with a lookup table or steepened curve:
    - `dist 0-1`: 30 + rand(120)
    - `dist 1-2`: 15 + rand(35)
    - `dist 2-3`: 5 + rand(15)
    - `dist 3-5`: 3 + rand(5)
    - `dist 5+`: 1 + rand(3)
- **Collision Protection**: Upgrade `stable_id` to `uint64_t` in `BuildingComponent` and `LotComponent`. Use `(uint64_t)bx << 32 | (uint32_t)by` for hashing.
- **Vehicle Sizing**: Introduce `VehicleSizeComponent` `{width, length}`. Update `spawnPersonalVehicle` to set these values (e.g., length=6 for cars). Roads (8 tiles wide) already accommodate these sizes.

## Risks / Trade-offs

- **[Risk] Entity Count** → Increased density means 10-20x more building entities per chunk. 
    - *Mitigation*: The current LOD system and chunk-based streaming are designed for this. We must monitor frame times during `generate_chunk_content`.
- **[Risk] Interior Collisions** → Higher building density increases the chance of multiple buildings having the same coordinate-based ID.
    - *Mitigation*: Upgrade to 64-bit `stable_id` as decided.
- **[Trade-off] Visual Density vs. Playability** → Smaller buildings (4x4) might feel cramped to navigate in a 1st/3rd person view.
    - *Mitigation*: "Infrastructure" (streets, platforms) stays 1:1 to preserve navigation paths. Portals to interiors remain 1:1.
