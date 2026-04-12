## Why

The current city scaling is generic and lacks the density and height characteristics of a dense urban environment like Chicago. By increasing subdivision counts and expanding the height range, we can create a more vibrant, authentic urban experience with a clear gradient from high-rise cores to dense residential row-houses.

## What Changes

- **Increased Macro Cell Size**: Increase `macro_cell_size` from 20 to 40 to allow for more granular subdivisions without changing the overall feel of the world.
- **Enhanced Subdivision Logic**: Update `subdivide_zone_into_blocks` and `subdivide_block_into_lots` to significantly increase building counts (e.g., URBAN_CORE from 4 to 64 buildings per zone).
- **Chicago Height Gradient**: Implement a steepened height curve based on distance from the URBAN_CORE, ranging from 150-floor glass towers to 1-4 floor dense slums.
- **Dense Alley Grid**: Extend mid-block alley logic (back-to-back lots) to CORPORATE and URBAN_CORE zones.
- **Scale-Aware Transit**: Differentiate between enterable station structures (which scale with lots) and infrastructure like rail platforms and tracks (which stay 1:1 for gameplay clarity).
- **Vehicle Dimensions**: Update vehicle spawning to support multi-tile sizes (6-tile cars, 12-tile trains) and ensure road widths accommodate them.
- **Collision Robustness**: Upgrade `stable_id` logic to prevent collisions as building density increases.

## Capabilities

### New Capabilities
- `city-scaling`: Updating macro cell size and subdivision logic to increase lot density.
- `height-gradient`: Implementing the steepened distance-based height curve for buildings.
- `transit-infrastructure-scaling`: Defining rules for what shrinks (buildings) and what stays 1:1 (infrastructure).
- `vehicle-dimensions`: Adding size tracking and multi-tile spawning for vehicles.

### Modified Capabilities
- (None - no existing specs found)

## Impact

Affected systems and components:
- `WorldConfigComponent` (macro_cell_size)
- `CityPlannerSystem` (subdivision logic)
- `CityGenerationSystem` (floor counts and shell creation)
- `TransitSystem` (platform and track logic)
- `PersonalVehicleComponent` and vehicle spawning logic
- `LotComponent` and building `stable_id` hashing
