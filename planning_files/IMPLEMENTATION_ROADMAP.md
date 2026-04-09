## Tier 1 — The Living City (Physical Legibility)

These features make the simulation *visible* without requiring the inspection system.
A new player should notice all of these within 10 minutes of play.

### 1A. Traffic Wear & Desire Paths
Foot traffic physically scars the city over time.

**What the player sees**: Floors along busy routes darken and crack. High-traffic
tiles shift from clean concrete (light grey) through worn (dark grey) to broken
rubble (brown). Quiet alleys stay pristine. The visual difference is *dramatic*
— color shifts, not just glyph swaps.

**What the player does**: Use traffic patterns to identify safe routes (quiet =
fewer guards), find hidden shortcuts NPCs have carved, or deliberately re-route
traffic by blocking paths (placing items, locking doors) to change the city.

- [ ] **[Heatmap]** `TrafficHeatmapSystem`: Track per-tile movement counts for all agents (L3 tick). Store as a simple `uint16_t` grid per chunk.
- [ ] **[Wear]** `FloorWearSystem`: Map heatmap density to 4 visual tiers with distinct color ramps (not just glyph changes). Thresholds: 0–50 = clean, 51–200 = scuffed, 201–500 = worn, 500+ = broken.
- [ ] **[Breach]** `ProximityAttrition`: Wall tiles adjacent to broken-tier floor tiles take 1% integrity damage per L4 tick. When integrity hits 0, wall becomes `BROKEN_WALL` (passable, illegal entry for guards).
- [ ] **[HUD]** When a `BROKEN_WALL` appears, emit a chunk-level HUD notification: "A wall has collapsed on [Street Name]."
- [ ] **[Player Action]** Player can use a Crowbar tool on worn walls (integrity < 30%) to force a breach. Noise event alerts nearby guards.

### 1B. Cognitive Grey — Agent Breakdown You Can See
Agents falling apart should *look* like they're falling apart.

**What the player sees**: Agents with low consciousness (`< 20%`) flicker to grey
(`#777777`), stagger (movement becomes erratic — occasional random-direction steps),
and drop items. An agent in full breakdown is unmistakable even from across the screen.

**What the player does**: Approach a grey-flickering agent to inspect or help (give
food/medicine). Grey agents are easy marks for pickpocketing. They also trespass
randomly into buildings, triggering guard violence — the player can intervene,
watch, or exploit the chaos.

- [ ] **[Visuals]** `CognitiveGreyEffect`: Agents with `Consciousness < 20%` flicker glyph color to `#777777` every 2–3 ticks.
- [ ] **[Logic]** `ActionSlipSystem`: Low-consciousness agents have 5% chance per tick to: drop held item, move in random direction, or attempt entry into nearest building (triggering trespass).
- [ ] **[Player Verb]** Player can GIVE food/medicine to a slipping agent via the trade interface — restores consciousness, builds relationship, generates a "Mercy" `InformationRecord` tradeable to religious factions.

### 1C. Whisper Zones — Fear Has a Sound
Neighborhoods that have suffered violence go quiet. The silence is the signal.

**What the player sees**: In chunks where recent deaths or guard raids occurred,
agents' overhead speech bubbles shrink to dim flickering dots (`.` and `,`).
Agents move faster and avoid eye contact (no IDLE states — always walking).
A HUD indicator shows "TENSE ZONE" when entering such an area.

**What the player does**: Whisper zones are dangerous but valuable — agents here
are paranoid and will share restricted information if the player's faction rep
is right. The player must get *closer* than normal to overhear conversations
(3→1 tile range), increasing risk of confrontation.

- [ ] **[Atmosphere]** `ChunkMoodSystem`: Track recent death/raid events per chunk. Decay over L4 ticks. Output a `ChunkMood` enum: NORMAL / TENSE / FEARFUL.
- [ ] **[Visuals]** In TENSE/FEARFUL chunks, replace overhead speech bubbles with dim `.`/`,` glyphs. Remove agent IDLE animations.
- [ ] **[HUD]** Display "TENSE ZONE" / "FEARFUL ZONE" in the chunk indicator area of the HUD when player enters.
- [ ] **[Mechanic]** In FEARFUL chunks, SPEAK interaction range reduced from 3 to 1 tile. NPCs share higher-value `InformationRecords` but may flee mid-conversation.

---

## Tier 1D — Coherent Building Interiors (Spatial Legibility)

Buildings should feel like real spaces with purpose, not random BSP partitions
with a single furniture item dropped in the center. Entering a building should
immediately tell the player *what this place is* and *who uses it*.

### Room Connectivity — Hallways & Doorways
Currently BSP splits create rooms but connections between them feel arbitrary.
Rooms should flow logically through hallways, with clear paths the player can
read at a glance.

