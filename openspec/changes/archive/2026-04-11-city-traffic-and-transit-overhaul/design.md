## Context

The current `TrafficFlowSystem` and `TransitSystem` implement basic entity movement but lack urban density and regulated flow. Vehicles move directly to buildings without respecting road structures or intersections. Transit is limited to single-entity "vehicles" (single glyphs). Pedestrians lack spatial awareness of transit schedules and arrivals.

The current `CityPlannerSystem` creates a road between every building, leading to an unrealistic "road-heavy" city and squished building aesthetics.

## Goals / Non-Goals

**Goals:**
- Implement a multi-state `TrafficLightComponent` and an `IntersectionSystem` for regulated traffic flow.
- Introduce `AmbientSpawnSystem` for dynamic city population based on player proximity.
- Support articulated multi-segment vehicles (L-trains, articulated buses).
- Enhance NPC pathfinding with "Transit-Aware" behaviors (Hurry, Clustering).
- Redesign `CityPlannerSystem` to support "super-blocks" (building clusters) and pedestrian paths.

**Non-Goals:**
- Full traffic simulation with complex pathfinding (A* for every car); simple lane-following is preferred for performance.
- Direct player-vehicle interaction (e.g., player driving cars).
- Dynamic road construction (roads are static and generated at startup).

## Decisions

### 1. Lane-Following over Direct-to-Target Movement
**Decision:** Transition `TrafficFlowSystem` to follow a sequence of `LaneComponent` targets instead of moving directly to a building entity.
**Rationale:** This ensures vehicles stay on roads and creates the visual of organized traffic.
**Alternatives:** Full NavMesh; discarded as overkill for a grid-based ASCII terminal game.

### 2. Articulated Segment Path Replay
**Decision:** Child segments of an articulated vehicle (train cars) will store a queue of the parent's previous N positions and "replay" them.
**Rationale:** This is computationally cheap and ensures the train segments always stay exactly on the tracks followed by the engine.
**Alternatives:** Rigid body constraints; too complex for the current ECS simulation loop.

### 3. Proximity-Based Ambient Spawning
**Decision:** The `AmbientSpawnSystem` will use a "Virtual Viewport" centered on active camera entities to manage spawning.
**Rationale:** Minimizes entity count while ensuring the player's immediate surroundings always feel busy.
**Alternatives:** Global city simulation; discarded due to performance constraints in large cities.

### 4. Block Clustering and Pedestrian Paths
**Decision:** `CityPlannerSystem` will group lots into N-by-M clusters (e.g., 2x2) and only place roads between these clusters. Some roads will be replaced with 2-tile wide `PEDESTRIAN_PATH` terrain.
**Rationale:** Increases building density and creates a more realistic urban feel by breaking up the repetitive road grid.
**Alternatives:** Manual city layout; discarded as the city is procedurally generated at scale.

## Risks / Trade-offs

- **[Risk] Performance Degeneracy** → Ambient traffic could balloon entity count. **Mitigation:** Strict caps on ambient entity counts and aggressive despawning outside the viewport.
- **[Risk] Intersection Deadlocks** → FIFO logic could lead to 4-way deadlocks. **Mitigation:** Implement a "fallback timeout" where vehicles proceed after a fixed delay regardless of priority if stuck.
- **[Risk] Path De-sync** → Fast-moving trains might have "gaps" between cars if position history isn't high-resolution. **Mitigation:** Sample parent position every sub-tick if necessary or use interpolated history.
- **[Risk] Accessibility** → Narrow paths might block larger vehicles. **Mitigation:** Ensure the planner maintains a connected graph for primary/secondary roads suitable for large vehicles.
