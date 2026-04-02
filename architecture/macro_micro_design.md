

## 14. Procedural Generation & Ensuring Reachability

A core tenet of Neon Oubliette is its dynamically generated environment. All elements that define traversable space (roads, paths, doors, bridges, elevators, stairwells) must adhere to a strict reachability graph to prevent unreachable areas.

### 14.1 Generation Phases

1.  **Macro-level Grid Initialization**:
    *   `GridGeneratorSystem` (Macro phase): Initializes the entire city grid (Macro `PositionComponent`s). This involves a base terrain generation (e.g., elevation, water bodies) and initial zoning.
    *   **Connectivity Graph**: A `ConnectivityGraphComponent` (global context entity) is maintained, where nodes are significant grid points (e.g., building entrances, road intersections) and edges represent traversable paths. This graph is used during generation to ensure all new elements are connected.

2.  **Road & Infrastructure Laying**:
    *   `RoadNetworkGeneratorSystem` (Macro phase): Based on city zoning and initial terrain, this system procedurally lays down `RoadComponent`s, ensuring all zoned areas are connected to a central network. Bridges are generated automatically where roads cross water bodies or ravines.
    *   **Pathfinding Validation**: Post-generation, a `PathfindingValidationSystem` confirms that all building entrances are reachable from at least one city edge, and all significant points are mutually reachable within the macro-grid. If not, the generation algorithm attempts to resolve the disconnections or flags them for manual intervention/regeneraton.

3.  **Building Shell Generation**:
    *   `BuildingGeneratorSystem` (Macro phase): Places `BuildingComponent` shells according to zoning. Each building receives an initial `FloorplanGeneratorComponent`.

4.  **Micro-level Interior Generation (On-Demand)**:
    *   `FloorplanGeneratorSystem` (Micro phase - run on building activation): When a player or system requests to interact with a building's interior (e.g., entering it, or a macro-system needs to deliver resources), this system generates the `FloorComponent`s, `RoomComponent`s, `DoorComponent`s, `StairwellComponent`s, and `ElevatorShaftComponent`s for that specific building's micro-registry.
    *   **Micro-Connectivity Graph**: Each building's micro-registry maintains its own `ConnectivityGraphComponent` (nodes = room entrances, stairs, elevators; edges = pathways). This ensures all rooms and floors within a building are traversable. `DoorComponent`s are automatically placed at room entrances and connected to the graph.

### 14.2 Key Components for Generation & Reachability

| Component | Layer | Description | Key Fields |
|-----------|-------|-------------|------------|
| `ConnectivityGraphComponent` | Macro/Micro Context | Graph of traversable nodes/edges for pathfinding and reachability validation. | `std::vector<Node>`, `std::vector<Edge>` |
| `FloorplanGeneratorComponent` | Micro | Blueprint/parameters for interior generation. | `template_id`, `density_param`, `room_types` |
| `DoorComponent` | Micro | Defines a traversable barrier within a building. | `is_locked`, `is_open`, `linked_rooms[2]` |
| `StairwellComponent` | Micro | Connects two adjacent floors. | `floor_lower`, `floor_upper` |
| `ElevatorShaftComponent` | Micro | Defines vertical travel path for elevators. | `min_floor`, `max_floor`, `stops` |
| `BridgeComponent` | Macro | Connects two road segments over a gap. | `length`, `width`, `underlying_terrain` |

### 14.3 Dynamic Reachability & Changes

*   **Destruction/Repair**: When `InfrastructureDegradationEvent` leads to `InfrastructureBreakdownEvent` for a road, bridge, or even a `DoorComponent` (e.g., jammed), the respective `ConnectivityGraphComponent` must be updated to reflect the impassable segment. This can lead to new unreachable areas until `InfrastructureRepairedEvent` occurs.
*   **Dynamic Modifications**: Construction or demolition (e.g., adding a new road, removing a building) will trigger updates to the macro `ConnectivityGraphComponent`. Micro-level changes (e.g., a room collapsing) will affect the micro graph.

---

## 15. Wildlife and Urban Ecosystems

Even in a futuristic mega-city, wildlife will find a way to exist, impacting and being impacted by the urban environment.

