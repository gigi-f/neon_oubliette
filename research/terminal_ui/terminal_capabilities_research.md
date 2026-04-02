# Terminal UI Capabilities Research
## Technical Specifications for Neon Oubliette Interface

---

## 1. Terminal Emulator Landscape

### 1.1 Reference Standards

**ANSI/ECMA-48**:
- Base control sequences for cursor positioning, colors, text attributes
- Universally supported across modern terminals
- Limitations: 8 standard colors, 8 bright variants, basic attributes (bold, underline, blink)

**xterm-256color**:
- 256 color palette (216 color cube + 16 grays + 16 system colors)
- De facto standard for modern terminal applications
- Supported by: iTerm2, Windows Terminal, GNOME Terminal, alacritty, wezterm

**True Color (24-bit RGB)**:
- `\033[38;2;R;G;Bm` foreground, `\033[48;2;R;G;Bm` background
- Support: >90% of modern terminals (https://github.com/termstandard/colors)
- Critical for Oubliette's nuanced visualization (health gradients, subtle state changes)

**Unicode Support**:
- U+2500 to U+257F: Box drawing characters (essential for UI frames)
- U+2580 to U+259F: Block elements (1/8 steps for "pixel" precision)
- U+2800 to U+28FF: Braille patterns (dot-matrix style density representation)
- Emoji: Variable support, avoid for core UI

### 1.2 Performance Characteristics

**Rendering Bottlenecks**:
- **Scrollback buffer**: Large buffers (>10k lines) slow down rendering
- **Unicode width calculation**: East Asian wide characters require double-width handling
- **True color overhead**: Minimal on GPU-accelerated terminals (alacritty, wezterm), noticeable on VTE-based

**Recommended Targets**:
- **Minimum**: xterm-256color, basic box drawing
- **Optimal**: True color, Unicode box drawing, 120+ fps capable (alacritty/wezterm)
- **Fallback**: Monochrome with bold attributes for accessibility

---

## 2. Terminal UI Frameworks & Libraries

### 2.1 C/C++ Libraries

**ncurses / ncursesw**:
- **Standard**: POSIX terminal control
- **Capabilities**: Window abstraction, input handling, color pairs (limited to 256/short)
- **Limitations**: No true color support (requires patches or extensions), single-threaded input
- **Oubliette Suitability**: Base layer only; too limiting for rich visual design

**ncurses vs ncursesw**:
- ncurses: 8-bit characters only
- ncursesw: Wide character/MBCS support (required for Unicode box drawing)

**notcurses** (modern replacement):
- **Author**: Nick Black
- **Repository**: https://github.com/dankamongmen/notcurses
- **Capabilities**:
  - True color throughout
  - Multimedia support (images/video via Sixel/KITTY/ITerm2 inline images)
  - Multiple threads, non-blocking input
  - 2D plane abstraction with z-ordering
  - Direct mode (immediate rendering) vs rendered mode (framebuffer)
  
**Critical Features for Oubliette**:
