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
    - [x] Define a `LotComponent` (world-space AABB, zone class, ownership entity, street-facing side enum)
    - [x] Introduce a `CityPlannerSystem` that runs once at world-gen and subdivides each macro-zone into rectangular city blocks separated by streets
    - [x] Block sizes vary by zone: Corporate/Commercial (medium), Residential (narrow), Slums (dense/small)
    - [x] Each block is further divided into individual lots; lot width/depth driven by zone density tables
    - [x] Each lot records which of its four edges is the **street-facing side**; this drives door placement for every building on that lot
    - [x] Corner lots may have two street-facing sides; recorded via bitmask
    - [x] Store the full lot grid in the ECS registry (entities linked to MacroZones)

- [x] **A.3 — Residential Streets**
    - [x] Row-house / apartment packing: buildings fill their lots edge-to-edge with only alley gaps
    - [x] Shared-wall detection: adjacent buildings on the same block share a wall tile rather than having a gap
    - [x] Setback rules: arterials require a 1-tile sidewalk; capillary streets require none
    - [x] Residential density gradient: lots nearest Urban Core are taller apartments; outer lots are 1–2 story row-houses

- [x] **A.4 — Commerce Hub Around Train Stations**
    - [x] `COMMERCE_HUB` anchor triggers a radial upzoning: lots within N tiles are zoned `MIXED_COMMERCIAL`
    - [x] Markets, vendors, and kiosks auto-spawned in the hub radius
    - [x] Elevated foot-traffic simulation: agents path through hubs when commuting

- [x] **A.5 — Street Hierarchy Integration**
    - [x] Ensure `InfrastructureNetworkSystem` arterials align to grid axes; no diagonal arterials inside city limits
    - [x] Sidewalk tiles auto-generated along all street edges; agents prefer sidewalks over road tiles when not in vehicles
    - [x] Alleys carved between back-to-back building rows for service access

- [x] **A.6 — Street-Facing Door Placement**
    - [x] Building generator reads the lot's `street-facing side`; places the primary door on that wall, horizontally centered (or offset for narrow lots)
    - [x] Buildings on alleys get a secondary service door on the alley-facing wall in addition to the primary street door
    - [x] Door tiles are never placed on a shared-wall edge (the edge touching an adjacent building on the same block)
    - [x] Train stations and large commercial buildings may have multiple primary doors evenly distributed along the street-facing wall
    - [x] Door placement validated post-generation: if no adjacent passable street tile exists within 1 tile of the door, the door is relocated to the nearest valid wall position

- [x] **A.8 — Interior Pathfinding Grid**
    - [x] Each building maintains its own independent 2D nav grid built from the BSP room layout; wall tiles are obstacles, door tiles are passable, furniture tiles are obstacles
    - [x] Interior nav grid stored inside `FloorComponent` (linked to building floors); rebuilt only when the interior layout changes
    - [x] `MovementSystem` and `PathfindingSystem` switch to the floor-local grid whenever an entity is in interior state (layer_id != 0)
    - [x] Agents and player can pathfind on the building nav grid to navigate between rooms and across floors
    - [x] Staircase tiles are edge connections between per-floor nav grids; A* treats them as valid transitions between layers

- [x] **A.9 — Interior Map Caching & Stability Contract**
    - [x] A building's interior layout is generated exactly **once** per building lifetime; the result is serialized into `BuildingInteriorComponent` via cereal and committed to chunk save data
    - [x] On every subsequent entry (player or agent), the saved layout is loaded — **never re-randomized** — so items looted in a previous visit stay gone and furniture remains in the same position
    - [x] Layout is regenerated only if the building undergoes structural change: demolition + rebuild (Phase K), a Raid event that destroys interior partitions, or a fire/explosion event
    - [x] Dirty flag on `BuildingInteriorComponent` marks the layout as needing re-save; flushed when the owning chunk is serialized to disk

---

#### Phase B — Building Entry Rules (Standard vs. God Mode)

- [x] **B.1 — Enforce Door-Only Entry in Standard Mode**
    - [x] Remove all code paths that allow walking directly into a building footprint tile
    - [x] Movement system: collide with all non-door building tiles; only process entry event on `DOOR` tiles
    - [x] Interior generation triggered exclusively by door-entry event (already partially working — harden the contract)
    - [x] Each exterior `DOOR` tile stores a `DoorMetadata` record: which wall it sits on (NORTH / SOUTH / EAST / WEST), its offset from the wall's left edge in tiles, and the building entity it belongs to

