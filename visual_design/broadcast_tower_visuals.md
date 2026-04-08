# Broadcast Tower Visual Metaphor Specification

## 1. Primary Entity Representation
The Broadcast Tower is a high-visibility urban landmark. It exists on Layer 0 (Physics) as a physical structure but emits effects across the Cognitive (L2) and Political (L4) layers.

| State | Glyph(s) | Default Color | Visual Metaphor |
| :--- | :--- | :--- | :--- |
| **Idle / Unpowered** | `▲` (top) + `║` (base) | `#AAAAAA` (Neutral Gray) | A silent, monolithic structure. |
| **Active (Broadcasting)**| `▲` (pulsing) | Faction Color | A rhythmic pulse of signal energy. |
| **Sabotaged / Broken** | `⫴` or `X` | `#708090` (SlateGray) + flickering Red | Signal leakage and structural failure. |
| **Hacked (Player)** | `▲` (oscillating) | `#FFCC33` (Gold) / Faction | A visible struggle for bandwidth. |

## 2. Signal Pulse Animation (The "Data Ripple")
When a tower broadcasts, it emits a "Data Ripple" every 10 simulation ticks. This is an ASCII particle effect that expands from the tower center.

- **Expansion Ticks**: 5 ticks (expands 1 tile per tick).
- **Glyph Sequence**: `.` → `,` → `~` → `*` → (fades).
- **Color**: Faction-specific (see below), at 30-50% brightness.

## 3. Faction-Specific Broadcast Metaphors
Each faction modifies the signal's visual "vibe" to reflect their ideological propagation style.

| Faction | Signal Glyph | Signal Color (Hex) | Vibe / Metaphor |
| :--- | :--- | :--- | :--- |
| **Aura-9 (Consensus)** | `-^-` | `#55AAFF` | **Medical/Order**: Rhythmic, clinical pulse. |
| **Malware-Alpha** | `#%&*` | `#FF5555` | **Glitch/Entropic**: Erratic, aggressive data bursts. |
| **The Architect (Maw)** | `~` | `#55FF55` | **Biological**: Organic, tendril-like expansion. |
| **The Signal (Void)** | `█▓▒░` | `#AA55FF` | **Geometric**: A heavy, absolute void-field. |
| **The Arbiter (Syndicate)**| `!` | `#FFCC33` | **Industrial**: Sharp, staccato alerts. |

## 4. God Mode Overlays (Signal Density)
In God Mode, the "Signal Density" is visualized as a background color field.
- **High Strength**: Solid faction color background (dimmed).
- **Low Strength**: Scattered faction-colored dots (`.`) in empty cells.
- **Interference**: Where two tower signals overlap, the background flickers between the two faction colors or shows a "Static" pattern (`%`).

## 5. Inspection Insights (ASCII Portraits)
When inspecting a tower, the "Signal Scan" insight is shown:
- **Clean Signal**: `[║║║║║║║║║║]` (Solid bars)
- **Degraded**: `[║║║║░░░░░░]` (Faded blocks)
- **Encrypted**: `[?#*&!$@%^]` (Garbled noise)

---
**Vision Artist: Design Lead**
*(2024)*