- [ ] **[Hallways]** After BSP leaf generation, run a corridor-carving pass: connect each room's center to its BSP sibling's center with a 1-wide hallway. Mark hallway tiles with a distinct floor glyph (`.` dim grey) so they read differently from room interiors.
- [ ] **[Door Placement]** Place doors (`+`) at the boundary between each hallway and room, not at arbitrary wall midpoints. Each room gets exactly one door unless it's a lobby (which gets 2+).
- [ ] **[Entrance Flow]** The entrance door always opens into a LOBBY or HALLWAY, never directly into a BEDROOM or SERVER_ROOM. BSP root split axis should align with the entrance wall to guarantee this.
- [ ] **[Connectivity Guarantee]** After hallway carving, BFS-validate that every room is reachable from the entrance. If not, add a rescue corridor to the nearest disconnected room.

### Room Furnishing — Density & Coherence
One item per room breaks immersion. Rooms should have 3–8 objects arranged in
zone-appropriate clusters that make the room's purpose obvious.

- [ ] **[Furniture Templates]** Define per-RoomType furniture templates (lists of {glyph, color, name, relative_position}). Example: KITCHEN gets a counter along one wall (`=`), stove (`%`), table (`T`), 2 chairs (`h`), sink (`~`). BEDROOM gets bed (`B`), nightstand (`n`), wardrobe (`W`), lamp (`i`).
- [ ] **[Placement Engine]** Replace single-center placement with wall-hugging and cluster logic: large items against walls, small items adjacent to large ones, walkable path from door preserved. Use a simple constraint: no furniture on the 1-tile path from door to room center.
- [ ] **[Density Scaling]** Furniture count scales with room area: `base_count + area / 6`. Minimum 2 items for any room ≥ 3×3. Maximum capped at `area / 3` to avoid impassable clutter.
- [ ] **[Loot Variety]** Each furniture piece has a loot table (not just kitchens/altars). Nightstands: personal items, credit chips. Desks: data drives, documents. Wardrobes: clothing, disguises. Crates: random zone-appropriate goods.

### Building Purpose — Thematic Coherence
Every building should have a clear *reason to exist* that the player can read
from the room layout and NPC activity inside.

- [ ] **[Building Archetypes]** Define 8–12 building archetypes per zone type (e.g., Residential → Apartment, Flophouse, Safehouse, Family Home; Corporate → Office Tower, Data Center, Executive Suite, Security HQ). Each archetype specifies a required room-type ratio (e.g., Apartment: 60% BEDROOM, 15% KITCHEN, 15% BATHROOM, 10% STORAGE).
- [ ] **[Archetype Assignment]** During zone interior generation, assign each building an archetype based on zone type + random weight. Store as `BuildingArchetypeComponent`.
- [ ] **[NPC Occupancy]** Assign 1–4 NPCs per building as "residents" or "workers" who pathfind inside during appropriate routines (SLEEPING in bedrooms at night, WORKING in offices during day). Their presence makes the building feel alive.
- [ ] **[Signage]** Building entrance tile gets a colored name label rendered above the door: "CHEN'S NOODLES" (commercial), "BLOCK 7-4A" (residential), "NEXACORP BRANCH 12" (corporate). Generated from faction + zone + RNG name tables.

---

## Tier 2 — Secrets & Discovery (Reward Curiosity)

These features reward players who inspect, explore, and pay attention.
They should feel like genuine discoveries, not tutorial popups.

### 2A. Sealed Vaults — Hidden Rooms with Real Consequences
Some rooms have no door. They exist in the simulation but are walled off.

**What the player sees**: Structural Inspection reveals "Structural Anomaly"
(purple highlight) on walls adjacent to void gaps. Sound leaks — if an NPC
pathfound into the void somehow, the player hears muffled speech through the
wall (dim overhead text visible through the wall tile). Items locked in vaults
create visible economic effects: local prices for that item category spike.

**What the player does**: Use a Crowbar or Explosive Charge on an anomaly wall
to breach in. This is loud (guard alert in 8-tile radius), possibly illegal,
but yields rare loot and can crash the local market for that item type.

- [ ] **[Generation]** BSP interior generator leaves 1–4% of building volume as `RoomTag::VOID_GAP`. Void rooms can spawn with high-tier loot.
- [ ] **[Inspection]** Structural Inspection highlights walls adjacent to VOID_GAPs as "Structural Anomaly" (purple wash).
- [ ] **[Audio]** `AcousticLeak`: NPC speech/movement in VOID_GAPs renders as dim overhead text through adjacent walls (1-tile range only, player must be stationary).
- [ ] **[Player Verb — Breach]** Player with Crowbar can force-open VOID_GAP walls (3-tick action, noise event, wall becomes BROKEN_WALL). Explosive Charge is instant but alerts all guards in chunk.
- [ ] **[Economy]** Items trapped in VOID_GAPs still count in scarcity calculations. Breaching a vault and selling its contents visibly shifts local market prices (trackable in Financial Forensics).

### 2B. Sedition & Street Sweeps — Crowds Are Dangerous
Guards react to visible social activity. Gathering is political.

**What the player sees**: When 4+ agents cluster and talk, a faint red overlay
pulses on the ground beneath them (visible in both Standard and God mode).
After a few L3 ticks, a guard squad spawns a "Disperse" task and moves to
break up the group — shoving agents apart, sometimes escalating to arrests.
The dispersed agents scatter, and the chunk mood shifts toward TENSE.

