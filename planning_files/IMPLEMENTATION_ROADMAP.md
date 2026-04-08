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
- [x] Drivable personal vehicles (scooters, bikes, cars, xci-fi vehicles)
- [x] Ridable trains, buses, xci-fi vehicles (by both player and agents) (Phase 4.2)
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
- [x] **A.2 — Urban Core & Skyscrapers**
    - [x] Tag the central macro-zone cluster as `URBAN_CORE`; apply a density multiplier to lot occupancy
    - [x] Skyscraper buildings: footprints 6–20 tiles wide, rendered with vertical ASCII metaphors (height encoded in glyph brightness / color tier)
    - [x] Ground-floor commercial mandatory on Urban Core lots facing arterial roads
    - [x] High pedestrian and vehicle traffic volume spawned proportionally to lot density
    - [x] Multiple train terminals placed at key arterial intersections; terminals are designated `COMMERCE_HUB` anchor points
- [x] **A.7 — Multi-Room Interior Generation**
    - [x] All buildings regardless of size must generate at least 2 interior rooms; minimum room count scales with footprint: small (2–4 tiles wide) = 2 rooms, medium (5–10) = 3–5 rooms, large (11+) = 6+ rooms
    - [x] Room layout generated via a BSP (binary space partition) split of the building footprint; minimum room dimension is 2×2 tiles to ensure navigability
    - [x] Each room assigned a functional tag based on building type: e.g., apartment → BEDROOM / KITCHEN / BATHROOM / LIVING; office tower → LOBBY / OFFICE / SERVER_ROOM / EXECUTIVE_SUITE; factory → FLOOR / STORAGE / SUPERVISOR_OFFICE
    - [x] Room tags drive furniture/item spawning: KITCHEN spawns food items, SERVER_ROOM spawns tech loot, BEDROOM spawns personal items and a BED tile
    - [x] Rooms are connected by internal door tiles in shared walls; at least one path must exist from the entrance door to every room (BFS-validated at gen time)
    - [x] Staircase tiles generated for multi-floor buildings; each floor is an independent BSP layout with matching staircase positions
    - [x] Interior room data stored in `BuildingInteriorComponent` (room list, door positions, stair positions) so it can be serialized per-chunk and restored on re-entry without regeneration
- [x] **A.10 — Window Tiles**
    - [x] Building generator places window tiles on exterior walls facing streets (not shared walls or alley walls) at a frequency set by building archetype: residential = many windows, server room = none, executive suite = floor-to-ceiling
    - [x] Window tiles are impassable but **FOV-transparent**: the existing FOV system treats them as see-through in both directions, allowing partial sight from sidewalk into ground-floor rooms and vice versa
    - [x] Window tiles do not allow entry; attempting to interact with a window from outside emits a HUD note ("You peer through the glass.") and opens a limited inspect panel for anything visible on the other side
    - [x] Broken windows (integrity < 50 per Phase K decay) become passable as a squeeze crawl point — treat as a 1-tile door with a movement speed penalty and a noise event that may trigger guards
- [x] **B.1 — Enforce Door-Only Entry in Standard Mode**
    - [x] Remove all code paths that allow walking directly into a building footprint tile
    - [x] Movement system: collide with all non-door building tiles; only process entry event on `DOOR` tiles
    - [x] Interior generation triggered exclusively by door-entry event (already partially working — harden the contract)
    - [x] Each exterior `DOOR` tile stores a `DoorMetadata` record: which wall it sits on (NORTH / SOUTH / EAST / WEST), its offset from the wall's left edge in tiles, and the building entity it belongs to
- [x] Spatially Consistent Exit Doors: interior exits match exterior entry wall/offset; BFS-validated connectivity
- [x] **C.1 — Held-Item Slot in HUD**
    - [x] Reserve a fixed HUD region (lower-left, adjacent to health bar) for the held item
    - [x] Render item glyph (ASCII) + short name (truncated to ~12 chars) in the slot
    - [x] If no item held, display `[empty]` in dim color
- [x] **C.2 — Item Status Indicators**
    - [x] Show charge count or durability bar beneath the glyph for tools/weapons
    - [x] Flash the slot briefly when the item is used or swapped
    - [x] Color-code by item category (consumable = green, weapon = red, tool = cyan, key = yellow)