- [x] **B.2 — Spatially Consistent Exit Door**
    - [x] When the player enters through an exterior door, the entry door's wall and offset are recorded on the player's `InteriorStateComponent`
    - [x] The interior map generated for that building places the exit door on the **same wall** and at the **same relative offset** as the entry door in world space — so walking out of the building deposits the player on the correct sidewalk tile
    - [x] For multi-floor buildings, the ground-floor interior always has its exit door matching the exterior entry; upper floors exit via staircases only, not exterior doors
    - [x] If the building has multiple exterior doors (e.g., a train station), each door generates its own corresponding interior exit tile at the matching position in the interior map
    - [x] Consistency validated at gen time: a BFS from the interior exit tile must reach all rooms; if blocked, the BSP room layout is re-partitioned until the path is valid

- [x] **B.3 — God Mode Direct Inspection / Entry**
    - [x] In God Mode, cursor hover over any building footprint tile opens an inline info panel (floor plan outline, occupants, room list)
    - [x] `G` key (or configurable bind) while cursor is over a building teleports the God-Mode viewport into that building's interior
    - [x] Interior view rendered as an overlay with a clear visual border distinguishing "you are inside X building" from the overworld
    - [x] God Mode interior view shows all rooms simultaneously on one plane (no door-to-door navigation required)
    - [x] Navigating back out via ESC or clicking outside the building footprint returns the viewport to the overworld

- [x] **B.4 — HUD Contextual Indicators**
    - [x] HUD always shows current entry context: `[OVERWORLD]`, `[INTERIOR: Reza Tower L3]`, `[GOD: Reza Tower]`
    - [x] While inside a building, HUD additionally shows current room tag (e.g., `[ROOM: KITCHEN]`) based on the player's tile position
    - [x] Minimap thumbnail updates to show interior floor plan when inside a building, with the player's current room highlighted

- [x] **B.5 — Sound Propagation Through Walls**
    - [x] Overhead speech (Phase F.2) and overheard conversations (Phase F.3) use an open-air range model on the overworld, but inside buildings sound is attenuated per wall crossed
    - [x] Build a room adjacency graph from the BSP layout: each node is a room, each edge is an internal door; edge weight = 0 for open doors, 1 for closed doors, 2 for solid walls with no door
    - [x] Flood the room graph from the speaking agent's current room; sound audibility drops by one tier per edge crossed — CLEAR (0 walls), MUFFLED (1–2), INAUDIBLE (3+)
    - [x] CLEAR speech renders at full opacity with normal color; MUFFLED speech renders in italic dim style (same as overheard in F.3); INAUDIBLE speech is not rendered at all
    - [x] Player outside a building hears speech from ground-floor rooms adjacent to window tiles as MUFFLED, providing atmospheric flavor without full eavesdropping ability
    - [x] Sound attenuation data recalculated lazily when room doors open/close; cached per room pair until topology changes

---

#### Phase C — HUD Held-Item Display

- [x] **C.1 — Held-Item Slot in HUD**
    - [x] Reserve a fixed HUD region (lower-left, adjacent to health bar) for the held item
    - [x] Render item glyph (ASCII) + short name (truncated to ~12 chars) in the slot
    - [x] If no item held, display `[empty]` in dim color
- [x] **C.2 — Item Status Indicators**
    - [x] Show charge count or durability bar beneath the glyph for tools/weapons
    - [x] Flash the slot briefly when the item is used or swapped
    - [x] Color-code by item category (consumable = green, weapon = red, tool = cyan, key = yellow)

---

#### Phase D — Cursor Interaction System

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

---

#### Phase E — Typed Interaction System

- [x] **E.1 — Interaction Mode Enum & State**
    - [x] Add `InteractionMode { SPEAK, OBSERVE, TRADE }` to the player state component
    - [x] Cycle through modes with `Tab` (or a dedicated bind); default is `OBSERVE`
    - [x] HUD shows the active interaction mode icon and label at all times (e.g., `[MODE: SPEAK]`)

- [x] **E.2 — Range Ring Rendering**
    - [x] When an interaction mode is active, render a border of colored tiles around the player showing max range
    - [x] SPEAK ring = blue outline; OBSERVE ring = white outline; TRADE ring = gold outline
    - [x] Ring redraws each tick in case the player moves; rendered on its own notcurses plane below entities

- [x] **E.3 — Interaction Dispatch**
    - [x] Pressing `Space` or left-clicking while the cursor is on a valid target fires the active interaction type
    - [x] Interaction events now include target coordinates for Standard Mode range checking and specific object targeting
    - [x] SPEAK: opens dialogue system (see Phase F)
    - [x] OBSERVE: opens inspection panel for target (same as inspect key, no additional privacy masking)
    - [ ] TRADE: opens the split-screen Barter Panel (see Phase T)

---

#### Phase F — Conversation & Speech System

*This is the single largest remaining system. Break implementation into sub-phases.*

