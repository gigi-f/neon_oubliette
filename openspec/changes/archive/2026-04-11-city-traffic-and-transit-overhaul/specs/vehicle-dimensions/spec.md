## MODIFIED Requirements

### Requirement: Multi-tile Vehicle Sizes
The system SHALL support multi-tile vehicle dimensions and articulated vehicle types:
- **Cars** SHALL be 6 tiles long.
- **Train cars** SHALL be 12 tiles long.
- **Trains** SHALL consist of multiple articulated train cars linked together.
- **Articulated Buses** SHALL consist of two segments linked via a flexible joint.
Vehicles SHALL be spawned as multi-tile obstacles on road tiles and track tiles, with articulated segments following the lead segment's path.

#### Scenario: Multi-car Train Spawning
- **WHEN** an L-train is spawned on a rail line
- **THEN** it SHALL consist of at least three 12-tile long articulated cars that follow the leader's exact track sequence.