- [x] **D.1 — Cursor Rendering**
    - [x] Render a highlight glyph over the tile currently under the mouse cursor using a dedicated notcurses plane
    - [x] Highlight color changes based on what is under the cursor: agent (yellow), item (cyan), door (green), empty (dim white)
    - [x] Keyboard cursor: arrow keys move the cursor tile-by-tile when mouse is not available; cursor snaps back to player on movement
- [x] **D.2 — Standard Mode Range Enforcement**
    - [x] Each interaction type carries a max range (tiles): Speak = 3, Observe = 6, Trade = 1
    - [x] Cursor highlight turns red when the target tile exceeds the active interaction's range
    - [x] Attempting to interact outside range emits a HUD notification ("Too far to trade")
    - [x] Range is measured as Chebyshev distance to match 8-directional movement
- [x] **D.3 — God Mode Click Interaction**
    - [x] Left-click on any visible tile opens the inspection panel for that entity (same data as `I` key inspect, no range limit)
    - [x] Right-click opens a context menu: Inspect / Follow / Teleport Cursor / Tag
    - [x] Middle-click (or `F` key) locks the camera onto the clicked agent and tracks them until dismissed
- [x] **E.1 — Interaction Mode Enum & State**
    - [x] Add `InteractionMode { SPEAK, OBSERVE, TRADE }` to the player state component
    - [x] Cycle through modes with `Tab` (or a dedicated bind); default is `OBSERVE`
    - [x] HUD shows the active interaction mode icon and label at all times (e.g., `[MODE: SPEAK]`)
- [x] **E.2 — Range Ring Rendering**
    - [x] Render a colored border (blue/white/gold) around the player matching active interaction range
    - [x] Real-time redraw on movement; plane-based z-ordering below entities
- [x] Dialogue system: modal panel, ASCII portraits, contextual NPC utterances (Phase F.4)
- [x] SPEAK interaction: triggers dialogue with agents within range (Phase E.3)
- [x] **Phase G — Social Graph & Relationship System (DONE)**
    - [x] **G.1 — RelationshipComponent**: Map of relationship records holding tier, affinity, and last interaction.
    - [x] **G.2 — Family Trees**: Family units assigned at spawn with shared homes and high initial affinity.
    - [x] **G.3 — Friendship Formation**: Repeated proximity + positive interaction increases affinity and upgrades tiers.
    - [x] **G.4 — Coworker Relationships**: Shared workplace leads to coworker status and affinity drift.
    - [x] **G.5 — Relationship-Driven Behaviors**: Gift-giving, socialization need fulfillment, and grief on death.
    - [x] **G.6 — Macro-Scale Relationship Simulation**: Affinity drift and social events for agents in cold chunks.
- [x] **Phase F.8 — Statistical Macro-Conversation**: Off-screen agents (MacroAgentRecord) converse statistically: affinity scores drift based on proximity and faction alignment per macro tick.
- [x] **Information Propagation (Phase F.8)**: "Viral" rumors spread through macro-cells statistically, materializing as live `InformationRecord` items when the chunk loads.
- [x] **K.1 — Building Health & Decay**: `BuildingHealthComponent` tracks integrity; `UrbanDecaySystem` simulates deterioration via weather/pollution and repairs via property value.
- [x] **K.2 — Maintenance & Squatting**: `BuildingHealthComponent` now tracks `num_squatters` and `maintenance_urgency`; `UrbanDecaySystem` calculates squatting impact; Agents seek "Squattable" buildings; Maintenance workers seek `REPAIR` tasks.
- [x] **K.3 — Demolition & Rebuilding**: `DemolitionSystem` clears building entities on 0 integrity; `RebuildingSystem` reconstructs structures on vacant lots via `RebuildEvent`.
- [x] **O.1 — Raw Material Layer**:
    - [x] `RawMaterialFieldComponent` implemented for chunk-level resource density.
    - [x] `ResourceNodeComponent` and `ExtractionProgressComponent` added for physical extraction points.
    - [x] `ResourceDistributionSystem` manages regeneration and scarcity calculation.
    - [x] `CityGenerationSystem` seeds fields and nodes based on Zone types (Industrial, Park, Slum).
    - [x] Agent behavior updated to seek and extract resources during WORK routine.
    - [x] Economic loop closed: Extraction rewards agents based on local scarcity and material value.
