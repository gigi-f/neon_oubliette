
## Political and Social Components

| Component | Description | Key Fields |
|-----------|-------------|------------|
| `FactionComponent` | Organized groups (corps, gangs, parties) | `name`, `ideology`, `resources`, `reputation`, `power_base` |
| `PublicOpinionComponent` (ctx) | City-wide sentiment | `issue_stances`, `faction_ratings` |
| `InfluenceComponent` | Entity's sway over decisions | `current_influence` |
| `BeliefComponent` (NPC) | NPC's stance on issues | `political_stance`, `economic_stance` |
| `VoterComponent` (NPC) | NPC's voting behavior | `vote_propensity`, `candidate_preference` |
| `FactionAffiliationComponent` (NPC) | NPC's link to factions | `faction_id`, `loyalty_level` |
| `CandidateComponent` | NPC running for office | `platform`, `funding`, `endorsements` |
| `CampaignEvent` | Campaign activities | `type`, `target_faction`, `message` |
| `PromiseComponent` | Campaign promise | `candidate_id`, `description`, `fulfilled_status` |
| `LieComponent` | False campaign claim | `candidate_id`, `description`, `exposed_status` |
| `ElectedOfficeComponent` | Held by winning candidates | `office_type`, `term_end_date` |
| `BillComponent` | Proposed laws | `title`, `effects`, `current_status` |
| `BriberyComponent` | Exchange of currency for action | `payer_id`, `recipient_id`, `amount`, `requested_action` |
| `FavorComponent` | Non-monetary obligation | `giver_id`, `receiver_id`, `description`, `fulfilled_status` |
| `BlackMarketComponent` | Illicit goods/services | `type`, `risk_level`, `goods_available` |
| `BackroomDealComponent` | Secret agreements | `participants`, `terms`, `hidden_effects` |
| `ScandalEvent` | Exposure of illicit activity | `type`, `involved_entities`, `impact` |


## Spatial and Verticality Components

| Component | Description | Key Fields |
|-----------|-------------|------------|
| `PositionComponent` | Entity's 3D grid coordinates | `int x`, `int y`, `int z` |
| `PlayerCurrentLayerComponent` | Tracks the player's active vertical layer | `int current_z` |
| `StairsComponent` | Connects two vertical layers | `int from_z`, `int to_z`, `std::pair<int, int> entrance_pos`, `std::pair<int, int> exit_pos` |
| `ElevatorComponent` | Provides vertical transport for entities | `std::set<int> accessible_z_layers`, `int current_z`, `float speed` |
| `ShaftComponent` | Represents a vertical open space (e.g., for elevators, ventilation) | `int min_z`, `int max_z`, `bool is_open_to_elements` |
| `FloorComponent` | Defines a solid walkable surface at a specific Z-level | `int z_level`, `MaterialType material`, `bool is_structural` |
| `CeilingComponent` | Defines a solid overhead surface at a specific Z-level | `int z_level`, `MaterialType material`, `bool is_structural` |
| `VerticalViewComponent` | Used by the 'floor scanner' tool to adjust rendering | `int min_visible_z`, `int max_visible_z` |

## UI and Player State Components

| Component | Description | Key Fields |
|-----------|-------------|------------|
| `HUDComponent` | Data for rendering the Head-Up Display | `health`, `energy`, `credits`, `current_layer_display`, `notifications` |
| `InventoryComponent` | Stores items held by an entity | `std::map<ItemType, int> items`, `int max_slots`, `float current_weight` |
