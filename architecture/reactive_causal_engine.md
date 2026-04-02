# Reactive Causal Engine: Architectural Specification

## 1. Overview
The **Reactive Causal Engine** is the core simulation backbone of Project Neon Oubliette. It enables a unified ECS registry to simulate multiple spatial scales (Macro-City and Micro-Interior) and five distinct causal layers (L0-L4) simultaneously. Causality flows both vertically (between layers) and horizontally (between spatial scales).

## 2. Spatial Scaling & Interior Activation
The engine uses an "On-Demand Interior Activation" protocol to manage simulation complexity.

### 2.1 Layer ID Isolation
To isolate micro-simulations while maintaining them in a single registry, a unique `layer_id` offset is used:
- **Macro Layer**: `layer_id = 0`.
- **Micro Layer**: `layer_id = (Building_Entity_ID * 100) + Floor_Level`.
This allows systems to filter entities based on their spatial context while allowing global systems (like Economy or Politics) to query all relevant entities regardless of their scale.

### 2.2 Interior Activation Protocol
1. **Trigger**: An NPC or Player moves onto a `BuildingComponent` or interacts with a `BuildingEntranceComponent`.
2. **Generation**: The `BuildingGenerationSystem` checks `InteriorGeneratedComponent`. If `is_generated == false`, it procedurally instantiates `FloorComponent`, `RoomComponent`, and vertical infrastructure.
3. **Transition**: The entity's `PositionComponent::layer_id` is updated to the corresponding micro-layer.
4. **Population Migration**: The `PopulationSystem` instantiates a Micro-NPC entity linked to the Macro-Citizen via `macro_citizen_id`, enabling persistent state across scales.

## 3. Causal Simulation Layers (L0-L4)
The simulation is divided into five interdependent layers, synchronized by the `SimulationCoordinator`.

| Layer | System | Description | Causal Output |
|-------|--------|-------------|---------------|
| **L0 Physics** | `InfrastructureSystem`, `EnvironmentalSystem` | Material decay, temperature, waste, and pollution. | Structural failure (BreakdownEvent), pollution accumulation. |
| **L1 Biology** | `EcosystemSystem`, `BiologySystem` | Metabolism, wildlife survival, and scavenge-driven population growth. | Population pressure, disease transmission (DiseaseEvent). |
| **L2 Cognitive** | `CognitiveSystem`, `AICore` | NPC mood, decision-making, and social relationships. | Individual labor output, faction loyalty shifts. |
| **L3 Economic** | `EconomicMarketSystem`, `EconomicSystem` | Aggregate wealth, GDP calculation, and taxation. | Unemployment rates, global trade liquidity. |
| **L4 Political** | `PoliticalOpinionSystem`, `PoliticalSystem` | Faction influence, public approval, and policy shifts. | Global "Pressure" modifiers (e.g., maintenance penalties). |

## 4. Cross-Layer Causality Examples
- **Physical Decay to Political Unrest**: `InfrastructureSystem` triggers a breakdown in a poor district (L0) -> NPCs in the district suffer mood penalties (L2) -> Faction loyalty to the ruling party drops (L4) -> Political influence shifts to a rival faction.
- **Waste to Wildlife**: `EnvironmentalSystem` tracks high waste in a building (L0) -> `EcosystemSystem` triggers a pest population spike (L1) -> NPCs experience health debuffs and increased biological stress (L1/L2).

## 5. Temporal Scheduling
The `SimulationCoordinator` manages turn-based execution:
- **High-Frequency (Every Turn)**: L0 Physics, L1 Biology, L2 Cognitive.
- **Low-Frequency (Periodic)**: L3 Economic, L4 Political.
This ensures macro-level trends emerge from micro-level interactions without overwhelming the simulation loop.
