```
╔═══════════════════════════════════════════════════════════════════╗
║  N E O N   O U B L I E T T E                                     ║
║  USER MANUAL — PART 3 OF 3                                        ║
║  Gameplay Systems  ·  Crime  ·  Economy  ·  Agent AI             ║
╚═══════════════════════════════════════════════════════════════════╝
```

---

## Conversation System

When you approach an NPC in SPEAK mode and press `E`, a dialogue
window opens with labelled choices (`a`, `b`, `c`…). Conversations
are generated using a grammar engine that draws from the NPC's:

  · Cognitive state  (mood, stress, opinion of you)
  · Faction alignment and social standing
  · Knowledge of recent events  (rumours, crimes, crises)
  · Speech profile  (tone, vocabulary)

During a conversation:

  · Neither you nor the NPC will move — movement is suppressed for
    both parties for the duration of the open dialogue window.
  · The NPC will respond dynamically based on your chosen options.
  · If the NPC declines a topic, their cognitive state updated.
  · The exchange is logged in your Dialogue Log (`L`).

### Information as Currency

NPCs carry InformationRecords — tagged facts about the city, crimes,
people, and events. You can offer these in barter (`x` in the barter
screen) to unlock better trade terms, secrets, or faction access.

---

## Barter & Trade System

Trade operates on a leverage-and-greed model rather than a fixed
price list.

  NPC greed margin  — each NPC has a baseline greed factor affecting
                      what they consider a fair exchange.

  Leverage          — built up through conversation, faction rep, and
                      information you offer. Higher leverage = better
                      terms for you.

  PRESSURE (`p`)    — applies social pressure to push the NPC toward
                      accepting your terms. Repeated pressure damages
                      the relationship.

  REQUEST (`s`)     — submits your current offer. The NPC evaluates it
                      against their greed margin and current leverage.

Items and InformationRecords can both be offered. The NPC can counter-
offer with their own items or information.

---

## Inspection System — What Each Mode Shows

Inspection is available in **both Standard Mode and God Mode**.

  · In Standard Mode, inspection is range-limited to 6 tiles from
    the player. Use `f` (lowercase) for Financial Forensics and
    `n` for your personal Intel Log. The targeting cursor determines
    which entity is inspected.

  · In God Mode, there is no range limit — the god cursor can be
    anywhere on the map. Use `F` (uppercase) for Financial Forensics.
    The Intel Log (`n`) is not available in God Mode.

See Part 2 for the full keybinding tables for each mode.

### Surface Scan (`i` / cyan)
Basic entity data: display name, glyph, position coordinates, zone,
any tags attached to the entity.

### Biological Audit (`I` / green)
Full biological state: organ health breakdown, metabolism rate, hunger
and thirst levels, vital signs, any active pathogens or infections,
body temperature.

### Cognitive Profile (`c` / magenta)
Mental state: current goals, memory records, emotional values, social
hierarchy position, relationship map, known faction affiliations.

### Financial Forensics (`f` / yellow)
Economic layer: credit balance, debt records, transaction provenance
history, stock holdings, market category memberships.

### Structural Analysis (`t` / red)
For buildings and terrain: structural health, zone type, floor count,
room breakdown, building interior state, heat island data, decay level.

### Intel Log (`n` — self only / grey)
Your personal chronological event log. Records actions you have taken,
crimes you have committed or witnessed, notable conversations, and
faction reputation changes.

---

## The Crime System

The city has a fully simulated crime economy. NPCs with high crime
risk scores will autonomously:

  · Steal from other agents          (STEAL_FROM_AGENT)
  · Mug agents for credits           (MUG_AGENT)
  · Carry goods as mules             (MULE_GOODS)
  · Sell contraband to fences        (SELL_CONTRABAND  /  SEEK_FENCE)

Guard NPCs respond to crime reports and wanted alerts:

  · Guards patrol designated zones and react to CrimeReportEvents.
  · A guard who witnesses or receives an alert will pursue the suspect
    (PURSUE task) and attempt arrest (ARREST task).
  · Arrested agents are processed by the faction legal system.