**What the player does**: The player can join the gathering (increasing its
sedition risk), eavesdrop for high-value rumors, deliberately draw guards
to the cluster as a distraction, or warn the group to scatter before guards
arrive. A player working for the Syndicate might *provoke* gatherings in
Corporate territory to destabilize it.

- [ ] **[Detection]** `ConversationalDensitySystem`: Flag tile clusters with 4+ talking agents. Render a faint red ground pulse on those tiles.
- [ ] **[Guard AI]** When sedition flag persists for 3+ L3 ticks in a guarded chunk, spawn "Disperse Crowd" guard task. Guards move to cluster, shove agents (forced movement), arrest agents with outstanding warrants.
- [ ] **[Consequence]** Dispersal events increment chunk death/raid counter (feeds into Whisper Zones — Tier 1C). Creates a feedback cycle: oppression → silence → underground activity → more oppression.
- [ ] **[Player Verb]** Player can WARN a cluster (new dialogue option when 4+ agents nearby): scatters the group before guards arrive, builds Syndicate rep, costs Corporate rep.

---

## Tier 3 — Faction Depth (Systemic Intrigue)

These features add strategic depth for players who engage with the faction layer.
They require Tier 1 systems to be legible.

### 3A. AGI Logic Wars — The Grid Fights Itself
When two AGI faction leaders' influence fields overlap, the environment destabilizes.

**What the player sees**: In overlap zones, wall and floor tiles randomly swap
glyphs and colors every few ticks — a visible, unsettling "glitch" effect.
Doors lock and unlock unpredictably. Agents in the zone become erratic
(conflicting directives from both AGIs). NPCs in conversation reference the
instability: "The grid is fighting itself again."

**What the player does**: Factions offer well-paid contracts to stabilize or
sabotage the contested data-nexus (a specific building in the overlap zone).
The player must navigate the glitching environment (which is mechanically
hazardous — random door locks can trap you) to reach the nexus and interact
with it. Stabilizing benefits one AGI's faction; sabotaging benefits the other.

- [ ] **[Visuals]** `LogicWarEffect`: In chunks where two AGI influence fields overlap, apply random glyph/color swaps to environment tiles every 3 ticks.
- [ ] **[Hazard]** Doors in Logic War zones randomly toggle locked/unlocked state each L2 tick. Locked doors display "ACCESS DENIED — GRID CONFLICT" on interaction.
- [ ] **[Quest Hook]** Factions in contested chunks issue "Stabilize Nexus" / "Sabotage Nexus" contracts visible in the NPC dialogue system. Completing one shifts the local influence field decisively.
- [ ] **[Player Verb]** INTERACT with a "Data Nexus" tile (special terminal in overlap-zone buildings) to choose: Stabilize (removes overlap, benefits Faction A) or Sabotage (collapses both fields, benefits Syndicate).

### 3B. Radicalization — Desperation Has a Face
When the city fails its citizens badly enough, they change.

**What the player sees**: An agent whose needs have been critically unmet (< 10%)
for a sustained period in a Syndicate-influenced area visibly transitions: their
glyph changes from `.` (citizen) to `!` (radical). Radicals stop following work
routines, start committing crimes (theft, vandalism), and actively recruit other
desperate agents through conversation.

**What the player does**: The player can accelerate or prevent radicalization.
Giving food to a desperate agent resets their radicalization timer. Sabotaging
a block's food supply (by destroying supply chain nodes) can deliberately push
a neighborhood toward revolt. A Syndicate-aligned player wants radicals;
a Corporate-aligned player wants to prevent them.

- [ ] **[Archetype]** `RadicalizationSystem`: Track consecutive ticks where `NeedsFulfillment < 10%` AND `SyndicateInfluence > 60%`. After threshold (20 L2 ticks), swap archetype to RADICAL. Change agent glyph to `!` (red).
- [ ] **[Behavior]** Radical agents: stop WORK routine, add STEAL and VANDALIZE goals, attempt to RECRUIT adjacent desperate agents (new dialogue option that lowers their radicalization threshold).
- [ ] **[Player Verb]** Player can GIVE food/supplies to pre-radical agents (resets their desperation timer). Alternatively, SABOTAGE supply nodes (destroy food-spawning items in shops) to accelerate neighborhood radicalization.
- [ ] **[Visible Consequence]** Chunks with 5+ radical agents trigger a "Civil Unrest" crisis event visible in the Crisis Dashboard. Guard patrols double. Faction influence becomes contested.

---

## [Theme: Player Tools]