### 15.1 Animal Entities
*   **Macro Animals**: For large, city-roaming animals (e.g., genetically engineered pigeons, feral cats, large urban predators if the setting allows), these will be `AnimalComponent` entities in the `macro_registry`.
    *   `AnimalComponent`: `species_id`, `current_state` (e.g., hunting, resting), `hunger_level`, `thirst_level`.
    *   `PositionComponent`: Tracks their location on the macro grid.
    *   `PathfindingComponent`: Allows macro animals to navigate roads, alleys, and parks.
*   **Micro Animals**: Smaller animals (e.g., rats, insects) that primarily reside within buildings will be `AnimalComponent` entities in `micro_registry` instances. Their behavior will be more localized.

### 15.2 Food Sources and Foraging
*   **ResourceNodes**: Certain `ResourceNodeComponent`s might be designated as natural food sources (e.g., 'dumpsters', 'green spaces', 'water ponds').
*   **WasteManagementSystem Interaction**: The `WasteComponent` (from sanitation) can act as a food source for scavengers.
*   **ForagingBehaviorSystem** (Macro/Micro Phase):
    *   **Macro**: Animals search for food-designated `ResourceNodeComponent`s or `WasteComponent`s within their perceived range.
    *   **Micro**: Animals within buildings might forage for `WasteComponent`s or `ItemComponent`s in unsecured areas.
    *   When food is consumed, it reduces the `hunger_level` of the animal and reduces the quantity of the food source.

### 15.3 Impact on City Systems
*   **PestControlSystem**: If `MicroAnimalComponent`s (e.g., `RatComponent`, `CockroachComponent`) in a building exceed a threshold, it could trigger a `PestInfestationEvent` leading to negative effects on `NPCComponent` mood, `ShopComponent` reputation, or `BuildingComponent` maintenance costs.
*   **DiseaseTransmissionSystem**: Certain animals could carry and transmit diseases, affecting `NPCComponent` health.
*   **WildlifeEncounters**: `NPCComponent`s might have random encounters with animals, leading to `MoodChangeEvent` or `InjuryEvent`.

## 16. Infrastructure Maintenance and Decay

All physical infrastructure degrades over time, requiring maintenance and repair.

### 16.1 Degradation
*   **ConditionComponent**: Every major infrastructure entity (`RoadComponent`, `BuildingComponent`, `PipeComponent`, `PowerLineComponent`, `BridgeComponent`, `ElevatorShaftComponent`, `DoorComponent`) will have a `ConditionComponent`.
    *   `ConditionComponent`: `current_health` (0-100%), `max_health`, `degradation_rate_per_tick`, `material_type` (influences repair cost/time).
*   **DegradationSystem** (Macro/Micro Phase): Periodically reduces `current_health` based on `degradation_rate_per_tick`, external factors (e.g., `WeatherComponent`, `TrafficDensity`), and age.
    *   When `current_health` drops below a threshold, it emits `InfrastructureDegradationEvent`.
    *   If `current_health` reaches 0%, it emits `InfrastructureBreakdownEvent`, rendering the component non-functional or severely impaired.

### 16.2 Repair and Maintenance
*   **MaintenanceJobComponent**: `BuildingComponent`s or city-level `DepartmentOfInfrastructure` will generate `MaintenanceJobComponent`s.
    *   `MaintenanceJobComponent`: `target_entity_id`, `required_resources` (e.g., `Steel`, `Concrete`), `required_skill_level` (for `NPCComponent`s), `urgency`.
*   **RepairSystem** (Macro/Micro Phase):
    *   `DepartmentOfInfrastructure` (Macro): Assigns `MaintenanceJobComponent`s to `ConstructionCrewComponent` entities (NPCs) based on availability, skill, and resources.
    *   `NPCComponent`s with appropriate `SkillComponent` (e.g., `Engineer`, `Mechanic`) consume `required_resources` and increase the `current_health` of the `target_entity_id` over time.
    *   Upon completion, `InfrastructureRepairedEvent` is emitted.

### 16.3 Sanitation and Utilities
*   **PipeComponent**: Entities representing water pipes, sewer pipes.
    *   `PipeComponent`: `fluid_type` (water, sewage), `flow_rate`, `pressure`, `is_leaking`.
    *   `LeadPipeComponent`: A specialized `PipeComponent` indicating lead content, potentially causing `HealthDebuffComponent` for `NPCComponent`s if water quality is poor.
