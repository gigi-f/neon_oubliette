# Verticality and Player View Architecture

## 1. Introduction
Project Neon Oubliette, being a hyper-detailed simulation of a futuristic mega-city, necessitates a robust system for handling verticality. The city is not merely a 2D plane but a complex, multi-layered structure with various interconnected levels. This document outlines the architectural approach to representing this vertical dimension within the simulation and how the player character will interact with and perceive it, specifically through a "floor scanner" tool.

## 2. 3D World Representation
The simulation world will be fundamentally grid-based, extending into three dimensions (X, Y, Z).
*   **X and Y**: Represent the horizontal coordinates on a given layer.
*   **Z**: Represents the vertical layer, floor, or elevation. Each integer value of Z corresponds to a distinct "floor" or vertical stratum within the city.
*   **Layer Properties**: Each Z-level (or a defined range of Z-levels) can have properties such as:
    *   `LayerHeight`: The physical height of the layer.
    *   `FloorType`: Material, structural integrity, permeability.
    *   `CeilingType`: Same as floor, but for the layer above.
    *   `AtmosphericConditions`: Air quality, temperature (which can vary per layer).
    *   `Accessibility`: Whether it's an open-air level, enclosed, subterranean, etc.

## 3. Entity Verticality
All entities within the ECS will possess a Z-coordinate as part of their `PositionComponent`.
*   **`PositionComponent` Update**: The `PositionComponent` will be extended to include `int z;` in addition to `int x;` and `int y;`.
*   **Inter-Layer Interaction**:
    *   **Movement**: Entities can move between layers using designated `StairsComponent`, `ElevatorComponent`, `RampComponent`, or `ShaftComponent`. Drones may freely traverse Z-levels (within structural limitations).
    *   **Falling/Dropping**: Objects can fall through gaps or weak floors, potentially affecting entities on lower levels.
    *   **Line of Sight**: Vision and sensor ranges will be adapted to account for obstructions across Z-levels.

## 4. Player's Current View/Layer
The player character (`PlayerComponent`) will always be associated with a specific active Z-level.
*   **`PlayerCurrentLayerComponent`**: A component attached to the player entity that tracks `current_z_level`.
*   **Input for Layer Change**:
    *   Interacting with `StairsComponent`, `ElevatorComponent`, etc., will trigger a change in the `PlayerCurrentLayerComponent`'s `current_z_level`.
    *   Movement systems will handle the Z-axis transition logic, potentially with animations or screen transitions.

## 5. Floor Scanner Tool
To enhance player perception and interaction with the multi-layered city, a "floor scanner" tool will be implemented.

### 5.1. Mechanism
*   **Activation**: The player can activate the scanner tool via a specific keybinding (e.g., 'S').
*   **Range**: The scanner will have a configurable vertical range, allowing the player to view a limited number of layers above and below their current `current_z_level` (e.g., `player_z +/- 2` layers).
*   **Toggle**: The scanner can be toggled on/off. When off, only the `current_z_level` is rendered normally.

### 5.2. View Manipulation & Rendering Implications (Notcurses)
Notcurses, being a terminal-based rendering library, requires creative solutions for multi-layer display.
*   **Layered Rendering**: Conceptually, Notcurses will render multiple 2D planes, each corresponding to a Z-level, but projected onto a single terminal screen.
*   **Visual Distinction**:
    *   **Active Layer (Player's `current_z_level`)**: Rendered normally, with full detail and color.
    *   **Scanned Layers (e.g., `player_z +/- 1, 2`)**: Rendered with visual cues to indicate they are not the active layer. This could involve:
        *   **Transparency Simulation**: Using dimmer colors, reduced contrast, or specific Notcurses "styles" to suggest transparency. Entities might appear as outlines or ghost images.
        *   **Color Tinting**: A distinct color overlay (e.g., blue for layers below, green for layers above) to quickly differentiate them.
        *   **Reduced Detail**: Only critical entities or structural elements are rendered, with less environmental clutter.
        *   **Grid Overlay**: A subtle grid pattern over scanned layers to emphasize their spatial separation.
    *   **Unseen Layers**: Not rendered at all, or only abstract representations are shown (e.g., "darkness" or "unknown").
*   **Dynamic Z-Scrolling**: When the player changes their active Z-level, the rendering focus smoothly shifts to the new layer, bringing it into full detail and making other layers scanned or unseen.

### 5.3. Information Display
The floor scanner is not just for visual traversal; it's a diagnostic tool.
*   **Entity Positions**: Shows entities (NPCs, drones, animals) on scanned layers.
*   **Infrastructure**: Reveals concealed infrastructure (pipes, wiring conduits, structural beams, vents) that might not be visible on the active layer.
*   **Resource Nodes**: Highlights resource deposits or points of interest on other layers.
*   **Environmental Data**: Displays data like:
    *   **Structural Integrity**: Weak points, damage, potential collapses.
    *   **Utility Status**: Broken pipes, overloaded circuits, gas leaks.
    *   **Air Quality/Pollution**: Concentrations of pollutants or toxins.
    *   **Hazard Zones**: Areas with high radiation, fire, or other dangers.
*   **Pathfinding Visualization**: Potentially highlights viable pathways across layers.

## 6. Inter-Layer Interactions
The Z-axis is not isolated; events and systems must propagate vertically.
*   **Sound**: Sound waves can travel through open shafts, stairwells, or even through thin floors, with appropriate dampening.
*   **Light**: Light can illuminate adjacent layers through openings.
*   **Environmental Effects**:
    *   **Fire/Smoke**: Can spread vertically through ventilation systems, stairwells, or structural breaches.
    *   **Water/Flooding**: Water leaks can propagate downwards, affecting multiple layers.
    *   **Pollution/Gas Leaks**: Can drift between layers based on air currents and ventilation.
*   **Structural Stress**: Damage on one layer can impact the structural integrity of layers above and below.
*   **System Connections**:
    *   **Power Grid**: Power lines connect generators/substations to devices across different floors.
    *   **Sanitation**: Pipe networks span multiple layers for water and waste.
    *   **Traffic Flow**: Drones, elevators, and intra-building transport systems manage vertical movement.
*   **Pathfinding**: The pathfinding system must be 3D-aware, calculating routes that include Z-axis transitions via stairs, elevators, or open-air drone routes.

## 7. Impact on Components and Systems
Many existing components and systems will need to be updated or extended to account for verticality.

*   **`PositionComponent`**: Add `int z;`.
*   **`RenderComponent`**: Adapt to handle multi-layer rendering based on player's `current_z_level` and scanner state.
*   **`PathfindingSystem`**: Update to support 3D A*.
*   **`PhysicsSystem`**: Account for gravity, falling objects, and inter-layer collisions.
*   **`EnvironmentalSystem`**: Manage vertical propagation of weather, pollution, and hazards.
*   **`InfrastructureSystem`**: Ensure utility networks (`PipeSegmentComponent`, `PowerLineComponent`) correctly span Z-levels.
*   **`AISystem`**: AI agents need 3D pathfinding and awareness of vertical obstacles/opportunities.
*   **New Components**: `StairsComponent`, `ElevatorComponent`, `ShaftComponent` (for open vertical spaces), `FloorComponent`, `CeilingComponent`.
*   **`SensorComponent`**: Range and line-of-sight calculations must be 3D.
*   **`CameraSystem`**: Manage the view frustum and active rendering layers based on player Z-level and scanner input.

This architectural blueprint establishes the foundation for a truly multi-layered, immersive simulation experience in Project Neon Oubliette.