### The Tactical Wirecutter
**What the player sees**: Power Junctions (yellow `*`) and Security Hubs (cyan `&`) on walls. Using a "Breaching Tool" causes the tile to flicker red `!` for 3 ticks before turning into a grey `x`. Building lights dim (background colors shift to dark grey).
**What the player does**: Equips "Breaching Tool" in the held-item slot, moves the cursor to a junction/hub tile, and presses `E` (Interaction).
**Why it's fun**: Manipulating the environment (darkness) allows for stealthy navigation and tactical entrapment of NPCs.
**Depends on**: Existing `BuildingInteriorComponent` and `InteractionSystem`.
**Tasks**:
- [ ] **[Tiles]** Add `PowerJunction` and `SecurityHub` tile types to `CityGenerationSystem` and `BuildingInteriorComponent`.
- [ ] **[Visuals]** Implement "Power Outage" visual effect in `RenderingSystem` that dims building tile backgrounds.
- [ ] **[Logic]** Update `AgentDecisionSystem` to apply `LowLight` state to NPCs (reduced FOV, increased alertness).
- [ ] **[Tool]** Define "Breaching Tool" item and map its use to the new tile types in `InteractionSystem`.

### The Long-Range Data Siphon
**What the player sees**: A dim, dashed "Range Tether" (`- - -`) connects player to cursor. When hovering over an agent/terminal within range, it turns bright green with a "DATA DOWNLOADING..." HUD bar. If range is exceeded, it turns red and snaps.
**What the player does**: Points cursor at target and holds `Space`. Must maintain proximity and LOS until the download bar fills.
**Why it's fun**: Turns hacking into a tense, physical tailing mini-game within the crowd.
**Depends on**: Existing Cursor Interaction and Range Enforcement.
**Tasks**:
- [ ] **[Visuals]** Add "Tether" line rendering to `RenderingSystem` using a dedicated plane.
- [ ] **[Mechanic]** `SiphonSystem`: Track distance/LOS between player and target while `Space` is held.
- [ ] **[Reward]** Transfer random `InformationRecord` or `Credits` to player inventory on successful completion.
- [ ] **[Detection]** Add "Siphon Detection" to `AgentDecisionSystem`; NPCs may notice the siphon and confront the player.

### Personal Vehicle "Eject" & "Impact"
**What the player sees**: A "Collision Path" (red dots `...`) ahead of driven vehicles. On "Eject," the vehicle glyph moves forward trailing sparks (`*`/`#`), while the player glyph `@` tumbles and stops.
**What the player does**: Press `X` while driving at high speed to "Bail Out," turning the vehicle into a projectile.
**Why it's fun**: Enables chaotic tactical distractions and "action movie" entry/exit maneuvers.
**Depends on**: Existing Drivable Vehicles.
**Tasks**:
- [ ] **[Physics]** `VehicleMomentumSystem`: Decaying velocity for driverless vehicles in `MovementSystem`.
- [ ] **[Visuals]** Add spark particle effects for driverless moving vehicles.
- [ ] **[Simulation]** `VehicleCollisionSystem`: Emit `NoiseEvent` and `DamageEvent` on impact with agents or walls.
- [ ] **[Action]** Map `X` key to `Bail Out` action; applies "Tumble" (stun/forced move) to the player.

---

## [Theme: Economic Leverage]

### Market Scarcity Tickers
**What the player sees**: Every shop tile (light yellow `$`) and market stall (cyan `=`) now features a floating, single-character "Market Pulse" glyph. A pulsing red `!` indicates high scarcity (astronomical prices), a steady white `$` indicates stable supply, and a bright green `v` indicates surplus (bargain prices). This is visible in the overworld without opening a menu.
**What the player does**: Approaches a high-scarcity shop (red `!`) and uses the `TRADE` interaction to sell specific items from their inventory at a 2x-5x credit markup, or uses the information to "hoard" items from stable areas to sell later.
**Why it's fun**: It turns the city grid into a living stock ticker. Players can visually "read" the economic desperation of a neighborhood and exploit it for profit or reputation.
**Depends on**: Existing `Dynamic Economics` and `BarterSystem`.
**Tasks**:
- [ ] **[Visuals]** Implement floating "Market Pulse" glyphs above shop/market tiles in `RenderingSystem`.
- [ ] **[Logic]** Map `MarketDemandComponent` scarcity multipliers to the 3-tier visual pulse (Red/White/Green).
- [ ] **[Barter]** Update `BarterSystem` to apply a "Local Scarcity Bonus" to the credit value of player items when selling at a high-demand node.

### Information Laundering
**What the player sees**: NPCs actively seeking intel now have a pulsing magenta `?` aura in the overworld. In the Split-Screen Barter UI, a new "Intel Ledger" panel appears. As the player drags `InformationRecord` items (rumors, dossiers) into the trade window, the NPC's "Greed Meter" (a yellow bar that usually resists trades) rapidly drains and is replaced by a magenta "Intel Value" bar.
**What the player does**: Selects `InformationRecord` items from their "Intel Log" (toggled by `N`) to trade for physical goods (Food, Tools, Weapons) when they don't have enough credits.
**Why it's fun**: It bridges the gap between "knowing things" and "having things." It makes the player feel like a true information broker who can survive on secrets alone.
**Depends on**: Existing `InformationRecord` and `BarterSystem`.
**Tasks**:
- [ ] **[UI]** Add "Intel Ledger" panel and magenta "Intel Value" bar to the `BarterSystem` UI.
- [ ] **[Logic]** Calculate "Intel Utility" in `BarterSystem` based on NPC faction affinity and the rumor's `veracity` and `hops`.
- [ ] **[Sim]** On trade success, trigger `InformationPropagandaEvent` that "consumes" the intel item and seeds it into the NPC's home chunk.
- [ ] **[Visuals]** Add a pulsing magenta `?` aura to NPCs in the overworld who have a high utility for the player's current intel.

