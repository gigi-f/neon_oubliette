# Notcurses Vertical Rendering Design Document

## 1. Core Principles for Verticality Display

*   **Player-Centric View**: The primary display always centers on the player's current Z-layer.
*   **Contextual Awareness**: Provide visual cues for layers immediately above and below the player's current layer, even when the floor scanner is inactive.
*   **Floor Scanner Clarity**: When activated, the floor scanner must clearly differentiate between the current layer, scanned layers, and unscanned layers.
*   **Performance**: Rendering multiple layers, especially with effects, must remain performant within Notcurses constraints.
*   **ASCII Aesthetic**: Maintain the core ASCII aesthetic throughout all vertical rendering.

## 2. Display Modes

### 2.1. Standard View (Floor Scanner Inactive)

*   **Current Layer (Player's Z-level)**:
    *   Full fidelity ASCII rendering.
    *   All entities, environmental features, and visual effects are rendered normally.
*   **Adjacent Layers (Z-1 and Z+1, if visible)**:
    *   **Representation**: Subtle visual cues indicating structures or movement that transcend layers (e.g., stairs leading up/down, shafts, elevators).
    *   **Rendering**: Could be achieved with:
        *   **Ghosting/Dimming**: Entities or tiles from adjacent layers appear slightly faded or with reduced brightness/contrast.
        *   **Simplified Outline**: Only the outer perimeter of solid objects on adjacent layers is rendered.
        *   **Partial Overlap**: If a structure occupies space across layers (e.g., a thick wall), its continuation on the adjacent layer is hinted at.
    *   **Focus**: This is a hint, not a full render. The player should not be able to interact with entities on these layers directly without moving to them.

### 2.2. Floor Scanner View (Floor Scanner Active)

When the `VerticalViewComponent` is active on the player entity, the rendering shifts to display multiple layers with enhanced detail based on the scanner's range.

*   **Current Layer (Player's Z-level)**:
    *   Remains full fidelity.
    *   Potentially has a visual overlay indicating it's the "active" layer being scanned from.
*   **Scanned Layers (within `VerticalViewComponent::range` above/below current Z)**:
    *   **Purpose**: To reveal detailed information about entities and environmental features on these layers.
    *   **Rendering Strategy**:
        *   **Wireframe/Outline Mode**: Objects on scanned layers are rendered as outlines or simplified wireframes.
        *   **Color-coded for Material/Type**: Different object types (e.g., `StairsComponent`, `ElevatorComponent`, `ShaftComponent`, `FloorComponent`, `CeilingComponent`, `EnemyComponent`, `ItemComponent`) could have distinct ASCII characters or color schemes.
        *   **Transparency/Dimming**: Scanned layers are rendered with a degree of transparency or significant dimming compared to the current layer, allowing the player to easily distinguish between the current and scanned layers.
        *   **Information Overlays**: Small, context-sensitive text overlays could appear when hovering (or a similar selection mechanism) over an entity on a scanned layer, showing its type or basic status.
    *   **Interaction**: Direct interaction with entities on scanned layers is generally not permitted, but the scanner might highlight pathways or potential threats.
*   **Unscanned Layers (beyond `VerticalViewComponent::range`)**:
    *   Similar to "Adjacent Layers" in Standard View, but potentially even more diminished or completely hidden.
    *   A simple indicator (e.g., "...") could denote further layers exist beyond the scan range.

## 3. Notcurses Implementation Considerations

*   **NCPANES and Z-Ordering**: Notcurses `NCPANES` are crucial.
    *   Each Z-layer could potentially be a separate `NCPANE`.
    *   How will Z-ordering be managed when displaying multiple panes? Will layers be rendered directly above/below each other, or will they be superimposed with transparency?
    *   **Challenge**: Notcurses panes do not inherently support transparency. Simulating transparency will require careful rendering.
*   **Alpha Blending/Color Palettes**:
    *   Notcurses primarily deals with characters and their foreground/background colors. True alpha blending across `NCPANES` is not straightforward.
    *   **Workaround**: Utilize distinct color palettes for scanned layers (e.g., a muted palette, grayscale, or specific "scanner" colors) to achieve the "dimming" or "transparency" effect.
    *   Could leverage `ncplane_set_base()` with transparent `cell` if possible, but this might affect background.
*   **Character Overlays**:
    *   For wireframe effects, custom ASCII characters could be used for borders and outlines of objects on scanned layers.
*   **Dynamic Pane Management**:
    *   When the scanner activates, `NCPANES` for scanned layers would need to be created/made visible. When deactivated, they would be destroyed/hidden.
*   **Performance of Multiple Panes**: Creating and managing many `NCPANES` dynamically could impact performance. We need to measure and optimize.
*   **Input Handling**: How will input (e.g., arrow keys) switch between scanned layers, and how will it be differentiated from movement on the current layer?

## 4. Consultation with Lead Librarian

I will consult @lead_librarian regarding the following specific Notcurses capabilities and best practices for vertical rendering:

*   **Simulating Transparency**: What are the most effective Notcurses techniques for simulating transparency or dimming effects across layers/panes, given the lack of true alpha blending for `NCPANES`? Are there specific `ncplane_set_base()` or `nc_cell` techniques that could be leveraged?
*   **Efficient Pane Management**: What are the best practices for dynamically creating, showing, hiding, and destroying `NCPANES` for optimal performance when the floor scanner activates/deactivates?
*   **Layer Superimposition vs. Adjacent Display**: What are the Notcurses idioms for displaying multiple "layers" of information? Is it better to attempt to superimpose them (simulating transparency) or display them adjacently (e.g., current layer in center, Z-1 above, Z+1 below, each in its own dedicated screen region)?
*   **Input for Layer Switching**: What Notcurses input mechanisms are best suited for allowing the player to "scroll" through scanned layers or select a specific layer for focus within the scanner view?

This detailed design document provides a solid foundation for implementing vertical rendering in Notcurses. The next step is to get the @lead_librarian's input to ensure our technical approach is sound and efficient.
