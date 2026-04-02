# ECS Component Definitions

## Macro Components

| Component | Description | Key Fields |
|-----------|-------------|------------|
| `CityComponent` | City level parameters | `size`, `seed`, `time_tick` |
| `PositionComponent` | Global grid position | `x`, `y` |
| `RoadComponent` | Road geometry & properties | `geometry`, `speed_limit`, `capacity` |
| `RailNetworkComponent` | Train network | `route`, `speed_limit` |
| `BuildingComponent` | Building metadata + micro registry reference | `height`, `zoning`, `floor_registry`, `occupant_count` |
| `ResourceNodeComponent` | Import/Export node | `type`, `flow_rate`, `is_import` |
| `CitizenComponent` | Global citizen traits | `age`, `skill`, `wealth`, `family_group` |
| `HousingPreferenceComponent` | Desired housing | `floor_range`, `building_type`, `proximity` |
| `GlobalResourceStockpile` (ctx) | Aggregated raw materials | `std::unordered_map<ResourceType,uint64_t>` |
| `MacroEventBus` (ctx) | Event bus for cross‑layer events | N/A |
| `BudgetComponent` | Building budget | `allocated`, `spent` |
| `TaxationComponent` | Tax rates | `property_tax_rate`, `income_tax_rate` |
| `MacroMarketComponent` | City economy | `GDP`, `unemployment_rate`, `tax_revenue` |
| `WorkstationComponent` | Job location | `position`, `job_type`, `capacity` |

## Micro Components

| Component | Description | Key Fields |
|-----------|-------------|------------|
| `FloorComponent` | Floor layout | `level`, `rooms`, `elevator_link` |
| `RoomComponent` | Interior room | `room_id`, `capacity`, `utility_requirements` |
| `ApartmentComponent` | Apartment unit | `floor`, `room`, `occupancy_status` |
| `NPCComponent` | NPC behavior | `mood`, `inventory_id`, `macro_citizen_id` |
| `ContainedByComponent` | Hierarchical containment | `container_id` |
| `ShopComponent` | Retail store | `inventory`, `price_list`, `sales_counter` |
| `ResidentComponent` | Occupant in micro | `macro_id`, `utility_consumption` |
| `UtilityAllocationComponent` | Utility status for unit | `power`, `water`, `waste` |
| `VehicleComponent` | Macro vehicle | `type`, `resource_transport`, `path`, `position` |
| `ElevatorControlComponent` | Elevator logic | `current_floor`, `direction`, `stop_queue` |
| `InteractionQueue` | NPC-NPC interactions | `target`, `type`, `timestamp` |
| `RelationshipComponent` | NPC affinity | `target_id`, `affinity_score`, `last_interacted` |
| `PurchaseEvent` | NPC purchase | `buyer_id`, `shop_id`, `item`, `quantity` |
| `RawMaterialDeliveryEvent` | Resource delivery to building | `resource_type`, `amount` |

---

*All component structs are PODs with trivial constructors; they will be registered with EnTT in `ecs/component_registration.cpp`.*