### Supply Chain "Interception"
**What the player sees**: "Mule" agents (dark grey `m`) following a visible, dotted "Logistics Path" (only visible in `Financial Forensics` inspection mode). If the player stands in the path, it turns pulsing red. If the player shoves or attacks the Mule, the path breaks and the Mule drops a `CargoCrate` (large brown `#`).
**What the player does**: Uses the `Financial Forensics` mode to identify a high-value shipment path, then uses the `InteractionMode` to "Redirect" (requires high Threat/Rep) or "Raid" the Mule.
**Why it's fun**: It turns the "background" economy into a physical heist. The player can disrupt the supply of a whole neighborhood by stopping a single agent.
**Depends on**: Existing `MuleComponent` and `Logistics` logic.
**Tasks**:
- [ ] **[Visuals]** Render "Logistics Path" as a dotted line in the `InspectionSystem` (Financial tab).
- [ ] **[Action]** Implement "Redirect Shipment" dialogue option in `InteractionSystem` (SPEAK mode) gated by player reputation.
- [ ] **[Sim]** Breaking a path triggers a `STOCK_OUT` event for the destination building, instantly flipping its "Market Pulse" glyph to red `!`.

### The Industrial Sabotage Terminal
**What the player sees**: External "Production Terminals" (bright yellow `T`) on the outside walls of factory buildings. Interacting with one while having a "Breaching Tool" causes the building's interior lights (visible through windows) to flicker and turn dim grey. An overhead text "PRODUCTION HALTED" appears over the building.
**What the player does**: Approaches a `T` terminal, equips a "Breaching Tool," and presses `E`. The player must stay stationary for 5 ticks while a progress bar fills.
**Why it's fun**: It allows the player to *create* economic leverage. By sabotaging a factory, they drive up local prices for its goods, making their own stockpiles more valuable.
**Depends on**: Existing `FactoryComponent` and `ProductionSystem`.
**Tasks**:
- [ ] **[Tiles]** Add `ProductionTerminal` tile to `CityGenerationSystem` for Industrial zones.
- [ ] **[Action]** Create "Sabotage Production" action in `InteractionSystem` that sets `FactoryComponent.is_active = false` for a set duration.
- [ ] **[Visuals]** Implement building-wide light-dimming effect in `RenderingSystem` when a factory is inactive.

### Ticker-Tape Plaza
**What the player sees**: Vertical 3x10 "Stock Pillars" in central urban plazas (URBAN_CORE). These display scrolling ASCII ticker symbols (e.g., `^CRPT`, `vSYND`, `-TECH`) in bright green (`#00ff00`), neon red (`#ff0055`), or neutral white. The scrolling speed doubles during market volatility (L4 economic shifts).
**What the player does**: Approaches a "Trading Terminal" (yellow `T`) at the pillar's base. Pressing `E` (Interaction) opens a streamlined "Stock Broker" UI to buy/sell shares using credits.
**Why it's fun**: It makes the abstract economy physical. Watching your chosen faction's stock surge after you've completed a mission for them provides immediate, visible reward.
**Depends on**: Existing `StockMarketSystem` and `UrbanCore` zoning.
**Tasks**:
- [ ] **[Visuals]** Add `StockPillar` multi-tile object to `CityGenerationSystem` in `URBAN_CORE` plazas.
- [ ] **[Animation]** Implement ticker-tape scrolling logic in `RenderingSystem` with speed tied to `MarketVolatilityComponent`.
- [ ] **[UI]** Create `StockBrokerUI` panel with buy/sell buttons and real-time sparklines.

### Market-Moving Leaks
**What the player sees**: In the `N.4 Intel Log`, certain `InformationRecords` (Rumors) are tagged with a gold `$` (Insider Info). Talking to a "Broker" NPC (identifiable by a yellow `B` glyph) adds a "Leak Tip" dialogue option.
**What the player does**: Chooses a "Market-Moving" rumor to share. The Broker's overhead speech bubble turns magenta and they yell "BUY! BUY! BUY!" or "LIQUIDATE!" before sprinting to the nearest terminal.
**Why it's fun**: It's "Insider Trading" as a gameplay mechanic. The player can sabotage a factory, find a "Sabotage Report" item, leak it to a broker, and profit from the resulting stock crash.
**Depends on**: Existing `InformationRecord` and `StockMarketSystem`.
**Tasks**:
- [ ] **[Logic]** Add `is_insider_info` flag to `InformationRecord` and link it to specific faction tickers.
- [ ] **[Dialogue]** Implement "Leak Tip" dialogue branch that updates the Broker's `GoalComponent` to `USE_TERMINAL`.
- [ ] **[Sim]** Trigger a `MarketSentimentEvent` on tip leak that forces a 5% ticker shift in the next L3 tick.