- [ ] **F.1 — Agent-to-Agent Conversation Engine**
    - [ ] Add `ConversationComponent` to agents: tracks current conversation partner entity, topic stack, and step counter
    - [ ] `ConversationSystem` (L1) selects conversation pairs from nearby agents with idle/leisure goals
    - [ ] Topic selection driven by agent `NeedsComponent`, current events (weather, faction news, economy), and relationship tier
    - [ ] Conversations have a duration (N steps); agents stand facing each other while conversing
    - [ ] Topics serialized as tagged string templates with variable substitution (agent name, price, location, etc.)

- [ ] **F.2 — Overhead Speech Rendering**
    - [ ] Speech rendered as floating text above speaker's tile on a dedicated high-Z notcurses plane
    - [ ] Text delivered in "chunks" — one phrase segment appears per simulation step, replacing the previous
    - [ ] Maximum visible text width = 20 chars; longer utterances automatically chunked with ellipsis continuation ("...going to the market" → next step → "...near the east gate.")
    - [ ] Speech bubble rendered in the speaker's faction color; fades (alpha ramp) as it reaches the last chunk
    - [ ] Player sees overhead speech only for agents within FOV; text of out-of-FOV conversation is not rendered

- [ ] **F.3 — Overheard Conversation Visibility**
    - [ ] Player can "overhear" conversations within 5 tiles even if not a participant
    - [ ] Overheard speech shown in italic / dim style to distinguish from addressed speech
    - [ ] Optional: conversation log panel (toggled with `L`) records last N overheard utterances with speaker name

- [x] **F.4 — Player Dialogue System**
    - [x] SPEAK interaction on an agent opens a full-screen modal dialogue panel
    - [x] Panel shows: speaker portrait (ASCII block), agent name/title, current utterance text, and response options
    - [x] Response options labeled `[a]`, `[b]`, `[c]`… generated contextually from the topic and relationship tier
    - [ ] Dialogue outcomes feed back into the simulation: selecting "buy info" transfers credits, selecting "threaten" lowers relationship score, etc.
    - [ ] Agents remember recent dialogue with the player (stored in `RelationshipComponent`); repeat visits unlock new topic branches

- [ ] **F.5 — Conversation Simulation at Macro Scale**
    - [ ] Off-screen agents (MacroAgentRecord) still "converse" statistically — relationship scores drift based on proximity and faction alignment
    - [ ] Major conversation outcomes (deals struck, rumors spread) logged as simulation events consumable by downstream systems

---

#### Phase G — Social Graph & Relationship System

- [ ] **G.1 — RelationshipComponent**
    - [x] Add `RelationshipComponent` to all human agents: map of `{ entity → RelationshipRecord }` where record holds tier (FAMILY / FRIEND / COWORKER / ACQUAINTANCE / STRANGER), affinity score (-100 to 100), last-interaction tick, and shared-home flag
    - [ ] Limit stored records per agent to ~50 to cap memory; LRU eviction for aging acquaintances

- [ ] **G.2 — Family Trees**
    - [ ] At agent spawn, assign family units: parents, siblings, and optionally children
    - [ ] Family members start with maximum affinity and FAMILY tier; affinity decays slowly if they never interact
    - [ ] Family members prefer to spawn in the same residential chunk and share a home building
    - [ ] Death events notify all FAMILY-tier relationships and apply a temporary needs/mood penalty

- [ ] **G.3 — Friendship Formation**
    - [ ] Repeated proximity + positive interaction ticks increase affinity between ACQUAINTANCE agents
    - [ ] When affinity crosses a threshold, tier upgrades to FRIEND
    - [ ] Friends visit each other's homes during LEISURE goal windows
    - [ ] Friends share information (rumors, economic tips) during conversations, propagating simulation state

- [ ] **G.4 — Coworker Relationships**
    - [ ] Agents assigned to the same workplace building become COWORKER-tier on first shared workday
    - [ ] Coworker affinity affected by workplace conditions (faction favorability, crowding, pay level)
    - [ ] Coworkers may form friendships over time if affinity crosses the friendship threshold

- [ ] **G.5 — Relationship-Driven Behaviors**
    - [ ] Gift-giving: high-affinity agents occasionally transfer a consumable or low-value item during LEISURE visits
    - [ ] Co-habitation: FAMILY and very close FRIEND pairs may register the same home tile; share food/water stockpiles
    - [ ] Grief / mourning: death of a FAMILY/FRIEND entity triggers temporary goal disruption (agent wanders, skips work)
    - [ ] Social needs: add `socialization` need score; agents seek known FRIEND/FAMILY entities when socialization is low

- [ ] **G.6 — Macro-Scale Relationship Simulation**
    - [ ] MacroAgentRecord stores a compressed social summary (family count, friend count, avg affinity) for off-screen processing
    - [ ] Social events (marriages, breakups, deaths) simulated statistically in macro-chunks and materialized when the chunk loads

---

#### Phase H — Religion System

