# Implementation Roadmap
## Project Neon Oubliette — Living City Focus

Last updated: June 2024

---

## Design Philosophy

**The simulation comes first. The player comes second.**

The primary goal of this project is a complex, vibrant, self-sustaining city that would
exist and evolve whether or not a player was present. Autonomous agents live, move, work,
trade, form factions, get sick, and die according to the simulation rules. The player is an
observer and participant in that world — not the reason the world exists.

When prioritizing features, always ask: *does this make the city more alive?* Agent behavior,
population dynamics, economic cycles, and emergent social structure take precedence over
player-facing abilities, UI polish, or content authored specifically for the player.

Player abilities (inspection, interaction, inventory) are tools for *reading* the simulation,
not for driving it.

---

## Actual Status (as of last audit)

### Done
- [x] ECS core: EnTT registry, event bus, dispatcher, phase-locked simulation coordinator
- [x] Turn management (L0/L1 every tick, L2 every 5, L3 every 10, L4 every 20)
- [x] notcurses rendering: player-centered camera, scrolling, true color, HUD, multi-plane z-order
- [x] Input system: movement, interaction, vertical layer change, inspect, inventory, ESC close
- [x] A* pathfinding (layer 0, obstacle-aware, 8-directional)
- [x] Movement system (shared for player + NPCs, collision-aware, world-bounds clamping)
- [x] Inspection system: modal panel, privacy masks, 5 inspection modes, ESC to close
- [x] Inventory panel: toggled with B, hidden by default, no overlap
- [x] HUD: health, credits, layer indicator, notifications, full controls legend
- [x] Building interior generation on door entry
- [x] Agent logic core: AgentDecisionSystem, AgentActionSystem, NeedsComponent, GoalComponent, CurrentPathComponent
- [x] Agent spawning: `AgentSpawnSystem` creates 100+ agents with varying archetypes (Citizen, Guard) at startup
- [x] World bounds enforcement: `MovementSystem` clamps all movement to map dimensions
- [x] Consumable items: `CityGenerationSystem` spawns food/water items; agents can detect and seek them
- [x] BioSim (Layer 1): `BiologySystem` implements temperature-based consciousness degradation and recovery
- [x] Inspection Overhaul (Phase 1.5): Proximity-based targeting, cross-layer insights, and unique ASCII visual metaphors for all 5 modes
- [x] Guard Patrols (Phase 1.3): Guards now assigned waypoints and patrol tasks
- [x] Global Weather System (Phase 5.3 Prep): Markov-chain weather cycle affecting environment and agents
- [x] Macro-Zoning System (Phase 2.1): WFC-lite solver assigns logical urban zones (Corporate, Slum, Industrial, etc.)
- [x] Infrastructure Skeleton (Phase 2.2): `InfrastructureNetworkSystem` carves rivers, primary roads, and rails across a 200x200 world
- [x] Topological Junctions: Automated resolution of arterial overlaps into Bridges, Level Crossings, and Intersections
- [x] Access Paths: BFS-based carving of paths connecting building doors to the nearest street
- [x] Causal Conductivity: `InfrastructureInfluenceSystem` applies field effects (cooling near rivers, economic boost near transit) to simulation layers
- [x] Zone-Aware Generation: `CityGenerationSystem` places buildings and terrain based on Macro-Zone and Arterial logic
- [x] Secondary Street Connectivity: Zoning-specific "Capillary" networks (Phase 2.2)
- [x] Chunk Architecture (Phase 3.1): `ChunkStreamingSystem` and `ChunkComponent` registered; 40x40 tile chunks (2x2 macro-cells) implemented.
- [x] World Streaming (Phase 3.1): Efficient chunk-based loading/unloading for massive worlds (tested at 8000x8000).
- [x] Massive World Population: `AgentSpawnSystem` now supports spawning thousands of agents into "cold storage" chunks.
- [x] Agent LOD (Phase 3.2): `MacroAgentRecord` implemented with statistical simulation for off-screen agents.
- [x] Large Scale Pathfinding (Phase 3.3): Hierarchical A* across Macro-Cells (Arterial Graph) then Local Tiles.
- [x] Day/night cycle affecting agent behavior (Phase 4.1): Routine-based goals (SLEEP, WORK, LEISURE).
- [x] Utility-Based Bartering (Phase 5.2): Trades evaluated via agent needs (hunger/thirst) and faction affinity.
- [x] Dynamic Economics (Phase 5.2): Chunk-level Supply/Demand and scarcity-based item valuation.
- [x] Faction Influence Fields (Phase 5.4): Layer 4 political simulation with influence diffusion across chunks.
- [x] Airports (Phase 2.3): Procedural airport zones with terminals, runways, and cargo logistics.
- [x] Colosseums (Phase 2.3): Procedural sports arenas with central arenas, seating, and Syndicate gladiators.

### Partially Done
- [x] Physics (Layer 0): temperature dissipation, weather effects, river cooling fields; pressure unused