- [x] **P.1 — ReputationComponent (Player)**: Player now initialized with `ReputationComponent`, `Layer4PoliticalComponent`, and `Layer2CognitiveComponent`; `FactionSystem` updated to route `AgentFactionReputationEvent` to player reputation tracking.

### Partially Done
- [x] Physics (Layer 0): temperature dissipation, weather effects, river cooling fields; pressure unused
- [x] **E.3 — Interaction Dispatch**
    - [x] Pressing `Space` or left-clicking (or `E` key) while the cursor is on a valid target fires the active interaction type
    - [x] Interaction events now include target coordinates for Standard Mode range checking and specific object targeting

### Not Started

---
#### Phase A — City Layout Overhaul (Grid-Based Urban Form)

*Goal: replace the current scatter-placement with a Chicago-style rectilinear grid where every building occupies a surveyed lot.*

- [x] **A.1 — Lot & Parcel System**
- [x] **A.3 — Residential Streets**
- [x] **A.4 — Commerce Hub Around Train Stations**
- [x] **A.5 — Street Hierarchy Integration**
- [x] **A.6 — Street-Facing Door Placement**
- [x] **A.8 — Interior Pathfinding Grid**
- [x] **A.9 — Interior Map Caching & Stability Contract**

---

#### Phase B — Building Entry Rules (Standard vs. God Mode)

- [x] **B.1 — Enforce Door-Only Entry in Standard Mode**
- [x] **B.2 — Spatially Consistent Exit Door**
- [x] **B.3 — God Mode Direct Inspection / Entry**
- [x] **B.4 — HUD Contextual Indicators**
- [x] **B.5 — Sound Propagation Through Walls**

---

#### Phase C — HUD Held-Item Display

- [x] **C.1 — Held-Item Slot in HUD**
- [x] **C.2 — Item Status Indicators**

---

#### Phase D — Cursor Interaction System

- [x] **D.1 — Cursor Rendering**
- [x] **D.2 — Standard Mode Range Enforcement**
- [x] **D.3 — God Mode Click Interaction**

---

#### Phase E — Typed Interaction System

- [x] **E.1 — Interaction Mode Enum & State**
- [x] **E.2 — Range Ring Rendering**
- [x] **E.3 — Interaction Dispatch**

---
#### Phase F — Conversation & Speech System

- [x] **F.1 — Systemic Dialogue Assembly Engine**
- [x] **F.2 — Content Domains: What NPCs Talk About**
- [x] **F.3 — Speech Styles, Dialects & Filtering**
- [x] **F.4 — Agent-to-Agent Conversation Engine**
- [x] **F.5 — Overhead Speech Rendering**
- [x] **F.6 — Overheard Conversations & Intelligence**
- [x] **F.7 — Player Dialogue Modal (Expansion of Existing Stub)**
- [x] **F.8 — Conversation Simulation at Macro Scale**

---

#### Phase G — Social Graph & Relationship System (DONE)

- [x] **G.1 — RelationshipComponent**
- [x] **G.2 — Family Trees**
- [x] **G.3 — Friendship Formation**
- [x] **G.4 — Coworker Relationships**
- [x] **G.5 — Relationship-Driven Behaviors**
- [x] **G.6 — Macro-Scale Relationship Simulation**

---

#### Phase H — Religion System
- [x] **H.1 — Religion Data Model**
- [x] **H.2 — Agent Religiosity**
- [x] **H.3 — Places of Worship**
- [x] **H.4 — Religious Gatherings & Emergent Ritual**
- [x] **H.5 — Faction & Religion Interplay**
- [x] **H.6 — Proselytizing Behavior**

---

#### Phase I — Crime & Underground Economy

*Goal: activate the existing guard, economics, and social graph systems through emergent criminal behavior.*

- [x] **I.1 — Crime Behavior Archetypes**
- [x] **I.2 — Theft & Mugging**
- [x] **I.3 — Black Market & Fencing**
- [x] **I.4 — Drug Manufacturing**
- [x] **I.5 — Player Wanted Level**
- [x] **I.6 — Guard Response System**

