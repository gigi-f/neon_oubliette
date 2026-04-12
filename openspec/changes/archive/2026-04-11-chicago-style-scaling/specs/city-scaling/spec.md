## ADDED Requirements

### Requirement: Increased Macro Cell Size
The system SHALL increase the default `macro_cell_size` from 20 to 40 tiles. This change SHALL apply to all world generation systems using `WorldConfigComponent`.

#### Scenario: Verify Macro Cell Size
- **WHEN** the system initializes the world configuration
- **THEN** the `macro_cell_size` value is 40 and `chunk_size` is 80.

### Requirement: Enhanced Zone Subdivision
The `CityPlannerSystem` SHALL subdivide zones into blocks and lots using the following "Chicago-Style" counts:
- **URBAN_CORE**: 4x4 blocks, 2x2 lots per block (64 buildings total)
- **RESIDENTIAL**: 4x2 blocks, 6x3 lots per block (144 buildings total)
- **SLUM**: 5x3 blocks, 8x3 lots per block (360 buildings total)
- **CORPORATE**: 2x2 blocks, 2x2 lots per block (16 buildings total)

#### Scenario: Urban Core Subdivision
- **WHEN** a zone of type URBAN_CORE is planned
- **THEN** the zone is divided into a 4x4 grid of blocks, each containing a 2x2 grid of lots.

### Requirement: Chicago Alley Grid
The `CityPlannerSystem` SHALL enforce `back_to_back = true` (mid-block alleys) for URBAN_CORE, CORPORATE, RESIDENTIAL, and SLUM zone types during block-to-lot subdivision.

#### Scenario: Corporate Alley Generation
- **WHEN** a CORPORATE block is subdivided into lots
- **THEN** an alley segment of type `ROAD_ALLEY` is created mid-block between the back-to-back lots.
