```
╔═══════════════════════════════════════════════════════════════════╗
║  N E O N   O U B L I E T T E                                     ║
║  USER MANUAL — PART 1 OF 3                                        ║
║  Introduction  ·  The City  ·  Simulation Layers                  ║
╚═══════════════════════════════════════════════════════════════════╝
```

---

## What Is Neon Oubliette?

Neon Oubliette is a terminal-based simulation of a living, breathing
mega-city. You are a single agent inside a system that keeps running
whether you act or not. Citizens argue, factions scheme, buildings
decay, economies collapse, and crimes ripple outward into political
consequences — all simultaneously, all the time.

You do not "win". You survive, observe, investigate, trade, and
influence. The city is not a backdrop; it is the game.

---

## Core Pillars

  HYPER-SIMULATION     Every entity has internal biological, cognitive,
                       economic, and political state that updates on
                       each tick independent of player input.

  MACRO-MICRO SCALING  A mugging in a back alley can affect a faction's
                       reputation score, alter public opinion, and
                       eventually shift zone ownership.

  ASCII AESTHETICS     The world is rendered in the terminal via
                       Notcurses. Every glyph on screen is a real
                       entity with simulated state.

  EXTENSIBILITY FIRST  Systems are modular and layered. New behaviours
                       emerge from the interaction of simple rules
                       rather than scripted events.

---

## The Simulation Layers

The simulation runs five stacked abstraction layers simultaneously.
Each layer feeds data upward into the next.

```
  ┌─────────────────────────────────────────────────────────────┐
  │  L4  POLITICAL    Factions · Laws · Territory · Directives  │
  │  L3  ECONOMIC     Markets · Credits · Debt · Supply Chains  │
  │  L2  COGNITIVE    Agents · Emotion · Memory · Social Nets   │
  │  L1  BIOLOGICAL   Organs · Metabolism · Hunger · Pathogens  │
  │  L0  PHYSICS      Temperature · Pressure · Materials        │
  └─────────────────────────────────────────────────────────────┘
```

A citizen's hunger (L1) drives them to seek food (L2 goal), which
costs credits (L3), which influences faction economic data (L4).

---

## The City: What You Will Find

### Terrain & Infrastructure

  · Streets, road tiles, and sidewalks form the ground layer.
  · Power grid nodes and conduit fields supply electricity to zones.
  · Infrastructure arterials carry supply chains between districts.
  · Transit routes connect stations; vehicles run on fixed schedules.

### Buildings

Buildings are multi-floor structures with procedurally generated
interiors. Each building has:

  · A zone classification  (Residential / Commercial / Industrial /
    Mixed / Abandoned)
  · Floors and rooms  accessed through entrances, stairs, elevators
  · Interior state   (intact / crumbling / abandoned / squatted)
  · Building health  that degrades over time via urban decay

Step onto an entrance tile and press E to enter. Once inside, `>` and
`<` move between floors. Press ESC to exit the interior view.

### Agents

Every person in the city is a fully-simulated agent:

  · Citizens  (`.`)  — civilians going about schedules
  · NPCs       (letter glyphs)  — named entities with goals, memories,
    faction affiliations, and criminal risk profiles
  · Guards     — faction-aligned law enforcement with patrol routes
  · Transit vehicles  — buses/trains running automated routes

Each agent has a schedule (home → work → leisure → home), needs
(food, water, rest), a social hierarchy position, an economic wallet,
and a reputation score across factions.

---

## The World Grid

The world is organised as a layered grid. The player occupies one
Z-layer at a time. Layers correspond to:

  · Ground streets (most common starting layer)
  · Building interiors (entered through doors)
  · Underground or elevated areas

The HUD always shows your current layer ID in the top bar.

---

## Factions

Factions control territory, issue directives to their members, and
react to events via reputation mechanics:

  · Each faction has a **public opinion score** that shifts based on
    crimes, economic outcomes, and crisis events.
  · Agents belong to factions and inherit danger levels and directives.
  · Faction leaders exist as named entities who can be spoken to,
    traded with, or observed.

---

## The Crisis System

The city is subject to ongoing macro-level crises:

  · Infection outbreaks   (biological layer)
  · Environmental hazards (pollution, heat islands)
  · Economic collapses    (market demand failures)
  · Power grid failures   (infrastructure cascade)

Crises propagate across zones. You can monitor active crises via the
Crisis Dashboard (see Part 2: Controls) and interact with them
indirectly through your choices.

---

## Religion

Religions are procedurally generated belief systems with influence
fields:

  · Each religion occupies worship place entities.
  · Religiosity scores affect agent decision-making and faction
    alignment.
  · Processions move through the city on scheduled paths.
  · Faction-religion stances determine alliances and conflicts.

---

## Build & Launch

  Prerequisites:  brew install cmake pkgconf notcurses cereal

  Build:   cd ~/Code/neon_oubliette/build && cmake --build . -j2

  Run:     cd ~/Code/neon_oubliette/build/bin && ./neon_oubliette

  Logs:    cat ~/Code/neon_oubliette/game.log

---

*Continued in MANUAL_part2.md — Controls & Interface*
