# Implementation Roadmap
## Project Neon Oubliette — Living City Focus

Last updated: April 2026

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
- [x] Movement system (shared for player + NPCs, collision-aware)
- [x] Inspection system: modal panel, privacy masks, 5 inspection modes, ESC to close
- [x] Inventory panel: toggled with B, hidden by default, no overlap
- [x] HUD: health, credits, layer indicator, notifications, full controls legend
- [x] Building interior generation on door entry
- [x] Agent logic written: AgentDecisionSystem, AgentActionSystem, NeedsComponent, GoalComponent, CurrentPathComponent — all correct but no agent entities are ever spawned

### Partially Done
- [ ] City generation: 60×30 hardcoded map, 3 hand-placed buildings, 1 street band — no procedural layout, no zoning
- [ ] BioSim (Layer 1): `Layer1BiologyComponent` exists (`consciousness_level`, `pain_level`) but no simulation tick logic
- [ ] Physics (Layer 0): temperature dissipation only; structural integrity, pressure unused
- [ ] Inspect panel: BioAudit shows `consciousness_level` and `pain_level` only; other tabs mostly empty

### Not Started
- [ ] Agent spawning (NPC entities with AgentComponent never created)
- [ ] Zoning system (zones, adjacency rules, zone-aware building placement)
- [ ] Procedural city layout (random but logical street grid + building placement)
- [ ] Large-scale world (200×200+) with chunk streaming or district transitions
- [ ] FOV / line of sight
- [ ] World bounds enforcement (player can walk off the map edge)
- [ ] Day/night cycle affecting agent behavior
- [ ] Economy, faction, barter (systems registered but all stub)
- [ ] Inspection modes fully implemented (i/I/c/f/t — see below)

### Known Issue: Inspection Modes All Look the Same
Keys `i`, `I`, `c`, `f`, `t` open the same modal with a different tab highlighted, but
appear identical in practice because:
1. Spawned entities (Mailman, Stray Dog) have no Layer0–4 simulation components, so
   every tab's content area is blank
2. Each tab has at most 1–2 fields rendered even when data exists (e.g. Bio = only
   `consciousness_level` + `pain_level`; Economy = only `cash_on_hand`)
3. Inspection targets must be on the exact same tile as the player — no range or cursor
4. The ASCII portrait is identical regardless of mode or target entity type
5. `t` is labelled "Structural Analysis" but actually reads `Layer4PoliticalComponent`
   (faction data) — label mismatch

All five modes need: richer component data, mode-appropriate layout and colour coding,
a range/cursor targeting system, and entity-type-aware portraits. Tracked in Phase 1.5 below.

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

## Phase 1: Immediate Playability

Goal: make the city feel alive with minimal code — activate the existing agent systems.

### 1.1 Spawn Agent Entities
The entire agent pipeline (decision → pathfinding → action → movement) is already written.
Nothing runs because no entities have `AgentComponent`. Fix: add a spawn pass in `main.cpp`
or a dedicated `AgentSpawnSystem` that places N agents on walkable tiles at startup.

- Spawn 20–50 agents on sidewalk/street tiles at game start
- Each agent gets: `AgentComponent`, `NPCComponent`, `NeedsComponent` (hunger/thirst),
  `AgentTaskComponent` (IDLE default), `RenderableComponent` (glyph `@` or `o`, colored by type),
  `PositionComponent`, `NameComponent`
- Verify the existing decision→action→move loop runs and agents visibly wander

### 1.2 Fix World Bounds
- Clamp movement to `[0, world_width)` × `[0, world_height)` in `MovementSystem`
- Pass world dimensions via a `WorldConfigComponent` on a singleton entity, or as constructor args

### 1.3 Basic Agent Variety
- Two archetypes: `Civilian` (wanders, seeks food) and `Guard` (patrols a fixed area)
- Guards get a patrol waypoint list; AgentActionSystem handles `PATROL` task type
- Color-code glyphs by archetype

### 1.4 Consumable Items in World
- Spawn a handful of food/water items on the map so agents can actually satisfy needs
- `ItemComponent` + `ConsumableComponent` already exist; just needs entities created

### 1.5 Inspection Mode Overhaul
The inspection panel is the player's primary tool for reading the simulation. Each mode
must show distinct, useful, and visually differentiated information.

**Prerequisite:** agents (Phase 1.1) must exist with all five layer components attached
so there is something to inspect.

**Targeting system:**
- Replace exact-tile collision with a small range (e.g. 1-tile radius or a directional cursor)
- Show a targeting reticule glyph when an inspect key is held / toggled
- Allow inspecting buildings and terrain tiles, not just NPCs

**`i` — Physical / Surface Scan** (`Layer0PhysicsComponent`)
- Structural integrity bar, temperature, material type
- For terrain: tile type, traversability, surface hazard flags
- Portrait: schematic/blueprint-style box drawing of the entity outline

**`I` — Biological Audit** (`Layer1BiologyComponent`)
- Expand beyond consciousness + pain: add hunger %, thirst %, estimated age, injury list
- Vital signs: heart rate estimate, blood chemistry flags (drugged, poisoned, starving)
- Portrait: anatomical silhouette, organs highlighted by health state (green/amber/red)

