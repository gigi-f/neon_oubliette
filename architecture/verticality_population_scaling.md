# Reactive Causal Engine: Verticality & Population Scaling

## 1. Vertical Transition Matrix
The **VerticalSystem** now manages a bidirectional transition matrix between the Macro-City (Layer 0) and procedural Micro-Interiors.
- **Interior Layer Convention**: Interiors use a calculated `layer_id`: `(building_entity_index * 100) + floor_level`.
- **Exit Logic**: Floor 0 of any interior is the "Exit Plane." A downward transition (`dz < 0`) on Floor 0 triggers a re-entry into the Macro-City at the building's specific $(X, Y)$ coordinates.

## 2. On-Demand Interior Generation
The **BuildingGenerationSystem** implements the *Interior Activation Protocol*.
- **Causal Layout**: When a building is entered, its interior is procedurally generated. Rooms are instantiated with specific socio-economic tags (`ApartmentComponent`, `WorkstationComponent`) and L0 physical components (`ConditionComponent`).
- **Persistence**: Once generated, the `InteriorGeneratedComponent` persists, ensuring that cross-layer causality (e.g., a pipe bursting in the basement) remains consistent even if the player exits the building.

## 3. Macro-Micro Population Migration
The **PopulationSystem** bridges the city's populace across scales.
- **Instantiation**: When a building is "Activated" (generated), the system searches for all `CitizenComponent` entities whose `HomeComponent` or `WorkplaceComponent` points to that building.
- **Migration**: These macro entities are instantiated as `NPCComponent` entities in the building's micro-layer. This ensures that the city is truly inhabited, and NPCs aren't just random "mobs" but persistent citizens with jobs and homes.

## 4. Circular Causality: Simulation Pressure
The engine now features **Feedback Pressure** where global metrics influence local physical decay.
- **neglect_modifier**: The `InfrastructureSystem` (L0) now reads the `PublicOpinionComponent` (L4). If political approval for the ruling infrastructure faction is low, a `maintenance_penalty` is applied to all local infrastructure decay rates, simulating systemic neglect.
- **Environmental Decay**: Local `WasteComponent` accumulation (L0) further accelerates `ConditionComponent` decay, creating a localized feedback loop where poor sanitation (L0) and low political will (L4) lead to rapid physical collapse.