### The Scarcity Riot
**What the player sees**: When a chunk's `MarketDemandComponent` for food/water stays >200% for 30 ticks, agents in that chunk replace their `.` glyph with a pulsing red `*` (Aggressive). They move toward the nearest Shop (`$`) or Warehouse, performing "Loot" animations (flickering red/white `!`).
**What the player does**: The player can join the riot to loot high-value items for free (heavy Rep penalty), or use the `GIVE` command to distribute food to rioters, which "calms" them (removes the `*` glyph) and grants massive Faction Rep.
**Why it's fun**: It turns economic failure into a physical, dangerous world event. The player's inventory becomes a tool for crowd control and social manipulation.
**Depends on**: Existing `Dynamic Economics` and `AgentDecisionSystem`.
**Tasks**:
- [ ] **[AI]** Add `RIOTER` archetype and logic to `AgentDecisionSystem` triggered by sustained scarcity.
- [ ] **[Visuals]** Implement pulsing `*` glyph and `Loot` animation for agents in riot state.
- [ ] **[Sim]** Rioters apply `IntegrityDamage` to building doors/walls and "consume" shop inventory items via `LootEvent`.

### Luxury Consumption Auras
**What the player sees**: High-wealth agents (Corporate archetype) in chunks with a surplus of luxury goods gain a "Luxe Aura" — their background tile color shifts from black to a deep purple (`#440044`) or gold (`#554400`). They emit a constant `~` particle effect (glitter).
**What the player does**: These agents carry unique "Luxury Items" (silk, spice, high-tier implants). The player can target these agents for pickpocketing or high-stakes trades that require unique "Elite Rumors."
**Why it's fun**: It provides a visual heatmap of where the "good loot" is. The player can see the economic health of a neighborhood just by looking at the glow of its citizens.
**Depends on**: Existing `IndividualWealth` and `ChunkSupply` systems.
**Tasks**:
- [ ] **[Visuals]** Add background-color override logic to `RenderingSystem` for agents based on wealth + local supply tiers.
- [ ] **[Loot]** Update `AgentSpawnSystem` to assign high-tier `LuxuryItem` components to agents with the "Luxe Aura."
- [ ] **[Feedback]** High-wealth agents gain unique "Arrogant" dialogue style when their aura is active.

## [Theme: Visible Consequences]

### The Squatter's Beacon
**What the player sees**: Buildings with 5+ squatters display a pulsing orange `*` at their door. Inside, rooms are cluttered with brown `~` (trash) and dim orange `^` (improvised beds). The squatter agents flicker between their base glyph and a dim orange `@`.
**What the player does**: Approaches the door and presses `E` (Interaction) to "Evict" (triggers forced movement for NPCs, causes high Faction Rep penalty) or "Support" (GIVE food/water via trade UI, gain Faction Rep with Syndicate).
**Why it's fun**: It transforms the abstract `num_squatters` into a physical territory that the player can manipulate. Deciding whether to clear a building for profit or aid the inhabitants creates a direct, visible social shift.
**Depends on**: Existing `BuildingHealthComponent` (num_squatters) and `InteractionSystem`.
**Tasks**:
- [ ] **[Visuals]** Implement pulsing orange `*` glyph for doorways with high squatter counts in `RenderingSystem`.
- [ ] **[Generation]** Update `BuildingInteriorComponent` to spawn `Trash` and `ImprovisedBed` tiles in rooms based on squatter count.
- [ ] **[Sim]** Map "Evict" and "Support" actions in `InteractionSystem` to update NPC `GoalComponent` and player `ReputationComponent`.

### Thermal Fugue Shivers
**What the player sees**: Agents in cold zones (< 5°C) gain a cyan `~` trailing particle effect and their glyph vibrates with a horizontal jitter. Their movement speed is halved. A "HYPOTHERMIC ZONE" indicator appears on the HUD when the player enters the cold field.
**What the player does**: Uses a "Thermal Patch" item on a shivering agent (restores temp and consciousness) or chooses "Strip Layer" to take their clothing/item, which drops the agent's consciousness to 0 instantly.
**Why it's fun**: It makes environmental temperature a tactile threat and opportunity. Seeing an agent shiver provides an immediate signal of vulnerability that the player can either alleviate or exploit for profit.
**Depends on**: Existing `BiologySystem` (temperature degradation) and `TemperatureField`.
**Tasks**:
- [ ] **[Animation]** Add horizontal glyph jitter and `~` particle trail in `RenderingSystem` for agents with low `BiologyComponent.body_temp`.
- [ ] **[Movement]** Apply speed multiplier based on temperature in `MovementSystem`.
- [ ] **[Action]** Define "Thermal Patch" and "Strip Layer" interactions in `InteractionSystem` and `BiologySystem`.

