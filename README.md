# Project Neon Oubliette

### Prerequisites: brew install cmake pkgconf notcurses cereal

### To build: cd /Users/gm1/Code/neon_oubliette/build && cmake .. && cmake --build . -j$(sysctl -n hw.ncpu)

### To run: cd /Users/gm1/Code/neon_oubliette/build/bin && ./neon_oubliette

### To view logs: cat /Users/gm1/Code/neon_oubliette/game.log

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

## Key Documents

- SIMULATION_LAYERS_ARCHITECTURE.md
- INSPECT_MECHANICS_SPEC.md  
- SYSTEM_INTERACTION_MATRIX.md
- ENTITY_DATA_SCHEMA.md

## Status

- [x] Design Phase Complete
- [x] Technical Research Complete
- [x] Architecture Polish (Simulation Coordinator)
- [ ] Implementation Planning