**`c` — Cognitive Profile** (`Layer2CognitiveComponent`)
- Current task/goal, dominant emotion, faction alignment, stress level
- Social connections count (friends, rivals)
- Portrait: mind-map style glyph cluster showing emotional state

**`f` — Financial Forensics** (`Layer3EconomicComponent`)
- Cash on hand, debt level, recent transaction count, wealth tier label
- For buildings: estimated property value, owner faction, last sale
- Portrait: balance-sheet ASCII table layout

**`t` — Political / Structural Analysis** (`Layer4PoliticalComponent`)
- Fix label mismatch: rename key or rename mode so "Structural Analysis" isn't showing
  faction data — either rename `t` to "Faction Scan" or reroute it to `Layer0` structural
  data and move faction to a new key
- Shows: faction allegiance, territory control %, wanted status, influence score
- Portrait: faction crest/symbol rendered in ASCII

**Shared improvements:**
- Each tab uses a distinct accent colour so switching tabs is visually obvious
- `[DATA ENCRYPTED / MASKED]` message should show *which* privacy tier is blocking
- Add a "no data" message when the entity simply doesn't have the component
  (currently the content area is silently blank)

---

## Phase 2: Procedural City with Logical Zoning

Goal: replace the hardcoded 60×30 test map with a procedurally generated city that follows
real urban logic. A corporate district should never be directly adjacent to slums.

### 2.1 Zone System
Define zone types and adjacency rules:

| Zone | Colour Code | Allowed Neighbours |
|------|-------------|-------------------|
| CORPORATE | `#0055FF` | COMMERCIAL, TRANSIT |
| COMMERCIAL | `#FFAA00` | CORPORATE, RESIDENTIAL, TRANSIT |
| RESIDENTIAL | `#00AA44` | COMMERCIAL, PARK, SLUM (buffer only) |
| SLUM | `#884400` | RESIDENTIAL (edge only), INDUSTRIAL |
| INDUSTRIAL | `#AA2200` | SLUM, TRANSIT, WAREHOUSE |
| PARK | `#006622` | RESIDENTIAL, COMMERCIAL |
| TRANSIT | `#666666` | Any (roads, rail corridors) |

Implementation: generate a coarse zone grid (e.g. 8×8 macro-cells for a 200×200 world),
apply adjacency constraint propagation (similar to WFC), then fill each macro-cell with
zone-appropriate buildings and street layouts.

### 2.2 Procedural Street Grid
- Primary streets every ~20 tiles (arterials)
- Secondary streets every ~8 tiles within zones
- Sidewalks flanking all streets
- Street width varies by zone: corporate = 4 tiles, slum = 2 tiles

### 2.3 Zone-Aware Building Placement
- Each zone has a building archetype pool (corporate → skyscrapers; slum → shacks; industrial → warehouses)
- Buildings placed on lots between streets, respecting setback rules
- Building density: corporate = sparse + tall floors, slum = dense + 1–2 floors
- Use `world_generator_params.json` (already in `data/configs/`) to drive parameters

### 2.4 Points of Interest
- One guaranteed hospital per RESIDENTIAL zone
- Market stalls in COMMERCIAL zones (agents can barter)
- Power substations in INDUSTRIAL (future: blackout events)
- Each POI tagged with a `PointOfInterestComponent` for agent goal-seeking

---

## Phase 3: Massive Scale — World Streaming

Goal: support a world of 200×200+ tiles (hundreds of buildings, thousands of agents)
without loading everything into memory at once.

### 3.1 Chunk Architecture

Divide the world into fixed-size chunks (e.g. 40×40 tiles). At any moment only a ring of
chunks around the player are active (typically 3×3 = 9 chunks).

```
[ ][ ][ ][ ][ ]
[ ][X][X][X][ ]    X = loaded chunks
[ ][X][P][X][ ]    P = player chunk
[ ][X][X][X][ ]
[ ][ ][ ][ ][ ]
```

- `ChunkComponent` on a chunk entity stores its grid coordinates and load state
- `ChunkStreamingSystem`: on player movement, determine newly entered chunk region,
  load adjacent chunks (generate or deserialize), unload far chunks (serialize and destroy entities)
- Terrain tiles are only ECS entities in loaded chunks; unloaded chunks store raw tile arrays

### 3.2 Agent LOD (Level of Detail)
Agents in unloaded chunks are not simulated as ECS entities. Instead they are tracked as
lightweight statistical records (`MacroAgentRecord`: position, faction, needs estimate).
When a chunk loads, records are upgraded to full ECS entities. When unloaded, entities
collapse back to records. This allows "thousands of agents" without thousands of ECS ticks.

- `MicroPresenceComponent` already exists for this (`is_active`, `micro_entity`) — extend it
- `PopulationSystem` (stub) is the right home for macro↔micro promotion/demotion logic

### 3.3 District Transitions (pre-streaming fallback)
As a simpler interim before full chunking, implement explicit district boundary transitions:

