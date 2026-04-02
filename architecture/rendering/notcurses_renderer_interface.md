# Notcurses Renderer Interface Design

## 1. Introduction
This document outlines the proposed interface for the `NotcursesRenderer` class, which will be responsible for all graphical output using the Notcurses library. It leverages the insights provided by the Lead Librarian regarding `ncplane` management, alpha blending, and flicker-free updates, specifically for rendering the verticality of our simulation and the "floor scanner" tool.

## 2. Core Responsibilities
The `NotcursesRenderer` will primarily handle:
*   Initialization and shutdown of the Notcurses context.
*   Management of `ncplane` objects for different Z-layers and UI elements.
*   Translating ECS entity data (Position, RenderComponent, etc.) into Notcurses rendering calls.
*   Applying visual effects (e.g., dimming, wireframe) to scanned layers.
*   Ensuring flicker-free updates by managing rendering cycles.
*   Processing input related to vertical navigation and the floor scanner.

## 3. High-Level Class Structure

```cpp
// Forward declarations to avoid circular dependencies
namespace entt { class registry; }
namespace NeonOubliette { class EventDispatcher; }
struct ncplane; // Notcurses ncplane

class NotcursesRenderer {
public:
    // Constructor
    NotcursesRenderer(std::shared_ptr<Notcurses::notcurses> nc_context,
                      entt::registry& registry,
                      NeonOubliette::EventDispatcher& event_dispatcher);

    // Destructor
    ~NotcursesRenderer();

    // Initialize the renderer (e.g., create base planes)
    void initialize();

    // Main update/render loop call for the current frame
    void update();

    // Input handling for verticality and scanner
    void handle_input(const ncinput& input);

    // Renderer-specific methods for floor scanner interaction
    void activate_floor_scanner(entt::entity player_entity_id);
    void deactivate_floor_scanner();
    void set_scanner_view_range(int range_above, int range_below); // Number of layers above/below player to scan

private:
    std::shared_ptr<Notcurses::notcurses> nc_context_;
    entt::registry& registry_;
    NeonOubliette::EventDispatcher& event_dispatcher_;

    entt::entity player_entity_id_; // Stored when scanner is active
    bool floor_scanner_active_ = false;
    int scanner_range_above_ = 0;
    int scanner_range_below_ = 0;

    // ncplane management
    std::map<int, ncplane*> layer_planes_; // Map Z-level to its ncplane
    ncplane* ui_plane_ = nullptr; // For general UI elements
    ncplane* console_plane_ = nullptr; // For debug/console output

    // Private rendering helper methods
    void render_layer(int z_level, ncplane* target_plane);
    void render_player_layer();
    void render_scanned_layers();
    void render_ui();
    void clear_all_planes();

    // Helper for applying visual effects
    void apply_scanner_effects(ncplane* plane, int z_level);
};
```

## 4. Input Handling Methods

*   `void handle_input(const ncinput& input)`:
    *   This method will be called by an input system (or directly by `main` loop for prototyping).
    *   It will interpret specific key presses (e.g., `PgUp`, `PgDown` for vertical navigation; 'S' for scanner toggle; `[`/`]` for scanner range adjustment).
    *   Based on input, it might:
        *   Change `player_entity_id_`'s `PositionComponent::z`.
        *   Toggle `floor_scanner_active_`.
        *   Adjust `scanner_range_above_` and `scanner_range_below_`.
        *   Emit events (e.g., `PlayerMovedVerticallyEvent`, `FloorScannerToggledEvent`).

## 5. Rendering Methods

*   `void update()`:
    *   This is the main entry point for rendering each frame.
    *   It will orchestrate calls to `render_player_layer()`, `render_scanned_layers()`, and `render_ui()`.
    *   It will manage the Z-order of `ncplane`s using `ncplane_set_bkgd_rgb()` and `ncplane_set_base()`.
    *   Crucially, it will call `ncnotcurses_render(nc_context_->ncs)` exactly once per frame at the end to ensure flicker-free updates.
*   `void render_player_layer()`:
    *   Identifies the player's current `z` level from `PositionComponent`.
    *   Ensures an `ncplane` exists for this `z` level.
    *   Iterates entities with `PositionComponent` and `RenderComponent` on this `z` level, drawing their ASCII representation and attributes onto the corresponding `ncplane`.
*   `void render_scanned_layers()`:
    *   Only active if `floor_scanner_active_` is true.
    *   Determines the range of layers to scan based on `scanner_range_above_` and `scanner_range_below_`.
    *   For each scanned layer:
        *   Ensures an `ncplane` exists.
        *   Renders entities on that layer.
        *   Applies visual effects (e.g., dimming or wireframe via `ncplane_set_bkgd_alpha()`, or by manually drawing characters with reduced alpha/different styles).
*   `void render_ui()`:
    *   Draws static and dynamic UI elements (e.g., player stats, inventory, debug info) onto `ui_plane_`.
    *   This plane should always be on top.
*   `void render_layer(int z_level, ncplane* target_plane)`:
    *   A private helper method to draw all relevant entities for a given `z_level` onto a `target_plane`. This avoids code duplication between player layer and scanned layers.
    *   Queries the `registry_` for entities with `PositionComponent` (matching `z_level`) and `RenderComponent`.
    *   Uses `ncplane_putstr()` or similar for drawing.
*   `void apply_scanner_effects(ncplane* plane, int z_level)`:
    *   A private helper method to apply visual modifiers based on whether a layer is scanned or the player's current layer. This could involve `ncplane_set_bkgd_alpha()` or direct character manipulation for wireframe.

## 6. ncplane Management Strategy

*   **Dynamic Plane Creation**: `ncplane`s for Z-layers will be created on demand when a layer is first rendered or becomes visible via scanning.
*   **Plane Pool/Cache**: To avoid constant creation/destruction, a pool or map (`layer_planes_`) will store `ncplane*` mapped by their `z_level`.
*   **Z-Order**: The renderer will explicitly manage the stacking order of `ncplane`s using `ncplane_above()` or `ncplane_below()`. The `ui_plane_` will generally be on top, followed by the player's current layer, and then scanned layers.
*   **Clearing**: Each frame, relevant planes will be cleared using `ncplane_erase()` or by overwriting characters. A `clear_all_planes()` helper will reset the canvas.

## 7. Data Flow

1.  **ECS Queries**: The `NotcursesRenderer` will directly query the `entt::registry` for entities possessing:
    *   `PositionComponent`: To get `x`, `y`, `z` coordinates.
    *   `RenderComponent`: To get the character to display (`glyph`), foreground/background colors (`color_fg`, `color_bg`), and attributes (`attributes`).
    *   `PlayerCurrentLayerComponent`: To identify the player entity and its current layer.
    *   `VerticalViewComponent`: To understand the player's scanned view and potential visual overrides.
2.  **Event Dispatcher**: The renderer will interact with the `EventDispatcher` for:
    *   Subscribing to `PlayerMovedVerticallyEvent` to update its internal state (if needed).
    *   Emitting `NotcursesRenderedEvent` (if we need external systems to react to rendering completion).
    *   Subscribing to UI-related events to update dynamic UI elements.

This interface provides a clear separation of concerns and a structured approach to integrating Notcurses into our ECS-driven simulation.
