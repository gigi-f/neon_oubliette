# System Scheduler Design

## 1. Purpose
The `SystemScheduler` is responsible for orchestrating the execution of all simulation systems, ensuring deterministic order, managing parallel execution of micro-systems, and bridging communication between macro and micro layers. It adheres to the simulation loop defined in `architecture/macro_micro_design.md`.

## 2. Core Loop Phases

The main simulation loop is structured into distinct phases to manage dependencies and parallelism:

### 2.1. Input Phase (Main Thread)
*   Handles player input, external events, and system setup.
*   **Systems**:
    *   `InputProcessingSystem`: Reads player commands, translates to game events.

### 2.2. Macro Simulation Phase (Main Thread)
*   All systems operating on the `macro_registry` run sequentially.
*   These systems should primarily *read* from the macro registry and *push commands* to building-specific command buffers, or *publish events* to the `MacroEventBus`.
*   **Systems (Examples)**:
    *   `CityTimeSystem`: Updates `CityComponent::time_tick`.
    *   `ResourceSpawningSystem`: Introduces raw materials at `ResourceNodeComponent`s into `GlobalResourceStockpile`.
    *   `TrafficFlowSystem`: Updates vehicle positions, handles congestion, dispatches `RawMaterialDeliveryEvent` to building command buffers upon arrival.
    *   `AllocationSystem`: Assigns citizens to housing (`ApartmentComponent`s) based on `HousingPreferenceComponent`.
    *   `TaxationSystem`: Calculates and applies taxes.
    *   `BudgetManagementSystem`: Allocates funds to buildings.
    *   `MacroEconomySystem`: Updates city-wide economic indicators.

### 2.3. Micro Task Dispatch Phase (Main Thread)
*   The scheduler iterates over all `BuildingComponent` entities in the `macro_registry`.
*   For each building, it dispatches a parallel task to simulate its `micro_registry`.
*   Tasks are queued for a thread pool.

### 2.4. Micro Simulation Phase (Parallel Threads)
*   Each task processes a single building's `micro_registry`.
*   Systems in this phase *read* from the micro registry, *push commands* to the building's local `CommandBuffer`, and potentially *publish events* to the building's local `MicroEventBus`.
*   **Systems (Examples) - run per building**:
    *   `NPCMovementSystem`: Updates `PositionComponent` for `NPCComponent`s within the building.
    *   `ShopInteractionSystem`: Processes `PurchaseEvent`s, updates inventories.
    *   `WorkstationSystem`: Simulates NPC work, updates `SkillComponent`s.
    *   `ElevatorSystem`: Controls `ElevatorCar` movement, picks up/drops off NPCs.
    *   `MicroUtilitySystem`: Manages `UtilityAllocationComponent`s based on `ResidentComponent` consumption.
    *   `InteractionSystem`: Handles `InteractionQueue` for NPC-NPC social events.

### 2.5. Command Buffer Apply Phase (Main Thread)
*   After all micro-simulation tasks complete, the main thread iterates over all `BuildingComponent`s.
*   For each `BuildingComponent`, it calls `CommandBuffer::apply()` on its associated `micro_registry`.
*   The order of application is deterministic: by `entt::entity::id` of the `BuildingComponent`. This ensures that all deferred state changes from micro-systems are applied in a consistent, non-racy manner.

### 2.6. Event Bus Dispatch Phase (Main Thread)
*   **`MicroEventBus` Processing**: Collects all events published to local `MicroEventBus` instances during the Micro Simulation Phase. These are then aggregated and processed.
*   **`MacroEventBus` Processing**: Processes events published to the global `MacroEventBus` (potentially triggered by Macro systems or aggregated Micro events).
*   Events can trigger further actions or state changes, potentially enqueueing new commands for the next tick.
*   **Systems (Examples)**:
    *   `EventAggregationSystem`: Collates micro-events (e.g., total sales from all shops).
    *   `MacroEconomicImpactSystem`: Reacts to macro events (e.g., a major disaster event).

### 2.7. Output Phase (Main Thread)
*   Renders the current state of the world to the terminal.
*   **Systems**:
    *   `RenderingSystem`: Uses Notcurses to draw macro map, building interiors, UI elements.
    *   `LoggingSystem`: Writes simulation logs.

## 3. System Registration and Dependencies

*   Systems will be registered with the scheduler using a dedicated function (e.g., `Scheduler::addSystem<SystemType>(phase, execution_order)`).
*   Dependencies between systems within the same phase will be managed by explicit execution order or implicitly through data flow.
*   All systems will conform to a common interface (e.g., `void update(entt::registry& registry, float dt)`).

## 4. Threading Model
*   **Main Thread**: Handles Input, Macro Simulation, Micro Task Dispatch, Command Buffer Apply, Event Bus Dispatch, and Output.
*   **Worker Threads**: A thread pool will execute Micro Simulation tasks in parallel. Each worker thread operates on a distinct `micro_registry`.

## 5. Next Steps
*   Define the concrete C++ interface for `SystemScheduler`.
*   Create a `system_registration.cpp` file to formally register all systems with the scheduler.
*   Begin implementing systems like `TrafficFlowSystem` using this scheduler design.
