# Physics Layer 0: Visual Metaphor Specification

## 1. Temperature Gradients (The "Thermal Trace")
The terminal shifts character color based on `temperature_celsius` to indicate thermal hazards.

| Temp Range | Human Descriptive | Hex Palette | Character Overlay / Modifier |
| :--- | :--- | :--- | :--- |
| **< -40°C** | Cryo-Critical | `#00FFFF` (Cyan) | `*` (Frost) or `❄` (if UTF8) |
| **-40 to 0°C**| Sub-Zero | `#B0E0E6` (Powder) | `.` (Ice crystals) |
| **0 to 15°C** | Cold | `#87CEEB` (Sky Blue) | None |
| **15 to 35°C** | Ambient (Safe) | Original | None |
| **35 to 60°C** | Fever | `#FFD700` (Gold) | `,` (Sweat/Distortion) |
| **60 to 100°C**| Scalding | `#FF8C00` (DarkOrange)| `~` (Heat haze) |
| **> 100°C** | Ignition | `#FF4500` (OrangeRed) | `!` (Combustion warning) |
| **> 800°C** | Plasma/Molten | `#FFFFFF` (White) | Flicker effect |

## 2. Weather Visual Effects
Weather occurs as an overlay layer (Layer 999 or via alpha blending).

### RAIN / HEAVY_RAIN
- **Glyph**: `'` or `|` (falling)
- **Color**: `#0055FF` (Blue)
- **Metaphor**: A curtain of vertical lines, shifting downward every tick.

### ACID_RAIN
- **Glyph**: `,` or `;`
- **Color**: `#7FFF00` (Chartreuse)
- **Metaphor**: Corrosive neon-yellow drizzle.

### SMOG
- **Glyph**: `.` (sparse)
- **Color**: `#708090` (SlateGray)
- **Metaphor**: A graying of the background, reducing contrast of primary glyphs.

### ELECTRICAL_STORM
- **Glyph**: `Z` or `/` (random positions)
- **Color**: `#FFFFFF` (White)
- **Metaphor**: Sudden high-contrast flashes of binary-like discharge.

## 3. River Cooling Fields
Large water bodies (Material: WATER) generate a localized atmospheric effect.

- **Range**: 3-tile Manhattan radius.
- **Visual**: A "Cooling Mist" effect.
- **Glyph**: `~` (faded) or `,`.
- **Color**: `#1E90FF` (DodgerBlue) at 50% opacity/blending.
- **Metaphor**: A pulsing cyan aura that stabilizes nearby thermal dissipation.

---
**Vision Artist: Design Lead**
*, (2024)*
