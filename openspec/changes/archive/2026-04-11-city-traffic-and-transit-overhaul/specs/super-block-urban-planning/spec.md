## ADDED Requirements

### Requirement: Super-block Building Clusters
The `CityPlannerSystem` SHALL support "super-blocks" where 2 to 4 buildings are placed directly adjacent to each other with no roads between them. Buildings in these clusters SHALL share a common lot boundary.

#### Scenario: Dense Block Generation
- **WHEN** the `CityPlannerSystem` subdivides an URBAN_CORE macro zone
- **THEN** it SHALL occasionally (based on density params) group 4 building lots into a 2x2 cluster with zero-tile road separation between them.

### Requirement: Pedestrian Alleys
The system SHALL support narrow (1-2 tile wide) pedestrian paths that replace standard 6-tile wide secondary roads in designated areas. These paths SHALL only be accessible by pedestrians and narrow personal vehicles (e.g., scooters).

#### Scenario: Narrow Alley Placement
- **WHEN** a secondary road would normally be placed between building clusters
- **THEN** the system SHALL have a chance to substitute that road with a 2-tile wide `PEDESTRIAN_PATH` terrain.