---
#### Phase J — Population Lifecycle & Demographics

*Goal: agents are born, age, and die — the city's composition shifts over simulation time.*

- [x] **J.1 — Age & Life Stage Component**
- [x] **J.2 — Birth System**
- [x] **J.3 — Death & Inheritance**
- [x] **J.4 — Demographic Pressure**
- [x] **J.5 — Generational Faction & Religion Drift**

---

#### Phase K — Environmental Decay & Urban Renewal

*Goal: buildings age, deteriorate, and can be demolished and rebuilt, making zoning a living contest.*

- [x] **K.1 — Building Health & Decay**
- [x] **K.2 — Maintenance & Squatting**
- [x] **K.3 — Demolition & Rebuilding**
- [x] **K.4 — Graffiti & Environmental Texture**

---

#### Phase L — Dynamic Crises & World Events

*Goal: periodic macro-scale perturbations that create distinct narrative chapters in each playthrough.*

- [x] **L.1 — Crisis System Core**
- [x] **L.2 — Economic Crises**
- [x] **L.3 — Biological / Environmental Crises**
- [x] **L.4 — Political / Faction Crises**
- [x] **L.5 — Power Grid Failure**
- [x] **L.6 — God Mode Crisis Dashboard**
    - [x] Dedicated UI panel for God Mode to monitor simulation "health" and systemic stress
    - [x] Real-time sparklines/graphs for Economic, Political, Biological, and Environmental stress metrics
    - [x] Active Crisis tracking: list of ongoing city-wide events with severity and time-to-resolution
    - [x] Causal Conductivity visualization: explicit list of propagation vectors (e.g., how a market crash leads to crime)
    - [x] Interactive toggle (V key) and integration with the God Mode HUD

---

#### Phase M — Sewer & Underground Network Layer

*Goal: a hidden traversal layer beneath the grid used by criminals, resistance factions, and alien entities.*

- [x] **M.1 — Sewer Map Generation**
- [x] **M.2 — Sewer as a Traversal Layer**
- [x] **M.3 — Sewer Inhabitants & Items**
- [x] **M.4 — Environmental Hazards**

---

#### Phase N — News & Information Propagation

*Goal: make the political simulation legible and manipulable through a formal information layer.*

- [x] **N.1 — Information Item Types**
- [x] **N.2 — Broadcast Towers**
- [x] **N.3 — Underground Media**
    - [x] `PirateNodeComponent` for clandestine, illegal broadcast stations
    - [x] `DataSlabComponent` for physical information storage items
    - [x] `UndergroundMediaSystem` manages pulse corruption, guard detection, and slab reading
    - [x] Integrated with `CityGenerationSystem` for Slum/Sewer seeding
    - [x] Visual pulse rendering in `RenderingSystem`
- [x] **N.4 — Player Information Interaction**
    - [x] Intel Log UI: dedicated history panel (toggled by 'N') showing all collected rumors, veracity, and hops.
    - [x] Rumor Trading: players can receive, buy, or share intel via the dialogue system.
    - [x] Verification: players can ask NPCs to verify rumors, updating veracity based on NPC trust.
    - [x] Pirate Node Hacking: players can discover and interact with pirate nodes to inject their own intel into the city's pulse sequence.
    - [x] Faction Impact: sharing specific intel types (Propaganda/Intelligence) directly shifts NPC loyalty and faction standing.
- [x] **N.5 — Information Decay & Verification**

---

#### Phase O — Supply Chains & Manufacturing

*Goal: close the economic loop — goods are produced, not just traded.*

- [x] **O.2 — Factory Buildings & Production Cycles**
    - [x] `FactoryComponent` and `ProductionRecipe` components implemented.
    - [x] `ProductionSystem` added to the simulation loop (Layer 3 Economic).
    - [x] `AgentTaskType::PRODUCE_GOODS` handled in `AgentActionSystem`.
    - [x] `CityGenerationSystem` seeds factories with recipes and starting materials in Industrial zones.
    - [x] `ProductionCompletedEvent` triggers item creation and HUD notification.
