# Color System Research - Neon Noir Palette

## Aesthetic Direction
Theme: Medical Diagnostic Interface Meets Cyberpunk Noir
- Player is diagnostic system jacked into failing city
- Clinical observation of decay

## Base Background Colors
| Hex | RGB | Name |
|-----|-----|------|
| #0A0A0F | 10,10,15 | Primary BG |
| #12121A | 18,18,26 | Secondary BG |
| #1A1A24 | 26,26,36 | Tertiary BG |

## Foreground Text
| Hex | RGB | Name |
|-----|-----|------|
| #E0E0E0 | 224,224,224 | Primary Text |
| #808090 | 128,128,144 | Secondary |
| #00FFFF | 0,255,255 | Accent |

## Layer Palettes

### BioSim (Medical)
- Healthy: #00FF88 (Neon Green)
- Nominal: #00FFFF (Cyan)
- Caution: #FFFF00 (Yellow)
- Critical: #FF0044 (Red-Pink)

### EcoSim (Financial)
- Wealth: #FFD700 (Gold)
- Profit: #32CD32 (Lime)
- Debt: #FF6B6B (Coral Red)

### InfraSim (Electrical)
- Nominal: #00FF00 (Green)
- Warning: #FFFF00 (Yellow)
- Critical: #FF0000 (Red)

### SocioSim (Social)
- Trusted: #00FF7F (Spring Green)
- Neutral: #FFFF00 (Yellow)
- Hostile: #FF4500 (Orange Red)

## True Color Implementation
Foreground: \033[38;2;R;G;Bm
Background: \033[48;2;R;G;Bm
Reset: \033[0m

## Accessibility
Deuteranopia-safe: Replace greens with blues
Protanopia-safe: Replace reds with oranges
