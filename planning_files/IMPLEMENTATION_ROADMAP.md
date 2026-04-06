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

### Partially Done
- [x] Physics (Layer 0): temperature dissipation, weather effects, river cooling fields; pressure unused

### Not Started

- [ ] Currently, the city layout is terrible. Buildings are small and have no coherent layout. I want the city to be based on a grid, like Chicago. There is a very dense urban core with large skyscrapers and lots of traffic, train terminals, etc. Buildings are restricted to "lots" that are planned by the city government. So along a residential street, you would have apartments crammed together filling up their lots, very close to one another. Train stations will be especially built up commerce hubs.
- [ ] Currently, buildings can either be walked directly into on the overworld map, or entered via the door which moves the player into an "interior  space". In God mode, the player should not need to enter a door to explore a building. In standard mode, the player should ONLY be able to enter a building via the door and entering the "interior space."
- [ ] Show the 'held' item on the HUD
- [ ] Add cursor interaction.  
    - [ ] In standard mode, the interaction is limited by range- for example you can speak with somebody no more than 3 tiles away, but if you wanted to trade you need to be 1 tile away. 
    - [ ] In God mode, can click on any screen element to interact. 
    - [ ] The tile beneath the mouse cursor should be highlighted.
- [ ] Currently, interaction ('E') is very vague and limited. Let's enforce "types" of interaction- speak, observe, trade. The "range" of the chosen interaction is shown on the map by differently colored outlined tiles. The type of interaction currently selected is also displayed on the HUD.
- [ ] Speaking/talking between agents is a MAJOR component of city life. We need a complex speaking system both from player to agent and agent to agent. Players should be able to overhear conversations within range. This will be shown as text above the speakers, and can be broken up into "chunks" that update with each game step. For example: "I am ready..." *step* "...To go to the store." Players should be able to talk to agents, which will open a dialogue box. Response options will be displayed ie [a] Yes [b] Not sure, etc.
- [ ] Human agents should have concentric circles of familiar agents organized by importance: family trees, which are the most important, friends, which are second, coworkers which are third. The closeness relates to how frequently agents visit with other agents, how likely they are to live together, share gifts, etc.
- [ ] Religion system, agacent but not necessarily limited to factions