- [x] **O.3 — Supply Chain Disruption**
    - [x] `SupplyChainDisruptionComponent` tracks `transport_bottleneck`, `labor_strike`, and `sabotage_risk`.
    - [x] `SupplyChainSystem` (Layer 3 Economic) updates disruption based on infrastructure health, public opinion, and active crises.
    - [x] `ProductionSystem` modified to scale efficiency by disruption levels and consume/lose stock due to bottlenecks.
    - [x] `AgentActionSystem` (Worker behavior) adds a chance for labor strikes to interrupt `PRODUCE_GOODS` tasks.
    - [x] `SupplyChainDisruptedEvent` for systemic signaling and visual feedback via speech/HUD.
- [x] **O.4 — Player & Agent Interaction with Manufacturing**
    - [x] `FactoryJobComponent` and `MuleComponent` for employment and logistics tracking.
    - [x] Player interaction: "Take a Job" and "Start Shift" at industrial facilities via the Interaction System.
    - [x] `ActivitySystem` integration: WORKING activity contributes to factory progress and pays wages (credits) to agents and players.
    - [x] Logistic "Mule" agents: Autonomous agents seek finished goods at factories and transport them to shops/markets.
    - [x] Industrial Inspection: Multi-layer inspection now reveals owner, active recipe, production progress, and supply chain disruptions.

---

#### Phase P — Player Reputation System

*Goal: the world's stance toward the player shifts based purely on simulated observed behavior — no explicit leveling.*

- [x] **P.1 — ReputationComponent (Player)**
- [x] **P.2 — Reputation Event Sources**
- [x] **P.3 — Faction Response to Reputation**
    - [x] Defined `ReputationTier` thresholds and helper `get_tier()` in `ReputationComponent`.
    - [x] Interaction blocks for `EXCOMMUNICATED` players in `InteractionSystem`.
    - [x] Trading markup/discounts based on Reputation Tiers in `BarterSystem`.
    - [x] Reputation-based greetings and tags in `DialogueSystem`.
    - [x] Guard pursuit and civilian flight response to `EXCOMMUNICATED` player in `AgentDecisionSystem`.
    - [x] Standing and Tier visibility in `InspectionSystem` (Political tab).
- [x] **P.4 — Reputation Decay & Recovery**
- [x] **P.5 — HUD & Inspection Integration**

---

#### Phase T — Advanced Trading & Barter System

*Goal: a full-featured, high-stakes bartering interface with visual inventory management and dynamic agent negotiation.*

- [x] **T.1 — Split-Screen Barter UI**
- [x] **T.2 — Dynamic Valuation & Bartering**
    - [x] Item categorization: Added `ItemMarketCategory` and `ItemMaterialComponent` to items.
    - [x] Chunk-level scarcity: `BarterSystem` now scales item value based on local `MarketDemandComponent` scarcity multipliers.
    - [x] Macro-economic tie-in: `BarterSystem` incorporates macro-zone `RawMaterialType` scarcity into base item pricing.
    - [x] Systemic utility: Trades are evaluated against agent needs (hunger/thirst), biological state (medical items for sick/elderly), and environmental context (tools/tech in storms).
    - [x] Economic loop: `EconomicSystem` aggregates item-category demand from agents and weather conditions.
- [x] **T.3 — Negotiation Mechanics**
    - [x] Rumors as secondary currency: Information utility scaled by veracity, hops, and faction relevance.
    - [x] Dynamic counter-offers: NPCs suggest specific items from player inventory to bridge value gaps based on biological needs (hunger/thirst/health).
    - [x] Pressure Mechanics: "Push your luck" leverage system with success rates tied to reputation, personality, and biological state (desperation/frustration).
    - [x] Visual Tension Layers: Barter UI with patience metaphors (☺/⚄/⚠), greed meters, and contextual NPC feedback.
    - [x] Social Feedback: Individual affinity shifts in `RelationshipComponent` based on trade success, insults, or failed pressure.

#### Misc
- [ ] Size/metric system: decide on the metric size of one world tile (ie 1 tile = 1 meter). Buildings, environment objects, etc. should be built to the real-world-equivalent scale.
- [ ] Buildings on the overworld should be a 1:1 ratio with their interior counterparts. Research the real world ratios of human to skyscraper.