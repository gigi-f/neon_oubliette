# Project Neon Oubliette

### Prerequisites: brew install cmake pkgconf notcurses cereal

### Debug build with ASAN [bug tracing]: cd /Users/gm1/Code/neon_oubliette/build && cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_FLAGS="-g -fsanitize=address -fno-omit-frame-pointer" -DCMAKE_C_FLAGS="-g -fsanitize=address -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" 2>&1 | tail -5

### To build: cmake --build --preset build-ninja-relwithdebinfo -j2
## Optional profiling build: cmake --preset ninja-time-trace && cmake --build -j2

### To run: cd /Users/gm1/Code/neon_oubliette/build/bin && ./neon_oubliette

### To view logs: cat /Users/gm1/Code/neon_oubliette/game.log

### OLD build: cd /Users/gm1/Code/neon_oubliette/build && cmake --build . -j2

## System-Centric Survival Simulation

A procedurally generated, hyper-detailed simulation of a futuristic mega-city.

## Core Pillars

1. Hyper-Simulation: Every entity has internal state
2. Macro-Micro Scaling: Small-scale events ripple upward
3. ASCII Aesthetics: Terminal-based graphics
4. Extensibility First: Modular, reusable systems

## Directory Structure

- design/: High-level architecture
- mechanics/: Game mechanics specifications
- systems/: System interaction specifications  
- data_schemas/: Data structure definitions
- research/: Technical research
- assets/ascii/: Visual design assets

## Simulation Layers

| Layer | Name | Focus |
|-------|------|-------|
| 0 | Physics | Temperature, pressure, materials |
| 1 | Biology | Organs, metabolism, pathogens |
| 2 | Cognitive | Agents, emotions, social networks |
| 3 | Economic | Markets, transactions, debt |
| 4 | Political | Factions, laws, territory |


