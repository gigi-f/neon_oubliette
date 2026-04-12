# Requirements

### Requirement: Scale Separation for Transit
The `TransitSystem` and `CityGenerationSystem` SHALL differentiate between "Enterable" structures and "Infrastructure" when scaling zones:
- **Enterable Structures** (Concourses, Terminals, Control Towers) SHALL use `createBuildingShell` and shrink in footprint based on the current lot size.
- **Infrastructure** (Platform safety tiles, rail tracks, road segments, turnstiles, and scanners) SHALL remain at a 1:1 scale (typically 1-2 tiles wide) regardless of lot density.

#### Scenario: Trans-Station Scaling
- **WHEN** an "Urban Transit Hub" is generated in a scaled URBAN_CORE lot
- **THEN** the concourse structure footprint shrinks according to the lot size, but the platform length and rail tracks remain at their 1:1 scale.
