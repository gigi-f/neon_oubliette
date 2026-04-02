# Project Neon Oubliette - Simulation Layers Architecture

## Philosophy
We adopt a **Multi-Scalar Entity-Component-System (ECS)** approach where every object exists simultaneously across multiple simulation layers. Rather than monolithic objects, we have **Layered Entities** - each layer operates independently but subscribes to events from other layers.

## The Layer Stack

### Layer 0: Physics & Topology (Foundation)
- **Granularity**: Discrete grid cells (1m3 resolution)
- **Simulates**: Temperature, pressure, material phase states, structural integrity
- **Update Frequency**: Every turn

### Layer 1: Biological Systems (The Organic)
- **Granularity**: Individual organisms down to organ systems
- **Simulates**: Organ health (liver, kidneys, heart, cybernetics), metabolic processes, neural states, pathogen vectors
- **Key Insight**: A dead body is not stateless - it becomes an ecosystem for decomposition

### Layer 2: Cognitive/Social (The Mind)
- **Granularity**: Individual agents with BDI (Belief-Desire-Intention) architecture
- **Simulates**: Social graph, memory system, motivation engine, faction allegiance

### Layer 3: Economic (The Flow)
- **Granularity**: Individual transactions up to macro-economic indicators
- **Simulates**: Inventory with provenance, market nodes, currency flows, resource networks

### Layer 4: Political/Power (The Structure)
- **Granularity**: Organizations down to specific policies
- **Simulates**: Law state, faction influence, infrastructure decay

## Cross-Layer Event Propagation

Example: Gunshot Wound
- L1 (Biology): Organ damage -> Blood loss event
- L2 (Social): Witness fear response -> Memory formation
- L3 (Economic): Medical resource consumption -> Price spike
- L4 (Political): Hospital report -> Crime stat update -> Patrol allocation

## Implementation Strategy

### Component-Based Design
Every entity has a LayerComponent for each relevant layer:
- PhysicsComponent (always)
- BiologyComponent (living things)
- CognitiveComponent (agents)
- WalletComponent (economic actors)
- AllegianceComponent (political actors)

### Phase-Locked Simulation
Different layers tick at different rates:
- Turn N: All layers process
- Turn N+1 to N+4: L0-L1 (fast physics/biology)
- Turn N+5: + L2 (cognition)
- Turn N+10: + L3 (economics batch)
