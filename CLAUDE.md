# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

```bash
# Standard build (RelWithDebInfo, Ninja)
cmake --build --preset build-ninja-relwithdebinfo

# Debug build (faster incremental)
cmake --build --preset build-ninja-debug

# Release build (ThinLTO + Unity)
cmake --build --preset build-ninja-release

# Run
cd build_ninja/bin && ./neon_oubliette

# View logs
cat game.log

# ASAN build (bug tracing)
mkdir -p build_asan && cd build_asan && cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_FLAGS="-g -fsanitize=address -fno-omit-frame-pointer" -DCMAKE_C_FLAGS="-g -fsanitize=address -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
cmake --build . -j$(sysctl -n hw.ncpu)

# Check cache performance
ccache -s
```

## AI Agent Tools & Workflow

This repository is optimized for AI-assisted development using `rtk`, `repomix`, `ast-grep`, and `grep-ast`.

### Workflow Commands
```bash
# Generate targeted architectural overview (pure logic blueprint)
repomix --config repomix.config.json

# Structural search (surgical extraction of logic blocks)
sg scan

# High-level Repo Map (compressed outline of headers/folders)
gast <path>

# Token-optimized operations (transparently handled by rtk hook)
rtk read <file>
rtk grep <pattern>
```

### Agent Rules
Refer to `.clauderules` for detailed token optimization and developer hygiene requirements, including:
- **RTK Enforcement**: All file/search operations MUST use terminal commands to hit the token compression proxy.
- **Compile & Correct**: Agents MUST verify localized edits with `./build.sh` before presenting diffs.
- **Level of Detail**: Use `gast` for mapping and selective `rtk read` for implementation logic.


Prerequisites: `brew install cmake pkgconf sdl2 sdl2_image sdl2_ttf cereal ccache`

Build outputs go to `build_ninja/`, `build_ninja_debug/`, etc. (matching preset names).

## Architecture

Neon Oubliette is a windowed SDL2 procedurally generated mega-city simulation. C++20, EnTT ECS, SDL2 for rendering.

### Simulation Layers

Five causally-linked layers tick at different rates. Lower layers feed upward:

| Layer | Name | Focus |
|-------|------|-------|
| L0 | Physics | Temperature, pressure, materials, structural integrity |
| L1 | Biology | Organs, metabolism, neural states, pathogens |
| L2 | Cognitive | Agents (BDI), memory, emotions, social networks |
| L3 | Economic | Markets, transactions, resource flows, provenance |
| L4 | Political | Organizations, policies, laws, faction influence |

### ECS + System Scheduler

All logic lives in **76 systems** registered in `ecs/system_registration.cpp`. Systems run in five ordered phases per tick:

```
Input → Macro → Micro → PostMicro → Output
```

- `ecs/system_scheduler.h` — `ISystem` interface + `Phase` enum
- `ecs/simulation_coordinator.h/cpp` — orchestrates macro/micro ticking across layers
- `ecs/command_buffer.h` — deferred entity mutations (avoid mid-tick registry invalidation)
- `ecs/event_declarations.h` — all event types for the EnTT dispatcher

### Key Component Files

- `ecs/components/components.h` — primary component definitions (large, ~68KB)
- `ecs/components/simulation_layers.h` — per-layer component tags/structs
- `ecs/component_registration.cpp` — registers all components with EnTT

### Notable Systems

- `ecs/systems/sdl_rendering_system.cpp` — SDL2-based rendering logic
- `ecs/systems/city_generation_system.cpp` — procedural city layout, zoning, connectivity graph
- `ecs/systems/agent_decision_system.cpp` — NPC BDI AI (~43KB)
- `ecs/systems/dialogue_system.cpp` — Ink/inkcpp scripted narrative (~65KB)
- `ecs/systems/chunk_streaming_system.cpp` — LOD/spatial streaming

### Config & Data

- `src/config/ConfigLoader.h/cpp` — JSON config loading with schema validation (nlohmann/json + json-schema-validator)
- `data/configs/` — world generator params, archetypes, religions, etc.
- `data/schemas/` — JSON schema definitions
- `data/dialogue/` — Ink dialogue scripts

### Architecture Docs

Detailed design documents live in `architecture/` and `design/`. Key reads:
- `architecture/simulation_loop.md` — full tick flow
- `architecture/macro_micro_design.md` — cross-layer causality
- `design/SIMULATION_LAYERS_ARCHITECTURE.md` — layer definitions

### Dependencies

- **EnTT** (v3.12.2) — ECS registry + event dispatcher
- **SDL2** — windowed graphical rendering
- **nlohmann/json** — JSON parsing
- **Cereal** — binary serialization
- **inkcpp** — Ink narrative scripting
- **backward-cpp** — crash stack traces
