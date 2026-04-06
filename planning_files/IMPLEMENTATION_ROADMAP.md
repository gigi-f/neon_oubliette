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

---

#### Phase A — City Layout Overhaul (Grid-Based Urban Form)

*Goal: replace the current scatter-placement with a Chicago-style rectilinear grid where every building occupies a surveyed lot.*

- [ ] **A.1 — Lot & Parcel System**
    - [ ] Define a `LotComponent` (world-space AABB, zone class, ownership entity)
    - [ ] Introduce a `CityPlannerSystem` that runs once at world-gen and subdivides each macro-zone into rectangular city blocks separated by streets
    - [ ] Block sizes vary by zone: Urban Core (large, 40–80 tiles), Residential (medium, 20–40), Industrial/Port (irregular but aligned)
    - [ ] Each block is further divided into individual lots; lot width/depth driven by zone density tables
    - [ ] Store the full lot grid in a spatial index for fast lookup (used by building placement and pathfinding)

- [ ] **A.2 — Urban Core & Skyscrapers**
    - [ ] Tag the central macro-zone cluster as `URBAN_CORE`; apply a density multiplier to lot occupancy
    - [ ] Skyscraper buildings: footprints 6–20 tiles wide, rendered with vertical ASCII metaphors (height encoded in glyph brightness / color tier)
    - [ ] Ground-floor commercial mandatory on Urban Core lots facing arterial roads
    - [ ] High pedestrian and vehicle traffic volume spawned proportionally to lot density
    - [ ] Multiple train terminals placed at key arterial intersections; terminals are designated `COMMERCE_HUB` anchor points

- [ ] **A.3 — Residential Streets**
    - [ ] Row-house / apartment packing: buildings fill their lots edge-to-edge with only alley gaps
    - [ ] Shared-wall detection: adjacent buildings on the same block share a wall tile rather than having a gap
    - [ ] Setback rules: arterials require a 1-tile sidewalk; capillary streets require none
    - [ ] Residential density gradient: lots nearest Urban Core are taller apartments; outer lots are 1–2 story row-houses

- [ ] **A.4 — Commerce Hubs Around Train Stations**
    - [ ] `COMMERCE_HUB` anchor triggers a radial upzoning: lots within N tiles are zoned `MIXED_COMMERCIAL`
    - [ ] Markets, vendors, and kiosks auto-spawned in the hub radius
    - [ ] Elevated foot-traffic simulation: agents path through hubs when commuting

- [ ] **A.5 — Street Hierarchy Integration**
    - [ ] Ensure `InfrastructureNetworkSystem` arterials align to grid axes; no diagonal arterials inside city limits
    - [ ] Sidewalk tiles auto-generated along all street edges; agents prefer sidewalks over road tiles when not in vehicles
    - [ ] Alleys carved between back-to-back building rows for service access

---

#### Phase B — Building Entry Rules (Standard vs. God Mode)

- [ ] **B.1 — Enforce Door-Only Entry in Standard Mode**
    - [ ] Remove all code paths that allow walking directly into a building footprint tile
    - [ ] Movement system: collide with all non-door building tiles; only process entry event on `DOOR` tiles
    - [ ] Interior generation triggered exclusively by door-entry event (already partially working — harden the contract)

- [ ] **B.2 — God Mode Direct Inspection / Entry**
    - [ ] In God Mode, cursor hover over any building footprint tile opens an inline info panel (floor plan outline, occupants, room list)
    - [ ] `G` key (or configurable bind) while cursor is over a building teleports the God-Mode viewport into that building's interior
    - [ ] Interior view rendered as an overlay with a clear visual border distinguishing "you are inside X building" from the overworld
    - [ ] Navigating back out via ESC or clicking outside the building footprint returns the viewport to the overworld

- [ ] **B.3 — HUD Contextual Indicators**
    - [ ] HUD always shows current entry context: `[OVERWORLD]`, `[INTERIOR: Reza Tower L3]`, `[GOD: Reza Tower]`
    - [ ] Minimap thumbnail updates to show interior floor plan when inside a building

---

#### Phase C — HUD Held-Item Display

