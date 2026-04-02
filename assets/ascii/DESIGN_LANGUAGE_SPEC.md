# ASCII Visual Design Language Specification
## Neon Oubliette Interface Standard

---

## 1. Character Resolution Architecture

### Cell Anatomy
Each terminal cell (typically 8x16 pixels in modern terminals) can display:
- **Single Unicode character** (U+0000 to U+10FFFF)
- **Foreground color** (24-bit RGB)
- **Background color** (24-bit RGB)
- **Attributes** (bold, italic, underline, blink, inverse, invisible)

**Effective Resolution**:
- Standard 80x24 terminal = 1,920 "pixels" (characters)
- With Braille patterns (U+2800-U+28FF): 2x4 dots per cell = 4,096 addressable points
- With block elements (U+2580-U+259F): Vertical/horizontal 1/8 steps

---

## 2. Simulation Layer Visual Signatures

Each layer has distinct visual rhythm and character vocabulary:

### 2.1 BioSim: The Clinical Monitor

**Character Set**:
