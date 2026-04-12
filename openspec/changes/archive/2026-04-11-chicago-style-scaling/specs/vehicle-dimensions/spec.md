## ADDED Requirements

### Requirement: Multi-tile Vehicle Sizes
The system SHALL support multi-tile vehicle dimensions:
- **Cars** SHALL be 6 tiles long.
- **Train cars** SHALL be 12 tiles long.
Vehicles SHALL be spawned as multi-tile obstacles on road tiles and track tiles.

#### Scenario: Car Obstacle Spawning
- **WHEN** a car is spawned on a road
- **THEN** it occupies 6 tiles as an obstacle.

### Requirement: Road Width Standards
Primary and secondary roads SHALL be at least 6 tiles wide to accommodate 2 lanes of 3-tile wide vehicles.

#### Scenario: Primary Road Width
- **WHEN** a `ROAD_PRIMARY` segment is generated
- **THEN** its width (ArterialType width or `ROAD_WIDTH_PRIMARY`) is at least 6 tiles.