- [ ] **C.1 — Held-Item Slot in HUD**
    - [ ] Reserve a fixed HUD region (lower-left, adjacent to health bar) for the held item
    - [ ] Render item glyph (ASCII) + short name (truncated to ~12 chars) in the slot
    - [ ] If no item held, display `[empty]` in dim color
- [ ] **C.2 — Item Status Indicators**
    - [ ] Show charge count or durability bar beneath the glyph for tools/weapons
    - [ ] Flash the slot briefly when the item is used or swapped
    - [ ] Color-code by item category (consumable = green, weapon = red, tool = cyan, key = yellow)

---

#### Phase D — Cursor Interaction System

- [ ] **D.1 — Cursor Rendering**
    - [ ] Render a highlight glyph over the tile currently under the mouse cursor using a dedicated notcurses plane
    - [ ] Highlight color changes based on what is under the cursor: agent (yellow), item (cyan), door (green), empty (dim white)
    - [ ] Keyboard cursor: arrow keys move the cursor tile-by-tile when mouse is not available; cursor snaps back to player on movement

- [ ] **D.2 — Standard Mode Range Enforcement**
    - [ ] Each interaction type carries a max range (tiles): Speak = 3, Observe = 6, Trade = 1
    - [ ] Cursor highlight turns red when the target tile exceeds the active interaction's range
    - [ ] Attempting to interact outside range emits a HUD notification ("Too far to trade")
    - [ ] Range is measured as Chebyshev distance to match 8-directional movement

- [ ] **D.3 — God Mode Click Interaction**
    - [ ] Left-click on any visible tile opens the inspection panel for that entity (same data as `I` key inspect, no range limit)
    - [ ] Right-click opens a context menu: Inspect / Follow / Teleport Cursor / Tag
    - [ ] Middle-click (or `F` key) locks the camera onto the clicked agent and tracks them until dismissed

---

#### Phase E — Typed Interaction System

- [ ] **E.1 — Interaction Mode Enum & State**
    - [ ] Add `InteractionMode { SPEAK, OBSERVE, TRADE }` to the player state component
    - [ ] Cycle through modes with `Tab` (or a dedicated bind); default is `OBSERVE`
    - [ ] HUD shows the active interaction mode icon and label at all times (e.g., `[MODE: SPEAK]`)

- [ ] **E.2 — Range Ring Rendering**
    - [ ] When an interaction mode is active, render a border of colored tiles around the player showing max range
    - [ ] SPEAK ring = blue outline; OBSERVE ring = white outline; TRADE ring = gold outline
    - [ ] Ring redraws each tick in case the player moves; rendered on its own notcurses plane below entities

- [ ] **E.3 — Interaction Dispatch**
    - [ ] Pressing `E` while the cursor is on a valid target fires the active interaction type
    - [ ] SPEAK: opens dialogue system (see Phase F)
    - [ ] OBSERVE: opens inspection panel for target (same as inspect key, no additional privacy masking)
    - [ ] TRADE: opens barter panel between player and target agent if within range and target is willing

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

- [ ] **F.4 — Player Dialogue System**
    - [ ] SPEAK interaction on an agent opens a full-screen modal dialogue panel
    - [ ] Panel shows: speaker portrait (ASCII block), agent name/title, current utterance text, and response options
    - [ ] Response options labeled `[a]`, `[b]`, `[c]`… generated contextually from the topic and relationship tier
    - [ ] Dialogue outcomes feed back into the simulation: selecting "buy info" transfers credits, selecting "threaten" lowers relationship score, etc.
    - [ ] Agents remember recent dialogue with the player (stored in `RelationshipComponent`); repeat visits unlock new topic branches

- [ ] **F.5 — Conversation Simulation at Macro Scale**
    - [ ] Off-screen agents (MacroAgentRecord) still "converse" statistically — relationship scores drift based on proximity and faction alignment
    - [ ] Major conversation outcomes (deals struck, rumors spread) logged as simulation events consumable by downstream systems

---

#### Phase G — Social Graph & Relationship System

- [ ] **G.1 — RelationshipComponent**
    - [ ] Add `RelationshipComponent` to all human agents: map of `{ entity → RelationshipRecord }` where record holds tier (FAMILY / FRIEND / COWORKER / ACQUAINTANCE / STRANGER), affinity score (-100 to 100), last-interaction tick, and shared-home flag
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