- When the player reaches the edge of the current 60×30 map, trigger a `DistrictTransitionEvent`
- Save the current district state; generate or load the adjacent district
- Fade/clear the screen, load new terrain + new agent set
- Player position wraps to the opposite edge

This can ship before chunk streaming and be replaced later.

### 3.4 World Map Scale Targets
| Milestone | World Size | Buildings | Agent Entities |
|-----------|------------|-----------|---------------|
| Phase 1 done | 60×30 | ~5 | 20–50 |
| Phase 2 done | 120×80 | ~30 | 100–200 |
| Phase 3 (transitions) | 400×400 logical | unlimited | 200 active |
| Phase 3 (chunks) | Unlimited | Unlimited | 500+ active |

---

## Phase 4: Agent Depth

Goal: agents feel like inhabitants, not random walkers.

### 4.1 Daily Schedule System
- Time-of-day cycle (configurable ticks per in-game hour)
- Agents have a schedule: `WORK` during day hours, `HOME` at night, `LEISURE` midday
- Schedule drives goal selection in `AgentDecisionSystem`

### 4.2 Agent Memory
- `AgentMemoryComponent`: remembers last known positions of food sources, danger zones, allies
- Decision system consults memory before doing a full world search
- Reduces pathfinding calls significantly at scale

### 4.3 Faction Affiliation
- Each agent has a `FactionComponent` (CORPORATE / GANG / CITIZEN / GUARD)
- Agents react differently to player inspection based on faction (suspicion, hostility, indifference)
- `FactionSystem` (stub) handles inter-faction tension changes

### 4.4 Agent Conversations / Interaction
- `E` on an NPC opens a minimal dialogue (name, faction, current need, one random rumor)
- Rumor system seeds procedural lore into the world

---

## Phase 5: Simulation Layer Depth

Goal: the underlying simulation layers (biology, economy, politics) start driving visible
world events rather than being inert stubs.

### 5.1 Biology Ticks (Layer 1)
- Hunger/thirst deplete health when unmet (already wired via `NeedsComponent`)
- Injury propagation: agents that collide with obstacles or hostile agents receive wounds
- `InspectionSystem` BioAudit tab shows live vitals

### 5.2 Economy Ticks (Layer 3)
- Market entities tick supply/demand based on agent consumption
- Credits transfer on item purchase; price fluctuates with scarcity
- Player HUD credits are spent at market stalls

### 5.3 Environmental Events (Layer 0)
- Random events: power outage (darkens a district), gas leak (damage zone), storm (movement penalty)
- Events broadcast via HUDNotificationEvent and affect tile properties

### 5.4 Weather System (Layer 0)
Weather is a persistent, world-wide Layer 0 condition that affects all agents and tiles
simultaneously — distinct from one-off environmental events.

**Weather states:**
| State | Glyph Overlay | Agent Effect | Tile Effect |
|-------|--------------|--------------|-------------|
| Clear | none | none | none |
| Overcast | dim colour tint (reduce RGB brightness ~20%) | morale -5% | none |
| Rain | `:` scattered on open tiles, scrolling downward | movement -1 speed, thirst refills slowly | wet flag on outdoor tiles |
| Heavy Rain | `|` dense, colour overlay `#2233AA` tint | movement -2, visibility reduced | flooding possible in low zones |
| Acid Rain | `!` in `#AAFF00`, outdoor damage zone | `pain_level` +1/tick outdoors, clothing degrades | corrodes metal tiles over time |
| Smog | `░` overlay in `#886644` | lung health degrades over long exposure | visibility radius halved |
| Electrical Storm | `*` flash effect, periodic | agents seek shelter (new goal: SEEK_SHELTER) | power fluctuations in INDUSTRIAL/CORPORATE zones |

**Implementation:**
- `WeatherComponent` on a singleton world-state entity: `current_weather`, `intensity (0.0–1.0)`,
  `duration_ticks_remaining`, `next_weather`
- `WeatherSystem` (Layer 0) ticks duration down, rolls next weather on expiry using a
  Markov chain transition matrix (e.g. Clear→Overcast more likely than Clear→Acid Rain)
- Weather transitions broadcast a `WeatherChangeEvent` consumed by `RenderingSystem`
  (overlay plane) and `AgentDecisionSystem` (behaviour modifier)
- Weather overlay rendered on a dedicated `ncplane` above the world plane, below the HUD
- Zone influences probability: INDUSTRIAL zones have higher smog chance; coastal/open zones
  more rain and storms
- `InspectionSystem` Physical tab shows current local weather conditions on terrain tiles
- Player HUD shows a weather glyph + label in the top-right corner of the HUD plane

---

## Technology Notes

- EnTT v3.13+ recommended for improved group performance at 10k+ entities
- notcurses: keep HUD plane pinned to top of z-stack every frame (already done)
- World gen parameters: drive from `data/configs/world_generator_params.json`
- A* pathfinding hard-limit currently `x/y < 100` — increase to chunk bounds before Phase 3

---

## Out of Scope (for now)
- Win/lose conditions
- Tutorial / onboarding
- Save/load (SerializationSystem is stubbed — revisit after Phase 3)
- Multiplayer
