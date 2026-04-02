# Notcurses Vertical Rendering and Floor Scanner Visualization

This document outlines the architectural approach for rendering verticality within Project Neon Oubliette using Notcurses, specifically focusing on how the player's current Z-layer and the "floor scanner" tool will be visualized.

## Core Principles

*   **Clarity over Realism**: Given Notcurses' terminal-based nature, visual clarity of layer separation and entity states is paramount.
*   **Player-Centric View**: The rendering will always center around the player's current Z-layer, with other layers being representations relative to this.
*   **Performance**: Minimize redraws and optimize Notcurses usage for smooth transitions and updates.

## 1. Player's Current Layer Rendering

The player's immediate environment (their current `PositionComponent::z` layer) will be rendered with full detail and normal fidelity.

*   **Visual Elements**: All entities, terrain, and interactive elements on this layer will be rendered using their standard Notcurses glyphs and colors.
*   **Active Interaction**: Player input will directly affect objects on this layer.

## 2. Floor Scanner Tool Visualization (VerticalViewComponent)

When the player activates the "floor scanner" tool, the `VerticalViewComponent` attached to the player entity will dictate which additional layers are visible and how they are rendered.

### 2.1. Layers Above/Below Current Z

Scanned layers will be rendered differently to distinguish them from the active layer and provide an intuitive sense of vertical displacement.

*   **Visual Cues**:
    *   **Dimming/Transparency**: Characters and colors on scanned layers could be rendered with reduced brightness or a more desaturated palette. Notcurses alpha blending capabilities will be explored for this.
    *   **Wireframe/Outline**: Entities on scanned layers might be represented by their outlines or a simplified "wireframe" glyph, rather than their full sprite.
    *   **Color Tinting**: A subtle, uniform color tint (e.g., light blue for "scanned") could be applied to all elements on visible scanned layers.
    *   **"Ghosted" Elements**: Non-interactive elements might appear "ghosted" or semi-transparent.
*   **Layer Indicators**:
    *   **On-screen Z-Axis Marker**: A small HUD element could display a vertical stack of Z-levels, highlighting the current layer and indicating which layers are currently being scanned (e.g., `[-2, -1, (0), +1, +2]` where `(0)` is current and `[-1, +1]` are scanned).
    *   **Border/Frame**: Each visible scanned layer might be enclosed within a subtle Notcurses line-drawing border that indicates its Z-level relative to the player.

### 2.2. View Manipulation

The floor scanner will temporarily adjust the rendering viewport to include selected layers.

*   **Adjacent Layers**: The default scanner mode might show the current layer, one layer above, and one layer below. `VerticalViewComponent::visible_layers` would store these relative Z-offsets.
*   **Scrolling/Paging**: The player might be able to "scroll" through scanned layers, effectively shifting the `visible_layers` array to inspect further up or down the vertical stack without changing their actual `PositionComponent::z`.
*   **Separate Viewports (Advanced)**: For highly detailed views, it might be possible to dynamically allocate distinct Notcurses plane objects for each visible layer, allowing for independent scrolling or even a split-screen effect. This would need careful performance consideration.

### 2.3. Information Display

Beyond just showing entities, the scanner could reveal specific data.

*   **Entity Highlights**: When an entity is "scanned," additional information (e.g., its name, status, health) could briefly appear as an overlay or in a dedicated information panel.
*   **Structural Integrity**: Highlight areas of structural weakness or damage on scanned layers.
*   **Utility Lines**: Show the flow of power, water, or data across different layers.

## 3. Notcurses Implementation Considerations

*   **Multiple `ncplane` Objects**: Each Z-layer could potentially correspond to its own `ncplane`. This allows for independent manipulation (e.g., dimming, scrolling) of each layer.
    *   **Z-Ordering of Planes**: `ncplane` objects have a natural Z-ordering, which we can leverage. The current layer's plane would be on top, with scanned layers below it.
*   **Alpha Blending**: Notcurses supports alpha values in `ncchannel` for glyphs, which can be used to achieve dimming or transparency effects for scanned layers.
*   **Pre-rendering/Caching**: For static elements on distant layers, pre-rendering them to an `ncplane` and then manipulating the plane's alpha or visual attributes might be more performant than re-rendering every frame.
*   **Input Handling**: The rendering system needs to be aware of the `VerticalViewComponent` state to know which layers to draw and how. Player input to activate/deactivate the scanner or change scanned layers would directly update this component.

## Consultation Points for @lead_librarian

1.  **Efficient Alpha Blending**: What is the most performant way to apply a dimming/transparency effect to an entire `ncplane` or a section of it in Notcurses? Are there specific `ncplane` flags or functions designed for this?
2.  **Glyph Manipulation for Wireframe**: How can we easily switch the rendering of complex entities on scanned layers from their full visual representation to a simplified wireframe/outline without completely re-parsing their visual definition? Is there a Notcurses feature for applying a "style" transform to a plane's contents?
3.  **Dynamic Plane Management**: What are the performance implications of creating/destroying `ncplane` objects dynamically for scanned layers versus having a fixed pool of planes and simply changing their content and visibility?
4.  **"Flicker-free" Updates**: Best practices for updating multiple `ncplane` objects concurrently to avoid visual artifacts or flicker, especially during fast layer-scrolling.
5.  **Notcurses-specific "Camera"**: Are there any Notcurses patterns or features that can assist in managing a "camera" that can shift its focus across Z-layers, effectively changing which `ncplane` is considered the primary, fully visible one?

This architectural plan provides a solid foundation for implementing vertical rendering. The next step is to get specific technical guidance from the @lead_librarian to ensure our implementation is robust and efficient.