*   **WaterSystem** (Macro/Micro Phase): Manages water distribution, pressure, and quality.
    *   `WaterMainBreakEvent`: Emitted when `PipeComponent` (especially main lines) `current_health` reaches 0%. Causes `WaterShortageEvent` in affected areas.
    *   `SanitationSystem`: Processes `WasteComponent`s, handles sewage flow. If blocked or broken, can lead to `DiseaseOutbreakEvent`.
*   **PowerGridSystem**: Manages electricity flow, `PowerLineComponent`s.
    *   `PowerOutageEvent`: Emitted when `PowerLineComponent` `current_health` reaches 0% or demand exceeds supply.

## 17. Labor, Professions, and Socio-Economic Impact

The city's economy is driven by its populace and their work.

### 17.1 Jobs and Skills
*   **JobComponent**: Defined for specific `WorkstationComponent`s (e.g., `Shop`, `Factory`, `Office`).
    *   `JobComponent`: `required_skill_type` (e.g., `Engineering`, `Retail`, `ManualLabor`), `salary_range`, `shift_schedule`, `max_occupants`.
*   **SkillComponent**: `NPCComponent`s possess a set of skills.
    *   `SkillComponent`: `std::map<SkillType, uint32_t> levels;` (e.g., `ENGINEERING: 75`, `RETAIL: 40`).
*   **EmploymentSystem** (Macro Phase):
    *   Matches `UnemployedCitizenComponent`s (Macro Citizens) with available `JobComponent`s based on `SkillComponent` and `HousingPreferenceComponent` (commute distance).
    *   Assigns `NPCComponent`s to `WorkstationComponent`s in micro registries, linking them via `JobAssignmentComponent`.
*   **Unemployment**: If `UnemployedCitizenComponent`s exceed a threshold, `MacroMarketComponent` might reflect increased social unrest or decreased economic output.

### 17.2 Socio-Economic Impact
*   **IncomeComponent**: `NPCComponent`s earn `CurrencyComponent` from `JobComponent`s.
*   **WealthDistributionSystem** (Macro Phase): Analyzes `IncomeComponent`s and `CurrencyComponent`s across all `CitizenComponent`s, informing `MacroMarketComponent` statistics like Gini coefficient or poverty rates.
*   **SocialStratificationSystem**: `NPCComponent`s might be assigned to `SocialClassComponent` based on wealth, job, and family lineage, influencing their `HousingPreferenceComponent`, `ShopInteractionSystem` choices, and `PoliticalAffiliationComponent`.

## 18. Political & Social Dynamics

The city is a hotbed of political intrigue and social movements.

### 18.1 Factions and Influence
*   **FactionComponent**: Macro entities representing political parties, corporations, gangs, or social movements.
    *   `FactionComponent`: `ideology`, `influence` (global metric), `resources` (e.g., `CurrencyComponent`), `leader_id` (links to `CitizenComponent`).
*   **PoliticalAffiliationComponent**: `NPCComponent`s (and `CitizenComponent`s) have allegiances.
    *   `PoliticalAffiliationComponent`: `faction_id`, `loyalty_score`.
*   **InfluenceSystem** (Macro Phase): Tracks how `FactionComponent`s exert influence over `PolicyComponent`s and other `FactionComponent`s through actions like:
    *   `DonationEvent`: `FactionComponent`s donate to `CitizenComponent`s (politicians) or other `FactionComponent`s.
    *   `LobbyingEvent`: Formal (or informal) attempts to sway policy decisions.

### 18.2 Political Campaigns and Promises
*   **CampaignComponent**: Factions or individual `CitizenComponent`s launch campaigns.
    *   `CampaignComponent`: `target_office`, `promises` (list of `PolicyChangeEvent`s they propose), `funding` (from `DonationEvent`s).
*   **ElectionSystem** (Macro Phase): Simulates elections based on `CampaignComponent`s, `NPCComponent` `PoliticalAffiliationComponent`s, and `MoodComponent`s (e.g., satisfaction with current policies).
    *   `ElectionResultEvent`: Determines winners, leading to `PolicyChangeEvent`s based on campaign promises (some are kept, some are broken).
*   **PublicOpinionSystem**: Aggregates `NPCComponent` `MoodComponent`s and `PoliticalAffiliationComponent`s to determine city-wide sentiment towards policies, factions, and leaders.

