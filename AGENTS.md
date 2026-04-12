# Project: Neon Oubliette

This file provides architectural context and build instructions for the Codex Agent.

## Build Commands

| Target | Command |
| :--- | :--- |
| Standard (Ninja) | `cmake --build --preset build-ninja-relwithdebinfo` |
| Debug | `cmake --build --preset build-ninja-debug` |
| Release | `cmake --build --preset build-ninja-release` |
| Run | `cd build_ninja/bin && ./neon_oubliette` |
| Logs | `cat game.log` |
| ASAN | `cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_FLAGS="-g -fsanitize=address" && cmake --build .` |

## Workflow Tools

- **rtk**: Token-optimized proxy for file/grep operations.
- **repomix**: Targeted architectural overview (`repomix --config repomix.config.json`).
- **ast-grep (sg)**: Surgical logic extraction (`sg scan`).
- **grep-ast (gast)**: High-level Repo Maps.

## Architecture

Neon Oubliette is a windowed SDL2 procedurally generated mega-city simulation.

### Simulation Layers (L0-L4)
- **L0 (Physics)**: Temperature, pressure, structural integrity.
- **L1 (Biology)**: Metabolism, pathogens.
- **L2 (Cognitive)**: Agent BDI, memory, emotions.
- **L3 (Economic)**: Markets, resource flows.
- **L4 (Political)**: Organizations, laws.

### ECS & System Scheduler
Logic lives in 76 systems registered in `ecs/system_registration.cpp`.
Flow: Input → Macro → Micro → PostMicro → Output.

Key Files:
- `ecs/components/components.h` (Primary components)
- `ecs/system_scheduler.h` (Phase enum)
- `src/config/ConfigLoader.h` (JSON config)