### Structural Scavenge Sites
**What the player sees**: Buildings with Integrity < 20% have walls that flicker between grey `#` and brown `x` (rubble). Standing next to these walls displays a gold `!` "SALVAGE" prompt on the HUD.
**What the player does**: Equips a "Wrecking Bar" in the held-item slot and holds `Space` while facing a flickering wall. After 5 ticks, the wall collapses into a "Scrap Pile" item (large brown `%`).
**Why it's fun**: It allows the player to actively participate in the city's decay. Dismantling a crumbling building for resources is a physical, rewarding loop that speeds up urban renewal or deterioration.
**Depends on**: Existing `BuildingHealthComponent` and `UrbanDecaySystem`.
**Tasks**:
- [ ] **[Visuals]** Implement wall glyph flickering in `RenderingSystem` tied to `BuildingHealthComponent.integrity`.
- [ ] **[Action]** Add "Salvage Wall" progress-bar action in `InteractionSystem` requiring "Wrecking Bar" tool.
- [ ] **[Sim]** Map successful salvage to wall removal, `integrity` reduction, and `ScrapPile` item spawning.

### Heat-Haze Phantoms
**What the player sees**: High-heat zones (> 45°C) cause translucent, flickering magenta `&` "Phantoms" to spawn. These mimic NPC movement paths but have no collision. The player's HUD border gains a rippling distortion effect.
**What the player does**: Uses a "Cooling Mist" tool to dispel a phantom (yielding a "Fugue Data" InformationRecord) or follows a phantom's path to a hidden loot cache before it vanishes.
**Why it's fun**: It turns the dangerous high-temperature environment into a source of systemic mystery. Phantoms act as guides to hidden value, rewarding players for exploring hazardous zones while managing their own temperature.
**Depends on**: Existing `BiologySystem` (heat degradation) and `InformationRecord`.
**Tasks**:
- [ ] **[Visuals]** Implement translucent magenta `&` phantom rendering and HUD ripple distortion in `RenderingSystem`.
- [ ] **[AI]** Create `PhantomMovementSystem` that generates paths toward hidden `LootCache` entities.
- [ ] **[Action]** Add "Cooling Mist" tool functionality and "Phantom Dispel" logic to `InteractionSystem`.

## [Theme: Risk And Reward]

### Overdrive Extraction
**What the player sees**: A `ResourceNode` (cyan `*`) pulses rapidly as the player extracts. A "NODE STABILITY" bar (Green -> Yellow -> Red) appears on the HUD. On "Overdrive," the node glyph flickers red `!` and emits white `@` (sparks). If stability hits 0, the node disappears in a 3x3 explosion of brown `x` (rubble).
**What the player does**: Holds `E` to extract at normal speed. Holds `Shift + E` to "Overdrive," extracting materials 3x faster while the Stability bar drains. Releasing `Shift` allows stability to slowly recover.
**Why it's fun**: It turns a passive "wait for bar to fill" mechanic into a high-stakes gamble. The player must decide exactly how much "extraction heat" they can risk before permanently losing the node or alerting nearby authorities.
**Depends on**: Existing `ResourceNodeComponent` and `ExtractionProgressComponent`.
**Tasks**:
- [ ] **[Logic]** Implement `ExtractionStabilitySystem` to track node heat and decay.
- [ ] **[Input]** Update `InputSystem` to detect `Shift + E` modifier for extraction.
- [ ] **[Visuals]** Add stability bar and spark particle effects to `RenderingSystem`.
- [ ] **[Simulation]** Trigger `NoiseEvent` and entity destruction when stability reaches 0.