### 18.3 Backroom Deals and Corruption
*   **CorruptionSystem** (Macro Phase):
    *   `BackroomDealEvent`s: Can be initiated by `FactionComponent`s or powerful `CitizenComponent`s, offering `influence_exchanged` or `CurrencyComponent` in exchange for `PolicyChangeEvent`s, or suppressing `CrimeEvent`s.
    *   `InvestigationEvent`: Triggered by `NPCComponent`s (e.g., journalists, police) or other `FactionComponent`s (rivals), can uncover `BackroomDealEvent`s.
    *   `ScandalEvent`: If `BackroomDealEvent`s are uncovered, it can severely reduce `influence` and `loyalty_score` for involved `FactionComponent`s and `CitizenComponent`s, potentially leading to `ArrestEvent` or `RecallElectionEvent`.

---


## 14. World Generation and Connectivity

### 14.1 Infrastructure Generation Logic
*   **Procedural Connectivity**: Generation algorithms for roads, pathways, bridges, and building entrances must ensure all habitable areas are reachable by at least one valid path for NPCs and vehicles.
*   **Pathfinding Graph Construction**: The `WorldGenerator` will output a `NavigationGraphComponent` (macro) for pathfinding, composed of connected nodes representing key locations, road segments, and building entry points.
*   **Dynamic Obstacles**: Mechanisms for temporary blockades (e.g., construction, broken infrastructure) will update the `NavigationGraphComponent` in real-time.

## 15. Environmental and Resource Management

### 15.1 Wildlife and Ecosystems
*   **CreatureComponent**: Entities for non-sentient life (e.g., rats, birds). Possesses `SpeciesComponent`, `HungerComponent`, `HabitatComponent`.
*   **Food Source Management**: Integration of natural food sources (`ForageNodeComponent`) and waste products (`WasteComponent`) into the resource economy.
*   **EcosystemSystem**: Manages population dynamics, resource consumption by wildlife, and interaction with city infrastructure (e.g., pests).

### 15.2 Sanitation and Utilities
*   **UtilityNetworkComponent (Macro)**: Tracks city-wide availability and flow of water, power, and waste processing capacity. Includes `PollutionLevelComponent`.
*   **PipeSegmentComponent (Micro)**: Details individual pipes, wires, and waste conduits within buildings. `MaterialComponent` (e.g., lead) impacts `WaterQualityComponent`.
*   **SanitationSystem**: Monitors waste accumulation, water quality, and sewage treatment. Can trigger `DiseaseEvent`s based on `PollutionComponent` and `WaterQualityComponent`.
*   **HazardEvents**: `WaterMainBreakEvent`, `PowerOutageEvent` – impacting micro-level `UtilityAllocationComponent`s and triggering `MaintenanceSystem` responses.

## 16. Infrastructure Lifecycle and Maintenance

### 16.1 Decay and Condition
*   **ConditionComponent**: All physical infrastructure entities (roads, pipes, buildings) possess a `ConditionComponent` (`float` 0.0-1.0).
*   **DecaySystem**: Slowly degrades `ConditionComponent` based on material, usage, and environmental factors. Triggers `DegradationEvent` upon reaching certain thresholds.

### 16.2 Maintenance and Repair
*   **MaintenanceSystem**: Processes `DegradationEvent`s to generate `WorkOrderComponent`s (macro/micro) for repairs.
*   **Resource Consumption**: Repairs consume `RawMaterialComponent`s (e.g., steel, concrete) and `LaborComponent` (NPCs with specific skills).
*   **RepairProgressComponent**: Tracks ongoing repairs, linking them to specific `WorkerComponent`s.

## 17. Labor Market and Employment

### 17.1 Job Types and Skills
*   **SkillComponent (NPC)**: Defines an NPC's proficiencies (e.g., "Engineering Lvl 3", "Negotiation Lvl 1").
*   **JobRequirementComponent (Workstation)**: Defines skills and education needed for a specific job at a `WorkstationComponent`.
*   **EmploymentContractComponent**: Links an NPC to a `WorkstationComponent` with terms (salary, hours, benefits).

### 17.2 LaborMarketSystem
*   Manages supply and demand for various job types across the city.
*   Influences NPC decisions regarding job seeking and career changes.
*   Generates `JobOpeningEvent`s and `LayoffEvent`s.

---

**Proposed New Document: `architecture/political_social_simulation.md`**

This document will detail the complex interactions of factions, politicians, public opinion, and influence.
