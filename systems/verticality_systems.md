# Verticality Systems

This document describes the systems responsible for managing vertical movement, floor-to-floor logistics, and the structural integrity of the Neon Oubliette's skyscraper-dungeons.

## 1. Elevator System (`ElevatorSystem`)

Handles the movement of elevator cars and the queuing of entities (NPCs and cargo) waiting for transport.

### Components Used:
- `ElevatorComponent`: Tracks `current_floor`, `target_floors` (queue), and `door_state`.
- `NPCComponent`: Used to check if an NPC is "waiting" at a floor.

### Logic Flow:
1. **Query**: Find all `ElevatorComponent` entities.
2. **Pathfinding**: For each elevator, determine the next floor in the `target_floors` queue using a SCAN algorithm (elevators prioritize moving in their current direction until no more requests remain in that direction).
3. **Arrival**: When `current_floor == next_target`, emit an `ElevatorEvent`.
4. **Transition**: Entities on the floor with a matching `target_floor` are moved into the elevator (or vice-versa).

## 2. Floor Transition System (`FloorTransitionSystem`)

Manages the seamless migration of entities between the `MacroRegistry` (city view) and individual `MicroRegistry` instances (building interiors).

### Process:
- **Scaling Down**: When a player or NPC enters a building, the system spawns their micro-representation in the relevant `MicroRegistry` floor and hides/deactivates the macro-entity.
- **Scaling Up**: When an entity exits the building, the micro-entity's state (wealth, items) is synced back to the macro-entity, and the micro-entity is destroyed/pooled.

## 3. Vertical Power Grid System (`PowerGridSystem`)

Simulates the distribution of electricity from macro power plants down to individual micro floors.

### Logic:
1. **Macro Supply**: `PowerPlantComponent` updates the `WorldStats` power pool.
2. **Building Demand**: Each `BuildingComponent` calculates the sum of `power_requirement` from its children `FloorComponent`s.
3. **Distribution**: Power is allocated to buildings based on priority/zoning. If supply < demand, lower-priority floors (usually lower levels or non-essential zones) experience brownouts, affecting `ResourceStockpile` production.