### Wanted Level

Your personal NOTORIETY score climbs when you:

  · Commit crimes observed by guards or citizens
  · Are reported by witnesses
  · Are caught carrying contraband items

Notoriety is shown in the HUD. High notoriety causes guards to pursue
and attempt to arrest you on sight.

---

## Agent AI & Schedules

Every agent runs a daily schedule:

```
  HOME → WORK → LEISURE → HOME → (repeat)
```

Between scheduled activities, agents:

  · Seek food or water when needs are low (SEEK_FOOD / SEEK_WATER)
  · Wander in a local radius (WANDER)
  · Idle at their current position (IDLE)
  · Patrol assigned zones (PATROL — guards and faction members)
  · Participate in religious processions (FOLLOW_LEADER)
  · Execute crime tasks if crime risk is high enough

Agents with speech components engage in autonomous conversations with
nearby agents, trading rumours and generating information records.

---

## Items

Items are physical entities with inventory components. They have:

  · A name and glyph
  · A value and market category
  · A material type
  · Optional contraband status
  · Optional stolen status (provenance tracked)

Finding items: dropped by NPCs on death, present in building rooms,
or obtained through trade.

Using items: open inventory (`b`), navigate to the item, press ENTER.
Some items trigger inspection events (scanners); others restore stats.

Dropping items: press `d` while the item is selected in inventory.
Dropped items appear at your position and remain in the world.

---

## Urban Decay

Buildings and zones degrade over time through the Urban Decay system:

  · BuildingHealthComponent tracks structural integrity.
  · Abandoned zones accumulate graffiti and waste.
  · Decayed buildings can be squatted by homeless agents (SQUAT task).
  · The Rebuilding System can restore structures when economic
    conditions allow.
  · Demolition events clear sites for new construction.

Decay is visible in tile appearance and reported under Structural
Analysis inspection.

---

## Infrastructure & Power Grid

Power grid nodes supply electricity to zones via conduit fields:

  · Buildings in powered zones function normally.
  · Power failures (from crisis events or infrastructure damage)
    propagate across the conduit field and affect entire districts.
  · The Power Grid system tracks supply/demand balance across the
    arterial graph.

Infrastructure arterials carry supply chains. Disrupting an arterial
node (via crisis cascade) can starve downstream zones of goods,
driving up market prices.

---

## Transit System

Transit vehicles run fixed routes between stations. You can:

  · Board a transit vehicle by moving onto its tile while it is
    stopped at a station.
  · Ride the vehicle to any stop on its route.
  · Disembark by pressing the vertical layer key (`>` / `<`) or
    moving to the exit tile.

Your position is updated each tick while riding. Other passengers
are simulated agents with their own destinations.

---

## Milestones

The Milestone system tracks significant events in your playthrough:

  · MilestoneRecords are created when threshold conditions are met
    (e.g. first crime committed, first faction rep milestone, first
    crisis survived).
  · Milestones are stored in your Intel Log and viewable via the
    HISTORY inspection mode (`n`).

---

## Saving & Serialisation

The game uses Cereal for binary serialisation. The simulation state
(entities, components, world grid) can be saved and restored between
sessions. Save files are written to the binary output directory.

---

## Tips

  · Start by observing. Use OBSERVE mode and inspection keys to
    understand who is around you before acting.

  · Faction reputation is slow to build and fast to lose. Crimes
    near faction members will tank your standing quickly.

  · Information is money. Pick up InformationRecords through
    conversations and trade them to fences or allies for credits.

  · The crisis dashboard in God Mode (`V`) gives the clearest
    picture of city-wide threats. Switch to God Mode briefly to
    orient yourself after a crisis event notification.

  · Buildings with high decay scores are good places to find
    abandoned items and low-traffic routes — but also where
    squatters and criminals are most active.

  · Guards do not pursue indefinitely — they return to patrol
    after losing line of contact. Breaking line of sight in a
    building interior often clears a pursuit.

---

*End of User Manual.*
*Part 1: Introduction & World  |  Part 2: Controls  |  Part 3: Systems*
