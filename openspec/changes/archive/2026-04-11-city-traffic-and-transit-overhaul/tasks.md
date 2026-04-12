## 1. Components & Events

- [x] 1.1 Create `TrafficLightComponent` in `ecs/components/infrastructure_components.h`.
- [x] 1.2 Create `LaneComponent` to define road paths.
- [x] 1.3 Create `ArticulatedSegmentComponent` for multi-car vehicles.
- [x] 1.4 Add `AmbientTagComponent` to distinguish essential from non-essential entities.
- [x] 1.5 Register new components in `ecs/component_registration.cpp`.

## 2. Intersection System

- [x] 2.1 Implement `IntersectionSystem` to handle traffic light cycles.
- [x] 2.2 Add FIFO right-of-way logic to `IntersectionSystem` for unprotected junctions.
- [x] 2.3 Integrate `IntersectionSystem` into `ecs/system_registration.cpp`.

## 3. Traffic & Transit Overhaul

- [x] 3.1 Update `TrafficFlowSystem` to support lane-following and intersection stops.
- [x] 3.2 Update `TransitSystem` to spawn multi-car trains using `ArticulatedSegmentComponent`.
- [x] 3.3 Implement the "Path Replay" logic in `TransitSystem` for articulated segments.
- [x] 3.4 Update `TransitSystem` to cluster pedestrians at stations.

## 4. City Planner & World Gen Overhaul (Super-blocks)

- [x] 4.1 Modify `CityPlannerSystem::subdivide_zone_into_blocks` to support larger clusters.
- [x] 4.2 Implement building lot grouping (2x2 or N-by-M) with zero road separation.
- [x] 4.3 Add chance to substitute secondary roads with 2-tile wide pedestrian paths.
- [x] 4.4 Update `CityGenerationSystem` to render `PEDESTRIAN_PATH` terrain.

## 5. Ambient Population

- [x] 5.1 Implement `AmbientSpawnSystem` with viewport-based spawning logic.
- [x] 5.2 Define spawn density parameters in `data/configs/world_generator_params.json`.
- [x] 5.3 Ensure aggressive despawning of ambient entities outside the simulation radius.

## 6. Pedestrian Behavior

- [x] 6.1 Add "Hurry" movement state to `MovementSystem` / `AgentTaskComponent`.
- [x] 6.2 Implement the "Hurry to Transit" trigger in `PedestrianMovementSystem`.

## 7. Testing & Validation

- [x] 7.1 Create `tests/traffic_intersection_test.cpp` to verify traffic light compliance.
- [x] 7.2 Verify articulated train movement stays on rails in a sample scenario.
- [x] 7.3 Manually verify "Super-block" visuals and road/pedestrian path density in an URBAN_CORE zone.