### Sensor-Blind Heists
**What the player sees**: High-value "Secure Crates" (bright green `[`) in Corporate zones. These crates project a rotating, dim red FOV cone (using `\`, `|`, `/` glyphs on the floor) representing their internal security sensor. The player's white FOV and the crate's red FOV overlap visibly.
**What the player does**: The player must time their movement to stay in the sensor's "blind spot" (the area behind the rotating cone) to reach the crate. If the player's `@` glyph enters a red sensor tile, a HUD alarm "SENSOR TRIPPED" appears and the crate turns grey `X` (locked).
**Why it's fun**: It makes "stealth" a physical, spatial puzzle using the existing FOV system. The reward is high-tier raw materials; the risk is a lockdown and an immediate Guard response.
**Depends on**: Existing `FOVSystem`.
**Tasks**:
- [ ] **[System]** Implement `SecuritySensorSystem` to manage rotating FOV cones for specific entities.
- [ ] **[Logic]** Add collision check between Player position and Active Sensor FOV tiles.
- [ ] **[Visuals]** Render red sensor cones using the secondary FOV layer in `RenderingSystem`.
- [ ] **[Sim]** Trigger `SecurityAlertEvent` (locks crate, spawns Guard "Investigate" task).

### The Scarcity Gold Rush
**What the player sees**: A "Market Distress" HUD alert: "CRITICAL [Material] SHORTAGE IN CHUNK [X,Y]". On the world map, that chunk's border pulses red. Inside the chunk, NPCs swap their base glyph for a pulsing red `!` (Desperate) and move aggressively toward any resource nodes or the player.
**What the player does**: Travel to the distressed chunk to extract the scarce material. Because of the shortage, the player must physically defend their extraction site from "Desperate" NPCs who will attempt to steal the materials as they are pulled from the ground.
**Why it's fun**: It creates a "Gold Rush" scenario. The reward is a 5x credit multiplier for selling that material in that chunk, but the risk is being swarmed by the very simulation you're trying to exploit.
**Depends on**: Existing `Dynamic Economics` and `RawMaterialFieldComponent`.
**Tasks**:
- [ ] **[Event]** Create `DistressEventSystem` to trigger scarcity spikes based on `EconomicSystem` data.
- [ ] **[AI]** Implement "Desperate" archetype in `AgentDecisionSystem` that prioritizes stealing items from the player.
- [ ] **[Visuals]** Add pulsing red chunk borders to the map rendering.
- [ ] **[Economy]** Update `BarterSystem` to apply a 500% price surge for "Distressed" materials in the affected chunk.

## [Theme: Player Tools]

### The Silent Breach
**What the player sees**: An intact window tile (cyan `+`) is replaced by a dark grey `.` (a small hole). The HUD displays "Breach Successful: Silent." If the player enters, the building background turns from black to a dim blue, indicating the interior is now "breached" but the alarm hasn't triggered.
**What the player does**: The player selects the "Glass Cutter" tool and clicks a window tile within 1 tile range.
**Why it's fun**: It rewards stealthy players by allowing them to bypass the noise-based "shatter" event of a broken window, enabling entry into hostile buildings without alerting guards.
**Depends on**: Existing `Window Tiles` and `BuildingHealthComponent`.
**Tasks**:
- [ ] **[Tool]** Add `GlassCutter` item to the registry with a "Quiet" property.
- [ ] **[Interaction]** Create a `SILENT_BREACH` action in `InteractionSystem` that drops window integrity below 50% without emitting a `NoiseEvent`.
- [ ] **[Visuals]** Implement the `.` (hole) glyph transition for windows with `integrity < 50%` that weren't shattered.

### Acoustic Remote-Scouting
**What the player sees**: When the player holds the "Sonic Sensor" tool, a faint grey ring (chebyshev distance indicator) expands from the cursor. Any NPC movement within that ring, even behind walls, causes a bright yellow `!` to flicker on their tile for 1 tick. HUD text reads: "SENSING: [Entity Type] - [Action]".
**What the player does**: Points the cursor at a distant tile (up to 8 tiles away) and holds `Right Click`.
**Why it's fun**: It turns cursor range into a tactical "X-ray" tool for sound. It allows players to plan breaches by "listening" to the room through the windows or walls before committing to an entry.
**Depends on**: Existing `Cursor Interaction` and `NoiseEvent` system.
**Tasks**:
- [ ] **[Visuals]** Implement "Acoustic Ring" rendering around the cursor position in `RenderingSystem`.
- [ ] **[Logic]** Create `RemoteListeningSystem` that filters `NoiseEvents` within the cursor's effective radius and creates temporary visual markers.
- [ ] **[Input]** Bind the `Sonic Sensor` item use to the cursor-holding state.

### The Glass-Shatter Lure
**What the player sees**: The player throws a "Heavy Object" (brown `*`) at a window. On impact, the window tile (`+`) explodes into a 3x3 spray of grey `,` particles. A pulsing red "Noise Wave" expands 10 tiles from the impact. Nearby guards (yellow `G`) swap their glyph to a pulsing `?` and immediately pathfind to the window.
**What the player does**: Selects "Debris" or "Rock" in inventory and clicks a window tile from a distance (utilizing the cursor interaction).
**Why it's fun**: It turns the "Broken window" mechanic into a primary tool for manipulation. The player can deliberately break a window to draw guards away from a secure entrance, creating a window of opportunity elsewhere.
**Depends on**: Existing `Window Tiles` and `Guard AI`.
**Tasks**:
- [ ] **[Action]** Implement "Throw Item" interaction that calculates a path to the cursor target.
- [ ] **[Logic]** Update `GuardDecisionSystem` to generate a high-priority "Investigate Shatter" goal when a `NoiseEvent` is tagged with `Source::Window`.
- [ ] **[Animation]** Create a `ShatterParticle` effect and "Noise Wave" overlay in `RenderingSystem`.

### Window-Sill Peek
**What the player sees**: When the player is adjacent to a window and uses the `OBSERVE` mode, the camera centers on the building's interior. The exterior map dims to near-black, while the interior rooms become brightly lit. NPC names and "Current Task" (e.g., "SLEEPING", "CLEANING GUN") are displayed in the HUD side-panel.
**What the player does**: Stands next to a window tile and presses `Space` while in `OBSERVE` interaction mode.
**Why it's fun**: It makes windows a vital "reconnaissance" layer. It gives the player a safe way to identify high-value targets or threats before they ever step inside a building.
**Depends on**: Existing `FOV System` and `Interaction Modes`.
**Tasks**:
- [ ] **[Action]** Add `WINDOW_PEEK` logic to the `OBSERVE` interaction when the target is a window.
- [ ] **[Visuals]** Implement "Interior Focus" rendering that overrides tile brightness based on proximity to the peeked window.
- [ ] **[Logic]** Grant temporary `Line of Sight` through the building interior during the peek action.
