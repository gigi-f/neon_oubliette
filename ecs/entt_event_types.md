

## Interaction Events

### `struct PickupItemEvent`

*   **Description**: Dispatched when the player attempts to pick up an item from the game world.
*   **Purpose**: Triggers the `InteractionSystem` to check for items at the player's location and add them to their inventory.
*   **Payload**:
    *   `entt::entity player_entity`: The ID of the player entity initiating the pickup.
    *   `int layer_id`: The ID of the layer the player is currently on.
    *   `int x`: The x-coordinate of the player's position.
    *   `int y`: The y-coordinate of the player's position.


## Inspection Events

### `struct InspectEvent`

*   **Description**: Dispatched when the player initiates an inspection of entities at a specific location.
*   **Purpose**: Triggers the `InspectionSystem` to gather information about entities at the player's current location and layer.
*   **Payload**:
    *   `entt::entity player_entity`: The ID of the player entity initiating the inspection.
    *   `int layer_id`: The ID of the layer being inspected.
    *   `int x`: The x-coordinate of the inspection target.
    *   `int y`: The y-coordinate of the inspection target.
