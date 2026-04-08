```
╔═══════════════════════════════════════════════════════════════════╗
║  N E O N   O U B L I E T T E                                     ║
║  USER MANUAL — PART 2 OF 3                                        ║
║  Controls  ·  Modes  ·  Interface                                 ║
╚═══════════════════════════════════════════════════════════════════╝
```

---

## Two Game Modes

The game operates in two distinct top-level modes. Your current mode
is shown in the HUD top bar.

```
  STANDARD MODE    You control a player character in the city.
                   The simulation runs in real time (or turn-by-turn
                   when you move).

  GOD MODE         A cursor decouples from the player. You can pan
                   across the entire city, follow agents, inspect
                   entities anywhere, and control simulation speed.
```

Toggle between modes:  `G`

---

## Universal Controls  (both modes)

  Key          Action
  ─────────────────────────────────────────────────────────────────
  `q` / `Q`    Quit the game
  `p` / `P`    Toggle pause / unpause
  `g` / `G`    Toggle God Mode on / off
  `l` / `L`    Toggle Dialogue Log panel
  `?`          Toggle controls help overlay
  `ESC`        Close open inspection window / dialogue window

---

## Standard Mode — Movement

WASD move the player one tile per keypress. Each move advances the
turn by one step, which ticks the world simulation.

  Key          Action
  ─────────────────────────────────────────────────────────────────
  `w` / `W`    Move north
  `s` / `S`    Move south
  `a` / `A`    Move west
  `d` / `D`    Move east
  `>`          Descend one Z-layer (or next floor inside a building)
  `<`          Ascend one Z-layer (or previous floor inside a building)

  Note: If you are riding a personal vehicle, WASD controls the
  vehicle instead of your body.

---

## Standard Mode — Cursor (Look/Target)

Arrow keys move a detached targeting cursor without moving the player.
This is used for inspecting or interacting with things at a distance.
Moving with WASD snaps the cursor back to the player.

  Key          Action
  ─────────────────────────────────────────────────────────────────
  `↑ ↓ ← →`   Move targeting cursor (decouples from player)
  `w/a/s/d`    Move player  (snaps cursor back to player position)

---

## Standard Mode — Interaction

  Key                   Action
  ─────────────────────────────────────────────────────────────────
  `e` / `E`             Interact with tile/entity at cursor position
  `SPACE` or `ENTER`    Also triggers interaction
  `TAB`                 Cycle interaction mode (OBSERVE → SPEAK → TRADE)

### Interaction Modes

Your current interaction mode determines what `E` / `SPACE` does when
you target an adjacent entity, and how far you can reach:

```
  OBSERVE    Range: 6 tiles   Look at entities, trigger inspection
  SPEAK      Range: 3 tiles   Initiate dialogue with an NPC
  TRADE      Range: 1 tile    Open barter screen with an NPC
```

The active interaction mode is shown in the HUD.

---

## Standard Mode — Inspection

Move the cursor over any entity, then press an inspection key.
All inspection modes open a tabbed panel displaying entity data.
Range limit: 6 tiles from the player.

  Key     Inspection Mode           Colour Accent
  ─────────────────────────────────────────────────────────────────
  `i`     Surface Scan              Cyan   — name, glyph, position
  `I`     Biological Audit          Green  — organs, metabolism, vitals
  `c`     Cognitive Profile         Magenta — emotion, goals, memory
  `f`     Financial Forensics       Yellow — credits, debts, transactions
  `t`     Structural Analysis       Red    — building health, zone data
  `n`/`N` Intel Log (self only)     Grey   — your personal history log

Press `ESC` to close the inspection panel.

---

## Standard Mode — Inventory

  Key                 Action
  ─────────────────────────────────────────────────────────────────
  `b` / `B`           Open / close inventory
  `↑ / ↓`            Navigate item list
  `ENTER`             Use / equip selected item (sets as held item)
  `d` / `D`           Drop selected item at current position
  `ESC`               Close inventory

Your currently held item is displayed in the HUD. A flash effect
confirms when you swap items.

---

## Standard Mode — Dialogue

When an NPC opens a conversation window:

  Key         Action
  ─────────────────────────────────────────────────────────────────
  `a` – `z`   Select a dialogue choice (choices are labelled a, b, c…)
  `ESC`       Close the dialogue window

While a dialogue is open, player and NPC movement is suppressed —
neither party will walk away mid-conversation.

---

## Standard Mode — Barter

Initiated by targeting an NPC in TRADE mode and pressing `E`.

  Key           Action
  ─────────────────────────────────────────────────────────────────
  `TAB`         Switch focus between your inventory and NPC inventory
  `↑ / ↓`      Navigate the focused inventory list
  `ENTER`       Toggle selected item in/out of your trade offer
  `s` / `S`     Submit trade offer (REQUEST)
  `p` / `P`     Apply pressure to improve terms (PRESSURE)
  `x` / `X`     Toggle an information record into your offer
  `ESC`         Close barter screen

Your offer and the NPC's counter-offer are displayed in separate
panels. The NPC's greed margin and current leverage are shown live.

---

## Standard Mode — Context Menu

Some interactions open a radial context menu.

  Key            Action
  ─────────────────────────────────────────────────────────────────
  `↑ / ↓`       Navigate menu options
  `ENTER`        Confirm selected option
  `ESC`          Close menu without acting

---

## God Mode Controls

In God Mode the camera cursor moves freely across the map, detached
from the player entirely.

### God Mode Movement

  Key              Action
  ─────────────────────────────────────────────────────────────────
  `w/a/s/d`        Move god cursor (also `↑↓←→`)
  `>`              Next map layer (outside) or next floor (inside)
  `<`              Previous map layer / floor
  `+` or `=`       Increase simulation speed
  `-` or `_`       Decrease simulation speed
  `SPACE`          Toggle pause

### God Mode Inspection

  Key     Inspection Mode
  ─────────────────────────────────────────────────────────
  `i`     Surface Scan at cursor
  `I`     Biological Audit at cursor
  `c`     Cognitive Profile at cursor
  `F`     Financial Forensics at cursor
  `t`     Structural Analysis at cursor

### God Mode Special

  Key     Action
  ─────────────────────────────────────────────────────────
  `f`     Follow agent under cursor (camera tracks it)
  `v`/`V` Toggle Crisis Dashboard overlay
  `ESC`   Exit interior focus  /  close inspection window

---

## The HUD

The top bar always displays:

```
  MODE: STANDARD  [RUNNING]
  LAYER: 2  TPS: 60.0
  CITIZENS: 312 | AGENTS: 48
```

Below that:

  · HP %  Credits  Layer  — player stats
  · Faction reputation bars  — your standing in major factions
  · Current interaction mode  — OBSERVE / SPEAK / TRADE
  · NOTORIETY level  — how wanted you are
  · Economy status label

Temporary notifications appear briefly in the HUD (e.g. "Too far to
speak.", "Holding stim-pack", "Mode: TRADE").

---

## The Crisis Dashboard  (God Mode: `V`)

A full-screen overlay showing:

  · Named active crises with severity bars and turn countdown
  · Per-category metrics (infection, pollution, economic, power)

---

## The Dialogue Log  (`L`)

A panel listing recent spoken exchanges — both player dialogue and
overheard NPC conversations in range. Toggled with `L`.

---

*Continued in MANUAL_part3.md — Gameplay Systems*