- [ ] **H.1 — Religion Data Model**
    - [ ] Define a `ReligionRecord` struct: name, primary deity (or philosophical principle), core dogma tags (ASCETIC / HEDONIST / HIERARCHICAL / EGALITARIAN / XENO_REVERENT / etc.), associated faction affinity modifiers, and holy-day schedule
    - [ ] Load religion definitions from a JSON schema (parallel to faction definitions); 6–10 religions at world-gen, each tied loosely to a zone cluster
    - [ ] Each religion has a home building type: Temple, Shrine, Underground Chapel, Broadcast Tower (for digital cults), etc.

- [ ] **H.2 — Agent Religiosity**
    - [ ] Add `ReligiosityComponent`: religion entity, devotion score (0–100), last-worship tick, and schism flag
    - [ ] Devotion affects mood, faction alignment drift, and willingness to accept certain trade/speech outcomes
    - [ ] Agents with high devotion prioritize attending worship events during LEISURE windows
    - [ ] Devotion decays slightly each day if the agent does not worship; rises on attending services or major holy days

- [ ] **H.3 — Places of Worship**
    - [ ] `CityGenerationSystem` places religion-specific buildings in appropriate zones (Temples in Corporate/Civic zones, Underground Chapels in Slum zones, Shrines in parks)
    - [ ] Interior generation for worship spaces: nave/altar layout, seating glyphs, distinct color palette per religion
    - [ ] Worship attendance triggers a `WorshipEvent`; nearby agents with matching religion entity join if in LEISURE goal

- [ ] **H.4 — Religious Gatherings & Emergent Ritual**
    - [ ] `ReligionSystem` (L2) schedules holy days based on in-game calendar; broadcasts a `HolyDayEvent` that overrides LEISURE goal for high-devotion agents
    - [ ] Processions: agents form a moving group that walks a fixed route through the city (uses existing group-pathfinding primitives)
    - [ ] Tension events: two processions of rival religions occupying the same tile cluster trigger a `ReligiousTensionEvent` which may escalate to conflict via the faction system

- [ ] **H.5 — Faction & Religion Interplay**
    - [ ] Each faction leader (AGI) has a stance toward each religion: PATRON / NEUTRAL / SUPPRESSOR
    - [ ] PATRON factions fund temple construction and grant devotion bonuses to followers
    - [ ] SUPPRESSOR factions periodically raid places of worship (generates `RaidEvent`) and impose devotion penalties
    - [ ] A religion can accumulate enough influence to spawn a new faction if suppressed long enough (emergent politicization)

- [ ] **H.6 — Proselytizing Behavior**
    - [ ] High-devotion agents have a chance to initiate a SPEAK-type conversation with STRANGER/ACQUAINTANCE agents with no religion or low devotion
    - [ ] Successful proselytizing (affinity > threshold, no rival religion) assigns the target agent to the religion with minimal starting devotion
    - [ ] Failed proselytizing lowers the affinity between the two agents

---

#### Phase I — Crime & Underground Economy

*Goal: activate the existing guard, economics, and social graph systems through emergent criminal behavior.*

- [ ] **I.1 — Crime Behavior Archetypes**
    - [ ] Add `PICKPOCKET`, `MUGGER`, `DEALER`, and `FENCE` agent archetypes to spawn tables; weighted heavily toward Slum and Industrial zones
    - [ ] Criminal agents have a `CrimeRiskComponent`: boldness score (0–100) that rises with hunger/debt and falls after successful guards response
    - [ ] Criminal actions dispatched as typed goal states: STEAL_FROM_AGENT, MULE_GOODS, SELL_CONTRABAND

- [ ] **I.2 — Theft & Mugging**
    - [ ] PICKPOCKET behavior: agent moves within 1 tile of a target with high credits or visible inventory, rolls vs. target awareness score; success silently transfers a random item or credit amount
    - [ ] MUGGING behavior: low-light tiles only (night cycle or unlit alleys); agent blocks target path and issues a demand; target can comply (transfer credits) or attempt to flee
    - [ ] Awareness of theft feeds into `GuardAlertSystem`: witnesses generate a `CrimeReportEvent`; nearby guards may respond depending on faction allegiance to the victim

- [ ] **I.3 — Black Market & Fencing**
    - [ ] FENCE building type placed in back-alley lots of Slum zones; not visible on the lot grid (off-registry)
    - [ ] Stolen items flagged with a `StolenFlag` component; cannot be sold at regular vendors until fenced
    - [ ] Fencing transfers stolen flag and revalues the item at 40–60% of market price; FENCE agent takes a cut
    - [ ] DEALER agents sell contraband items (narcotics, illegal tech) with no `StolenFlag` but flagged `Contraband`; possession triggers guard search if player or agent is stopped

