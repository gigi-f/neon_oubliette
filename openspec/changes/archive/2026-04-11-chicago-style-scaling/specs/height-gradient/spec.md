## ADDED Requirements

### Requirement: Distance-Based Building Height
The `CityGenerationSystem` SHALL determine building floor counts based on distance to the `URBAN_CORE` according to the following gradient:
- **URBAN_CORE**: 30 to 150 floors
- **CORPORATE** (dist 1-2): 15 to 50 floors
- **COMMERCIAL** (dist 2-3): 5 to 20 floors
- **RESIDENTIAL** (dist 3-5): 3 to 8 floors
- **SLUM** (dist 5+): 1 to 4 floors
- **INDUSTRIAL**: 1 to 5 floors

#### Scenario: Residential Height Gradient
- **WHEN** a building is generated in a RESIDENTIAL zone at distance 4 from the core
- **THEN** the floor count is between 3 and 8 based on the gradient formula.

### Requirement: Building ID Uniqueness
The system SHALL calculate building `stable_id` using a 64-bit hash (e.g., `(uint64_t)bx << 32 | (uint32_t)by`) to prevent collisions at higher lot densities.

#### Scenario: Unique Stable IDs
- **WHEN** multiple buildings are generated in close proximity
- **THEN** each building is assigned a unique `stable_id` that is used to retrieve its interior.
