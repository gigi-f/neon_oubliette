# Expansion Roadmap: Player-Facing Depth

Design rule: **every system must have a visible symptom and a player verb.**
If the player can't see it or act on it, it doesn't ship.

---

## Priority Tiers

Features are ordered by impact-to-effort ratio. Each tier is a self-contained
deliverable that can be playtested independently before moving to the next.

---

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