- [ ] **I.4 — Drug Manufacturing**
    - [ ] Certain Industrial zone buildings can be designated `CLANDESTINE_LAB` during world-gen or through faction investment
    - [ ] Labs consume raw chemical inputs (spawned at docks/cargo) and produce contraband output on a tick cycle
    - [ ] Guards assigned to a faction that controls the lab may ignore it; rival faction guards can raid it (generates `RaidEvent` already used by Phase H)

- [ ] **I.5 — Player Wanted Level**
    - [ ] Add `WantedComponent` to player: per-faction wanted score (0–5 stars equivalent) and a global notoriety value
    - [ ] Wanted score rises on: witnessed theft, fleeing a guard, carrying flagged contraband, trespassing in faction territory
    - [ ] Wanted score decays over time if player avoids triggering faction sensors; paying a bribe to a corrupt guard resets one faction's score to 0
    - [ ] HUD displays wanted level as a row of glyph indicators color-coded by faction; guards on that faction's patrol begin actively seeking player above threshold 3

- [ ] **I.6 — Guard Response System**
    - [ ] `GuardResponseSystem` listens for `CrimeReportEvent` and `WantedAlert` events; assigns nearby off-duty guards a PURSUE or INVESTIGATE goal
    - [ ] Chadse behavior: guard maintains line-of-sight pursuit; player can break chase by entering a building, hiding in a crowd, or reaching an out-of-faction chunk
    - [ ] Arrested player: if a guard closes to 0 range, player is detained — credits confiscated, contraband removed, teleported to faction holding cell building

---

#### Phase J — Population Lifecycle & Demographics

*Goal: agents are born, age, and die — the city's composition shifts over simulation time.*

- [ ] **J.1 — Age & Life Stage Component**
    - [ ] Add `AgeComponent`: current age in sim-days, life stage enum (CHILD / YOUNG_ADULT / ADULT / ELDER), and generation index
    - [ ] Sim-day to real-tick conversion configurable; default ~1 sim-year per ~10 real minutes at normal speed
    - [ ] Life stage transitions trigger goal and archetype changes: CHILD cannot work; ELDER has reduced movement speed and increased medical need

- [ ] **J.2 — Birth System**
    - [ ] Coupled ADULT agents with high affinity and stable housing have a probability each sim-year of producing a CHILD entity
    - [ ] Child spawned as a new agent with FAMILY-tier relationships to both parents and any existing siblings
    - [ ] Children placed in the parents' home tile; gain independent pathfinding when transitioning to YOUNG_ADULT
    - [ ] Birth rate modulated by chunk living conditions: high crowding/low food suppresses it; high commerce/low stress boosts it

- [ ] **J.3 — Death & Inheritance**
    - [ ] Natural death probability rises steeply past ELDER stage; also triggered by sustained critical needs, violence, or disease
    - [ ] On death: entity emits `AgentDeathEvent`, family notified (G.2 grief behavior fires), home tile becomes vacant
    - [ ] Owned property (home lot, business) transferred to highest-affinity FAMILY member; if none, reverts to faction or becomes derelict
    - [ ] Macro-Agent death simulated statistically per chunk; materialized correctly when the chunk loads

- [ ] **J.4 — Demographic Pressure**
    - [ ] `DemographicsSystem` (L3) tracks per-chunk age distribution, birth rate, and death rate
    - [ ] Overpopulated chunks (density > threshold) push excess agents toward adjacent lower-density chunks via a migration goal
    - [ ] Declining chunks (net death rate > birth rate) see building dereliction accelerate (Phase K) and faction influence weaken
    - [ ] God Mode "Demographics" overlay shows per-chunk population pyramid rendered as a small ASCII bar chart

- [ ] **J.5 — Generational Faction & Religion Drift**
    - [ ] Children inherit parents' faction affinity and religion with slight random drift (±10 affinity)
    - [ ] Over generations, dominant faction/religion in a chunk can shift without any direct intervention
    - [ ] `HistorySystem` log major demographic turning points (first generation to majority-shift a chunk) as simulation milestones

---

#### Phase K — Environmental Decay & Urban Renewal

*Goal: buildings age, deteriorate, and can be demolished and rebuilt, making zoning a living contest.*

- [ ] **K.1 — Building Health & Decay**
    - [ ] Add `BuildingHealthComponent`: integrity (0–100), last-maintenance tick, and decay rate
    - [ ] Decay rate driven by zone economic health (chunk Supply/Demand index) and weather exposure (acid rain in Industrial zones decays faster)
    - [ ] Visual decay tiers: 100–75 = normal glyphs; 74–50 = dim color; 49–25 = broken-window glyph variants; 24–0 = derelict (roof collapsed, no interior access)

- [ ] **K.2 — Maintenance & Squatting**
    - [ ] Buildings owned by solvent agents or factions receive periodic maintenance ticks that restore integrity
    - [ ] Derelict buildings (integrity < 20) become squat candidates: homeless agents and CHILD-stage agents with no home register the derelict tile as their home
    - [ ] Squats generate a `SquatEvent`; faction owner can issue an eviction (assigns a guard squad to clear the building)

