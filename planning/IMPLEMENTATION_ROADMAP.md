# Implementation Roadmap
## Project Neon Oubliette - Development Milestones

---

## Phase Overview

| Phase | Duration | Focus | Deliverable |
|-------|----------|-------|-------------|
| 0 | Week 1 | Foundation | Event bus, clock, world state |
| 1 | Week 2-3 | Rendering | notcurses integration, 60fps |
| 2 | Week 4-5 | BioSim Layer | Organs, drugs, trauma |
| 3 | Week 6 | Inspect System | UI panels, visualizations |
| 4 | Week 7 | Integration | Cross-layer events |
| 5 | Week 8 | Prototype | Playable BioSim demo |

---

## Phase 0: Core Foundation (Week 1)

### Milestone 0.1: Event Bus Implementation
- IEventBus interface with EnTT signals
- Thread-safe event queuing
- Deterministic dispatch order
- Zero-allocation during dispatch

### Milestone 0.2: Simulation Clock
- Phase-locked timing
- Layer frequencies: L0-L1 every tick, L2 every 5, L3 every 10, L4 every 50
- Tick counter with save/restore

### Milestone 0.3: World State Management
- Entity creation/destruction with UUID
- Component add/remove/get
- Query system for component combinations
- State snapshot capture/restore

---

## Phase 1: Rendering Foundation (Week 2-3)

### Milestone 1.1: notcurses Integration
- IRenderer implementation
- True color (24-bit) display
- Unicode box drawing
- 60fps at 120x40 resolution

### Milestone 1.2: Input System
- IInputHandler implementation
- Keyboard and mouse input
- Non-blocking poll mode

### Milestone 1.3: UI Framework
- Panel container
- Text rendering with word wrap
- Bar charts with gradient fill

---

## Phase 2: BioSim Layer (Week 4-5)

### Milestone 2.1: Anatomy Components
- OrganState for: heart, lungs, liver, kidneys, brain
- CirculatorySystem with blood volume/oxygen
- Organ failure detection

### Milestone 2.2: Vital Signs
- Heart rate (60-180 BPM)
- Blood pressure
- Oxygen saturation (0-100%)
- Core temperature (35-42C)

### Milestone 2.3: Metabolism & Drugs
- Blood chemistry dictionary
- Drug metabolism (half-life based)
- 3 example drugs: morphine, adrenaline, alcohol

### Milestone 2.4: Trauma System
- Wound types: laceration, gunshot, burn
- Bleeding rate calculation
- Pain level (0-10)
- Basic healing

---

## Phase 3: Inspect System (Week 6)

### Milestone 3.1: Inspectable Interface
- IInspectable on entities
- InspectReport generation
- Privacy mask application

### Milestone 3.2: BioSim Inspect Panel
- Anatomical ASCII diagram
- Vital signs dashboard
- Organ status grid

---

## Phase 4: Cross-Layer Integration (Week 7)

### Milestone 4.1: Event Propagation
- BioSim emits: DeathEvent, InjuryEvent
- Emergency medical cost fluctuation
- Supply/demand reaction

---

## Phase 5: Prototype Demo (Week 8)

### Milestone 5.1: Test Scenario
- Procedurally generated city block (10x10)
- 50 NPCs with varying health
- 1 hospital with supplies

### Milestone 5.2: Playability
- Tutorial explains Inspect
- Win: Save 10 patients
- Lose: 5 preventable deaths

---

## Technology Stack

- EnTT: v3.12.2
- notcurses: v3.0.9
- C++20
- CMake 3.20+

## Risk Register

| Risk | Mitigation |
|------|------------|
| notcurses performance | Fallback to ncurses |
| Save determinism | Automated testing |