### Not Started
- [x] FOV / line of sight
- [x] Drivable personal vehicles (scooters, bikes, cars, sci-fi vehicles)
- [x] Ridable trains, buses, sci-fi vehicles (by both player and agents) (Phase 4.2)
- [x] Item and tool usage
- [x] Nature spaces (parks)
- [x] Brainstorming session to create a coherent sci-fi world and vibe: then implement story concepts- ie each faction is led by a different AGI or superhuman intelligence with different, competing goals and ways of interacting with their followers. (DONE)
- [x] Stockmarket that agents actually participate in
- [x] Futuristic sports- colloseums? (Phase 2.3)
- [x] Robot to human social hierarchy, expectations and interactions (tied into the "story" of the game-world) (Phase 4.2)
- [x] Extraterrestial life, loosely influenced by "Book of the New Sun" (tied into the "story" of the game-world)
    - [x] Xeno Biology Layer (Cacogen/Hierodule species)
    - [x] Influence Auras (XenoSystem) affecting simulation layers
    - [x] Unique ASCII portraits and Inspection insights
    - [x] Integration with Chunk Streaming and Macro-Agent Record
- [x] God mode, alternative gameplay style in which the game runs at a steady clip (say 2fps default with ability to change) but the player can pause time, and then use a cursor (highlighted square on the map) to investigate items, agents, buildings, etc. "God overview" that shows running actions or developments across all agents, economy, politics, etc. Ensure easy way to switch gameplay modes. Break into sub steps as possible.

---

## Phase Overview

| Phase | Focus | Key Deliverable |
|-------|-------|-----------------|
| 1 | Immediate Playability | Agents alive, world feels inhabited |
| 1.5 | Inspection Overhaul | All 5 modes show distinct, useful simulation data |
| 2 | Procedural City | Zoned, connected, randomized map |
| 3 | Massive Scale | Large world (400+ macro tiles) with streaming |
| 4 | Agent Depth | Schedules, needs, social fabric |
| 5 | Simulation Layers | Biology, economy, factions tick meaningfully |

---

## Phase 1: Immediate Playability (DONE)

Goal: ensure the city feels inhabited and simulation systems are linked.

---

## Phase 2: Procedural City with Logical Zoning & Connectivity (DONE)

Goal: replace the hardcoded test maps with a procedurally generated city that follows
real urban logic and provides physical connectivity between structures.

---

## Phase 3: Massive Scale — World Streaming & LOD (DONE)

Goal: support a world of 400×400+ macro-tiles without loading everything into memory.

### 3.1 Chunk Architecture (DONE)
- [x] Define `ChunkComponent` and `MacroAgentRecord`.
- [x] Divide the world into 40×40 chunks (align with 2x2 macro-cells).
- [x] **Hot Region:** 3x3 chunks around player are fully instantiated ECS entities.
- [x] **Efficient Streaming:** Optimized `ChunkStreamingSystem` with chunk map and state tracking.
- [x] **Massive World:** Scaled simulation to 8000x8000 tile grid (160,000 macro tiles).

### 3.2 Agent LOD (Level of Detail) (DONE)
- [x] **MacroAgentRecord:** Components defined for storing agent state during dematerialization.
- [x] **Seamless Transition:** Statistical agents are "Materialized" into ECS entities when player approaches.
- [x] **Statistical Simulation:** `simulate_macro_agents` implements survival decay for off-screen agents.

### 3.3 Large Scale Pathfinding (DONE)
- [x] **Hierarchical A*:** Pathfind across Macro-Cells (Arterial Graph) then Local Tiles.
- [x] **Spatial Indexing:** Infrastructure Node (Junction) graph for global navigation.
- [x] **Segmented Pathing:** Agents automatically request new local paths when reaching macro-nodes.

---

## Phase 4: Agent Depth (IN PROGRESS)

### 4.1 Daily Schedule System (DONE)
- [x] Time-of-day cycle.
- [x] `ScheduleComponent` and `RoutineState` (SLEEPING, WORKING, LEISURE, COMMUTING).
- [x] Persistent home/work location assignment in `AgentSpawnSystem`.
- [x] Routine-based goal selection in `AgentDecisionSystem`.
- [x] Statistical routine movement in `ChunkStreamingSystem` (macro-sim).

### 4.2 Social Hierarchy & Interaction (DONE)
- [x] `SocialHierarchyComponent` defining status and class titles.
- [x] `SocialInteractionSystem` implementing status-based yielding and deference.
- [x] Faction and species-aware status assignment (Synths vs. Humans).
- [x] Integration with Inspection system for behavioral insights.

### 4.3 Agent Memory
- `AgentMemoryComponent`: remembers food, danger, allies.

### 4.3 Faction Affiliation
- Deeper faction reaction logic.

---

## Phase 5: Simulation Layer Depth

### 5.1 Biology Ticks (Layer 1)
- Injury propagation and wound tracking.

### 5.2 Economy Ticks (Layer 3)
- Supply/demand at market entities.

### 5.3 Weather System (Layer 0)
- Persistent weather states (Clear, Rain, Acid Rain, Smog) with mechanical effects. (BASIC IMPLEMENTATION DONE)
