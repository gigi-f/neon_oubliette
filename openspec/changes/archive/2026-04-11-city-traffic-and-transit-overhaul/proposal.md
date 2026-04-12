## Why

The current city simulation lacks atmospheric density and believable urban flow. While vehicles and transit exist, they function primarily as abstract resource transporters or simple point-to-point movers, leaving the streets feeling empty and static. Furthermore, the city layout is currently "majority road," with roads cutting between every single building, creating a "squished" and repetitive aesthetic. To create a "living" world, we need visible traffic congestion, regulated intersections, and a more diverse urban fabric that supports multi-building "super-blocks" and narrow pedestrian paths.

## What Changes

- **Intersection Management System**: Implementation of functional traffic lights and stop signs at road junctions.
- **Lane-Based Traffic Flow**: Vehicles will respect lanes, follow traffic rules, and maintain safe distances to create "bumper-to-bumper" traffic.
- **Ambient Traffic Spawning**: A system to generate non-essential "filler" vehicles and pedestrians to populate the city based on local density and time of day.
- **Pedestrian Transit Interaction**: NPCs will now demonstrate "hurry" behaviors, running to catch departing trains or buses, and clustering at stops.
- **Multi-Car Train Simulation**: Transition from single-glyph trains to multi-car L-trains that roll through the city.
- **Super-block Subdivision**: **BREAKING** Modification to the `CityPlannerSystem` to group buildings into "super-blocks" where 2-4 buildings can sit flush against each other without intermediate roads.
- **Pedestrian Path Integration**: Substitution of some secondary roads with 1-2 tile wide pedestrian-only paths/alleys.
- **Traffic Sound/Visual Feedback**: (Optional/Secondary) Adding ambient city noise and clearer visual indicators for traffic states.

## Capabilities

### New Capabilities
- `traffic-intersection-management`: Logic for traffic lights, stop signs, and right-of-way at junctions.
- `ambient-city-traffic`: Systems for spawning and despawning non-essential vehicles and pedestrians to maintain "busy" visuals.
- `pedestrian-transit-interaction`: AI behaviors for NPCs navigating to/from stations, including "running for the ride" states.
- `super-block-urban-planning`: Logic for grouping lots into contiguous blocks and placing pedestrian paths instead of roads.

### Modified Capabilities
- `vehicle-dimensions`: Extend to support multi-car/articulated vehicles (trains and buses).
- `transit-infrastructure-scaling`: Add requirements for bus stops and train platforms to handle higher pedestrian density and visual "wait" zones.

## Impact

- **ECS Systems**: Major updates to `TrafficFlowSystem`, `TransitSystem`, and `CityPlannerSystem`; new `IntersectionSystem` and `AmbientSpawnSystem`.
- **Components**: New `TrafficLightComponent`, `LaneComponent`, and updates to `VehicleComponent`.
- **World Generation**: `CityGenerationSystem` will need to place traffic lights and handle new block/path configurations.
- **Performance**: Increased entity count due to ambient traffic; will require efficient LOD (Level of Detail) handling.
