
### InventoryToggleEvent

**Description:**
This event is dispatched when the user requests to toggle the visibility of the in-game inventory. It signals to interested systems, such as the `RenderingSystem`, to update their state regarding inventory display.

**Purpose:**
To decouple input handling from rendering logic, allowing the `InputSystem` to simply report user intent, and the `RenderingSystem` (or other UI management systems) to react accordingly.

**Payload:**
None. The event itself acts as the signal.

### PlayerMoveEvent

**Description:**
Dispatched when the player requests to move horizontally (e.g., North, South, East, West).

**Purpose:**
To signal player movement intent, allowing `PlayerMovementSystem` to update the player's `PositionComponent`.

**Payload:**
- `int dx`: Change in x-coordinate.
- `int dy`: Change in y-coordinate.

### PlayerLayerChangeEvent

**Description:**
Dispatched when the player requests to move vertically between layers (e.g., Up, Down).

**Purpose:**
To signal player vertical movement intent, allowing `PlayerMovementSystem` to update the player's `PositionComponent` (specifically the z-coordinate) and `PlayerCurrentLayerComponent`.

**Payload:**
- `int dz`: Change in z-coordinate (e.g., +1 for up, -1 for down).
