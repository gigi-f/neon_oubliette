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
- [x] Agent spawning: `AgentSpawnSystem` creates 40+ agents with varying archetypes (Citizen, Guard) at startup
- [x] World bounds enforcement: `MovementSystem` clamps all movement to map dimensions
- [x] Consumable items: `CityGenerationSystem` spawns food/water items; agents can detect and seek them
- [x] BioSim (Layer 1): `BiologySystem` implements temperature-based consciousness degradation and recovery
- [x] Inspection Overhaul (Phase 1.5): Proximity-based targeting, cross-layer insights, and unique ASCII visual metaphors for all 5 modes
- [x] Guard Patrols (Phase 1.3): Guards now assigned waypoints and patrol tasks
- [x] Global Weather System (Phase 5.3 Prep): Markov-chain weather cycle affecting environment and agents
- [x] Macro-Zoning System (Phase 2.1): WFC-lite solver assigns logical urban zones (Corporate, Slum, Industrial, etc.)
- [x] Zone-Aware Generation: `CityGenerationSystem` places buildings and terrain based on Macro-Zone logic

### Partially Done
- [ ] Procedural city layout: Initial zoning implemented; advanced arterial grid pending
- [ ] Physics (Layer 0): temperature dissipation, weather effects; pressure unused

### Not Started
- [ ] Zoning adjacency constraint refinement (more complex rules)
- [ ] Large-scale world (200×200+) with chunk streaming or district transitions
- [ ] FOV / line of sight
- [ ] Day/night cycle affecting agent behavior
- [ ] Economy, faction, barter (systems registered but all stub)

---

## Phase Overview

| Phase | Focus | Key Deliverable |
|-------|-------|-----------------|
| 1 | Immediate Playability | Agents alive, world feels inhabited |
| 1.5 | Inspection Overhaul | All 5 modes show distinct, useful simulation data |
| 2 | Procedural City | Zoned, logical, randomized map |
| 3 | Massive Scale | Large world with streaming/transitions |
| 4 | Agent Depth | Schedules, needs, social fabric |
| 5 | Simulation Layers | Biology, economy, factions tick meaningfully |

---

## Phase 1: Immediate Playability (DONE)

Goal: ensure the city feels inhabited and simulation systems are linked.

### 1.1 Spawn Agent Entities (DONE)
### 1.2 Fix World Bounds (DONE)
### 1.3 Basic Agent Variety (DONE)
### 1.4 Consumable Items in World (DONE)
### 1.5 Inspection Mode Overhaul (DONE)

---

## Phase 2: Procedural City with Logical Zoning (IN PROGRESS)

Goal: replace the hardcoded 60×30 test map with a procedurally generated city that follows
real urban logic.

### 2.1 Zone System (DONE)
- [x] Define zone types and adjacency rules
- [x] Implement WFC-lite solver for Macro-Grid zoning
- [x] MacroZoneComponent for district-wide simulation modifiers

### 2.2 Procedural Street Grid (PARTIAL)
- [x] Basic switching based on zone type
- [ ] Primary streets every ~20 tiles (arterials)
- [ ] Secondary streets within zones

### 2.3 Zone-Aware Building Placement (DONE)
- [x] Building archetypes pool (corporate -> Skyscrapers, slum -> Shanties)
- [x] Density-driven placement logic

---

## Phase 3: Massive Scale — World Streaming

Goal: support a world of 200×200+ tiles without loading everything into memory.

### 3.1 Chunk Architecture
- Divide the world into 40×40 chunks. Active 3×3 region.
- `ChunkStreamingSystem` loads/unloads chunks based on player position.

### 3.2 Agent LOD (Level of Detail)
- `MacroAgentRecord` for agents in unloaded chunks.
- Promotion/demotion logic for ECS entities.

---

## Phase 4: Agent Depth

### 4.1 Daily Schedule System
- Time-of-day cycle.
- `WORK`, `HOME`, `LEISURE` goal selection.

### 4.2 Agent Memory
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