- [ ] **K.3 — Demolition & Rebuilding**
    - [ ] Corporate and Civic faction AGIs may invest influence to demolish a derelict or low-value building on a prime lot
    - [ ] Demolition is a timed process (N ticks); renders the lot as rubble tiles during that period
    - [ ] Rebuilding assigns a new building type per current zoning rules; displaces any squatters to the nearest available housing in the same chunk
    - [ ] Newly built buildings spawn at integrity 100 with faction-appropriate glyph style

- [ ] **K.4 — Graffiti & Environmental Texture**
    - [ ] Low-integrity tiles have a chance each tick to gain a graffiti overlay glyph from a per-faction or per-religion tag palette
    - [ ] Graffiti is a mild faction influence signal: high-density tagging in a chunk nudges faction influence fields
    - [ ] Guard NPCs assigned a CLEAN_GRAFFITI behavior by corporate factions; removing tags is a visible patrol action

---

#### Phase L — Dynamic Crises & World Events

*Goal: periodic macro-scale perturbations that create distinct narrative chapters in each playthrough.*

- [ ] **L.1 — Crisis System Core**
    - [ ] Add `CrisisSystem` (L4) that runs on the longest tick interval; maintains a global crisis queue and cooldown timer
    - [ ] Each crisis type is a data record: name, trigger conditions, affected layers, duration in ticks, and resolution conditions
    - [ ] Active crises broadcast a persistent `CrisisActiveEvent` that downstream systems subscribe to

- [ ] **L.2 — Economic Crises**
    - [ ] **Stock Market Crash**: triggered when aggregate stock volatility exceeds threshold; sets all chunk demand indices to 20% of normal for N ticks; agents reduce spending, unemployment spikes, crime rises
    - [ ] **Supply Shortage**: a specific item category becomes globally scarce (e.g., all food supply drops 80%); agents prioritize survival needs over work goals; black market price for that item spikes

- [ ] **L.3 — Biological / Environmental Crises**
    - [ ] **Plague**: `BiologySystem` gains a contagion channel; infected agents spread disease within FOV range; symptoms degrade consciousness scores; high-density chunks are hotspots; factions respond differently (quarantine vs. denial)
    - [ ] **Flood**: heavy rain over multiple ticks causes river tiles to expand by 1–3 tiles; low-elevation chunks become impassable until weather clears; infrastructure influence of flooded roads zeroed out

- [ ] **L.4 — Political / Faction Crises**
    - [ ] **AGI Broadcast**: a faction leader AGI transmits a city-wide message; all agents within range of a broadcast tower receive an immediate ±20 affinity bump toward that faction; rival factions attempt counter-broadcasts the following tick
    - [ ] **Coup Attempt**: a faction's influence field crosses a dominance threshold in the Urban Core macro-zone; triggers a multi-tick military occupation sequence where guard squads from the ascending faction occupy key buildings
    - [ ] **Xeno Incursion**: Cacogen/Hierodule entities mass-spawn in a chunk and expand their influence aura; human factions temporarily form a coalition response

- [ ] **L.5 — Power Grid Failure**
    - [ ] Add a `PowerGridComponent` to Industrial zone generator buildings; grid health driven by maintenance (K.2) and economic activity
    - [ ] Grid failure darkens affected chunks: no artificial light tiles, commerce buildings close, faction surveillance systems offline
    - [ ] Criminal activity surges in dark chunks; repair crews (faction-assigned agents) work to restore grid over N ticks

- [ ] **L.6 — God Mode Crisis Dashboard**
    - [ ] Crisis overlay panel in God Mode lists all active crises with name, affected chunks, severity bar, and estimated resolution tick
    - [ ] Player can "seed" a crisis manually from a God Mode menu (sandbox/observer tool — no gameplay advantage)

---

#### Phase M — Sewer & Underground Network Layer

*Goal: a hidden traversal layer beneath the grid used by criminals, resistance factions, and alien entities.*

- [ ] **M.1 — Sewer Map Generation**
    - [ ] `SewerGenerationSystem` runs after lot placement (Phase A); carves a sewer network aligned to the street grid directly beneath arterials and primary capillaries
    - [ ] Sewer tunnels are 1–2 tiles wide; junctions occur at street intersections; major collector tunnels radiate from the Industrial zone toward the river outfall
    - [ ] Maintenance shafts: vertical connections between the overworld and sewer layer placed beneath manhole-cover tiles; rendered with a distinct glyph in the overworld

- [ ] **M.2 — Sewer as a Traversal Layer**
    - [ ] Player can enter the sewer via manhole tiles (interact with `E`); exits at any other manhole cover
    - [ ] Sewer layer rendered as a separate vertical level using existing z-order system; dimly lit (torch/bioluminescent glyph sources)
    - [ ] Pathfinding extended to sewer layer; agents with CRIMINAL or XENO archetypes use it by preference when aboveground guard density is high

