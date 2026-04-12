## 1. Constants and Core Config

- [x] 1.1 Update `macro_cell_size` from 20 to 40 in `ecs/components/components.h`.
- [x] 1.2 Update `MACRO_CELL_SIZE` constant in `src/main.cpp` and related initialization logic.
- [x] 1.3 Ensure `chunk_size` is correctly updated (to 80) in `WorldConfigComponent` initialization.

## 2. City Planning (Subdivision)

- [x] 2.1 Update `subdivide_zone_into_blocks` in `ecs/systems/city_planner_system.cpp` with Chicago counts (e.g., URBAN_CORE 4x4 splits).
- [x] 2.2 Update `subdivide_block_into_lots` with new lot counts (e.g., URBAN_CORE 2x2, RESIDENTIAL 6x3, SLUM 8x3).
- [x] 2.3 Set `back_to_back = true` for CORPORATE, URBAN_CORE, RESIDENTIAL, and SLUM zone types in `subdivide_block_into_lots`.
- [x] 2.4 Verify block and lot creation logic handles fractional sizes correctly (via integer division).

## 3. City Generation (Height and Hashing)

- [x] 3.1 Update `BuildingComponent` and `LotComponent` in `ecs/components/components.h` to use `uint64_t` for `stable_id`.
- [x] 3.2 Implement the steepened height gradient in `generateZoneInterior` and `generateUrbanCoreInterior` based on `dist_to_core`.
- [x] 3.3 Update `stable_id` hashing formula in `CityGenerationSystem.h` to `(uint64_t)bx << 32 | (uint32_t)by`.
- [x] 3.4 Ensure `createBuildingShell` and `createSkyscraperShell` correctly render facade glyphs and windows for small footprints (4x4).

## 4. Transit and Infrastructure Scaling

- [x] 4.1 Audit `placeTrainTerminals` in `CityGenerationSystem.h` to ensure station concourse uses `createBuildingShell` (shrinks) while platforms stay 1:1.
- [x] 4.2 Audit `generateTransitInterior` to ensure rail tracks and platform safety tiles remain 1:1 scale.
- [x] 4.3 Verify `ROAD_WIDTH_SECONDARY` and `ROAD_WIDTH_PRIMARY` in `base_types.h` are sufficient for 6-tile vehicles.

## 5. Vehicle Dimensions

- [x] 5.1 Define `VehicleSizeComponent` in `ecs/components/components.h` with `length` and `width`.
- [x] 5.2 Update `spawnPersonalVehicle` in `CityGenerationSystem.h` to set `VehicleSizeComponent` (e.g., 6 for cars, 1 for bikes).
- [x] 5.3 Update `SizeComponent` for vehicles to match `VehicleSizeComponent` dimensions.

## 6. Validation and Testing

- [x] 6.1 Run the world generator and inspect the visual layout for URBAN_CORE and RESIDENTIAL zones.
- [x] 6.2 Verify that no `stable_id` collisions occur during interior lookup.
- [x] 6.3 Test vehicle movement and collision with new multi-tile dimensions.
