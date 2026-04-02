# Configuration Management Architecture

## 1. Guiding Principles
*   **Data-Driven Design**: Maximize external configuration over hardcoded values for flexibility and modding.
*   **Human-Readable Formats**: Utilize JSON or YAML for ease of authoring and review.
*   **Schema Validation**: Ensure data integrity and prevent runtime errors through validation during loading.
*   **Layered Configuration**: Support default values, mod overrides, and runtime adjustments.

## 2. Configuration Formats
*   **Primary**: JSON (JavaScript Object Notation) for most structured data. Widely supported, good for machine parsing.
*   **Alternative**: YAML (YAML Ain't Markup Language) for more human-centric configurations, especially for larger, more narrative-heavy or script-like definitions (e.g., NPC behavior trees, quest definitions).

## 3. Categories of Configurable Data

### 3.1 World Generation & Map Data
*   **`world_generator_params.json`**:
    *   `city_seed`: Initial seed for procedural generation.
    *   `size_x`, `size_y`, `num_districts`: Dimensions and subdivisions.
    *   `terrain_types`: Noise function parameters for ground, water, elevation.
    *   `resource_node_distribution`: Placement rules for raw material nodes.
*   **`building_blueprints/*.json`**:
    *   Defines `BuildingComponent` properties: `height`, `zoning`, `base_capacity`, `default_floor_layout_id`.
    *   References `floor_layouts/*.json`.
*   **`road_templates/*.json`**:
    *   Defines `RoadComponent` properties: `geometry_pattern`, `max_speed_default`, `material_type`.

### 3.2 ECS Component Defaults & Archetypes
*   **`component_defaults/*.json`**:
    *   Provides initial values for various components when new entities are created.
    *   E.g., `DefaultNPCComponent.json` might include base `mood`, `inventory_size`.
    *   E.g., `DefaultConditionComponent.json` might include `initial_value = 1.0`, `base_decay_rate = 0.001`.
*   **`archetypes/*.json`**:
    *   Combines multiple component defaults to define common entity types.
    *   E.g., `CitizenArchetype.json` might specify `NPCComponent`, `SkillComponent` (empty), `HungerComponent` (default value).
    *   E.g., `TruckArchetype.json` might specify `VehicleComponent`, `PositionComponent`.

### 3.3 Game Rules & Parameters
*   **`game_rules.json`**:
    *   `simulation_speed_multiplier`: Global tick rate.
    *   `tax_rates`: Default `TaxationComponent` values.
    *   `max_population_density`.
    *   `event_trigger_thresholds`: Conditions for specific `MacroEventBus` events.
*   **`resource_definitions.json`**:
    *   Defines `ResourceType` properties: `base_value`, `rarity`, `stackable`.
    *   Includes `production_chains` (inputs/outputs for crafting/manufacturing).

### 3.4 Behavior Definitions
*   **`npc_behavior_trees/*.yaml`**:
    *   Complex decision-making logic for NPCs.
    *   Hierarchical structures defining goals, actions, and conditions.
    *   Leverages a custom domain-specific language (DSL) for behavior.
*   **`faction_agendas/*.yaml`**:
    *   Defines goals, preferred policies, and interaction rules for `FactionComponent`s.

## 4. Configuration Loading Process

### 4.1 `ConfigLoader` Module
*   A dedicated C++ module (`src/config/ConfigLoader.h/.cpp`) responsible for reading and parsing configuration files.
*   Utilizes a library like `jsoncpp` or `yaml-cpp` for parsing.
*   Includes schema validation against predefined JSON schemas (e.g., using `nlohmann/json-schema-validator`).

### 4.2 Loading Phases
1.  **Core Defaults**: Load `game_rules.json`, `resource_definitions.json`, and `component_defaults/*.json`.
2.  **World Generation**: Load `world_generator_params.json` and generate the initial city structure.
3.  **Building Blueprints**: Load `building_blueprints/*.json` and `floor_layouts/*.json` as needed by the generator.
4.  **Archetypes**: Load `archetypes/*.json` to define entity templates.
5.  **Behavior & Faction Data**: Load `npc_behavior_trees/*.yaml` and `faction_agendas/*.yaml`.

### 4.3 Integration with ECS
*   `ConfigLoader` populates `entt::registry` with entities and components based on archetype definitions.
*   For complex data like behavior trees, `ConfigLoader` parses the YAML and constructs the internal representation for the `BehaviorTreeComponent`.
*   A `RuntimeConfig` context component can hold global game parameters loaded from `game_rules.json`.

## 5. Modding and Overrides
*   The `ConfigLoader` will support a hierarchical loading order (e.g., `base_data/` -> `mod_data/mod_A/` -> `mod_data/mod_B/`).
*   New or modified configuration files in mod directories will override defaults or previous mod definitions.
*   Schema validation will be applied to mod data to ensure compatibility.