- [ ] **M.3 — Sewer Inhabitants & Items**
    - [ ] Cacogen/Hierodule entities spawn preferentially in deep sewer junctions; their influence auras affect the bio layer even from underground
    - [ ] Resistance faction agents use sewers to move between chunks without crossing faction checkpoints
    - [ ] Abandoned caches of contraband and scavenged items spawn at dead-end sewer branches

- [ ] **M.4 — Environmental Hazards**
    - [ ] Flood crises (L.3) cause sewer water level to rise; traversal blocked in lowest-elevation segments
    - [ ] Toxic runoff tiles in Industrial zone sewers deal slow health damage without protective gear
    - [ ] Sewer ambush: CRIMINAL agents patrolling sewers can initiate a mugging (I.2) at higher success rate than overworld due to no witnesses

---

#### Phase N — News & Information Propagation

*Goal: make the political simulation legible and manipulable through a formal information layer.*

- [ ] **N.1 — Information Item Types**
    - [ ] Define `InformationRecord`: content tag (RUMOR / PROPAGANDA / INTELLIGENCE / PRICE_TIP), source faction, origin tick, veracity score (0–100), and propagation radius
    - [ ] Information records are attached to agents as inventory-like items that spread during conversations (F.1)

- [ ] **N.2 — Broadcast Towers**
    - [ ] Faction-controlled broadcast towers (already placed by faction influence system) emit propaganda `InformationRecord`s each L2 tick to all agents within range
    - [ ] Propaganda content shifts agent opinion scores by a small amount per exposure; repeated exposure has diminishing returns
    - [ ] Tower range and power determined by the faction's current influence score in that chunk

- [ ] **N.3 — Underground Media**
    - [ ] Pirate Radio building type (small, hidden in Slum zone back-lots) emits counter-propaganda for resistance factions
    - [ ] Pamphlet item: player or rebel agents can craft/carry pamphlets; dropping one in a tile deposits a low-radius propaganda source that slowly propagates to passersby
    - [ ] Broadcast towers emit `JamSignal` events that suppress pirate radio within range; destroying the jammer (item interaction) restores pirate signal

- [ ] **N.4 — Player Information Interaction**
    - [ ] Player can OBSERVE a broadcast tower (Phase E) to read current propaganda content in an inspection panel
    - [ ] Player can intercept a conversation (Phase F.3) and receive the `InformationRecord` being exchanged, adding it to a personal "intel log"
    - [ ] Player can plant false information by crafting a forged rumor item and passing it to an agent via TRADE interaction; the veracity score is low but the content spreads normally

- [ ] **N.5 — Information Decay & Verification**
    - [ ] InformationRecords age each tick; veracity decreases as the record propagates more than N hops from its source
    - [ ] Agents with high intelligence stats occasionally "verify" a rumor by cross-referencing with another agent's record; if discrepancy found, both records flagged as DISPUTED
    - [ ] DISPUTED information causes recipient agents to ignore the opinion-shift effect

---

#### Phase O — Supply Chains & Manufacturing

*Goal: close the economic loop — goods are produced, not just traded.*

- [ ] **O.1 — Raw Material Layer**
    - [ ] Define raw material types: Metal Ore, Chemicals, Bio-Fiber, Rare Earth (xeno-adjacent), Energy Cells
    - [ ] Raw materials spawn at world edges (port/dockland buildings, mining zones in non-urban macro-cells) and are transported inward by cargo vehicles (existing Phase 4.2 logistics)
    - [ ] Each raw material has a chunk-level stockpile counter; cargo vehicles replenish it on a schedule

- [ ] **O.2 — Factory Buildings & Production Cycles**
    - [ ] Industrial zone buildings can be tagged `FACTORY` with a recipe: N units of input material → M units of output item per production tick
    - [ ] Factory output goes directly into the chunk `SupplyComponent` for that item type, feeding the existing Supply/Demand system
    - [ ] Factory workers are ADULT agents assigned to the building; production rate scales with staffing level and worker satisfaction (needs component)

- [ ] **O.3 — Supply Chain Disruption**
    - [ ] If raw material stockpile in a chunk drops to zero, all factories consuming it pause production that tick and workers receive a forced idle goal
    - [ ] Disruption propagates: downstream chunks that relied on this factory's output also see supply drop next tick
    - [ ] Disruptions can be caused by: flood events blocking cargo routes, strike behavior (workers with low satisfaction refuse work), faction raids on competing supply lines

- [ ] **O.4 — Player & Agent Interaction with Manufacturing**
    - [ ] Player can sabotage a factory (requires contraband item + TRADE/USE at machine tile) to trigger a production halt for N ticks
    - [ ] Agents with entrepreneurial archetype can invest credits to start a small workshop in a vacant lot (scaled-down factory with lower throughput)
    - [ ] Supply chain status visible in God Mode economics overlay: per-chunk production rate, input stockpile bars, and bottleneck indicators

---

#### Phase P — Player Reputation System

*Goal: the world's stance toward the player shifts based purely on simulated observed behavior — no explicit leveling.*

- [ ] **P.1 — ReputationComponent (Player)**
    - [ ] Add per-faction reputation score (-100 to 100) as a map on the player entity; initialized to 0 (neutral) for all factions
    - [ ] Global notoriety value (0–100): a faction-agnostic measure of how widely the player's actions are known; rises faster in dense chunks

- [ ] **P.2 — Reputation Event Sources**
    - [ ] Witnessing agents generate `ObservationEvent` on notable player actions: entering restricted territory, trading with a faction rival, picking up a faction-flagged item, fleeing guards
    - [ ] Positive reputation events: completing a TRADE with a faction member at fair value, defending an agent from a mugging, delivering intelligence to a faction contact
    - [ ] Reputation changes are proportional to the witness's own faction affinity and their level of certainty (FOV clarity, range)

- [ ] **P.3 — Faction Response to Reputation**
    - [ ] High positive reputation (> 60) with a faction: guards ignore minor infractions, vendors offer discount pricing, faction AGI may attempt to initiate dialogue via a courier agent
    - [ ] High negative reputation (< -60) with a faction: guards assigned surveillance goal on player, vendors refuse trade, faction posts a bounty (adds a permanent `BountyHunter` agent archetype targeting player)
    - [ ] Conflicting reputations: high positive with one faction and high negative with its rival causes neutral third-party agents to treat the player with suspicion

- [ ] **P.4 — Reputation Decay & Recovery**
    - [ ] All reputation scores drift slowly toward 0 each sim-day if no new events reinforce them — past deeds are forgotten
    - [ ] Players can accelerate recovery with a specific faction by paying a tribute (credits transfer at their headquarters building) or completing an observed service act near their territory
    - [ ] Notoriety decays only when the player avoids dense chunks for an extended period (lying low)

- [ ] **P.5 — HUD & Inspection Integration**
    - [ ] HUD displays a compact reputation bar for the one or her factions with the most extreme current scores
    - [ ] Inspecting any agent (Phase E OBSERVE) shows that agent's known reputation estimate of the player (adds narrative flavor)
    - [ ] God Mode reputation overlay: per-chunk color heat map showing net player standing across all factions weighted by local faction dominance

---

#### Phase T — Advanced Trading & Barter System

*Goal: a full-featured, high-stakes bartering interface with visual inventory management and dynamic agent negotiation.*

- [ ] **T.1 — Split-Screen Barter UI**
    - [ ] TRADE interaction opens a dedicated notcurses modal covering 80% of the screen
    - [ ] **LHS (Shopkeeper)**: Vertical list of the agent's inventory with glyphs, names, and current market value in credits ($)
    - [ ] **RHS (Player)**: Vertical list of the player's inventory items and current credit balance
    - [ ] Navigation: `Up/Down` to browse items; `Tab` to switch sides; `Enter` to select/deselect items for the 'Offer Bundle'
- [ ] **T.2 — Dynamic Valuation & Bartering**
    - [ ] Real-time 'Trade Balance' indicator at the bottom: sum of player offer values minus sum of shopkeeper offer values
    - [ ] Shopkeeper evaluation: agents calculate 'Desire Score' for player items based on their `NeedsComponent` (e.g. food is worth 200% to a starving agent) and `FactionComponent`
    - [ ] **Counter-Offers**: If the player's offer is slightly below the agent's threshold, the agent may automatically select a small 'filler' item from the player's inventory or ask for more credits
- [ ] **T.3 — Negotiation Mechanics**
    - [ ] **Seller Requests**: Shopkeepers may lock certain items in their inventory behind specific requests ("I only trade this for high-tier tech loot")
    - [ ] Haggling Skill: Player's `notoriety` and `reputation` (Phase P) directly affect the starting price multiplier (Reputation 100 = 80% cost; Reputation -100 = 200% cost)
    - [ ] Rejection Feedback: Agents provide overhead speech (Phase F.2) explaining why a trade was rejected ("This is an insult," "I have no use for this junk")
- [ ] **T.4 — Trade Finalization & Memory**
    - [ ] Pressing `S` (Submit) validates the trade balance; items are swapped and credits transferred atomically
    - [ ] Agents remember bad trades: several consecutive low-ball offers from the player may cause the agent to temporarily block the TRADE interaction with a HUD note ("This person is a timewaster")
    - [ ] Bulk trading: `Ctrl+Enter` to quickly add all items of a specific category to the offer
